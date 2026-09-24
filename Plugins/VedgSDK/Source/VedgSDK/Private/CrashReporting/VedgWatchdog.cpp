// Fill out your copyright notice in the Description page of Project Settings.

#include "CrashReporting/VedgWatchdog.h"
#include "Http/VedgEventQueue.h"
#include "Identity/VedgMachineIdentity.h"
#include "VedgSDKSettings.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformProperties.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString UVedgWatchdog::GetSessionFilePath()
{
	return FPaths::ProjectSavedDir() / TEXT("VedgSDK/watchdog_session.json");
}

void UVedgWatchdog::StartUp(UVedgEventQueue* InEventQueue, const FString& InSessionId)
{
	EventQueue = InEventQueue;
	SessionId = InSessionId;

	CheckPreviousSession();
	WriteHeartbeat(false, false);

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVedgWatchdog::Tick), HeartbeatInterval);
}

void UVedgWatchdog::Shutdown()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	WriteHeartbeat(true, false);
}

void UVedgWatchdog::MarkCrashed()
{
	WriteHeartbeat(false, true);
}

bool UVedgWatchdog::Tick(float)
{
	WriteHeartbeat(false, false);
	return true;
}

void UVedgWatchdog::WriteHeartbeat(bool bCleanExit, bool bCrashed)
{
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("clean_exit"), bCleanExit);
	Json->SetBoolField(TEXT("crashed"), bCrashed);
	Json->SetNumberField(TEXT("heartbeat"), FDateTime::UtcNow().ToUnixTimestampDecimal());
	Json->SetNumberField(TEXT("pid"), (double)FPlatformProcess::GetCurrentProcessId());
	Json->SetStringField(TEXT("session_id"), SessionId);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Json, Writer);

	const FString FilePath = GetSessionFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
	FFileHelper::SaveStringToFile(Output, *FilePath);
}

void UVedgWatchdog::CheckPreviousSession()
{
	const FString FilePath = GetSessionFilePath();

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FilePath))
	{
		return;
	}

	TSharedPtr<FJsonObject> Previous;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Previous) || !Previous.IsValid())
	{
		return;
	}

	bool bCleanExit = true;
	Previous->TryGetBoolField(TEXT("clean_exit"), bCleanExit);
	if (bCleanExit)
	{
		return;
	}

	double LastHeartbeat = 0.0;
	Previous->TryGetNumberField(TEXT("heartbeat"), LastHeartbeat);
	const double Now = FDateTime::UtcNow().ToUnixTimestampDecimal();
	const bool bStale = (Now - LastHeartbeat) > StaleThresholdSec;

	FString PreviousSessionId;
	Previous->TryGetStringField(TEXT("session_id"), PreviousSessionId);
	if (PreviousSessionId.IsEmpty())
	{
		PreviousSessionId = SessionId;
	}

	const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();

	{
		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("game"), Settings->GetGameSlug());
		Body->SetBoolField(TEXT("clean_exit"), false);
		EventQueue->Enqueue(FString::Printf(TEXT("/reports/session/%s/end"), *PreviousSessionId), Body);
	}

	{
		const TSharedRef<FJsonObject> Build = MakeShared<FJsonObject>();
		Build->SetStringField(TEXT("version"), UVedgSDKSettings::GetVersion());
		Build->SetStringField(TEXT("env"), VedgEnvToString(Settings->ResolveEnv()));
		Build->SetStringField(TEXT("platform"), FString(FPlatformProperties::PlatformName()));

		const TSharedRef<FJsonObject> Session = MakeShared<FJsonObject>();
		// The watchdog reports about the *previous* (crashed) session; we have
		// no record of its uid, only the current one's -- left null.
		Session->SetField(TEXT("uid"), MakeShared<FJsonValueNull>());

		const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetObjectField(TEXT("build"), Build);
		Data->SetObjectField(TEXT("session"), Session);
		Data->SetObjectField(TEXT("hardware"), FVedgMachineIdentity::CollectHardwareSnapshot());
		Data->SetObjectField(TEXT("previous_session"), Previous);
		Data->SetBoolField(TEXT("stale"), bStale);

		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("game"), Settings->GetGameSlug());
		Body->SetStringField(TEXT("type"), TEXT("watchdog"));
		Body->SetStringField(TEXT("machine_id"), FVedgMachineIdentity::GetMachineId());
		Body->SetStringField(TEXT("session_id"), PreviousSessionId);
		Body->SetObjectField(TEXT("data"), Data);
		EventQueue->Enqueue(TEXT("/reports/event"), Body);
	}
}
