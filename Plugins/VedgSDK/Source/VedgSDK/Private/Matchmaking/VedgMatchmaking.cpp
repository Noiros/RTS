// Fill out your copyright notice in the Description page of Project Settings.

#include "Matchmaking/VedgMatchmaking.h"
#include "Http/VedgHttpRequest.h"
#include "VedgSDKModule.h"
#include "VedgSDKSettings.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "IWebSocket.h"
#include "Misc/CoreMisc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WebSocketsModule.h"

namespace
{
	const TArray<int32> NoReconnectCodes = { 4000, 4001, 4003, 4004, 4009 };

	FString Encode(const FString& Value)
	{
		return FGenericPlatformHttp::UrlEncode(Value);
	}
}

void UVedgMatchmaking::Shutdown()
{
	Close();
}

void UVedgMatchmaking::Open(const FString& PlayerToken)
{
	if (IsRunningDedicatedServer())
	{
		UE_LOG(LogVedgSDK, Log, TEXT("Matchmaking WebSocket is disabled on dedicated servers"));
		return;
	}
	Token = PlayerToken;
	bWantsConnection = true;
	Connect();
}

void UVedgMatchmaking::Close()
{
	bWantsConnection = false;
	bIsOpen = false;
	if (PingHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PingHandle);
		PingHandle.Reset();
	}
	if (ReconnectHandle.IsValid())
	{
		FTSTicker::RemoveTicker(ReconnectHandle);
		ReconnectHandle.Reset();
	}
	if (Socket.IsValid())
	{
		const TSharedPtr<IWebSocket> Closing = Socket;
		Socket.Reset();
		Closing->OnConnected().Clear();
		Closing->OnConnectionError().Clear();
		Closing->OnClosed().Clear();
		Closing->OnMessage().Clear();
		Closing->Close();
	}
}

void UVedgMatchmaking::On(const FString& FuncName, TFunction<void(const TSharedPtr<FJsonObject>&)> Handler)
{
	Handlers.Add(FuncName, MoveTemp(Handler));
}

void UVedgMatchmaking::SendJson(const FString& FuncName, const FJsonObjectWrapper& Args)
{
	Send(FuncName, Args.JsonObject);
}

void UVedgMatchmaking::Send(const FString& FuncName, const TSharedPtr<FJsonObject>& Args)
{
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("func"), FuncName);
	Payload->SetObjectField(TEXT("args"), Args.IsValid() ? Args : MakeShared<FJsonObject>());
	if (!bIsOpen)
	{
		Pending.Add(Payload);
		return;
	}
	SendRaw(Payload);
}

void UVedgMatchmaking::Find(const FString& Queue, const FString& Code)
{
	const TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
	Args->SetStringField(TEXT("gamemode"), Queue);
	Args->SetStringField(TEXT("code"), Code);
	Send(TEXT("find_server"), Args);
}

void UVedgMatchmaking::Cancel()
{
	Send(TEXT("cancel_matchmaking"));
}

void UVedgMatchmaking::SetReady(bool bReady, const FString& Queue)
{
	const TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
	Args->SetBoolField(TEXT("ready"), bReady);
	Args->SetStringField(TEXT("gamemode"), Queue);
	Send(TEXT("set_ready"), Args);
}

void UVedgMatchmaking::Queues(TFunction<void(const TArray<TSharedPtr<FJsonValue>>&)> OnDone) const
{
	const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();
	const FString Url = FString::Printf(TEXT("%s/matchmaking/%s/queues?env=%s"), *Settings->GetApiUrl(), *Encode(Settings->GetGameSlug()), *Encode(Settings->ResolveGameEnv()));
	FVedgHttpRequest::Request(Url, TEXT("GET"), FString(), [OnDone](FVedgHttpResult Result)
	{
		const TArray<TSharedPtr<FJsonValue>>* Items;
		if (Result.bValid && Result.ResponseObject.IsValid() && Result.ResponseObject->TryGetArrayField(TEXT("queues"), Items))
		{
			OnDone(*Items);
			return;
		}
		OnDone({});
	});
}

FString UVedgMatchmaking::WsUrl() const
{
	const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();
	const FString Base = Settings->GetApiUrl().Replace(TEXT("https://"), TEXT("wss://")).Replace(TEXT("http://"), TEXT("ws://"));
	return FString::Printf(TEXT("%s/ws?token=%s&game=%s&env=%s"), *Base, *Encode(Token), *Encode(Settings->GetGameSlug()), *Encode(Settings->ResolveGameEnv()));
}

void UVedgMatchmaking::Connect()
{
	FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
	if (Socket.IsValid())
	{
		// A stale socket's late close must not drive the new one's state.
		Socket->OnConnected().Clear();
		Socket->OnConnectionError().Clear();
		Socket->OnClosed().Clear();
		Socket->OnMessage().Clear();
	}
	Socket =FWebSocketsModule::Get().CreateWebSocket(WsUrl());

	TWeakObjectPtr<UVedgMatchmaking> WeakThis(this);
	Socket->OnConnected().AddLambda([WeakThis]()
	{
		if (UVedgMatchmaking* Self = WeakThis.Get())
		{
			Self->HandleOpen();
		}
	});
	Socket->OnConnectionError().AddLambda([WeakThis](const FString& Error)
	{
		if (UVedgMatchmaking* Self = WeakThis.Get())
		{
			UE_LOG(LogVedgSDK, Error, TEXT("Cannot connect to matchmaking WebSocket: %s"), *Error);
			Self->HandleClosed(0, Error);
		}
	});
	Socket->OnClosed().AddLambda([WeakThis](int32 Code, const FString& Reason, bool)
	{
		if (UVedgMatchmaking* Self = WeakThis.Get())
		{
			Self->HandleClosed(Code, Reason);
		}
	});
	Socket->OnMessage().AddLambda([WeakThis](const FString& Message)
	{
		if (UVedgMatchmaking* Self = WeakThis.Get())
		{
			Self->HandleMessage(Message);
		}
	});

	UE_LOG(LogVedgSDK, Log, TEXT("Connecting to matchmaking WebSocket"));
	Socket->Connect();
}

void UVedgMatchmaking::HandleOpen()
{
	bIsOpen = true;
	UE_LOG(LogVedgSDK, Log, TEXT("Matchmaking WebSocket connected"));

	Send(TEXT("ping"));
	TWeakObjectPtr<UVedgMatchmaking> WeakThis(this);
	PingHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
	{
		if (UVedgMatchmaking* Self = WeakThis.Get())
		{
			Self->Send(TEXT("ping"));
			return true;
		}
		return false;
	}), PingInterval);

	OnConnected.Broadcast();
	TArray<TSharedRef<FJsonObject>> Queued = MoveTemp(Pending);
	Pending.Reset();
	for (const TSharedRef<FJsonObject>& Payload : Queued)
	{
		SendRaw(Payload);
	}
}

void UVedgMatchmaking::HandleMessage(const FString& Message)
{
	TSharedPtr<FJsonObject> Packet;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
	FString FuncName;
	if (!FJsonSerializer::Deserialize(Reader, Packet) || !Packet.IsValid() || !Packet->TryGetStringField(TEXT("func"), FuncName))
	{
		return;
	}
	const TSharedPtr<FJsonObject>* Args;
	Dispatch(FuncName, Packet->TryGetObjectField(TEXT("args"), Args) ? *Args : MakeShared<FJsonObject>());
}

void UVedgMatchmaking::HandleClosed(int32 Code, const FString& Reason)
{
	if (PingHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PingHandle);
		PingHandle.Reset();
	}
	if (bIsOpen)
	{
		bIsOpen = false;
		UE_LOG(LogVedgSDK, Log, TEXT("Matchmaking WebSocket closed (%d %s)"), Code, *Reason);
	}
	OnDisconnected.Broadcast(Code, Reason);
	if (NoReconnectCodes.Contains(Code))
	{
		bWantsConnection = false;
	}
	ReconnectLater();
}

void UVedgMatchmaking::Dispatch(const FString& FuncName, const TSharedPtr<FJsonObject>& Args)
{
	FJsonObjectWrapper Wrapper;
	Wrapper.JsonObject = Args;

	if (FuncName == TEXT("matchmaker_status"))
	{
		OnStatusChanged.Broadcast(Wrapper);
	}
	else if (FuncName == TEXT("connect_to_server"))
	{
		OnConnectToServer.Broadcast(Wrapper);
	}
	else if (FuncName == TEXT("banned"))
	{
		bWantsConnection = false;
		FString Reason;
		Args->TryGetStringField(TEXT("reason"), Reason);
		OnBanned.Broadcast(Reason);
	}

	if (const TFunction<void(const TSharedPtr<FJsonObject>&)>* Handler = Handlers.Find(FuncName))
	{
		(*Handler)(Args);
	}
	OnPacketReceived.Broadcast(FuncName, Wrapper);
}

void UVedgMatchmaking::ReconnectLater()
{
	if (ReconnectHandle.IsValid() || !bWantsConnection)
	{
		return;
	}
	TWeakObjectPtr<UVedgMatchmaking> WeakThis(this);
	ReconnectHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
	{
		UVedgMatchmaking* Self = WeakThis.Get();
		if (!Self)
		{
			return false;
		}
		Self->ReconnectHandle.Reset();
		if (Self->bWantsConnection && !Self->bIsOpen)
		{
			UE_LOG(LogVedgSDK, Log, TEXT("Reconnecting to matchmaking WebSocket"));
			Self->Connect();
		}
		return false;
	}), ReconnectDelay);
}

void UVedgMatchmaking::SendRaw(const TSharedRef<FJsonObject>& Payload)
{
	if (!Socket.IsValid() || !Socket->IsConnected())
	{
		Pending.Add(Payload);
		return;
	}
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Payload, Writer);
	Socket->Send(Out);
}
