// Fill out your copyright notice in the Description page of Project Settings.

#include "Heatmap/VedgHeatmapCapture.h"
#include "Http/VedgEventQueue.h"
#include "Http/VedgHttpRequest.h"
#include "VedgSDKModule.h"
#include "VedgSDKSettings.h"
#include "VedgSDKSubsystem.h"
#include "Async/Async.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Tags every logged error as a heatmap "error" event, like VedgLogger.err().
class FVedgErrorDevice : public FOutputDevice
{
public:
	explicit FVedgErrorDevice(UVedgHeatmapCapture* InOwner) : Owner(InOwner) {}

	virtual void Serialize(const TCHAR*, ELogVerbosity::Type Verbosity, const FName&) override
	{
		if ((Verbosity & ELogVerbosity::VerbosityMask) != ELogVerbosity::Error)
		{
			return;
		}
		TWeakObjectPtr<UVedgHeatmapCapture> WeakOwner = Owner;
		AsyncTask(ENamedThreads::GameThread, [WeakOwner]()
		{
			UVedgHeatmapCapture* Capture = WeakOwner.Get();
			if (Capture && Capture->IsActive())
			{
				Capture->RecordEvent(TEXT("error"));
			}
		});
	}

	virtual bool CanBeUsedOnAnyThread() const override { return true; }

private:
	TWeakObjectPtr<UVedgHeatmapCapture> Owner;
};

namespace
{
	FString ToJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}
}

void UVedgHeatmapCapture::StartUp(UVedgEventQueue* InEventQueue)
{
	EventQueue = InEventQueue;
	ErrorDevice = MakeShared<FVedgErrorDevice>(this);
	GLog->AddOutputDevice(ErrorDevice.Get());
}

void UVedgHeatmapCapture::Shutdown()
{
	if (ErrorDevice.IsValid())
	{
		GLog->RemoveOutputDevice(ErrorDevice.Get());
		ErrorDevice.Reset();
	}
	if (FlushHandle.IsValid())
	{
		FTSTicker::RemoveTicker(FlushHandle);
		FlushHandle.Reset();
	}
}

void UVedgHeatmapCapture::StartCapture(const FString& MapId, float CellSize, int32 MapVersion)
{
	Start(MapId, CellSize, MapVersion, nullptr);
}

void UVedgHeatmapCapture::EndCapture()
{
	Stop(nullptr);
}

void UVedgHeatmapCapture::ResetTime()
{
	if (SessionId.IsEmpty())
	{
		return;
	}
	const FString MapId = CurrentMapId;
	const float CellSize = CurrentCellSize;
	const int32 MapVersion = CurrentMapVersion;
	TWeakObjectPtr<UVedgHeatmapCapture> WeakThis(this);
	Stop([WeakThis, MapId, CellSize, MapVersion]()
	{
		if (UVedgHeatmapCapture* Self = WeakThis.Get())
		{
			Self->Start(MapId, CellSize, MapVersion, nullptr);
		}
	});
}

void UVedgHeatmapCapture::Start(const FString& MapId, float CellSize, int32 MapVersion, TFunction<void()> OnDone)
{
	if (!SessionId.IsEmpty() || bStarting)
	{
		return;
	}
	const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();
	if (Settings->GetApiUrl().IsEmpty())
	{
		UE_LOG(LogVedgSDK, Error, TEXT("cannot start heatmap capture, api_url is empty"));
		return;
	}

	CurrentMapId = MapId;
	CurrentMapVersion = MapVersion;
	CurrentCellSize = CellSize;
	bHasLastPosition = false;
	bStarting = true;

	// started_at is on this client's own monotonic clock, like every sample.
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game"), Settings->GetGameSlug());
	Body->SetStringField(TEXT("map_id"), MapId);
	Body->SetNumberField(TEXT("map_version"), MapVersion);
	Body->SetNumberField(TEXT("started_at"), (double)UVedgSDKSubsystem::NowMs());

	TWeakObjectPtr<UVedgHeatmapCapture> WeakThis(this);
	FVedgHttpRequest::Request(Settings->GetApiUrl() + TEXT("/reports/heatmap/sessions/start"), TEXT("POST"), ToJson(Body), [WeakThis, OnDone](FVedgHttpResult Result)
	{
		UVedgHeatmapCapture* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		Self->bStarting = false;
		FString NewSessionId;
		const TSharedPtr<FJsonValue> Field = Result.ResponseObject.IsValid() ? Result.ResponseObject->TryGetField(TEXT("session_id")) : nullptr;
		if (!Result.bValid || !Field.IsValid() || !Field->TryGetString(NewSessionId))
		{
			UE_LOG(LogVedgSDK, Error, TEXT("heatmap session start failed"));
			return;
		}
		Self->SessionId = NewSessionId;
		Self->FlushHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(Self, &UVedgHeatmapCapture::FlushTick), FlushIntervalSec);
		if (OnDone)
		{
			OnDone();
		}
	});
}

void UVedgHeatmapCapture::Stop(TFunction<void()> OnDone)
{
	if (SessionId.IsEmpty())
	{
		return;
	}

	const FString Ending = SessionId;
	if (FlushHandle.IsValid())
	{
		FTSTicker::RemoveTicker(FlushHandle);
		FlushHandle.Reset();
	}
	SessionId.Reset();
	bHasLastPosition = false;

	Flush(Ending, [Ending, OnDone]()
	{
		const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();
		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("game"), Settings->GetGameSlug());
		FVedgHttpRequest::Request(Settings->GetApiUrl() + TEXT("/reports/heatmap/sessions/") + Ending + TEXT("/end"), TEXT("POST"), ToJson(Body), [OnDone](FVedgHttpResult)
		{
			if (OnDone)
			{
				OnDone();
			}
		});
	});
}

void UVedgHeatmapCapture::RecordEvent(const FString& Event)
{
	Record(Event, MakeShared<FJsonValueNull>());
}

void UVedgHeatmapCapture::RecordEventValue(const FString& Event, float Value)
{
	Record(Event, MakeShared<FJsonValueNumber>(Value));
}

// Through the persisted outbox: the only path that survives an event fired right as the game closes.
void UVedgHeatmapCapture::Record(const FString& Event, const TSharedPtr<FJsonValue>& Value)
{
	if (SessionId.IsEmpty() || !EventQueue)
	{
		UE_LOG(LogVedgSDK, Warning, TEXT("heatmap RecordEvent(\"%s\") called with no active capture session"), *Event);
		return;
	}

	const TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
	Sample->SetNumberField(TEXT("ts"), (double)UVedgSDKSubsystem::NowMs());
	Sample->SetField(TEXT("value"), Value);

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game"), UVedgSDKSettings::Get()->GetGameSlug());
	Body->SetStringField(TEXT("session_id"), SessionId);
	Body->SetStringField(TEXT("event"), Event);
	TArray<TSharedPtr<FJsonValue>> Samples;
	Samples.Add(MakeShared<FJsonValueObject>(Sample));
	Body->SetArrayField(TEXT("samples"), Samples);
	EventQueue->Enqueue(TEXT("/reports/heatmap/events"), Body);
}

void UVedgHeatmapCapture::ReportPosition(const FVector& Position)
{
	if (SessionId.IsEmpty())
	{
		return;
	}

	const int64 Now = UVedgSDKSubsystem::NowMs();
	if (bHasLastPosition && Now - LastSampleAtMs < SampleIntervalMs)
	{
		return;
	}

	if (!bHasLastPosition)
	{
		bHasLastPosition = true;
		LastSampleAtMs = Now;
		LastPosition = Position;
		LastVoxel = VoxelOf(Position);
		EnqueueVoxel(LastVoxel, Now);
		return;
	}

	const FVector From = LastPosition;
	const int64 FromTs = LastSampleAtMs;
	LastPosition = Position;
	LastSampleAtMs = Now;
	EnqueuePath(From, Position, FromTs, Now);
}

FIntVector UVedgHeatmapCapture::VoxelOf(const FVector& Pos) const
{
	return FIntVector(FMath::FloorToInt(Pos.X / CurrentCellSize), FMath::FloorToInt(Pos.Y / CurrentCellSize), FMath::FloorToInt(Pos.Z / CurrentCellSize));
}

// Each crossed voxel gets a timestamp interpolated along the path, so
// nearest-ts correlation (fps, events) lands where the player actually was.
void UVedgHeatmapCapture::EnqueuePath(const FVector& From, const FVector& To, int64 FromTs, int64 ToTs)
{
	const double Dist = FVector::Dist(From, To);
	if (Dist > MaxInterpolateDistance)
	{
		const FIntVector Voxel = VoxelOf(To);
		if (LastVoxel != Voxel)
		{
			LastVoxel = Voxel;
			EnqueueVoxel(Voxel, ToTs);
		}
		return;
	}

	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Dist / (CurrentCellSize * 0.5)));
	for (int32 i = 0; i <= Steps; ++i)
	{
		const double T = (double)i / Steps;
		const FIntVector Voxel = VoxelOf(FMath::Lerp(From, To, T));
		if (LastVoxel == Voxel)
		{
			continue;
		}
		LastVoxel = Voxel;
		EnqueueVoxel(Voxel, FMath::RoundToInt64(FMath::Lerp((double)FromTs, (double)ToTs, T)));
	}
}

void UVedgHeatmapCapture::EnqueueVoxel(const FIntVector& Voxel, int64 Ts)
{
	if (Buffer.Num() >= MaxBufferEntries)
	{
		Buffer.RemoveAt(0);
	}
	const TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
	Sample->SetNumberField(TEXT("x"), (Voxel.X + 0.5) * CurrentCellSize);
	Sample->SetNumberField(TEXT("y"), (Voxel.Y + 0.5) * CurrentCellSize);
	Sample->SetNumberField(TEXT("z"), (Voxel.Z + 0.5) * CurrentCellSize);
	Sample->SetNumberField(TEXT("ts"), (double)Ts);
	Buffer.Add(MakeShared<FJsonValueObject>(Sample));
}

bool UVedgHeatmapCapture::FlushTick(float)
{
	Flush(SessionId);
	return true;
}

void UVedgHeatmapCapture::Flush(const FString& ForSessionId, TFunction<void()> OnDone)
{
	if (Buffer.Num() == 0 || ForSessionId.IsEmpty())
	{
		if (OnDone)
		{
			OnDone();
		}
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Batch = MoveTemp(Buffer);
	Buffer.Reset();
	const int32 Count = Batch.Num();

	const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game"), Settings->GetGameSlug());
	Body->SetStringField(TEXT("session_id"), ForSessionId);
	Body->SetStringField(TEXT("event"), TEXT("movement"));
	Body->SetArrayField(TEXT("samples"), Batch);

	FVedgHttpRequest::Request(Settings->GetApiUrl() + TEXT("/reports/heatmap/positions"), TEXT("POST"), ToJson(Body), [Count, OnDone](FVedgHttpResult Result)
	{
		if (!Result.bValid)
		{
			UE_LOG(LogVedgSDK, Warning, TEXT("heatmap positions flush failed, dropping batch of %d samples"), Count);
		}
		if (OnDone)
		{
			OnDone();
		}
	});
}
