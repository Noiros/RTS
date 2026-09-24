// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "VedgSDKSubsystem.generated.h"

class UVedgEventQueue;
class UVedgRemoteConfig;
class UVedgPerformanceTelemetry;
class UVedgLogShipper;
class UVedgWatchdog;

// Central entry point for the Vedg SDK, equivalent to the VedgSDK autoload
// singleton in the Godot addon. A GameInstanceSubsystem so it attaches
// automatically to the project's Blueprint GameInstance (GI_Main) with no
// reparenting required -- reachable from Blueprint via
// Get Game Instance -> Get Subsystem (VedgSDK Subsystem).
//
// Auth is out of scope: the host game's own login flow calls SetUid() once
// it succeeds, exactly like the Godot addon.
UCLASS()
class VEDGSDK_API UVedgSDKSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

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

	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	void SendEvent(const FString& Event, float Value = 0.f, const FString& Uid = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	FString GetSessionId() const { return SessionId; }

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	FString GetGameSlug() const;

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	FString GetApiUrl() const;

	UFUNCTION(BlueprintPure, Category = "VedgSDK")
	FString GetEnv() const;

	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	UVedgRemoteConfig* GetRemoteConfig() const { return RemoteConfig; }

private:
	void LinkDeviceIdentity(const FString& Uid);
	void SendBootReport();
	bool HeartbeatTick(float DeltaTime);
	void OnMapLoaded(UWorld* LoadedWorld);

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

	FString SessionId;
	FString Uid;
	bool bSessionStartReported = false;
	double SessionStartTime = 0.0;

	FTSTicker::FDelegateHandle HeartbeatHandle;
	FDelegateHandle MapLoadedHandle;

	static constexpr double HeartbeatIntervalSec = 60.0;
};
