// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "VedgSDKTypes.h"
#include "VedgEventQueue.generated.h"

// Persistent, batching, retrying outbox for everything the SDK reports to the
// backend. Two independent queues, both persisted to disk as JSON arrays and
// reloaded at StartUp: a generic outbox (arbitrary {path, body} POSTs, one
// POST per entry) and a dedicated analytics outbox (batched POSTs to
// /analytics/events). Mirrors VedgEventQueue from the Godot addon, including
// its constants.
//
// A UObject (not a plain C++ class) so async HTTP completion lambdas can
// capture a TWeakObjectPtr and safely no-op if this queue is destroyed while
// a request is in flight (e.g. PIE stopping mid-flush).
//
// ponytail: outbox entries due in the same tick are sent concurrently rather
// than strictly sequentially like the Godot original (which awaits each POST
// before starting the next). Same effective behavior -- up to MaxOutboxPerFlush
// in flight per tick, same retry/drop rules -- just using UE's HTTP module the
// way it's meant to be used instead of hand-rolling a continuation chain to
// match a Godot coroutine artifact.
UCLASS()
class VEDGSDK_API UVedgEventQueue : public UObject
{
	GENERATED_BODY()

public:
	void StartUp();
	void Shutdown();

	// No-op if the API URL is not configured.
	void Enqueue(const FString& Path, TSharedPtr<FJsonObject> Body);
	void EnqueueAnalyticsEvent(const FVedgAnalyticsEvent& Event);

	void SaveState();
	void LoadState();

	// Empties both queues and persists the empty state. Used by tests so
	// fixture data never lingers in the on-disk outbox for a later real run
	// to try to flush.
	void Reset();

	int32 GetOutboxCount() const { return Outbox.Num(); }
	int32 GetAnalyticsCount() const { return Analytics.Num(); }
	int32 GetPendingCount() const { return Outbox.Num() + Analytics.Num(); }

	static double Backoff(int32 Attempts);

	static constexpr double FlushInterval = 3.0;
	static constexpr int32 MaxOutboxEntries = 300;
	static constexpr int32 MaxAnalyticsEntries = 500;
	static constexpr double MaxEntryAgeSec = 7.0 * 24.0 * 3600.0;
	static constexpr int32 MaxOutboxPerFlush = 10;
	static constexpr int32 AnalyticsBatchSize = 50;
	static constexpr double BaseBackoffSec = 5.0;
	static constexpr double MaxBackoffSec = 300.0;

private:
	bool Tick(float DeltaTime);
	void Flush();
	void FlushOutbox();
	void FlushAnalytics();
	void OnOutboxEntryComplete(const FString& EntryId, const FVedgHttpResult& Result);
	void OnAnalyticsFlushComplete(int32 BatchSize, const FVedgHttpResult& Result);
	void FinishFlushIfIdle();

	static FString GetOutboxFilePath();
	static FString GetAnalyticsFilePath();
	static void SaveJsonArray(const FString& FilePath, const TArray<TSharedPtr<FJsonValue>>& Array);
	static TArray<TSharedPtr<FJsonValue>> LoadJsonArray(const FString& FilePath);

	static TSharedPtr<FJsonValue> EntryToStorageJson(const FVedgOutboxEntry& Entry);
	static FVedgOutboxEntry EntryFromStorageJson(const TSharedPtr<FJsonValue>& Value);
	static TSharedPtr<FJsonValue> AnalyticsEventToStorageJson(const FVedgAnalyticsEvent& Event);
	static FVedgAnalyticsEvent AnalyticsEventFromStorageJson(const TSharedPtr<FJsonValue>& Value);
	// Wire payload for an analytics event omits the internal created_at / empty uid & device_uid fields.
	static TSharedPtr<FJsonValue> AnalyticsEventToWireJson(const FVedgAnalyticsEvent& Event);

	TArray<FVedgOutboxEntry> Outbox;
	TArray<FVedgAnalyticsEvent> Analytics;
	double AnalyticsRetryAt = 0.0;
	int32 AnalyticsFailures = 0;

	bool bFlushing = false;
	bool bDirty = false;
	int32 PendingRequests = 0;

	FTSTicker::FDelegateHandle TickerHandle;
};
