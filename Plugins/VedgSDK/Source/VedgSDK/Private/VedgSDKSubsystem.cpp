// Fill out your copyright notice in the Description page of Project Settings.

#include "VedgSDKSubsystem.h"
#include "VedgSDKSettings.h"
#include "VedgSDKModule.h"
#include "VedgSDKTypes.h"
#include "CoreGlobals.h"
#include "CrashReporting/VedgLogShipper.h"
#include "CrashReporting/VedgWatchdog.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "Http/VedgEventQueue.h"
#include "Identity/VedgMachineIdentity.h"
#include "Misc/CoreMisc.h"
#include "Misc/Guid.h"
#include "Performance/VedgPerformanceTelemetry.h"
#include "RemoteConfig/VedgRemoteConfig.h"
#include "UObject/UObjectGlobals.h"

void UVedgSDKSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	SessionStartTime = FDateTime::UtcNow().ToUnixTimestampDecimal();

	const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();
	if (Settings->GetGameSlug().IsEmpty())
	{
		UE_LOG(LogVedgSDK, Warning, TEXT("VedgSDK: GameSlug is not configured (Project Settings > Vedg SDK)"));
	}
	if (Settings->GetApiUrl().IsEmpty())
	{
		UE_LOG(LogVedgSDK, Warning, TEXT("VedgSDK: ApiUrl is not configured for env '%s' (Project Settings > Vedg SDK)"), *VedgEnvToString(Settings->ResolveEnv()));
	}

	EventQueue = NewObject<UVedgEventQueue>(this);
	EventQueue->StartUp();

	LogShipper = NewObject<UVedgLogShipper>(this);
	LogShipper->StartUp(EventQueue, SessionId);

	PerformanceTelemetry = NewObject<UVedgPerformanceTelemetry>(this);
	PerformanceTelemetry->StartUp(EventQueue, SessionId);

	if (!GIsEditor)
	{
		Watchdog = NewObject<UVedgWatchdog>(this);
		Watchdog->StartUp(EventQueue, SessionId);
	}

	RemoteConfig = NewObject<UVedgRemoteConfig>(this);
	RemoteConfig->Fetch();

	SendBootReport();

	SendEvent(TEXT("heartbeat"));
	HeartbeatHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVedgSDKSubsystem::HeartbeatTick), HeartbeatIntervalSec);

	// Automatic "here and there" event: fires on every level transition
	// (UGameplayStatics::OpenLevel, seamless travel, etc.) with no Blueprint
	// wiring required. Doesn't fire for the very first PIE/standalone world
	// (that one is already covered by the boot report + session_start).
	MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UVedgSDKSubsystem::OnMapLoaded);
}

void UVedgSDKSubsystem::Deinitialize()
{
	if (HeartbeatHandle.IsValid())
	{
		FTSTicker::RemoveTicker(HeartbeatHandle);
		HeartbeatHandle.Reset();
	}
	if (MapLoadedHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
		MapLoadedHandle.Reset();
	}

	if (bSessionStartReported)
	{
		const double Elapsed = FDateTime::UtcNow().ToUnixTimestampDecimal() - SessionStartTime;
		SendEvent(TEXT("session_end"), (float)Elapsed);
		bSessionStartReported = false;
	}

	// Closes out the /reports/session/start row from SendBootReport(). Same
	// reliability rationale as that call: don't depend on the native crash-handler
	// binary (which also posts session/:id/end, but only if it launched).
	if (EventQueue)
	{
		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("game"), GetGameSlug());
		Body->SetBoolField(TEXT("clean_exit"), true);
		EventQueue->Enqueue(FString::Printf(TEXT("/reports/session/%s/end"), *SessionId), Body);
	}

	// A clean Deinitialize never runs on a hard crash/kill -- that gap is
	// exactly what Watchdog's next-launch detection covers.
	if (Watchdog)
	{
		Watchdog->Shutdown();
	}
	if (PerformanceTelemetry)
	{
		PerformanceTelemetry->Shutdown();
	}
	if (LogShipper)
	{
		LogShipper->Shutdown();
	}
	if (EventQueue)
	{
		EventQueue->Shutdown();
	}

	Super::Deinitialize();
}

bool UVedgSDKSubsystem::HeartbeatTick(float)
{
	SendEvent(TEXT("heartbeat"));
	return true;
}

void UVedgSDKSubsystem::OnMapLoaded(UWorld* LoadedWorld)
{
	SendEvent(FString::Printf(TEXT("level_loaded_%s"), *GetNameSafe(LoadedWorld)));
}

void UVedgSDKSubsystem::SetUid(const FString& NewUid)
{
	Uid = NewUid;
	if (!bSessionStartReported && !NewUid.IsEmpty())
	{
		bSessionStartReported = true;
		SendEvent(TEXT("session_start"));
		LinkDeviceIdentity(NewUid);
	}
}

void UVedgSDKSubsystem::LinkSessionToPlayer(const FString& InUid)
{
	if (!EventQueue)
	{
		return;
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game"), GetGameSlug());
	Body->SetStringField(TEXT("uid"), InUid);
	EventQueue->Enqueue(FString::Printf(TEXT("/reports/session/%s/link"), *SessionId), Body);
}

void UVedgSDKSubsystem::LinkDeviceIdentity(const FString& InUid)
{
	if (!EventQueue || IsRunningDedicatedServer())
	{
		return;
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("device_uid"), FVedgMachineIdentity::GetDeviceUid());
	Body->SetStringField(TEXT("uid"), InUid);
	EventQueue->Enqueue(TEXT("/reports/identity/link"), Body);
}

void UVedgSDKSubsystem::AssignMachineToUser(const FString& InUid)
{
	FVedgMachineIdentity::AssignToUser(EventQueue, GetGameSlug(), InUid);
}

void UVedgSDKSubsystem::SendEvent(const FString& Event, float Value, const FString& InUid)
{
	if (!EventQueue)
	{
		return;
	}

	FVedgAnalyticsEvent AnalyticsEvent;
	AnalyticsEvent.EventName = Event;
	AnalyticsEvent.Value = Value;
	AnalyticsEvent.SessionId = SessionId;
	AnalyticsEvent.Uid = !InUid.IsEmpty() ? InUid : Uid;
	if (!IsRunningDedicatedServer())
	{
		AnalyticsEvent.DeviceUid = FVedgMachineIdentity::GetDeviceUid();
	}

	EventQueue->EnqueueAnalyticsEvent(AnalyticsEvent);
}

FString UVedgSDKSubsystem::GetGameSlug() const
{
	return UVedgSDKSettings::Get()->GetGameSlug();
}

FString UVedgSDKSubsystem::GetApiUrl() const
{
	return UVedgSDKSettings::Get()->GetApiUrl();
}

FString UVedgSDKSubsystem::GetEnv() const
{
	return VedgEnvToString(UVedgSDKSettings::Get()->ResolveEnv());
}

void UVedgSDKSubsystem::SendBootReport()
{
	if (!EventQueue)
	{
		return;
	}

	const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();

	// Registers the session so every later report's session_id resolves to a real
	// row. Godot leaves this solely to the native Vedg-Crash-Handler binary (see
	// FVedgCrashHandlerLauncher) - that binary's own launch reliability has an
	// open, unresolved risk (see the plan's crash-handler section), so this engine-
	// side call is a deliberate reliability improvement over the literal Godot
	// behavior, not a 1:1 port: it guarantees session registration even if the
	// binary never launches. NOTE: this replaces a prior version of this function
	// that posted type:"session_start" to /reports/event - "session_start" is not
	// a member of the server's GAME_REPORT_TYPES enum, so that call would have
	// been rejected outright; this is the actual /reports/session/start contract.
	const TSharedRef<FJsonObject> SessionBody = MakeShared<FJsonObject>();
	SessionBody->SetStringField(TEXT("game"), Settings->GetGameSlug());
	SessionBody->SetStringField(TEXT("session_id"), SessionId);
	SessionBody->SetStringField(TEXT("kind"), IsRunningDedicatedServer() ? TEXT("server") : TEXT("client"));
	SessionBody->SetStringField(TEXT("machine_id"), FVedgMachineIdentity::GetMachineId());
	SessionBody->SetStringField(TEXT("app_version"), UVedgSDKSettings::GetVersion());
	SessionBody->SetStringField(TEXT("platform"), FString(FPlatformProperties::PlatformName()));
	SessionBody->SetStringField(TEXT("env"), VedgEnvToString(Settings->ResolveEnv()));
	EventQueue->Enqueue(TEXT("/reports/session/start"), SessionBody);

	FVedgMachineIdentity::PostHardwareSnapshot(EventQueue, Settings->GetGameSlug());
}
