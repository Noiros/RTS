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

	// Crash dialog branding, written to crash_handler.json next to the native crash handler at packaging
	// time (VedgSDK.Build.cs). Empty title/product name = the handler falls back to the game slug.
	// {game} is replaced by the product name.
	UPROPERTY(Config, EditAnywhere, Category = "Crash Handler")
	FString CrashWindowTitle;

	UPROPERTY(Config, EditAnywhere, Category = "Crash Handler")
	FString CrashProductName;

	UPROPERTY(Config, EditAnywhere, Category = "Crash Handler")
	FString CrashAccentColor = TEXT("#E23B3B");

	UPROPERTY(Config, EditAnywhere, Category = "Crash Handler")
	FString CrashTitle = TEXT("Oops, {game} has crashed");

	UPROPERTY(Config, EditAnywhere, Category = "Crash Handler", meta = (MultiLine = true))
	FString CrashMessage = TEXT("{game} closed unexpectedly.\n\nYou can restart it right away. To help us fix this crash, please leave the box below checked to send the diagnostic logs.");

	UPROPERTY(Config, EditAnywhere, Category = "Crash Handler")
	FString CrashSendLogsLabel = TEXT("Send diagnostic logs");

	UPROPERTY(Config, EditAnywhere, Category = "Crash Handler")
	FString CrashRestartButton = TEXT("Restart the game");

	UPROPERTY(Config, EditAnywhere, Category = "Crash Handler")
	FString CrashCloseButton = TEXT("Close");

	UPROPERTY(Config, EditAnywhere, Category = "Crash Handler")
	bool bCrashSendLogsDefault = true;

	static const UVedgSDKSettings* Get();

	FString GetGameSlug() const;

	// Game env: -environment=<value>, then ENV, then GameEnv, then "prod".
	// Mirrors VedgSDK.env() in the Godot addon.
	FString ResolveGameEnv() const;

	// The one and only Vedg API.
	FString GetApiUrl() const;

	static FString GetVersion();
};
