// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "VedgSDKTypes.h"

// Dedicated-server side of matchmaking (the server the matchmaker spawned),
// mirrors match_server.gd. Identity comes from the SERVER_ID / TOKEN env vars.
// Callbacks run on the game thread.
class VEDGSDK_API FVedgMatchServer
{
public:
	using FOnOk = TFunction<void(bool)>;

	static bool IsMatchmade();
	static FString ServerId();
	static FString ServerToken();
	static FString ApiUrl();

	// MATCH env var if valid JSON, else POST server_match. Cached.
	static void LoadMatch(TFunction<void(const TSharedRef<FJsonObject>&)> OnDone);

	static FString Queue();
	static FString MapName();
	static TArray<TSharedPtr<FJsonValue>> Teams();
	static int32 MaxPlayers();
	static FString Fill();
	static TSharedRef<FJsonObject> Custom();

	static void ReportReady(FOnOk OnDone = nullptr);
	static void ReportStatus(const FString& Status, FOnOk OnDone = nullptr);
	static void ReportClosed(FOnOk OnDone = nullptr);

	// The API's answer ({"authorized": bool, ...}), {"authorized": false} on failure.
	static void AuthorizePlayer(const FString& PlayerToken, const FString& JoinTicket, TFunction<void(const TSharedRef<FJsonObject>&)> OnDone);

	// Retries every 2s until delivered (dropped on 4xx): a lost disconnect leaves a ghost player.
	static void PlayerDisconnected(const FString& Uid, FOnOk OnDone = nullptr);

	static void Players(TFunction<void(const TArray<TSharedPtr<FJsonValue>>&)> OnDone);

private:
	static void Post(const FString& Endpoint, const TSharedRef<FJsonObject>& Body, TFunction<void(const FVedgHttpResult&)> OnDone);
	static FString Env(const TCHAR* Name);
};
