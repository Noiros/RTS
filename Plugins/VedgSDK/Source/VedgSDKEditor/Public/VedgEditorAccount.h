// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

struct FVedgHttpResult;

// Studio (Vedg) account of the person using the editor, used to fetch the game's build, heatmap and deploy keys.
// Mirrors VedgEditorAccount (dock/vedg_account.gd). The refresh token lives in the per-user editor ini, never in
// Config/Default*.ini; the build key is cached in Saved/VedgSDK/build_key, read by VedgSDK.Build.cs.
class VEDGSDKEDITOR_API FVedgEditorAccount
{
public:
	// Called with the key, or an empty string on failure (already logged).
	using FOnKey = TFunction<void(const FString&)>;

	static bool IsLoggedIn();
	static FString Email();

	// OnDone receives an empty string on success, else the error.
	static void Login(const FString& Email, const FString& Password, TFunction<void(const FString&)> OnDone);
	static void Logout();

	// Fetches the build key and caches it for packaging; falls back to the cached one when offline.
	static void RefreshBuildKey(FOnKey OnDone = nullptr);
	static void HeatmapKey(FOnKey OnDone);
	static void DeployKey(FOnKey OnDone);

	static FString BuildKeyFile();
	static FString ErrorOf(const FVedgHttpResult& Result, const FString& Fallback);

	// Login / logout: the dock shows its tabs to logged-in users only.
	static FSimpleMulticastDelegate OnAccessChanged;

private:
	static void Token(TFunction<void(const FString&)> OnDone);
	static void FetchKey(const FString& Path, FOnKey OnDone);
	static void StoreSession(const TSharedPtr<class FJsonObject>& Data);
	static FString Setting(const TCHAR* Key);
	static void SetSetting(const TCHAR* Key, const FString& Value);
};
