// Fill out your copyright notice in the Description page of Project Settings.

#include "RemoteConfig/VedgRemoteConfig.h"
#include "Http/VedgHttpRequest.h"
#include "VedgSDKSettings.h"

void UVedgRemoteConfig::Fetch()
{
	const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();
	const FString ApiUrl = Settings->GetApiUrl();
	if (ApiUrl.IsEmpty())
	{
		return;
	}
	const FString Url = ApiUrl + TEXT("/remote-config/") + Settings->GetGameSlug();

	TWeakObjectPtr<UVedgRemoteConfig> WeakThis(this);
	FVedgHttpRequest::Request(Url, TEXT("GET"), FString(),
		[WeakThis](FVedgHttpResult Result)
		{
			UVedgRemoteConfig* Self = WeakThis.Get();
			if (!Self || !Result.bValid || !Result.ResponseObject.IsValid())
			{
				return;
			}
			Self->Config = Result.ResponseObject;
		});
}

FJsonObjectWrapper UVedgRemoteConfig::GetDict(const FString& Key) const
{
	FJsonObjectWrapper Wrapper;
	if (Config.IsValid())
	{
		const TSharedPtr<FJsonObject>* Found;
		if (Config->TryGetObjectField(Key, Found) && Found->IsValid())
		{
			Wrapper.JsonObject = *Found;
		}
	}
	return Wrapper;
}
