// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VedgSDKTypes.h"

// Thin fire-and-forget HTTP wrapper. No retry, no auth headers, no base-URL
// logic -- callers always pass a fully-qualified URL. Mirrors VedgRequest
// from the Godot addon. Retry/backoff lives one layer up, in FVedgEventQueue.
class VEDGSDK_API FVedgHttpRequest
{
public:
	// BearerToken adds "Authorization: Bearer" (player-authenticated routes:
	// friends, lobbies). The JSON body is parsed on failures too, so callers can
	// read the API's {"error": ...} / ban fields.
	// Headers: extra request headers (editor tooling: X-Heatmap-Key, X-Deploy-Key).
	static void Request(const FString& Url, const FString& Verb, const FString& JsonBody, TFunction<void(FVedgHttpResult)> OnComplete, float Timeout = 10.f, const FString& BearerToken = FString(), const TMap<FString, FString>& Headers = {});
};
