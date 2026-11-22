// Fill out your copyright notice in the Description page of Project Settings.

#include "Auth/VedgAttestation.h"
#include "Identity/VedgMachineIdentity.h"
#include "VedgSDKSettings.h"
#include "VedgSDKSubsystem.h"

// OpenSSL's UI type clashes with UE's UI namespace.
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include "openssl/evp.h"
#include "openssl/hmac.h"
THIRD_PARTY_INCLUDES_END
#undef UI

FString FVedgAttestation::BuildKey()
{
	return FString(UTF8_TO_TCHAR(VEDG_BUILD_KEY)).TrimStartAndEnd();
}

TSharedRef<FJsonObject> FVedgAttestation::LoginFields(const UVedgSDKSubsystem* Sdk)
{
	const int64 Timestamp = (int64)(FDateTime::UtcNow().ToUnixTimestampDecimal() * 1000.0);
	const FString Version = UVedgSDKSettings::GetVersion();
	const FString Key = BuildKey();

	FString Signature;
	if (!Key.IsEmpty())
	{
		const FString Message = FString::Printf(TEXT("%s|%s||%s|%lld"), *Sdk->GetGameSlug(), *Sdk->GetSessionId(), *Version, Timestamp);
		const FTCHARToUTF8 KeyUtf8(*Key);
		const FTCHARToUTF8 MessageUtf8(*Message);

		uint8 Digest[EVP_MAX_MD_SIZE];
		unsigned int DigestLen = 0;
		HMAC(EVP_sha256(), KeyUtf8.Get(), KeyUtf8.Length(), reinterpret_cast<const uint8*>(MessageUtf8.Get()), MessageUtf8.Length(), Digest, &DigestLen);
		for (unsigned int i = 0; i < DigestLen; ++i)
		{
			Signature += FString::Printf(TEXT("%02x"), Digest[i]);
		}
	}

	const TSharedRef<FJsonObject> Attestation = MakeShared<FJsonObject>();
	Attestation->SetStringField(TEXT("session_id"), Sdk->GetSessionId());
	Attestation->SetStringField(TEXT("machine_id"), FVedgMachineIdentity::GetMachineId());
	Attestation->SetStringField(TEXT("app_version"), Version);
	Attestation->SetNumberField(TEXT("timestamp"), (double)Timestamp);
	Attestation->SetStringField(TEXT("signature"), Signature);

	const TSharedRef<FJsonObject> Fields = MakeShared<FJsonObject>();
	Fields->SetStringField(TEXT("game"), Sdk->GetGameSlug());
	Fields->SetObjectField(TEXT("attestation"), Attestation);
	return Fields;
}
