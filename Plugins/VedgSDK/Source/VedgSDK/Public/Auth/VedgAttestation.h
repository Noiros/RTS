// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UVedgSDKSubsystem;

// Signed build attestation sent with /auth/connect, mirrors attestation.gd.
// The key is compiled in from the VEDG_BUILD_KEY env var (see VedgSDK.Build.cs);
// without it the signature is empty and the API treats the build as unofficial.
class VEDGSDK_API FVedgAttestation
{
public:
	static TSharedRef<FJsonObject> LoginFields(const UVedgSDKSubsystem* Sdk);
	static FString BuildKey();
};
