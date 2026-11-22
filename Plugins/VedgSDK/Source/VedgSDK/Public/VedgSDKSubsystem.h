// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "VedgSDKTypes.h"
#include "VedgSDKSubsystem.generated.h"

class UVedgEventQueue;
class UVedgRemoteConfig;
class UVedgPerformanceTelemetry;
class UVedgLogShipper;
class UVedgWatchdog;
class UVedgMatchmaking;
class UVedgSocial;
class UVedgHeatmapCapture;

// Central entry point for the Vedg SDK, equivalent to the VedgSDK autoload
// singleton in the Godot addon. A GameInstanceSubsystem so it attaches
// automatically to the project's Blueprint GameInstance (GI_Main) with no
// reparenting required -- reachable from Blueprint via
// Get Game Instance -> Get Subsystem (VedgSDK Subsystem).
//
// Login: UVedgLoginPanel / FVedgLogin, which end with CompleteLogin(); a game
// with its own auth can call SetUid() directly instead.
UCLASS()
class VEDGSDK_API UVedgSDKSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// The subsystem of WorldContext's game instance, null outside a game world.
	static UVedgSDKSubsystem* Get(const UObject* WorldContext);

	// Shared monotonic clock (ms since process start) every telemetry stream
	// (heatmap, analytics, fps) is timestamped with - deliberately not wall-clock.
	static int64 NowMs();

	// Call once the host game's own auth flow succeeds. Fires session_start
	// and links device identity on the first non-empty call this session.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	void SetUid(const FString& NewUid);

	// Retroactively attaches the current (already-started) session to a
	// player if login happens after the session already began anonymously.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	void LinkSessionToPlayer(const FString& Uid);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	void AssignMachineToUser(const FString& Uid);

	// Session wiring after a successful login (SetUid, session/machine links,
	// login_success / register_success). No-op on a failed result.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	void CompleteLogin(const FVedgLoginResult& Result);

	// Also tagged against the active heatmap session, if any.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	void SendEvent(const FString& Event, float Value = 0.f, const FString& Uid = TEXT(""));

	// Current game state (e.g. "menu", "lobby", "in_game"), tagged on every perf
	// sample. Leaving a state sends "state:<name>" valued with the seconds spent in it.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	void SetState(const FString& NewState);

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	FString GetState() const { return State; }

	int64 GetStateStartedMs() const { return StateStartedMs; }

	// Free-form graphics settings, attached to every perf report.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	void SetConfig(const TMap<FString, FString>& NewConfig);

	const TMap<FString, FString>& GetConfig() const { return Config; }

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	FString GetUid() const { return Uid; }

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	FString GetSessionId() const { return SessionId; }

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	FString GetGameSlug() const;

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	FString GetApiUrl() const;

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	FString GetEnv() const;

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	UVedgRemoteConfig* GetRemoteConfig() const { return RemoteConfig; }

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	UVedgMatchmaking* GetMatchmaking() const { return Matchmaking; }

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	UVedgSocial* GetSocial() const { return Social; }

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	UVedgHeatmapCapture* GetHeatmap() const { return Heatmap; }

private:
	void LinkDeviceIdentity(const FString& Uid);
	void SendBootReport();
	bool HeartbeatTick(float DeltaTime);
	void OnMapLoaded(UWorld* LoadedWorld);
	void CloseState();

	UPROPERTY()
	TObjectPtr<UVedgEventQueue> EventQueue;

	UPROPERTY()
	TObjectPtr<UVedgRemoteConfig> RemoteConfig;

	UPROPERTY()
	TObjectPtr<UVedgPerformanceTelemetry> PerformanceTelemetry;

	UPROPERTY()
	TObjectPtr<UVedgLogShipper> LogShipper;

	UPROPERTY()
	TObjectPtr<UVedgWatchdog> Watchdog;

	UPROPERTY()
	TObjectPtr<UVedgMatchmaking> Matchmaking;

	UPROPERTY()
	TObjectPtr<UVedgSocial> Social;

	UPROPERTY()
	TObjectPtr<UVedgHeatmapCapture> Heatmap;

	FString SessionId;
	FString Uid;
	FString State;
	int64 StateStartedMs = 0;
	TMap<FString, FString> Config;
	bool bSessionStartReported = false;
	double SessionStartTime = 0.0;

	FTSTicker::FDelegateHandle HeartbeatHandle;
	FDelegateHandle MapLoadedHandle;

	static constexpr double HeartbeatIntervalSec = 60.0;
};
