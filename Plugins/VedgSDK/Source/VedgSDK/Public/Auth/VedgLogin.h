// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "VedgSDKTypes.h"

class UVedgSDKSubsystem;

// The login requests (login/account.gd + providers), UI-agnostic:
// UVedgLoginPanel drives them, or call them directly from your own UI and
// finish with Complete(). Callbacks run on the game thread.
class VEDGSDK_API FVedgLogin
{
public:
	using FOnResult = TFunction<void(const FVedgLoginResult&)>;

	// Session wiring after a successful login, mirrors VedgLogin.start().
	static void Complete(UVedgSDKSubsystem* Sdk, const FVedgLoginResult& Result);

	// ---- Vedg account (email + password) ----
	static void Register(UVedgSDKSubsystem* Sdk, const FString& Email, const FString& Password, const FString& Pseudo, FOnResult OnDone);
	static void Authenticate(UVedgSDKSubsystem* Sdk, const FString& Email, const FString& Password, FOnResult OnDone);
	// Logs back in from saved credentials (refresh token, or legacy password).
	static void Resume(UVedgSDKSubsystem* Sdk, const TSharedRef<FJsonObject>& Saved, FOnResult OnDone);

	// ---- Steam ----
	// The game supplies the hex Web API ticket from its own Steamworks binding
	// (ISteamUser::GetAuthTicketForWebApi); it is prefixed here.
	static void CheckSteamStatus(UVedgSDKSubsystem* Sdk, const FString& TicketHex, TFunction<void(const FVedgSteamStatus&)> OnDone);
	static void RegisterSteam(const FString& TicketHex, FOnResult OnDone);

	// ---- account.gd ----
	// Re-signed on each retry so the attestation timestamp stays fresh.
	static void ConnectSession(UVedgSDKSubsystem* Sdk, const FString& PlatformToken, TFunction<void(const FVedgHttpResult&)> OnDone);
	static bool IsFinal(int32 Code) { return Code >= 400 && Code < 500; }
	static FString ErrorOf(const FVedgHttpResult& Result, const FString& Fallback);

	// ---- Saved credentials ----
	// -login <email> <password> on the command line (test clients).
	static bool GetCliLogin(FString& OutUser, FString& OutPassword);
	static void SaveCredentials(const FString& Email, const FString& RefreshToken, const FString& Pseudo = FString());
	static TSharedPtr<FJsonObject> GetSavedLogin();
	static void ClearCredentials();

private:
	static void PlatformLogin(const FString& Email, const FString& Password, FOnResult OnDone);
	static void RefreshSession(const FString& RefreshToken, FOnResult OnDone);
	static void Connect(UVedgSDKSubsystem* Sdk, const FVedgLoginResult& Authed, FOnResult OnDone);
	static FVedgLoginResult ParseSession(const FVedgHttpResult& Result);
	static FVedgLoginResult Failed(const FVedgHttpResult& Result, const FString& Fallback);
	static void Post(const FString& Path, const TSharedRef<FJsonObject>& Body, TFunction<void(const FVedgHttpResult&)> OnDone);
	static void Request(const FString& Url, TFunction<FString()> Body, TFunction<void(const FVedgHttpResult&)> OnDone, int32 Attempt = 0);
};
