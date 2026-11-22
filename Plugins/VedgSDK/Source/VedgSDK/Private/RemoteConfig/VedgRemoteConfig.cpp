// Fill out your copyright notice in the Description page of Project Settings.

#include "RemoteConfig/VedgRemoteConfig.h"
#include "Http/VedgHttpRequest.h"
#include "VedgSDKModule.h"
#include "VedgSDKSettings.h"
#include "GenericPlatform/GenericPlatformHttp.h"

void UVedgRemoteConfig::Fetch(const FString& Key, bool bForce)
{
	FetchThen(Key, bForce, nullptr);
}

void UVedgRemoteConfig::FetchThen(const FString& Key, bool bForce, TFunction<void(bool)> OnDone)
{
	if (Config.Contains(Key) && !bForce)
	{
		OnKeyFetched.Broadcast(Key, true);
		if (OnDone)
		{
			OnDone(true);
		}
		return;
	}

	const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();
	const FString Url = FString::Printf(TEXT("%s/remote-config/%s/%s?env=%s"), *Settings->GetApiUrl(), *Settings->GetGameSlug(), *FGenericPlatformHttp::UrlEncode(Key), *FGenericPlatformHttp::UrlEncode(Settings->ResolveGameEnv()));

	TWeakObjectPtr<UVedgRemoteConfig> WeakThis(this);
	FVedgHttpRequest::Request(Url, TEXT("GET"), FString(), [WeakThis, Key, OnDone](FVedgHttpResult Result)
	{
		UVedgRemoteConfig* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (Result.bValid)
		{
			// Only objects are kept by FVedgHttpResult; GetDict only reads objects anyway.
			TSharedPtr<FJsonValue> Value = MakeShared<FJsonValueNull>();
			if (Result.ResponseObject.IsValid())
			{
				Value = MakeShared<FJsonValueObject>(Result.ResponseObject);
			}
			Self->Config.Add(Key, Value);
			UE_LOG(LogVedgSDK, Log, TEXT("Remote config key \"%s\" loaded"), *Key);
		}
		Self->OnKeyFetched.Broadcast(Key, Result.bValid);
		if (OnDone)
		{
			OnDone(Result.bValid);
		}
	});
}

FJsonObjectWrapper UVedgRemoteConfig::GetDict(const FString& Key) const
{
	FJsonObjectWrapper Wrapper;
	const TSharedPtr<FJsonValue>* Value = Config.Find(Key);
	const TSharedPtr<FJsonObject>* Object;
	if (Value && Value->IsValid() && (*Value)->TryGetObject(Object))
	{
		Wrapper.JsonObject = *Object;
	}
	return Wrapper;
}
