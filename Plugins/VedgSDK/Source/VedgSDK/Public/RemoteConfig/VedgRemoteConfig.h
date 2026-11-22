// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "JsonObjectWrapper.h"
#include "VedgRemoteConfig.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVedgOnRemoteConfigKey, const FString&, Key, bool, bSuccess);

// Per-key remote config, GET /remote-config/{game}/{key}?env=, fetched on
// demand and cached for the session. Bypasses the event queue entirely --
// mirrors VedgRemoteConfig from the Godot addon.
UCLASS(BlueprintType)
class VEDGSDK_API UVedgRemoteConfig : public UObject
{
	GENERATED_BODY()

public:
	// No-op (success) if the key is already cached and !bForce. Fires OnKeyFetched.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	void Fetch(const FString& Key, bool bForce = false);

	void FetchThen(const FString& Key, bool bForce, TFunction<void(bool)> OnDone);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK")
	FJsonObjectWrapper GetDict(const FString& Key) const;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK")
	FVedgOnRemoteConfigKey OnKeyFetched;

private:
	TMap<FString, TSharedPtr<FJsonValue>> Config;
};
