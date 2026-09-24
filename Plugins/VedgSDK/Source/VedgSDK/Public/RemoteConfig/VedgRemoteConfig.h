// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "JsonObjectWrapper.h"
#include "VedgRemoteConfig.generated.h"

// One-shot GET /remote-config/{game_slug} fetched at boot, no auth, no
// retry, no disk cache. Bypasses the event queue entirely -- mirrors
// VedgRemoteConfig from the Godot addon, the simplest module in the SDK.
UCLASS()
class VEDGSDK_API UVedgRemoteConfig : public UObject
{
	GENERATED_BODY()

public:
	void Fetch();

	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	FJsonObjectWrapper GetDict(const FString& Key) const;

private:
	TSharedPtr<FJsonObject> Config;
};
