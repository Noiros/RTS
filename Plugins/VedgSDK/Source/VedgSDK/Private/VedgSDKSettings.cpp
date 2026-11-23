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

	const FString ApiEnv = TEXT("prod");
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
	return TEXT("https://api.vedgstudio.com/prod");
}

FString UVedgSDKSettings::GetVersion()
{
	return GetDefault<UGeneralProjectSettings>()->ProjectVersion;
}
