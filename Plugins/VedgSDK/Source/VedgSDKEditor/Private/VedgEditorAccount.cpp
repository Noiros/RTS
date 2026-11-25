// Fill out your copyright notice in the Description page of Project Settings.

#include "VedgEditorAccount.h"
#include "Http/VedgHttpRequest.h"
#include "VedgSDKModule.h"
#include "VedgSDKSettings.h"
#include "VedgSDKTypes.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FSimpleMulticastDelegate FVedgEditorAccount::OnAccessChanged;

namespace
{
	const TCHAR* Section = TEXT("VedgSDK.Account");
	const TCHAR* EmailKey = TEXT("Email");
	const TCHAR* RefreshKey = TEXT("RefreshToken");
	constexpr double IdTokenLifetimeSec = 50.0 * 60.0;

	FString IdToken;
	double IdTokenUntil = 0.0;

	FString ApiUrl()
	{
		return UVedgSDKSettings::Get()->GetApiUrl();
	}

	FString Slug()
	{
		return FGenericPlatformHttp::UrlEncode(UVedgSDKSettings::Get()->GetGameSlug());
	}

	FString ToJson(const TMap<FString, FString>& Fields)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& Field : Fields)
		{
			Object->SetStringField(Field.Key, Field.Value);
		}
		FString Out;
		FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out));
		return Out;
	}
}

FString FVedgEditorAccount::Setting(const TCHAR* Key)
{
	FString Value;
	GConfig->GetString(Section, Key, Value, GEditorPerProjectIni);
	return Value;
}

void FVedgEditorAccount::SetSetting(const TCHAR* Key, const FString& Value)
{
	GConfig->SetString(Section, Key, *Value, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

bool FVedgEditorAccount::IsLoggedIn()
{
	return !Setting(RefreshKey).IsEmpty();
}

FString FVedgEditorAccount::Email()
{
	return Setting(EmailKey);
}

FString FVedgEditorAccount::BuildKeyFile()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("VedgSDK/build_key"));
}

FString FVedgEditorAccount::ErrorOf(const FVedgHttpResult& Result, const FString& Fallback)
{
	if (Result.StatusCode == 0)
	{
		return FString::Printf(TEXT("Cannot reach %s"), *ApiUrl());
	}
	FString Error;
	return Result.ResponseObject.IsValid() && Result.ResponseObject->TryGetStringField(TEXT("error"), Error) ? Error : Fallback;
}

void FVedgEditorAccount::StoreSession(const TSharedPtr<FJsonObject>& Data)
{
	IdToken = Data->GetStringField(TEXT("idToken"));
	IdTokenUntil = FPlatformTime::Seconds() + IdTokenLifetimeSec;
	FString Refresh;
	if (Data->TryGetStringField(TEXT("refreshToken"), Refresh) && !Refresh.IsEmpty())
	{
		SetSetting(RefreshKey, Refresh);
	}
}

void FVedgEditorAccount::Login(const FString& InEmail, const FString& Password, TFunction<void(const FString&)> OnDone)
{
	const FString Body = ToJson({{TEXT("email"), InEmail}, {TEXT("password"), Password}});
	FVedgHttpRequest::Request(ApiUrl() + TEXT("/auth/vedg/login"), TEXT("POST"), Body, [InEmail, OnDone](FVedgHttpResult Result)
	{
		if (Result.StatusCode != 200 || !Result.ResponseObject.IsValid() || !Result.ResponseObject->HasTypedField<EJson::String>(TEXT("idToken")))
		{
			OnDone(ErrorOf(Result, TEXT("Login failed")));
			return;
		}
		StoreSession(Result.ResponseObject);
		SetSetting(EmailKey, InEmail);
		RefreshBuildKey();
		OnAccessChanged.Broadcast();
		OnDone(FString());
	});
}

void FVedgEditorAccount::Logout()
{
	IdToken.Reset();
	IdTokenUntil = 0.0;
	SetSetting(RefreshKey, FString());
	SetSetting(EmailKey, FString());
	IFileManager::Get().Delete(*BuildKeyFile(), false, false, true);
	OnAccessChanged.Broadcast();
}

void FVedgEditorAccount::Token(TFunction<void(const FString&)> OnDone)
{
	if (!IdToken.IsEmpty() && FPlatformTime::Seconds() < IdTokenUntil)
	{
		OnDone(TEXT("vedg_") + IdToken);
		return;
	}
	const FString Refresh = Setting(RefreshKey);
	if (Refresh.IsEmpty())
	{
		OnDone(FString());
		return;
	}
	FVedgHttpRequest::Request(ApiUrl() + TEXT("/auth/vedg/refresh"), TEXT("POST"), ToJson({{TEXT("refreshToken"), Refresh}}), [OnDone](FVedgHttpResult Result)
	{
		if (Result.StatusCode != 200 || !Result.ResponseObject.IsValid() || !Result.ResponseObject->HasTypedField<EJson::String>(TEXT("idToken")))
		{
			if (Result.StatusCode >= 400 && Result.StatusCode < 500)
			{
				Logout();
			}
			OnDone(FString());
			return;
		}
		StoreSession(Result.ResponseObject);
		OnDone(TEXT("vedg_") + IdToken);
	});
}

void FVedgEditorAccount::FetchKey(const FString& Path, FOnKey OnDone)
{
	Token([Path, OnDone](const FString& Auth)
	{
		if (Auth.IsEmpty())
		{
			UE_LOG(LogVedgSDK, Warning, TEXT("Vedg: not logged in, cannot read %s"), *Path);
			OnDone(FString());
			return;
		}
		FVedgHttpRequest::Request(ApiUrl() + Path, TEXT("GET"), FString(), [Path, OnDone](FVedgHttpResult Result)
		{
			FString Key;
			if (Result.StatusCode != 200 || !Result.ResponseObject.IsValid() || !Result.ResponseObject->TryGetStringField(TEXT("key"), Key))
			{
				UE_LOG(LogVedgSDK, Warning, TEXT("Vedg: cannot read %s (%s)"), *Path, *ErrorOf(Result, FString::Printf(TEXT("HTTP %d"), Result.StatusCode)));
			}
			OnDone(Key);
		}, 10.f, Auth);
	});
}

void FVedgEditorAccount::RefreshBuildKey(FOnKey OnDone)
{
	FetchKey(FString::Printf(TEXT("/admin/games/%s/build-key"), *Slug()), [OnDone](const FString& Key)
	{
		FString Cached;
		FFileHelper::LoadFileToString(Cached, *BuildKeyFile());
		if (!Key.IsEmpty() && Key != Cached.TrimStartAndEnd())
		{
			FFileHelper::SaveStringToFile(Key, *BuildKeyFile());
		}
		if (OnDone)
		{
			OnDone(Key.IsEmpty() ? Cached.TrimStartAndEnd() : Key);
		}
	});
}

void FVedgEditorAccount::HeatmapKey(FOnKey OnDone)
{
	FetchKey(FString::Printf(TEXT("/admin/games/%s/heatmap-key"), *Slug()), MoveTemp(OnDone));
}

void FVedgEditorAccount::DeployKey(FOnKey OnDone)
{
	FetchKey(FString::Printf(TEXT("/admin/infra/deploy-key?game=%s"), *Slug()), MoveTemp(OnDone));
}
