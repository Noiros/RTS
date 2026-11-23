// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VedgSDKTypes.h"
#include "VedgSDKSettings.generated.h"

// Project-wide configuration for the Vedg SDK: game slug and game environment. Edited via Project Settings > Vedg SDK,
// saved to Config/DefaultVedgSDK.ini. Mirrors VedgSDKDatas from the Godot addon.
UCLASS(Config = VedgSDK, defaultconfig, meta = (DisplayName = "Vedg SDK"))
class VEDGSDK_API UVedgSDKSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Free-text slug identifying this game to the backend, sent as "game" in almost every payload.
	UPROPERTY(Config, EditAnywhere, Category = "Vedg SDK")
	FString GameSlug;

	// Game environment sent as "env" to matchmaking, lobbies and remote config.
	// Empty = "prod".
	UPROPERTY(Config, EditAnywhere, Category = "Vedg SDK")
	FString GameEnv;

	static const UVedgSDKSettings* Get();

	FString GetGameSlug() const;

	// Game env: -environment=<value>, then ENV, then GameEnv, then "prod".
	// Mirrors VedgSDK.env() in the Godot addon.
	FString ResolveGameEnv() const;

	// The one and only Vedg API.
	FString GetApiUrl() const;

	static FString GetVersion();
};
