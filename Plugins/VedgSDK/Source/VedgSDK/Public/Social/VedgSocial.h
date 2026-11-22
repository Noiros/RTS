// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "JsonObjectWrapper.h"
#include "VedgSDKTypes.h"
#include "VedgSocial.generated.h"

class UVedgMatchmaking;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVedgOnSocialUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVedgOnSocialPlayer, const FString&, Uid, const FString&, Pseudo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVedgOnSocialUid, const FString&, Uid);
// Fired after every action (RequestFriend, Kick, ...). ErrorText is "" on success.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVedgOnSocialAction, const FString&, Action, bool, bSuccess, const FText&, ErrorText);

// Friends + groups (lobbies), mirrors social.gd. Authenticated with the
// matchmaking token and kept live by its WebSocket pushes. Owned by the SDK
// subsystem: GetSocial().
UCLASS(BlueprintType)
class VEDGSDK_API UVedgSocial : public UObject
{
	GENERATED_BODY()

public:
	void StartUp(UVedgMatchmaking* InMatchmaking);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void Refresh();

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void RefreshFriends();

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void RefreshGroup();

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void RequestFriend(const FString& Pseudo);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void AcceptFriend(const FString& Uid);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void DeclineFriend(const FString& Uid);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void RemoveFriend(const FString& Uid);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void InviteToGroup(const FString& Uid);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void AcceptGroupInvite(const FString& Uid);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void DeclineGroupInvite(const FString& Uid);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void Kick(const FString& Uid);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void JoinFriend(const FString& Uid);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void TransferOwner(const FString& Uid);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Social")
	void LeaveGroup();

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Social")
	TArray<FVedgSocialEntry> GetFriends() const { return ToEntries(Friends); }

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Social")
	TArray<FVedgSocialEntry> GetFriendRequests() const { return ToEntries(FriendRequests); }

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Social")
	TArray<FVedgSocialEntry> GetSentRequests() const { return ToEntries(SentRequests); }

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Social")
	TArray<FVedgSocialEntry> GetGroupInvites() const { return ToEntries(GroupInvites); }

	// Raw group object ({members, owner, ready_players, pseudos, ...}).
	UFUNCTION(BlueprintPure, Category = "VedgSDK|Social")
	FJsonObjectWrapper GetGroup() const;

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Social")
	TArray<FString> GetMembers() const;

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Social")
	FString GetOwner() const;

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Social")
	bool IsGrouped() const { return GetMembers().Num() > 1; }

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Social")
	bool IsGroupOwner() const;

	// Empty Uid = the local player.
	UFUNCTION(BlueprintPure, Category = "VedgSDK|Social")
	bool IsReady(const FString& Uid) const;

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Social")
	FString GetPseudo(const FString& Uid) const;

	// Raw friend entry by uid across friends / requests / sent, null if none.
	TSharedPtr<FJsonObject> GetFriend(const FString& Uid) const;

	// User-facing message for a failed request, empty on success.
	static FText ErrorText(const FVedgHttpResult& Result);

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Social")
	FVedgOnSocialUpdated OnFriendsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Social")
	FVedgOnSocialPlayer OnFriendRequestReceived;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Social")
	FVedgOnSocialUpdated OnGroupUpdated;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Social")
	FVedgOnSocialUpdated OnGroupLeft;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Social")
	FVedgOnSocialPlayer OnGroupInviteReceived;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Social")
	FVedgOnSocialUid OnGroupInviteDeclined;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Social")
	FVedgOnSocialAction OnActionFinished;

	TArray<TSharedPtr<FJsonObject>> Friends;
	TArray<TSharedPtr<FJsonObject>> FriendRequests;
	TArray<TSharedPtr<FJsonObject>> SentRequests;
	TArray<TSharedPtr<FJsonObject>> GroupInvites;
	TSharedRef<FJsonObject> Group = MakeShared<FJsonObject>();

private:
	UFUNCTION()
	void HandleConnected();

	UFUNCTION()
	void HandlePacket(const FString& FuncName, const FJsonObjectWrapper& Args);

	void FriendsAction(const FString& Action, const FString& Query);
	void Lobby(const FString& Action, const FString& Uid = FString());
	void Request(const FString& Path, const FString& Verb, TFunction<void(const FVedgHttpResult&)> OnDone);
	void Finish(const FString& Action, const FVedgHttpResult& Result);
	FString LocalUid() const;
	TArray<FVedgSocialEntry> ToEntries(const TArray<TSharedPtr<FJsonObject>>& Items) const;

	UPROPERTY()
	TObjectPtr<UVedgMatchmaking> Matchmaking;
};
