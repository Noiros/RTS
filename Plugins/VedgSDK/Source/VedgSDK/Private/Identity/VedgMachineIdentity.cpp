// Fill out your copyright notice in the Description page of Project Settings.

#include "Identity/VedgMachineIdentity.h"
#include "Http/VedgEventQueue.h"
#include "VedgSDKModule.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformProperties.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FString GCachedMachineId;
	FString GCachedDeviceUid;

	FString GetIdentityFilePath()
	{
		// FPlatformProcess::UserSettingsDir() resolves to %LOCALAPPDATA% on Windows, but
		// Godot's OS.get_config_dir() (used by the Godot addon for this same file) resolves
		// to the *roaming* %APPDATA%. Ask explicitly for the roaming-user context so this
		// device_uid file -- and the studio-wide device identity it represents -- is shared
		// with the Godot titles on the same machine, per the port's design decision.
		FPlatformProcess::ApplicationSettingsContext Context;
		Context.Location = FPlatformProcess::ApplicationSettingsContext::Context::RoamingUser;
		Context.bIsEpic = false;
		const FString RoamingAppData = FPlatformProcess::GetApplicationSettingsDir(Context);
		return RoamingAppData / TEXT("Vedg/Services/identity.json");
	}
}

FString FVedgMachineIdentity::GetMachineId()
{
	if (GCachedMachineId.IsEmpty())
	{
		GCachedMachineId = FPlatformMisc::GetDeviceId();
		if (GCachedMachineId.IsEmpty())
		{
			GCachedMachineId = FPlatformMisc::GetLoginId();
		}
	}
	return GCachedMachineId;
}

FString FVedgMachineIdentity::GetDeviceUid()
{
	if (GCachedDeviceUid.IsEmpty())
	{
		GCachedDeviceUid = LoadOrCreateDeviceUid();
	}
	return GCachedDeviceUid;
}

FString FVedgMachineIdentity::LoadOrCreateDeviceUid()
{
	const FString FilePath = GetIdentityFilePath();

	FString Text;
	if (FFileHelper::LoadFileToString(Text, *FilePath))
	{
		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			FString DeviceUid;
			if (JsonObject->TryGetStringField(TEXT("device_uid"), DeviceUid) && !DeviceUid.IsEmpty())
			{
				return DeviceUid;
			}
		}
	}

	const FString NewDeviceUid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

	const TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("device_uid"), NewDeviceUid);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
	if (!FFileHelper::SaveStringToFile(Output, *FilePath))
	{
		UE_LOG(LogVedgSDK, Error, TEXT("Failed to persist device identity to %s"), *FilePath);
	}

	return NewDeviceUid;
}

void FVedgMachineIdentity::AssignToUser(UVedgEventQueue* EventQueue, const FString& GameSlug, const FString& Uid)
{
	if (!EventQueue)
	{
		return;
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game"), GameSlug);
	Body->SetStringField(TEXT("machine_id"), GetMachineId());
	Body->SetStringField(TEXT("uid"), Uid);
	EventQueue->Enqueue(TEXT("/reports/machine/assign"), Body);
}

TSharedRef<FJsonObject> FVedgMachineIdentity::CollectHardwareSnapshot()
{
	const TSharedRef<FJsonObject> Hardware = MakeShared<FJsonObject>();
	Hardware->SetStringField(TEXT("os"), FString(FPlatformProperties::PlatformName()));
	Hardware->SetStringField(TEXT("os_version"), FPlatformMisc::GetOSVersion());
	Hardware->SetStringField(TEXT("cpu"), FPlatformMisc::GetCPUBrand());
	Hardware->SetStringField(TEXT("gpu"), FPlatformMisc::GetPrimaryGPUBrand());
	Hardware->SetNumberField(TEXT("ram_mb"), (double)(FPlatformMemory::GetStats().TotalPhysical / (1024ull * 1024ull)));
	Hardware->SetStringField(TEXT("unique_id"), GetMachineId());
	return Hardware;
}

void FVedgMachineIdentity::PostHardwareSnapshot(UVedgEventQueue* EventQueue, const FString& GameSlug)
{
	if (!EventQueue)
	{
		return;
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game"), GameSlug);
	Body->SetStringField(TEXT("machine_id"), GetMachineId());
	Body->SetObjectField(TEXT("hardware"), CollectHardwareSnapshot());
	EventQueue->Enqueue(TEXT("/reports/machine"), Body);
}
