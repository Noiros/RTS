// Fill out your copyright notice in the Description page of Project Settings.

#include "VedgSDKSettings.h"
#include "VedgSDKModule.h"
#include "GeneralProjectSettings.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

const UVedgSDKSettings* UVedgSDKSettings::Get()
{
	return GetDefault<UVedgSDKSettings>();
}

FString UVedgSDKSettings::GetGameSlug() const
{
	return GameSlug;
}

EVedgEnv UVedgSDKSettings::ResolveEnv() const
{
	FString EnvString;
	if (FParse::Value(FCommandLine::Get(), TEXT("environment="), EnvString))
	{
		EVedgEnv Parsed;
		if (VedgEnvFromString(EnvString, Parsed))
		{
			return Parsed;
		}
		UE_LOG(LogVedgSDK, Warning, TEXT("Invalid -environment=%s, falling back"), *EnvString);
	}

	EnvString = FPlatformMisc::GetEnvironmentVariable(TEXT("ENV"));
	if (!EnvString.IsEmpty())
	{
		EVedgEnv Parsed;
		if (VedgEnvFromString(EnvString, Parsed))
		{
			return Parsed;
		}
		UE_LOG(LogVedgSDK, Warning, TEXT("Invalid ENV=%s, falling back"), *EnvString);
	}

	return DefaultEnv;
}

FString UVedgSDKSettings::GetApiUrl() const
{
	const FString EnvKey = VedgEnvToString(ResolveEnv());
	if (const FString* Url = ApiUrls.Find(EnvKey))
	{
		return *Url;
	}
	if (const FString* DevUrl = ApiUrls.Find(TEXT("dev")))
	{
		return *DevUrl;
	}
	return FString();
}

FString UVedgSDKSettings::GetAuxApiUrl(const FString& Key) const
{
	const FVedgAuxApiUrlSet* Set = AuxApiUrls.Find(Key);
	if (!Set)
	{
		return FString();
	}

	const FString EnvKey = VedgEnvToString(ResolveEnv());
	if (const FString* Url = Set->Urls.Find(EnvKey))
	{
		return *Url;
	}
	if (const FString* DevUrl = Set->Urls.Find(TEXT("dev")))
	{
		return *DevUrl;
	}
	return FString();
}

FString UVedgSDKSettings::GetVersion()
{
	return GetDefault<UGeneralProjectSettings>()->ProjectVersion;
}
