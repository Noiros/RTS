// Fill out your copyright notice in the Description page of Project Settings.

#include "VedgSDKSettings.h"
#include "VedgSDKModule.h"
#include "GeneralProjectSettings.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Algo/AllOf.h"

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
	if (FParse::Value(FCommandLine::Get(), TEXT("api-env="), EnvString))
	{
		EVedgEnv Parsed;
		if (VedgEnvFromString(EnvString, Parsed))
		{
			return Parsed;
		}
		UE_LOG(LogVedgSDK, Warning, TEXT("Invalid -api-env=%s, falling back"), *EnvString);
	}

	EnvString = FPlatformMisc::GetEnvironmentVariable(TEXT("VEDG_API_ENV"));
	if (!EnvString.IsEmpty())
	{
		EVedgEnv Parsed;
		if (VedgEnvFromString(EnvString, Parsed))
		{
			return Parsed;
		}
		UE_LOG(LogVedgSDK, Warning, TEXT("Invalid VEDG_API_ENV=%s, falling back"), *EnvString);
	}

	return DefaultEnv;
}

FString UVedgSDKSettings::ResolveGameEnv() const
{
	FString Resolved;
	if (!FParse::Value(FCommandLine::Get(), TEXT("environment="), Resolved))
	{
		Resolved = FPlatformMisc::GetEnvironmentVariable(TEXT("ENV"));
	}
	if (Resolved.IsEmpty())
	{
		Resolved = GameEnv;
	}

	const FString ApiEnv = VedgEnvToString(ResolveEnv());
	if (Resolved.IsEmpty())
	{
		return ApiEnv;
	}

	const bool bValid = Resolved.Len() <= 32 && Algo::AllOf(Resolved, [](TCHAR C) { return FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-'); });
	if (!bValid)
	{
		UE_LOG(LogVedgSDK, Error, TEXT("Unrecognized environment \"%s\" - falling back to \"%s\""), *Resolved, *ApiEnv);
		return ApiEnv;
	}
	return Resolved;
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
