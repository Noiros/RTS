// Fill out your copyright notice in the Description page of Project Settings.

#include "Auth/VedgLogin.h"
#include "Auth/VedgAttestation.h"
#include "Http/VedgHttpRequest.h"
#include "Identity/VedgMachineIdentity.h"
#include "VedgSDKModule.h"
#include "VedgSDKSettings.h"
#include "VedgSDKSubsystem.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "Hash/Blake3.h"
#include "Misc/AES.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr int32 MaxAttempts = 3;
	constexpr float RetryDelaySec = 2.f;

	FString ToJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	FString StringField(const TSharedPtr<FJsonObject>& Object, const FString& Field, const FString& Fallback = FString())
	{
		FString Value;
		return Object.IsValid() && Object->TryGetStringField(Field, Value) ? Value : Fallback;
	}

	FString CredentialsPath()
	{
		return FPaths::ProjectSavedDir() / TEXT("VedgSDK/user.dat");
	}

	// Bound to this machine, like the Godot file encrypted with OS.get_unique_id().
	FAES::FAESKey CredentialsKey()
	{
		const FTCHARToUTF8 MachineId(*FVedgMachineIdentity::GetMachineId());
		const FBlake3Hash Hash = FBlake3::HashBuffer(MachineId.Get(), MachineId.Length());
		FAES::FAESKey Key;
		FMemory::Memcpy(Key.Key, Hash.GetBytes(), sizeof(Key.Key));
		return Key;
	}
}

void FVedgLogin::Complete(UVedgSDKSubsystem* Sdk, const FVedgLoginResult& Result)
{
	if (!Result.bSuccess)
	{
		UE_LOG(LogVedgSDK, Error, TEXT("Login aborted (%s)"), *Result.Error);
		return;
	}
	if (!Sdk)
	{
		return;
	}

	Sdk->SetUid(Result.Uid);
	Sdk->LinkSessionToPlayer(Result.Uid);
	Sdk->AssignMachineToUser(Result.Uid);
	Sdk->SendEvent(Result.bIsNewAccount ? TEXT("register_success") : TEXT("login_success"), 0.f, Result.Uid);
}

// ---- Vedg account ----

void FVedgLogin::Register(UVedgSDKSubsystem* Sdk, const FString& Email, const FString& Password, const FString& Pseudo, FOnResult OnDone)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("email"), Email);
	Body->SetStringField(TEXT("password"), Password);
	Body->SetStringField(TEXT("pseudo"), Pseudo);

	TWeakObjectPtr<UVedgSDKSubsystem> WeakSdk(Sdk);
	Post(TEXT("/auth/register"), Body, [WeakSdk, Email, Password, Pseudo, OnDone](const FVedgHttpResult& Result)
	{
		if (!Result.bValid)
		{
			OnDone(Failed(Result, TEXT("account_creation_failed")));
			return;
		}
		Authenticate(WeakSdk.Get(), Email, Password, [Email, Pseudo, OnDone](const FVedgLoginResult& Authed)
		{
			FVedgLoginResult Out = Authed;
			if (Out.bSuccess)
			{
				Out.bIsNewAccount = true;
				Out.Pseudo = Pseudo;
				SaveCredentials(Email, Out.Refresh, Pseudo);
			}
			OnDone(Out);
		});
	});
}

void FVedgLogin::Authenticate(UVedgSDKSubsystem* Sdk, const FString& Email, const FString& Password, FOnResult OnDone)
{
	TWeakObjectPtr<UVedgSDKSubsystem> WeakSdk(Sdk);
	PlatformLogin(Email, Password, [WeakSdk, OnDone](const FVedgLoginResult& Authed)
	{
		Connect(WeakSdk.Get(), Authed, OnDone);
	});
}

void FVedgLogin::PlatformLogin(const FString& Email, const FString& Password, FOnResult OnDone)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("email"), Email);
	Body->SetStringField(TEXT("password"), Password);
	Post(TEXT("/auth/vedg/login"), Body, [OnDone](const FVedgHttpResult& Result) { OnDone(ParseSession(Result)); });
}

void FVedgLogin::RefreshSession(const FString& RefreshToken, FOnResult OnDone)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("refreshToken"), RefreshToken);
	Post(TEXT("/auth/vedg/refresh"), Body, [OnDone](const FVedgHttpResult& Result) { OnDone(ParseSession(Result)); });
}

void FVedgLogin::Resume(UVedgSDKSubsystem* Sdk, const TSharedRef<FJsonObject>& Saved, FOnResult OnDone)
{
	const FString User = StringField(Saved, TEXT("user"));
	const FString SavedPseudo = StringField(Saved, TEXT("pseudo"));
	TWeakObjectPtr<UVedgSDKSubsystem> WeakSdk(Sdk);

	FOnResult OnSession = [WeakSdk, User, SavedPseudo, OnDone](const FVedgLoginResult& Authed)
	{
		if (!Authed.bSuccess)
		{
			if (!Authed.bRetryable)
			{
				ClearCredentials();
			}
			OnDone(Authed);
			return;
		}
		FVedgLoginResult Out = Authed;
		if (Out.Pseudo.IsEmpty())
		{
			Out.Pseudo = SavedPseudo;
		}
		SaveCredentials(User, Out.Refresh, Out.Pseudo);
		Connect(WeakSdk.Get(), Out, OnDone);
	};

	const FString Refresh = StringField(Saved, TEXT("refresh"));
	if (!Refresh.IsEmpty())
	{
		RefreshSession(Refresh, OnSession);
	}
	else
	{
		PlatformLogin(User, StringField(Saved, TEXT("psw")), OnSession);
	}
}

void FVedgLogin::Connect(UVedgSDKSubsystem* Sdk, const FVedgLoginResult& Authed, FOnResult OnDone)
{
	if (!Authed.bSuccess)
	{
		OnDone(Authed);
		return;
	}
	ConnectSession(Sdk, TEXT("vedg_") + Authed.Token, [Authed, OnDone](const FVedgHttpResult& Result)
	{
		FString Token;
		if (!Result.bValid || !Result.ResponseObject.IsValid() || !Result.ResponseObject->TryGetStringField(TEXT("token"), Token))
		{
			OnDone(Failed(Result, TEXT("firebase_request_failed")));
			return;
		}
		FVedgLoginResult Out = Authed;
		Out.Token = Token;
		OnDone(Out);
	});
}

FVedgLoginResult FVedgLogin::ParseSession(const FVedgHttpResult& Result)
{
	FString IdToken;
	if (Result.bValid && Result.ResponseObject.IsValid() && Result.ResponseObject->TryGetStringField(TEXT("idToken"), IdToken))
	{
		const TSharedPtr<FJsonObject>& Data = Result.ResponseObject;
		FVedgLoginResult Out;
		Out.bSuccess = true;
		Out.Token = IdToken;
		Out.Refresh = StringField(Data, TEXT("refreshToken"));
		Out.Uid = StringField(Data, TEXT("uid"), StringField(Data, TEXT("localId")));
		Out.Pseudo = StringField(Data, TEXT("pseudo"));
		return Out;
	}
	return Failed(Result, TEXT("firebase_request_failed"));
}

FVedgLoginResult FVedgLogin::Failed(const FVedgHttpResult& Result, const FString& Fallback)
{
	FVedgLoginResult Out = FVedgLoginResult::Fail(ErrorOf(Result, Fallback), StringField(Result.ResponseObject, TEXT("ban_reason")), StringField(Result.ResponseObject, TEXT("banned_until")));
	Out.bRetryable = !IsFinal(Result.StatusCode);
	return Out;
}

// ---- Steam ----

void FVedgLogin::CheckSteamStatus(UVedgSDKSubsystem* Sdk, const FString& TicketHex, TFunction<void(const FVedgSteamStatus&)> OnDone)
{
	ConnectSession(Sdk, TEXT("steam_") + TicketHex, [OnDone](const FVedgHttpResult& Result)
	{
		FVedgSteamStatus Status;
		Status.Code = Result.StatusCode;
		if (Result.StatusCode == 403)
		{
			Status.bExists = true;
			Status.bBanned = true;
			Status.Error = StringField(Result.ResponseObject, TEXT("error"), TEXT("banned"));
			Status.Reason = StringField(Result.ResponseObject, TEXT("ban_reason"));
			Status.Until = StringField(Result.ResponseObject, TEXT("banned_until"));
		}
		else if (Result.StatusCode == 404)
		{
		}
		else if (!Result.bValid || !Result.ResponseObject.IsValid())
		{
			Status.bUnreachable = true;
		}
		else
		{
			Status.bExists = true;
			Status.Uid = StringField(Result.ResponseObject, TEXT("uid"));
			Status.Token = StringField(Result.ResponseObject, TEXT("token"));
		}
		OnDone(Status);
	});
}

void FVedgLogin::RegisterSteam(const FString& TicketHex, FOnResult OnDone)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("token"), TEXT("steam_") + TicketHex);
	Post(TEXT("/auth/register_external"), Body, [OnDone](const FVedgHttpResult& Result)
	{
		if (!Result.bValid)
		{
			OnDone(FVedgLoginResult::Fail(ErrorOf(Result, TEXT("account_creation_failed"))));
			return;
		}
		FVedgLoginResult Out;
		Out.bSuccess = true;
		const TSharedPtr<FJsonObject>* Account;
		if (Result.ResponseObject.IsValid() && Result.ResponseObject->TryGetObjectField(TEXT("account"), Account))
		{
			Out.Uid = StringField(*Account, TEXT("uid"));
		}
		OnDone(Out);
	});
}

// ---- account.gd ----

void FVedgLogin::Request(const FString& Url, TFunction<FString()> Body, TFunction<void(const FVedgHttpResult&)> OnDone, int32 Attempt)
{
	FVedgHttpRequest::Request(Url, TEXT("POST"), Body(), [Url, Body, OnDone, Attempt](FVedgHttpResult Result)
	{
		if (Result.bValid || IsFinal(Result.StatusCode) || Attempt >= MaxAttempts - 1)
		{
			OnDone(Result);
			return;
		}
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Url, Body, OnDone, Attempt](float)
		{
			Request(Url, Body, OnDone, Attempt + 1);
			return false;
		}), RetryDelaySec);
	});
}

void FVedgLogin::Post(const FString& Path, const TSharedRef<FJsonObject>& Body, TFunction<void(const FVedgHttpResult&)> OnDone)
{
	const FString Json = ToJson(Body);
	Request(UVedgSDKSettings::Get()->GetApiUrl() + Path, [Json]() { return Json; }, OnDone);
}

void FVedgLogin::ConnectSession(UVedgSDKSubsystem* Sdk, const FString& PlatformToken, TFunction<void(const FVedgHttpResult&)> OnDone)
{
	TWeakObjectPtr<UVedgSDKSubsystem> WeakSdk(Sdk);
	Request(UVedgSDKSettings::Get()->GetApiUrl() + TEXT("/auth/connect"), [WeakSdk, PlatformToken]()
	{
		const TSharedRef<FJsonObject> Fields = WeakSdk.IsValid() ? FVedgAttestation::LoginFields(WeakSdk.Get()) : MakeShared<FJsonObject>();
		Fields->SetStringField(TEXT("token"), PlatformToken);
		return ToJson(Fields);
	}, OnDone);
}

FString FVedgLogin::ErrorOf(const FVedgHttpResult& Result, const FString& Fallback)
{
	if (Result.StatusCode == 429)
	{
		return TEXT("too_many_requests");
	}
	return StringField(Result.ResponseObject, TEXT("error"), Fallback);
}

// ---- Saved credentials ----

bool FVedgLogin::GetCliLogin(FString& OutUser, FString& OutPassword)
{
	TArray<FString> Args;
	FString(FCommandLine::Get()).ParseIntoArrayWS(Args);
	const int32 Index = Args.IndexOfByPredicate([](const FString& Arg) { return Arg == TEXT("-login") || Arg == TEXT("--login"); });
	if (Index == INDEX_NONE || Index + 2 >= Args.Num())
	{
		return false;
	}
	OutUser = Args[Index + 1].TrimQuotes();
	OutPassword = Args[Index + 2].TrimQuotes();
	return true;
}

void FVedgLogin::SaveCredentials(const FString& Email, const FString& RefreshToken, const FString& Pseudo)
{
	const TSharedRef<FJsonObject> Saved = MakeShared<FJsonObject>();
	Saved->SetStringField(TEXT("user"), Email);
	Saved->SetStringField(TEXT("refresh"), RefreshToken);
	Saved->SetStringField(TEXT("pseudo"), Pseudo);

	const FTCHARToUTF8 Plain(*ToJson(Saved));
	// 4-byte length prefix, then the JSON, zero-padded to the AES block size.
	TArray<uint8> Data;
	const int32 Length = Plain.Length();
	Data.Append(reinterpret_cast<const uint8*>(&Length), sizeof(Length));
	Data.Append(reinterpret_cast<const uint8*>(Plain.Get()), Length);
	Data.SetNumZeroed(Align(Data.Num(), FAES::AESBlockSize));
	FAES::EncryptData(Data.GetData(), Data.Num(), CredentialsKey());

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CredentialsPath()), true);
	if (!FFileHelper::SaveArrayToFile(Data, *CredentialsPath()))
	{
		UE_LOG(LogVedgSDK, Error, TEXT("Cant write credentials to %s"), *CredentialsPath());
	}
}

TSharedPtr<FJsonObject> FVedgLogin::GetSavedLogin()
{
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *CredentialsPath(), FILEREAD_Silent) || Data.Num() < (int32)FAES::AESBlockSize || Data.Num() % FAES::AESBlockSize != 0)
	{
		return nullptr;
	}
	FAES::DecryptData(Data.GetData(), Data.Num(), CredentialsKey());

	int32 Length = 0;
	FMemory::Memcpy(&Length, Data.GetData(), sizeof(Length));
	if (Length <= 0 || Length > Data.Num() - (int32)sizeof(Length))
	{
		return nullptr;
	}
	const FUTF8ToTCHAR Json(reinterpret_cast<const ANSICHAR*>(Data.GetData() + sizeof(Length)), Length);

	TSharedPtr<FJsonObject> Saved;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FString(Json.Length(), Json.Get()));
	if (!FJsonSerializer::Deserialize(Reader, Saved) || !Saved.IsValid() || !Saved->HasField(TEXT("user")) || !(Saved->HasField(TEXT("refresh")) || Saved->HasField(TEXT("psw"))))
	{
		return nullptr;
	}
	return Saved;
}

void FVedgLogin::ClearCredentials()
{
	IFileManager::Get().Delete(*CredentialsPath(), false, false, true);
}
