// Fill out your copyright notice in the Description page of Project Settings.

#include "Matchmaking/VedgMatchServer.h"
#include "Http/VedgHttpRequest.h"
#include "VedgSDKModule.h"
#include "VedgSDKSettings.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformMisc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	TSharedRef<FJsonObject> GMatch = MakeShared<FJsonObject>();

	FString MatchString(const FString& Field, const FString& Fallback)
	{
		FString Value;
		return GMatch->TryGetStringField(Field, Value) ? Value : Fallback;
	}

	FVedgMatchServer::FOnOk OkOf(FVedgMatchServer::FOnOk OnDone)
	{
		return OnDone ? OnDone : [](bool) {};
	}
}

FString FVedgMatchServer::Env(const TCHAR* Name)
{
	return FPlatformMisc::GetEnvironmentVariable(Name);
}

bool FVedgMatchServer::IsMatchmade()
{
	return !Env(TEXT("SERVER_ID")).IsEmpty() && !Env(TEXT("TOKEN")).IsEmpty();
}

FString FVedgMatchServer::ServerId()
{
	return Env(TEXT("SERVER_ID"));
}

FString FVedgMatchServer::ServerToken()
{
	return Env(TEXT("TOKEN"));
}

FString FVedgMatchServer::ApiUrl()
{
	const FString Override = Env(TEXT("VEDG_API_URL"));
	return !Override.IsEmpty() ? Override : UVedgSDKSettings::Get()->GetApiUrl();
}

void FVedgMatchServer::LoadMatch(TFunction<void(const TSharedRef<FJsonObject>&)> OnDone)
{
	if (GMatch->Values.Num() > 0)
	{
		OnDone(GMatch);
		return;
	}

	const FString Raw = Env(TEXT("MATCH"));
	if (!Raw.IsEmpty())
	{
		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid())
		{
			GMatch = Parsed.ToSharedRef();
			OnDone(GMatch);
			return;
		}
		UE_LOG(LogVedgSDK, Error, TEXT("MATCH environment variable is not valid JSON, fetching it from the API"));
	}

	Post(TEXT("server_match"), MakeShared<FJsonObject>(), [OnDone](const FVedgHttpResult& Result)
	{
		if (Result.bValid && Result.ResponseObject.IsValid())
		{
			GMatch = Result.ResponseObject.ToSharedRef();
		}
		OnDone(GMatch);
	});
}

FString FVedgMatchServer::Queue()
{
	return MatchString(TEXT("queue"), Env(TEXT("GAMEMODE")));
}

FString FVedgMatchServer::MapName()
{
	return MatchString(TEXT("map"), Env(TEXT("MAP")));
}

TArray<TSharedPtr<FJsonValue>> FVedgMatchServer::Teams()
{
	const TArray<TSharedPtr<FJsonValue>>* Items;
	return GMatch->TryGetArrayField(TEXT("teams"), Items) ? *Items : TArray<TSharedPtr<FJsonValue>>();
}

int32 FVedgMatchServer::MaxPlayers()
{
	int32 Value = 0;
	GMatch->TryGetNumberField(TEXT("max_players"), Value);
	return Value;
}

FString FVedgMatchServer::Fill()
{
	const TSharedPtr<FJsonValue> Value = GMatch->TryGetField(TEXT("fill"));
	FString Out;
	return Value.IsValid() && Value->TryGetString(Out) ? Out : FString();
}

TSharedRef<FJsonObject> FVedgMatchServer::Custom()
{
	const TSharedPtr<FJsonObject>* Object;
	return GMatch->TryGetObjectField(TEXT("custom"), Object) && Object->IsValid() ? Object->ToSharedRef() : MakeShared<FJsonObject>();
}

void FVedgMatchServer::ReportReady(FOnOk OnDone)
{
	Post(TEXT("server_ready"), MakeShared<FJsonObject>(), [OnDone = OkOf(OnDone)](const FVedgHttpResult& Result) { OnDone(Result.bValid); });
}

void FVedgMatchServer::ReportStatus(const FString& Status, FOnOk OnDone)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("status"), Status);
	Post(TEXT("server_status"), Body, [OnDone = OkOf(OnDone)](const FVedgHttpResult& Result) { OnDone(Result.bValid); });
}

void FVedgMatchServer::ReportClosed(FOnOk OnDone)
{
	Post(TEXT("server_closed"), MakeShared<FJsonObject>(), [OnDone = OkOf(OnDone)](const FVedgHttpResult& Result) { OnDone(Result.bValid); });
}

void FVedgMatchServer::AuthorizePlayer(const FString& PlayerToken, const FString& JoinTicket, TFunction<void(const TSharedRef<FJsonObject>&)> OnDone)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("player_token"), PlayerToken);
	Body->SetStringField(TEXT("join_ticket"), JoinTicket);
	Post(TEXT("server_player_auth"), Body, [OnDone](const FVedgHttpResult& Result)
	{
		if (Result.bValid && Result.ResponseObject.IsValid())
		{
			OnDone(Result.ResponseObject.ToSharedRef());
			return;
		}
		const TSharedRef<FJsonObject> Denied = MakeShared<FJsonObject>();
		Denied->SetBoolField(TEXT("authorized"), false);
		OnDone(Denied);
	});
}

void FVedgMatchServer::PlayerDisconnected(const FString& Uid, FOnOk OnDone)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("player_uid"), Uid);
	Post(TEXT("server_player_disconnect"), Body, [Uid, OnDone = OkOf(OnDone)](const FVedgHttpResult& Result)
	{
		if (Result.bValid)
		{
			OnDone(true);
			return;
		}
		if (Result.StatusCode >= 400 && Result.StatusCode < 500)
		{
			UE_LOG(LogVedgSDK, Error, TEXT("Dropping disconnect of %s after client error %d"), *Uid, Result.StatusCode);
			OnDone(false);
			return;
		}
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Uid, OnDone](float)
		{
			PlayerDisconnected(Uid, OnDone);
			return false;
		}), 2.f);
	});
}

void FVedgMatchServer::Players(TFunction<void(const TArray<TSharedPtr<FJsonValue>>&)> OnDone)
{
	Post(TEXT("server_players"), MakeShared<FJsonObject>(), [OnDone](const FVedgHttpResult& Result)
	{
		const TArray<TSharedPtr<FJsonValue>>* Items;
		if (Result.bValid && Result.ResponseObject.IsValid() && Result.ResponseObject->TryGetArrayField(TEXT("players"), Items))
		{
			OnDone(*Items);
			return;
		}
		OnDone({});
	});
}

void FVedgMatchServer::Post(const FString& Endpoint, const TSharedRef<FJsonObject>& Body, TFunction<void(const FVedgHttpResult&)> OnDone)
{
	Body->SetStringField(TEXT("server"), ServerId());
	Body->SetStringField(TEXT("token"), ServerToken());
	Body->SetStringField(TEXT("server_token"), ServerToken());

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Body, Writer);
	FVedgHttpRequest::Request(ApiUrl() + TEXT("/matchmaker/") + Endpoint, TEXT("POST"), Json, [OnDone](FVedgHttpResult Result) { OnDone(Result); });
}
