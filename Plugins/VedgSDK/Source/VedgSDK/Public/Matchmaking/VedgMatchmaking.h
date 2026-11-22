// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "JsonObjectWrapper.h"
#include "VedgMatchmaking.generated.h"

class IWebSocket;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVedgOnMatchmakingConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVedgOnMatchmakingDisconnected, int32, Code, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVedgOnMatchmakingPacket, const FString&, FuncName, const FJsonObjectWrapper&, Args);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVedgOnMatchmakingData, const FJsonObjectWrapper&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVedgOnMatchmakingBanned, const FString&, Reason);

// Player-side matchmaking WebSocket (also carries the social pushes), mirrors
// matchmaking.gd. Owned by the SDK subsystem: GetMatchmaking().
UCLASS(BlueprintType)
class VEDGSDK_API UVedgMatchmaking : public UObject
{
	GENERATED_BODY()

public:
	void Shutdown();

	// Connects with the player token from login, reconnecting until Close().
	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Matchmaking")
	void Open(const FString& PlayerToken);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Matchmaking")
	void Close();

	// Queued until the socket is open.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Matchmaking")
	void SendJson(const FString& FuncName, const FJsonObjectWrapper& Args);

	void Send(const FString& FuncName, const TSharedPtr<FJsonObject>& Args = nullptr);

	// C++ handler for one packet name (replaces any previous one).
	void On(const FString& FuncName, TFunction<void(const TSharedPtr<FJsonObject>&)> Handler);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Matchmaking")
	void Find(const FString& Queue, const FString& Code = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Matchmaking")
	void Cancel();

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Matchmaking")
	void SetReady(bool bReady, const FString& Queue);

	// GET /matchmaking/{game}/queues - the queues configured for this env.
	void Queues(TFunction<void(const TArray<TSharedPtr<FJsonValue>>&)> OnDone) const;

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Matchmaking")
	bool IsOpen() const { return bIsOpen; }

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Matchmaking")
	FString GetToken() const { return Token; }

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Matchmaking")
	FVedgOnMatchmakingConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Matchmaking")
	FVedgOnMatchmakingDisconnected OnDisconnected;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Matchmaking")
	FVedgOnMatchmakingPacket OnPacketReceived;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Matchmaking")
	FVedgOnMatchmakingData OnStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Matchmaking")
	FVedgOnMatchmakingData OnConnectToServer;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Matchmaking")
	FVedgOnMatchmakingBanned OnBanned;

private:
	void Connect();
	void HandleOpen();
	void HandleMessage(const FString& Message);
	void HandleClosed(int32 Code, const FString& Reason);
	void Dispatch(const FString& FuncName, const TSharedPtr<FJsonObject>& Args);
	void ReconnectLater();
	void SendRaw(const TSharedRef<FJsonObject>& Payload);
	FString WsUrl() const;

	TSharedPtr<IWebSocket> Socket;
	TMap<FString, TFunction<void(const TSharedPtr<FJsonObject>&)>> Handlers;
	TArray<TSharedRef<FJsonObject>> Pending;
	FString Token;
	bool bIsOpen = false;
	bool bWantsConnection = false;

	FTSTicker::FDelegateHandle PingHandle;
	FTSTicker::FDelegateHandle ReconnectHandle;

	static constexpr float PingInterval = 10.f;
	static constexpr float ReconnectDelay = 5.f;
};
