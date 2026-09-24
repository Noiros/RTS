// Fill out your copyright notice in the Description page of Project Settings.

#include "Http/VedgEventQueue.h"
#include "Http/VedgHttpRequest.h"
#include "VedgSDKSettings.h"
#include "VedgSDKModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FString GetVedgSavedDir()
	{
		return FPaths::ProjectSavedDir() / TEXT("VedgSDK");
	}
}

FString UVedgEventQueue::GetOutboxFilePath()
{
	return GetVedgSavedDir() / TEXT("outbox.json");
}

FString UVedgEventQueue::GetAnalyticsFilePath()
{
	return GetVedgSavedDir() / TEXT("analytics_outbox.json");
}

void UVedgEventQueue::StartUp()
{
	IFileManager::Get().MakeDirectory(*GetVedgSavedDir(), true);
	LoadState();

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVedgEventQueue::Tick), FlushInterval);
}

void UVedgEventQueue::Shutdown()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	SaveState();
}

void UVedgEventQueue::Enqueue(const FString& Path, TSharedPtr<FJsonObject> Body)
{
	if (UVedgSDKSettings::Get()->GetApiUrl().IsEmpty())
	{
		return;
	}

	FVedgOutboxEntry Entry;
	Entry.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Entry.Path = Path;
	Entry.Body = Body;
	Entry.CreatedAt = FDateTime::UtcNow().ToUnixTimestampDecimal();
	Entry.Attempts = 0;
	Entry.NextAttemptAt = 0.0;

	Outbox.Add(Entry);
	if (Outbox.Num() > MaxOutboxEntries)
	{
		UE_LOG(LogVedgSDK, Warning, TEXT("Outbox full (%d), dropping oldest pending report"), MaxOutboxEntries);
		Outbox.RemoveAt(0);
	}

	bDirty = true;
	SaveState();
}

void UVedgEventQueue::EnqueueAnalyticsEvent(const FVedgAnalyticsEvent& InEvent)
{
	if (UVedgSDKSettings::Get()->GetApiUrl().IsEmpty())
	{
		return;
	}

	FVedgAnalyticsEvent Event = InEvent;
	Event.CreatedAt = FDateTime::UtcNow().ToUnixTimestampDecimal();

	Analytics.Add(Event);
	if (Analytics.Num() > MaxAnalyticsEntries)
	{
		UE_LOG(LogVedgSDK, Warning, TEXT("Analytics outbox full (%d), dropping oldest pending event"), MaxAnalyticsEntries);
		Analytics.RemoveAt(0);
	}

	bDirty = true;
	SaveState();
}

bool UVedgEventQueue::Tick(float)
{
	Flush();
	return true;
}

void UVedgEventQueue::Flush()
{
	if (bFlushing)
	{
		return;
	}
	bFlushing = true;
	PendingRequests = 0;

	FlushOutbox();
	FlushAnalytics();

	FinishFlushIfIdle();
}

void UVedgEventQueue::FinishFlushIfIdle()
{
	if (PendingRequests <= 0)
	{
		bFlushing = false;
		if (bDirty)
		{
			SaveState();
		}
	}
}

void UVedgEventQueue::FlushOutbox()
{
	if (Outbox.Num() == 0)
	{
		return;
	}

	const FString ApiUrl = UVedgSDKSettings::Get()->GetApiUrl();
	const double Now = FDateTime::UtcNow().ToUnixTimestampDecimal();

	int32 Sent = 0;
	for (const FVedgOutboxEntry& Entry : Outbox)
	{
		if (Sent >= MaxOutboxPerFlush)
		{
			break;
		}
		if (Entry.NextAttemptAt > Now)
		{
			continue;
		}
		++Sent;

		FString Body;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
		FJsonSerializer::Serialize(Entry.Body.ToSharedRef(), Writer);

		const FString Url = ApiUrl + Entry.Path;
		const FString EntryId = Entry.Id;

		++PendingRequests;
		TWeakObjectPtr<UVedgEventQueue> WeakThis(this);
		FVedgHttpRequest::Request(Url, TEXT("POST"), Body,
			[WeakThis, EntryId](FVedgHttpResult Result)
			{
				if (UVedgEventQueue* Self = WeakThis.Get())
				{
					Self->OnOutboxEntryComplete(EntryId, Result);
				}
			});
	}
}

void UVedgEventQueue::OnOutboxEntryComplete(const FString& EntryId, const FVedgHttpResult& Result)
{
	const int32 Index = Outbox.IndexOfByPredicate([&EntryId](const FVedgOutboxEntry& E) { return E.Id == EntryId; });
	if (Index != INDEX_NONE)
	{
		FVedgOutboxEntry& Entry = Outbox[Index];
		const double Now = FDateTime::UtcNow().ToUnixTimestampDecimal();

		if (Result.bValid)
		{
			FString ReportType;
			if (Entry.Body.IsValid())
			{
				Entry.Body->TryGetStringField(TEXT("type"), ReportType);
			}
			const FString Suffix = ReportType.IsEmpty() ? FString() : FString::Printf(TEXT(" (type=%s)"), *ReportType);
			UE_LOG(LogVedgSDK, Log, TEXT("Delivered report to %s%s"), *Entry.Path, *Suffix);
			Outbox.RemoveAt(Index);
			bDirty = true;
		}
		else if (Result.StatusCode >= 400 && Result.StatusCode < 500)
		{
			UE_LOG(LogVedgSDK, Warning, TEXT("Dropping report to %s after client error %d (won't succeed on retry)"), *Entry.Path, Result.StatusCode);
			Outbox.RemoveAt(Index);
			bDirty = true;
		}
		else if (Now - Entry.CreatedAt > MaxEntryAgeSec)
		{
			UE_LOG(LogVedgSDK, Warning, TEXT("Dropping report to %s after %d attempts (too old)"), *Entry.Path, Entry.Attempts);
			Outbox.RemoveAt(Index);
			bDirty = true;
		}
		else
		{
			Entry.Attempts += 1;
			Entry.NextAttemptAt = Now + Backoff(Entry.Attempts);
			bDirty = true;
		}
	}

	--PendingRequests;
	FinishFlushIfIdle();
}

void UVedgEventQueue::FlushAnalytics()
{
	if (Analytics.Num() == 0)
	{
		return;
	}

	const double Now = FDateTime::UtcNow().ToUnixTimestampDecimal();
	if (Now < AnalyticsRetryAt)
	{
		return;
	}

	Analytics.RemoveAll([Now](const FVedgAnalyticsEvent& E) { return Now - E.CreatedAt > MaxEntryAgeSec; });
	if (Analytics.Num() == 0)
	{
		return;
	}

	const int32 BatchSize = FMath::Min(AnalyticsBatchSize, Analytics.Num());

	TArray<TSharedPtr<FJsonValue>> EventsJson;
	for (int32 i = 0; i < BatchSize; ++i)
	{
		EventsJson.Add(AnalyticsEventToWireJson(Analytics[i]));
	}

	const TSharedRef<FJsonObject> BodyObject = MakeShared<FJsonObject>();
	BodyObject->SetStringField(TEXT("game"), UVedgSDKSettings::Get()->GetGameSlug());
	BodyObject->SetArrayField(TEXT("events"), EventsJson);

	FString BodyString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(BodyObject, Writer);

	const FString Url = UVedgSDKSettings::Get()->GetApiUrl() + TEXT("/analytics/events");

	++PendingRequests;
	TWeakObjectPtr<UVedgEventQueue> WeakThis(this);
	FVedgHttpRequest::Request(Url, TEXT("POST"), BodyString,
		[WeakThis, BatchSize](FVedgHttpResult Result)
		{
			if (UVedgEventQueue* Self = WeakThis.Get())
			{
				Self->OnAnalyticsFlushComplete(BatchSize, Result);
			}
		});
}

void UVedgEventQueue::OnAnalyticsFlushComplete(int32 BatchSize, const FVedgHttpResult& Result)
{
	if (Result.bValid || (Result.StatusCode >= 400 && Result.StatusCode < 500))
	{
		if (!Result.bValid)
		{
			UE_LOG(LogVedgSDK, Warning, TEXT("Dropping %d analytics event(s) after client error %d (won't succeed on retry)"), BatchSize, Result.StatusCode);
		}
		else
		{
			UE_LOG(LogVedgSDK, Log, TEXT("Delivered %d analytics event(s)"), BatchSize);
		}
		Analytics.RemoveAt(0, BatchSize);
		AnalyticsFailures = 0;
		AnalyticsRetryAt = 0.0;
		bDirty = true;
	}
	else
	{
		AnalyticsFailures += 1;
		AnalyticsRetryAt = FDateTime::UtcNow().ToUnixTimestampDecimal() + Backoff(AnalyticsFailures);
	}

	--PendingRequests;
	FinishFlushIfIdle();
}

double UVedgEventQueue::Backoff(int32 Attempts)
{
	const double Base = FMath::Min(BaseBackoffSec * FMath::Pow(2.0, (double)(Attempts - 1)), MaxBackoffSec);
	return Base + FMath::FRand() * BaseBackoffSec;
}

void UVedgEventQueue::SaveState()
{
	TArray<TSharedPtr<FJsonValue>> OutboxJson;
	for (const FVedgOutboxEntry& Entry : Outbox)
	{
		OutboxJson.Add(EntryToStorageJson(Entry));
	}
	SaveJsonArray(GetOutboxFilePath(), OutboxJson);

	TArray<TSharedPtr<FJsonValue>> AnalyticsJson;
	for (const FVedgAnalyticsEvent& Event : Analytics)
	{
		AnalyticsJson.Add(AnalyticsEventToStorageJson(Event));
	}
	SaveJsonArray(GetAnalyticsFilePath(), AnalyticsJson);

	bDirty = false;
}

void UVedgEventQueue::Reset()
{
	Outbox.Empty();
	Analytics.Empty();
	AnalyticsRetryAt = 0.0;
	AnalyticsFailures = 0;
	SaveState();
}

void UVedgEventQueue::LoadState()
{
	Outbox.Empty();
	for (const TSharedPtr<FJsonValue>& Value : LoadJsonArray(GetOutboxFilePath()))
	{
		Outbox.Add(EntryFromStorageJson(Value));
	}

	Analytics.Empty();
	for (const TSharedPtr<FJsonValue>& Value : LoadJsonArray(GetAnalyticsFilePath()))
	{
		Analytics.Add(AnalyticsEventFromStorageJson(Value));
	}
}

void UVedgEventQueue::SaveJsonArray(const FString& FilePath, const TArray<TSharedPtr<FJsonValue>>& Array)
{
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Array, Writer);
	if (!FFileHelper::SaveStringToFile(Output, *FilePath))
	{
		UE_LOG(LogVedgSDK, Error, TEXT("Failed to persist %s"), *FilePath);
	}
}

TArray<TSharedPtr<FJsonValue>> UVedgEventQueue::LoadJsonArray(const FString& FilePath)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	if (!FPaths::FileExists(FilePath))
	{
		return Result;
	}

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FilePath))
	{
		return Result;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	FJsonSerializer::Deserialize(Reader, Result);
	return Result;
}

TSharedPtr<FJsonValue> UVedgEventQueue::EntryToStorageJson(const FVedgOutboxEntry& Entry)
{
	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("id"), Entry.Id);
	Obj->SetStringField(TEXT("path"), Entry.Path);
	if (Entry.Body.IsValid())
	{
		Obj->SetObjectField(TEXT("body"), Entry.Body);
	}
	Obj->SetNumberField(TEXT("created_at"), Entry.CreatedAt);
	Obj->SetNumberField(TEXT("attempts"), Entry.Attempts);
	Obj->SetNumberField(TEXT("next_attempt_at"), Entry.NextAttemptAt);
	return MakeShared<FJsonValueObject>(Obj);
}

FVedgOutboxEntry UVedgEventQueue::EntryFromStorageJson(const TSharedPtr<FJsonValue>& Value)
{
	FVedgOutboxEntry Entry;
	const TSharedPtr<FJsonObject>* Obj;
	if (!Value.IsValid() || !Value->TryGetObject(Obj))
	{
		return Entry;
	}

	(*Obj)->TryGetStringField(TEXT("id"), Entry.Id);
	(*Obj)->TryGetStringField(TEXT("path"), Entry.Path);
	const TSharedPtr<FJsonObject>* BodyObj;
	if ((*Obj)->TryGetObjectField(TEXT("body"), BodyObj))
	{
		Entry.Body = *BodyObj;
	}
	(*Obj)->TryGetNumberField(TEXT("created_at"), Entry.CreatedAt);
	(*Obj)->TryGetNumberField(TEXT("attempts"), Entry.Attempts);
	(*Obj)->TryGetNumberField(TEXT("next_attempt_at"), Entry.NextAttemptAt);
	return Entry;
}

TSharedPtr<FJsonValue> UVedgEventQueue::AnalyticsEventToStorageJson(const FVedgAnalyticsEvent& Event)
{
	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("event"), Event.EventName);
	Obj->SetNumberField(TEXT("value"), Event.Value);
	Obj->SetStringField(TEXT("session_id"), Event.SessionId);
	Obj->SetStringField(TEXT("uid"), Event.Uid);
	Obj->SetStringField(TEXT("device_uid"), Event.DeviceUid);
	Obj->SetNumberField(TEXT("created_at"), Event.CreatedAt);
	return MakeShared<FJsonValueObject>(Obj);
}

FVedgAnalyticsEvent UVedgEventQueue::AnalyticsEventFromStorageJson(const TSharedPtr<FJsonValue>& Value)
{
	FVedgAnalyticsEvent Event;
	const TSharedPtr<FJsonObject>* Obj;
	if (!Value.IsValid() || !Value->TryGetObject(Obj))
	{
		return Event;
	}

	(*Obj)->TryGetStringField(TEXT("event"), Event.EventName);
	(*Obj)->TryGetNumberField(TEXT("value"), Event.Value);
	(*Obj)->TryGetStringField(TEXT("session_id"), Event.SessionId);
	(*Obj)->TryGetStringField(TEXT("uid"), Event.Uid);
	(*Obj)->TryGetStringField(TEXT("device_uid"), Event.DeviceUid);
	(*Obj)->TryGetNumberField(TEXT("created_at"), Event.CreatedAt);
	return Event;
}

TSharedPtr<FJsonValue> UVedgEventQueue::AnalyticsEventToWireJson(const FVedgAnalyticsEvent& Event)
{
	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("event"), Event.EventName);
	Obj->SetNumberField(TEXT("value"), Event.Value);
	Obj->SetStringField(TEXT("session_id"), Event.SessionId);
	if (!Event.Uid.IsEmpty())
	{
		Obj->SetStringField(TEXT("uid"), Event.Uid);
	}
	if (!Event.DeviceUid.IsEmpty())
	{
		Obj->SetStringField(TEXT("device_uid"), Event.DeviceUid);
	}
	return MakeShared<FJsonValueObject>(Obj);
}
