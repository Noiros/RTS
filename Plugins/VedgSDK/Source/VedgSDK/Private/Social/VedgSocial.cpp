// Fill out your copyright notice in the Description page of Project Settings.

#include "Social/VedgSocial.h"
#include "Http/VedgHttpRequest.h"
#include "Matchmaking/VedgMatchmaking.h"
#include "UI/VedgText.h"
#include "VedgSDKSettings.h"
#include "VedgSDKSubsystem.h"
#include "GenericPlatform/GenericPlatformHttp.h"

namespace
{
	FString Encode(const FString& Value)
	{
		return FGenericPlatformHttp::UrlEncode(Value);
	}

	FString CtxQuery()
	{
		const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();
		return TEXT("game=") + Encode(Settings->GetGameSlug()) + TEXT("&env=") + Encode(Settings->ResolveGameEnv());
	}

	FString StringOf(const TSharedPtr<FJsonObject>& Object, const FString& Field)
	{
		FString Value;
		return Object.IsValid() && Object->TryGetStringField(Field, Value) ? Value : FString();
	}

	bool BoolOf(const TSharedPtr<FJsonObject>& Object, const FString& Field)
	{
		bool bValue = false;
		return Object.IsValid() && Object->TryGetBoolField(Field, bValue) && bValue;
	}

	TArray<FString> StringsOf(const TSharedRef<FJsonObject>& Object, const FString& Field)
	{
		TArray<FString> Out;
		Object->TryGetStringArrayField(Field, Out);
		return Out;
	}

	TArray<TSharedPtr<FJsonObject>> ObjectsOf(const TSharedPtr<FJsonObject>& Object, const FString& Field)
	{
		TArray<TSharedPtr<FJsonObject>> Out;
		const TArray<TSharedPtr<FJsonValue>>* Items;
		if (Object.IsValid() && Object->TryGetArrayField(Field, Items))
		{
			for (const TSharedPtr<FJsonValue>& Item : *Items)
			{
				const TSharedPtr<FJsonObject>* Entry;
				if (Item.IsValid() && Item->TryGetObject(Entry))
				{
					Out.Add(*Entry);
				}
			}
		}
		return Out;
	}

	void RemoveUid(TArray<TSharedPtr<FJsonObject>>& Items, const FString& Uid)
	{
		Items.RemoveAll([&Uid](const TSharedPtr<FJsonObject>& Item) { return StringOf(Item, TEXT("uid")) == Uid; });
	}

	// Same as Godot's String.to_snake_case() for the API's error codes (camelCase or spaced).
	FString ToSnakeCase(const FString& In)
	{
		FString Out;
		for (int32 i = 0; i < In.Len(); ++i)
		{
			const TCHAR C = In[i];
			if (C == TEXT(' ') || C == TEXT('-'))
			{
				Out.AppendChar(TEXT('_'));
				continue;
			}
			if (FChar::IsUpper(C) && i > 0 && FChar::IsLower(In[i - 1]))
			{
				Out.AppendChar(TEXT('_'));
			}
			Out.AppendChar(FChar::ToLower(C));
		}
		return Out;
	}
}

void UVedgSocial::StartUp(UVedgMatchmaking* InMatchmaking)
{
	Matchmaking = InMatchmaking;
	Matchmaking->OnConnected.AddDynamic(this, &UVedgSocial::HandleConnected);
	Matchmaking->OnPacketReceived.AddDynamic(this, &UVedgSocial::HandlePacket);
	if (Matchmaking->IsOpen())
	{
		Refresh();
	}
}

FString UVedgSocial::LocalUid() const
{
	const UVedgSDKSubsystem* Sdk = Cast<UVedgSDKSubsystem>(GetOuter());
	return Sdk ? Sdk->GetUid() : FString();
}

void UVedgSocial::HandleConnected()
{
	Refresh();
}

void UVedgSocial::Refresh()
{
	RefreshFriends();
	RefreshGroup();
}

void UVedgSocial::RefreshFriends()
{
	TWeakObjectPtr<UVedgSocial> WeakThis(this);
	Request(TEXT("/friends/list"), TEXT("GET"), [WeakThis](const FVedgHttpResult& Result)
	{
		UVedgSocial* Self = WeakThis.Get();
		if (!Self || !Result.bValid || !Result.ResponseObject.IsValid())
		{
			return;
		}
		const TArray<TSharedPtr<FJsonObject>> Items = ObjectsOf(Result.ResponseObject, TEXT("items"));
		Self->Friends = Items.FilterByPredicate([](const TSharedPtr<FJsonObject>& F) { return StringOf(F, TEXT("type")) == TEXT("friend"); });
		Self->FriendRequests = Items.FilterByPredicate([](const TSharedPtr<FJsonObject>& F) { return StringOf(F, TEXT("type")) == TEXT("pending"); });
		Self->SentRequests = Items.FilterByPredicate([](const TSharedPtr<FJsonObject>& F) { return StringOf(F, TEXT("type")) == TEXT("requested"); });
		Self->OnFriendsUpdated.Broadcast();
	});
}

void UVedgSocial::RefreshGroup()
{
	TWeakObjectPtr<UVedgSocial> WeakThis(this);
	Request(TEXT("/lobbies/group?") + CtxQuery(), TEXT("GET"), [WeakThis](const FVedgHttpResult& Result)
	{
		UVedgSocial* Self = WeakThis.Get();
		if (!Self || !Result.bValid || !Result.ResponseObject.IsValid())
		{
			return;
		}
		const TSharedPtr<FJsonObject>* Current;
		Self->Group = Result.ResponseObject->TryGetObjectField(TEXT("group"), Current) && Current->IsValid() ? Current->ToSharedRef() : MakeShared<FJsonObject>();
		Self->GroupInvites = ObjectsOf(Result.ResponseObject, TEXT("invites"));
		Self->OnGroupUpdated.Broadcast();
	});
}

void UVedgSocial::RequestFriend(const FString& Pseudo)
{
	FriendsAction(TEXT("request_friend"), TEXT("pseudo=") + Encode(Pseudo));
}

void UVedgSocial::AcceptFriend(const FString& Uid)
{
	FriendsAction(TEXT("accept_friend_request"), TEXT("req_uid=") + Encode(Uid));
}

void UVedgSocial::DeclineFriend(const FString& Uid)
{
	FriendsAction(TEXT("decline"), TEXT("uid=") + Encode(Uid));
}

void UVedgSocial::RemoveFriend(const FString& Uid)
{
	FriendsAction(TEXT("remove"), TEXT("uid=") + Encode(Uid));
}

void UVedgSocial::InviteToGroup(const FString& Uid)
{
	Lobby(TEXT("send_invite"), Uid);
}

void UVedgSocial::AcceptGroupInvite(const FString& Uid)
{
	RemoveUid(GroupInvites, Uid);
	Lobby(TEXT("accept_invite"), Uid);
}

void UVedgSocial::DeclineGroupInvite(const FString& Uid)
{
	RemoveUid(GroupInvites, Uid);
	Lobby(TEXT("decline_invite"), Uid);
}

void UVedgSocial::Kick(const FString& Uid)
{
	Lobby(TEXT("kick_member"), Uid);
}

void UVedgSocial::JoinFriend(const FString& Uid)
{
	Lobby(TEXT("join_friend"), Uid);
}

void UVedgSocial::TransferOwner(const FString& Uid)
{
	Lobby(TEXT("transfer_owner"), Uid);
}

void UVedgSocial::LeaveGroup()
{
	Lobby(TEXT("leave_group"));
}

FJsonObjectWrapper UVedgSocial::GetGroup() const
{
	FJsonObjectWrapper Wrapper;
	Wrapper.JsonObject = Group;
	return Wrapper;
}

TArray<FString> UVedgSocial::GetMembers() const
{
	return StringsOf(Group, TEXT("members"));
}

FString UVedgSocial::GetOwner() const
{
	return StringOf(Group, TEXT("owner"));
}

bool UVedgSocial::IsGroupOwner() const
{
	return IsGrouped() && GetOwner() == LocalUid();
}

bool UVedgSocial::IsReady(const FString& Uid) const
{
	return StringsOf(Group, TEXT("ready_players")).Contains(Uid.IsEmpty() ? LocalUid() : Uid);
}

TSharedPtr<FJsonObject> UVedgSocial::GetFriend(const FString& Uid) const
{
	for (const TArray<TSharedPtr<FJsonObject>>* List : { &Friends, &FriendRequests, &SentRequests })
	{
		for (const TSharedPtr<FJsonObject>& Entry : *List)
		{
			if (StringOf(Entry, TEXT("uid")) == Uid)
			{
				return Entry;
			}
		}
	}
	return nullptr;
}

FString UVedgSocial::GetPseudo(const FString& Uid) const
{
	const TSharedPtr<FJsonObject>* Pseudos;
	FString Pseudo;
	if (Group->TryGetObjectField(TEXT("pseudos"), Pseudos) && (*Pseudos)->TryGetStringField(Uid, Pseudo))
	{
		return Pseudo;
	}
	Pseudo = StringOf(GetFriend(Uid), TEXT("pseudo"));
	if (!Pseudo.IsEmpty())
	{
		return Pseudo;
	}
	for (const TSharedPtr<FJsonObject>& Invite : GroupInvites)
	{
		if (StringOf(Invite, TEXT("uid")) == Uid)
		{
			return StringOf(Invite, TEXT("pseudo"));
		}
	}
	return FString();
}

FText UVedgSocial::ErrorText(const FVedgHttpResult& Result)
{
	if (Result.bValid)
	{
		return FText::GetEmpty();
	}
	const FString Error = StringOf(Result.ResponseObject, TEXT("error"));
	return FText::FromString(FVedgText::GetString(TEXT("vedg_social_error_") + ToSnakeCase(Error), FVedgText::GetString(TEXT("vedg_social_error_generic"), TEXT("Error"))));
}

void UVedgSocial::HandlePacket(const FString& FuncName, const FJsonObjectWrapper& Args)
{
	const TSharedPtr<FJsonObject> Data = Args.JsonObject.IsValid() ? Args.JsonObject : MakeShared<FJsonObject>();
	const FString Uid = StringOf(Data, TEXT("uid"));
	const FString Pseudo = StringOf(Data, TEXT("pseudo"));

	if (FuncName == TEXT("friend_request_received"))
	{
		RefreshFriends();
		OnFriendRequestReceived.Broadcast(Uid, Pseudo);
	}
	else if (FuncName == TEXT("friend_updated"))
	{
		RefreshFriends();
	}
	else if (FuncName == TEXT("update_group"))
	{
		Group = Data.ToSharedRef();
		OnGroupUpdated.Broadcast();
	}
	else if (FuncName == TEXT("removed_from_group"))
	{
		Group = MakeShared<FJsonObject>();
		OnGroupLeft.Broadcast();
		OnGroupUpdated.Broadcast();
	}
	else if (FuncName == TEXT("recieved_group_invite"))
	{
		RemoveUid(GroupInvites, Uid);
		const TSharedRef<FJsonObject> Invite = MakeShared<FJsonObject>();
		Invite->SetStringField(TEXT("uid"), Uid);
		Invite->SetStringField(TEXT("pseudo"), Pseudo);
		GroupInvites.Add(Invite);
		OnGroupInviteReceived.Broadcast(Uid, Pseudo);
	}
	else if (FuncName == TEXT("group_invite_declined"))
	{
		OnGroupInviteDeclined.Broadcast(Uid);
	}
}

void UVedgSocial::FriendsAction(const FString& Action, const FString& Query)
{
	TWeakObjectPtr<UVedgSocial> WeakThis(this);
	Request(TEXT("/friends/") + Action + TEXT("?") + Query, TEXT("POST"), [WeakThis, Action](const FVedgHttpResult& Result)
	{
		if (UVedgSocial* Self = WeakThis.Get())
		{
			Self->RefreshFriends();
			Self->Finish(Action, Result);
		}
	});
}

void UVedgSocial::Lobby(const FString& Action, const FString& Uid)
{
	FString Path = TEXT("/lobbies/") + Action + TEXT("?") + CtxQuery();
	if (!Uid.IsEmpty())
	{
		Path += TEXT("&uid=") + Encode(Uid);
	}
	TWeakObjectPtr<UVedgSocial> WeakThis(this);
	Request(Path, TEXT("POST"), [WeakThis, Action](const FVedgHttpResult& Result)
	{
		if (UVedgSocial* Self = WeakThis.Get())
		{
			Self->Finish(Action, Result);
		}
	});
}

void UVedgSocial::Finish(const FString& Action, const FVedgHttpResult& Result)
{
	OnActionFinished.Broadcast(Action, Result.bValid, ErrorText(Result));
}

void UVedgSocial::Request(const FString& Path, const FString& Verb, TFunction<void(const FVedgHttpResult&)> OnDone)
{
	FVedgHttpRequest::Request(UVedgSDKSettings::Get()->GetApiUrl() + Path, Verb, FString(), [OnDone](FVedgHttpResult Result) { OnDone(Result); }, 10.f, Matchmaking ? Matchmaking->GetToken() : FString());
}

TArray<FVedgSocialEntry> UVedgSocial::ToEntries(const TArray<TSharedPtr<FJsonObject>>& Items) const
{
	TArray<FVedgSocialEntry> Out;
	for (const TSharedPtr<FJsonObject>& Item : Items)
	{
		FVedgSocialEntry& Entry = Out.AddDefaulted_GetRef();
		Entry.Uid = StringOf(Item, TEXT("uid"));
		Entry.Pseudo = GetPseudo(Entry.Uid);
		Entry.bOnline = BoolOf(Item, TEXT("online"));
		Entry.Game = StringOf(Item, TEXT("game"));
	}
	return Out;
}
