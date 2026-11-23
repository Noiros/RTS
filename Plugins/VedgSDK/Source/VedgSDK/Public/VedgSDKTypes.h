// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "VedgSDKTypes.generated.h"

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

// Outcome of a login step (VedgLogin / UVedgLoginPanel). Error is an API /
// translation key (see FVedgText).
USTRUCT(BlueprintType)
struct VEDGSDK_API FVedgLoginResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	FString Token;

	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	FString Uid;

	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	FString Pseudo;

	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	bool bIsNewAccount = false;

	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	FString Error;

	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	FString Reason;

	// Ban end (ISO date), empty when permanent or not banned.
	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	FString Until;

	FString Refresh;
	bool bRetryable = false;

	static FVedgLoginResult Fail(const FString& InError, const FString& InReason = FString(), const FString& InUntil = FString())
	{
		FVedgLoginResult Result;
		Result.Error = InError;
		Result.Reason = InReason;
		Result.Until = InUntil;
		return Result;
	}
};

// Result of /auth/connect with a Steam token, mirrors login_steam.gd check_status().
struct FVedgSteamStatus
{
	bool bExists = false;
	bool bBanned = false;
	bool bUnreachable = false;
	int32 Code = 0;
	FString Uid;
	FString Token;
	FString Error;
	FString Reason;
	FString Until;
};

// One friend / request / invite row, Blueprint view of VedgSocial's lists.
USTRUCT(BlueprintType)
struct VEDGSDK_API FVedgSocialEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	FString Uid;

	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	FString Pseudo;

	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	bool bOnline = false;

	// Game slug the friend is currently in, empty if none/offline.
	UPROPERTY(BlueprintReadOnly, Category = "VedgSDK")
	FString Game;
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
