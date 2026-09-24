// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "VedgWatchdog.generated.h"

class UVedgEventQueue;

// Heartbeat file rewritten every 5s; if the *previous* run's file says it
// didn't exit cleanly, the *next* launch reports the abnormal termination.
// This is the primary crash-detection mechanism for this SDK (no native
// crash-handler sidecar, see the port's design notes) -- mirrors
// VedgWatchdog from the Godot addon, which uses the same next-launch
// detection as its own fallback path for exactly the same reliability
// reason. Only constructed outside the editor.
UCLASS()
class VEDGSDK_API UVedgWatchdog : public UObject
{
	GENERATED_BODY()

public:
	void StartUp(UVedgEventQueue* InEventQueue, const FString& InSessionId);
	void Shutdown();

	// Best-effort only: intended to be called from a crash-handler context,
	// where a full engine/file-IO stack may already be compromised. The
	// reliable path is CheckPreviousSession() on the *next* launch, not this
	// write succeeding.
	void MarkCrashed();

private:
	bool Tick(float DeltaTime);
	void WriteHeartbeat(bool bCleanExit, bool bCrashed);
	void CheckPreviousSession();

	static FString GetSessionFilePath();

	UPROPERTY()
	TObjectPtr<UVedgEventQueue> EventQueue;

	FString SessionId;
	FTSTicker::FDelegateHandle TickerHandle;

	static constexpr double HeartbeatInterval = 5.0;
	static constexpr double StaleThresholdSec = 15.0;
};
