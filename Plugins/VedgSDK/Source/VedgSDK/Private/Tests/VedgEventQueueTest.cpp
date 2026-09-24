// Fill out your copyright notice in the Description page of Project Settings.

#include "Http/VedgEventQueue.h"
#include "VedgSDKSettings.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Covers the one genuinely branchy piece of the SDK: the queue's pure logic
// (backoff bounds, cap eviction, JSON round-trip persistence). Retry/drop
// behavior against real HTTP responses (4xx, timeouts, too-old entries) is
// exercised manually in PIE per the plugin's verification plan -- mocking
// FHttpModule for this single safety-net test isn't worth the infrastructure.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVedgEventQueueTest, "VedgSDK.EventQueue", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVedgEventQueueTest::RunTest(const FString& Parameters)
{
	const double Backoff1 = UVedgEventQueue::Backoff(1);
	TestTrue(TEXT("Backoff(1) is within [BaseBackoffSec, 2 * BaseBackoffSec)"), Backoff1 >= 5.0 && Backoff1 < 10.0);

	const double BackoffHigh = UVedgEventQueue::Backoff(20);
	TestTrue(TEXT("Backoff(20) is capped at MaxBackoffSec + jitter"), BackoffHigh >= 300.0 && BackoffHigh < 305.0);

	// Enqueue no-ops without a configured API URL -- point it at a
	// throwaway local address so the cap/persistence logic below actually runs.
	UVedgSDKSettings* Settings = GetMutableDefault<UVedgSDKSettings>();
	const TMap<FString, FString> SavedUrls = Settings->ApiUrls;
	const FString SavedSlug = Settings->GameSlug;
	Settings->ApiUrls = { { TEXT("dev"), TEXT("http://127.0.0.1:9") } };
	Settings->GameSlug = TEXT("vedg-sdk-test");

	UVedgEventQueue* Queue = NewObject<UVedgEventQueue>(GetTransientPackage());

	for (int32 i = 0; i < UVedgEventQueue::MaxOutboxEntries + 10; ++i)
	{
		Queue->Enqueue(TEXT("/reports/event"), MakeShared<FJsonObject>());
	}
	TestEqual(TEXT("Outbox caps at MaxOutboxEntries"), Queue->GetOutboxCount(), UVedgEventQueue::MaxOutboxEntries);

	for (int32 i = 0; i < UVedgEventQueue::MaxAnalyticsEntries + 10; ++i)
	{
		FVedgAnalyticsEvent Event;
		Event.EventName = TEXT("test_event");
		Queue->EnqueueAnalyticsEvent(Event);
	}
	TestEqual(TEXT("Analytics outbox caps at MaxAnalyticsEntries"), Queue->GetAnalyticsCount(), UVedgEventQueue::MaxAnalyticsEntries);

	Queue->SaveState();
	UVedgEventQueue* ReloadedQueue = NewObject<UVedgEventQueue>(GetTransientPackage());
	ReloadedQueue->LoadState();
	TestEqual(TEXT("Reloaded outbox count matches saved state"), ReloadedQueue->GetOutboxCount(), Queue->GetOutboxCount());
	TestEqual(TEXT("Reloaded analytics count matches saved state"), ReloadedQueue->GetAnalyticsCount(), Queue->GetAnalyticsCount());

	// Don't leave fixture data in the on-disk outbox for a later real run to try to flush.
	ReloadedQueue->Reset();
	Settings->ApiUrls = SavedUrls;
	Settings->GameSlug = SavedSlug;

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
