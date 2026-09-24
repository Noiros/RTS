// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "VedgSDKTypes.generated.h"

UENUM(BlueprintType)
enum class EVedgEnv : uint8
{
	Local,
	Dev,
	Prod
};

VEDGSDK_API FString VedgEnvToString(EVedgEnv Env);
VEDGSDK_API bool VedgEnvFromString(const FString& Value, EVedgEnv& OutEnv);

// Unreal's reflection system can't nest TMap<FString, TMap<...>> directly as a
// UPROPERTY, so an auxiliary API URL set (per-env URLs for one keyed service,
// e.g. login) is wrapped in this one-field struct. Mirrors one entry of Godot's
// custom_api_urls: Dictionary[String, Dictionary].
USTRUCT()
struct FVedgAuxApiUrlSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Vedg SDK")
	TMap<FString, FString> Urls;
};

// Internal plumbing types, not Blueprint-exposed.

struct FVedgHttpResult
{
	bool bValid = false;
	int32 StatusCode = 0;
	TSharedPtr<FJsonObject> ResponseObject;
};

struct FVedgOutboxEntry
{
	FString Id;
	FString Path;
	TSharedPtr<FJsonObject> Body;
	double CreatedAt = 0.0;
	int32 Attempts = 0;
	double NextAttemptAt = 0.0;
};

struct FVedgAnalyticsEvent
{
	FString EventName;
	float Value = 0.f;
	FString SessionId;
	FString Uid;       // empty = omitted from the outgoing payload
	FString DeviceUid; // empty = omitted from the outgoing payload
	double CreatedAt = 0.0;
};
