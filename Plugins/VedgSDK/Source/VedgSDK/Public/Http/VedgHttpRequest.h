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
	static void Request(const FString& Url, const FString& Verb, const FString& JsonBody, TFunction<void(FVedgHttpResult)> OnComplete, float Timeout = 10.f);
};
