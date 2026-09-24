// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VedgSDKTypes.h"
#include "VedgSDKSettings.generated.h"

// Project-wide configuration for the Vedg SDK: game slug, per-environment API
// base URLs and the default environment. Edited via Project Settings > Vedg SDK,
// saved to Config/DefaultVedgSDK.ini. Mirrors VedgSDKDatas from the Godot addon.
UCLASS(Config = VedgSDK, defaultconfig, meta = (DisplayName = "Vedg SDK"))
class VEDGSDK_API UVedgSDKSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Free-text slug identifying this game to the backend, sent as "game" in almost every payload.
	UPROPERTY(Config, EditAnywhere, Category = "Vedg SDK")
	FString GameSlug;

	// Base API URL per environment (keys: "local", "dev", "prod").
	UPROPERTY(Config, EditAnywhere, Category = "Vedg SDK")
	TMap<FString, FString> ApiUrls;

	UPROPERTY(Config, EditAnywhere, Category = "Vedg SDK")
	EVedgEnv DefaultEnv = EVedgEnv::Dev;

	// Auxiliary API URL sets keyed by service name (e.g. "objectwars_api" for
	// auth/login), distinct from the main telemetry ApiUrls above. Mirrors Godot's
	// custom_api_urls.
	UPROPERTY(Config, EditAnywhere, Category = "Vedg SDK")
	TMap<FString, FVedgAuxApiUrlSet> AuxApiUrls;

	// Key into AuxApiUrls that VedgAuth resolves its base URL from.
	UPROPERTY(Config, EditAnywhere, Category = "Vedg SDK")
	FString LoginApiUrlKey = TEXT("objectwars_api");

	static const UVedgSDKSettings* Get();

	FString GetGameSlug() const;

	// Resolution order: -environment=<value> on the command line, then the ENV
	// environment variable, then DefaultEnv. Invalid values fall back with a warning.
	EVedgEnv ResolveEnv() const;

	// ApiUrls[ResolveEnv()], falling back to ApiUrls["dev"], else empty.
	FString GetApiUrl() const;

	// AuxApiUrls[Key][ResolveEnv()], falling back to AuxApiUrls[Key]["dev"], else
	// empty. Used by VedgAuth via LoginApiUrlKey.
	FString GetAuxApiUrl(const FString& Key) const;

	static FString GetVersion();
};
