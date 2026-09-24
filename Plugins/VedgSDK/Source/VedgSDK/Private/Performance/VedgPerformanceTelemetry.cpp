// Fill out your copyright notice in the Description page of Project Settings.

#include "Performance/VedgPerformanceTelemetry.h"
#include "Http/VedgEventQueue.h"
#include "Identity/VedgMachineIdentity.h"
#include "VedgSDKSettings.h"
#include "Dom/JsonObject.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"

void UVedgPerformanceTelemetry::StartUp(UVedgEventQueue* InEventQueue, const FString& InSessionId)
{
	EventQueue = InEventQueue;
	SessionId = InSessionId;
	FrameTimeMin = TNumericLimits<double>::Max();
	FrameTimeMax = 0.0;

	EndFrameHandle = FCoreDelegates::OnEndFrame.AddUObject(this, &UVedgPerformanceTelemetry::OnEndFrame);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVedgPerformanceTelemetry::Tick), FlushInterval);
}

void UVedgPerformanceTelemetry::Shutdown()
{
	if (EndFrameHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(EndFrameHandle);
		EndFrameHandle.Reset();
	}
	if (TickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Flush();
}

void UVedgPerformanceTelemetry::OnEndFrame()
{
	const double DeltaTime = FApp::GetDeltaTime();
	FrameCount += 1;
	FrameTimeSum += DeltaTime;
	FrameTimeMin = FMath::Min(FrameTimeMin, DeltaTime);
	FrameTimeMax = FMath::Max(FrameTimeMax, DeltaTime);
}

bool UVedgPerformanceTelemetry::Tick(float)
{
	Flush();
	return true;
}

void UVedgPerformanceTelemetry::Flush()
{
	if (FrameCount == 0 || !EventQueue)
	{
		return;
	}

	const double FrameTimeAvg = FrameTimeSum / FrameCount;
	const double FpsAvg = FrameTimeAvg > 0.0 ? 1.0 / FrameTimeAvg : 0.0;
	const double FpsMin = FrameTimeMax > 0.0 ? 1.0 / FrameTimeMax : 0.0;
	const double FpsMax = FrameTimeMin > 0.0 ? 1.0 / FrameTimeMin : 0.0;

	const TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
	Sample->SetNumberField(TEXT("t"), FDateTime::UtcNow().ToUnixTimestampDecimal());
	Sample->SetNumberField(TEXT("fps_avg"), FpsAvg);
	Sample->SetNumberField(TEXT("fps_min"), FpsMin);
	Sample->SetNumberField(TEXT("fps_max"), FpsMax);
	Sample->SetNumberField(TEXT("frame_time_avg_ms"), FrameTimeAvg * 1000.0);
	Sample->SetNumberField(TEXT("frame_time_max_ms"), FrameTimeMax * 1000.0);

	TArray<TSharedPtr<FJsonValue>> Samples;
	Samples.Add(MakeShared<FJsonValueObject>(Sample));

	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("samples"), Samples);

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game"), UVedgSDKSettings::Get()->GetGameSlug());
	Body->SetStringField(TEXT("type"), TEXT("fps"));
	Body->SetStringField(TEXT("session_id"), SessionId);
	Body->SetStringField(TEXT("machine_id"), FVedgMachineIdentity::GetMachineId());
	Body->SetObjectField(TEXT("data"), Data);

	EventQueue->Enqueue(TEXT("/reports/event"), Body);

	FrameCount = 0;
	FrameTimeSum = 0.0;
	FrameTimeMin = TNumericLimits<double>::Max();
	FrameTimeMax = 0.0;
}
