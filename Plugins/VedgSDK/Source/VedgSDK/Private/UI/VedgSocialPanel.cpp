// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/VedgSocialPanel.h"
#include "Social/VedgSocial.h"
#include "UI/VedgText.h"
#include "VedgSDKSettings.h"
#include "VedgSDKSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor PanelColor(0.012f, 0.012f, 0.016f, 0.97f);
	const FLinearColor OnlineColor(0.04f, 1.f, 0.f);
	const FLinearColor OfflineColor(0.05f, 0.05f, 0.05f);
	const FLinearColor StatusColor(1.f, 0.55f, 0.47f);

	FString StringOf(const TSharedPtr<FJsonObject>& Object, const FString& Field)
	{
		FString Value;
		return Object.IsValid() && Object->TryGetStringField(Field, Value) ? Value : FString();
	}

	bool IsOnlineEntry(const TSharedPtr<FJsonObject>& Entry)
	{
		bool bOnline = false;
		return Entry.IsValid() && Entry->TryGetBoolField(TEXT("online"), bOnline) && bOnline;
	}
}

TSharedRef<SWidget> UVedgSocialPanel::RebuildWidget()
{
	if (!IsDesignTime())
	{
		const UVedgSDKSubsystem* Sdk = UVedgSDKSubsystem::Get(this);
		Social = Sdk ? Sdk->GetSocial() : nullptr;
	}

	const auto Section = [](const FString& TitleKey, TSharedPtr<SVerticalBox>& OutList, TAttribute<EVisibility> SectionVisibility, TSharedPtr<SWidget> Footer = nullptr)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox).Visibility(SectionVisibility)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)
			[
				SNew(STextBlock).Text(FVedgText::Get(TitleKey)).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
			+ SVerticalBox::Slot().AutoHeight()[SAssignNew(OutList, SVerticalBox)];
		if (Footer.IsValid())
		{
			Box->AddSlot().AutoHeight().Padding(0.f, 4.f)[Footer.ToSharedRef()];
		}
		return Box;
	};
	const auto VisibleIf = [this](TFunction<bool(const UVedgSocial&)> Predicate)
	{
		return TAttribute<EVisibility>::CreateLambda([this, Predicate]() { return Social && Predicate(*Social) ? EVisibility::Visible : EVisibility::Collapsed; });
	};

	const TSharedRef<SWidget> LeaveButton = SNew(SButton)
		.HAlign(HAlign_Center)
		.OnClicked_Lambda([this]() { if (Social) { Social->LeaveGroup(); } return FReply::Handled(); })
		[
			SNew(STextBlock).Text(FVedgText::Get(TEXT("vedg_social_leave")))
		];

	const TSharedRef<SWidget> Content = SNew(SBox)
		.WidthOverride(420.f)
		[
			SNew(SBorder)
			.BorderBackgroundColor(PanelColor)
			.Padding(24.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SAssignNew(PseudoBox, SEditableTextBox).HintText(FVedgText::Get(TEXT("vedg_social_add_placeholder")))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { AddFriendPressed(); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FVedgText::Get(TEXT("vedg_social_add_btn")))
						]
					]
				]
				+ SScrollBox::Slot().Padding(0.f, 4.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return Status; })
					.Visibility_Lambda([this]() { return Status.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
					.AutoWrapText(true)
					.ColorAndOpacity(StatusColor)
				]
				+ SScrollBox::Slot()[Section(TEXT("vedg_social_group"), GroupList, VisibleIf([](const UVedgSocial& S) { return S.IsGrouped(); }), LeaveButton)]
				+ SScrollBox::Slot()[Section(TEXT("vedg_social_group_invites"), InvitesList, VisibleIf([](const UVedgSocial& S) { return S.GroupInvites.Num() > 0; }))]
				+ SScrollBox::Slot()[Section(TEXT("vedg_social_requests"), RequestsList, VisibleIf([](const UVedgSocial& S) { return S.FriendRequests.Num() > 0; }))]
				+ SScrollBox::Slot()[Section(TEXT("vedg_social_friends"), FriendsList, EVisibility::Visible)]
				+ SScrollBox::Slot()[Section(TEXT("vedg_social_sent"), SentList, VisibleIf([](const UVedgSocial& S) { return S.SentRequests.Num() > 0; }))]
			]
		];

	if (Social)
	{
		Social->OnFriendsUpdated.AddUniqueDynamic(this, &UVedgSocialPanel::Redraw);
		Social->OnGroupUpdated.AddUniqueDynamic(this, &UVedgSocialPanel::Redraw);
		Social->OnGroupInviteReceived.AddUniqueDynamic(this, &UVedgSocialPanel::HandleInvite);
		Social->OnActionFinished.AddUniqueDynamic(this, &UVedgSocialPanel::HandleAction);
		Social->Refresh();
		Redraw();
	}
	return Content;
}

void UVedgSocialPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	PseudoBox.Reset();
	GroupList.Reset();
	InvitesList.Reset();
	RequestsList.Reset();
	FriendsList.Reset();
	SentList.Reset();
}

void UVedgSocialPanel::HandleInvite(const FString&, const FString&)
{
	Redraw();
}

void UVedgSocialPanel::HandleAction(const FString& Action, bool bSuccess, const FText& ErrorText)
{
	Status = ErrorText;
	if (Action == TEXT("request_friend") && bAddPending)
	{
		bAddPending = false;
		if (bSuccess)
		{
			PseudoBox->SetText(FText::GetEmpty());
			Status = FVedgText::Get(TEXT("vedg_social_request_sent"));
		}
	}
}

void UVedgSocialPanel::AddFriendPressed()
{
	const FString Pseudo = PseudoBox->GetText().ToString().TrimStartAndEnd();
	if (Pseudo.IsEmpty() || !Social)
	{
		return;
	}
	bAddPending = true;
	Social->RequestFriend(Pseudo);
}

void UVedgSocialPanel::Redraw()
{
	if (!Social || !FriendsList.IsValid())
	{
		return;
	}
	UVedgSocial* S = Social;
	const FString LocalUid = UVedgSDKSubsystem::Get(this) ? UVedgSDKSubsystem::Get(this)->GetUid() : FString();
	const TArray<FString> Members = S->GetMembers();

	GroupList->ClearChildren();
	for (const FString& Uid : Members)
	{
		TArray<TPair<FString, TFunction<void()>>> Actions;
		if (S->IsGroupOwner() && Uid != LocalUid)
		{
			Actions.Emplace(TEXT("vedg_social_make_owner"), [S, Uid]() { S->TransferOwner(Uid); });
			Actions.Emplace(TEXT("vedg_social_kick"), [S, Uid]() { S->Kick(Uid); });
		}
		TArray<FString> Tags;
		if (Uid == S->GetOwner())
		{
			Tags.Add(FVedgText::GetString(TEXT("vedg_social_owner"), TEXT("Owner")));
		}
		if (S->IsReady(Uid))
		{
			Tags.Add(FVedgText::GetString(TEXT("vedg_social_ready"), TEXT("Ready")));
		}
		AddRow(GroupList, S->GetPseudo(Uid), FString::Join(Tags, TEXT(", ")), Uid == LocalUid || IsOnlineEntry(S->GetFriend(Uid)), Actions);
	}

	InvitesList->ClearChildren();
	for (const TSharedPtr<FJsonObject>& Invite : S->GroupInvites)
	{
		const FString Uid = StringOf(Invite, TEXT("uid"));
		TArray<TPair<FString, TFunction<void()>>> Actions;
		Actions.Emplace(TEXT("vedg_social_accept"), [S, Uid]() { S->AcceptGroupInvite(Uid); });
		Actions.Emplace(TEXT("vedg_social_decline"), [S, Uid]() { S->DeclineGroupInvite(Uid); });
		AddRow(InvitesList, S->GetPseudo(Uid), FString(), true, Actions);
	}

	RequestsList->ClearChildren();
	for (const TSharedPtr<FJsonObject>& Entry : S->FriendRequests)
	{
		const FString Uid = StringOf(Entry, TEXT("uid"));
		TArray<TPair<FString, TFunction<void()>>> Actions;
		Actions.Emplace(TEXT("vedg_social_accept"), [S, Uid]() { S->AcceptFriend(Uid); });
		Actions.Emplace(TEXT("vedg_social_decline"), [S, Uid]() { S->DeclineFriend(Uid); });
		AddRow(RequestsList, S->GetPseudo(Uid), FString(), false, Actions);
	}

	FriendsList->ClearChildren();
	TArray<TSharedPtr<FJsonObject>> Friends = S->Friends;
	Friends.StableSort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B) { return IsOnlineEntry(A) && !IsOnlineEntry(B); });
	const FString GameSlug = UVedgSDKSettings::Get()->GetGameSlug();
	for (const TSharedPtr<FJsonObject>& Entry : Friends)
	{
		const FString Uid = StringOf(Entry, TEXT("uid"));
		TArray<TPair<FString, TFunction<void()>>> Actions;
		if (IsOnlineEntry(Entry) && StringOf(Entry, TEXT("game")) == GameSlug && !Members.Contains(Uid))
		{
			Actions.Emplace(TEXT("vedg_social_invite"), [S, Uid]() { S->InviteToGroup(Uid); });
		}
		Actions.Emplace(TEXT("vedg_social_remove"), [S, Uid]() { S->RemoveFriend(Uid); });
		AddRow(FriendsList, S->GetPseudo(Uid), PresenceText(Entry), IsOnlineEntry(Entry), Actions);
	}

	SentList->ClearChildren();
	for (const TSharedPtr<FJsonObject>& Entry : S->SentRequests)
	{
		AddRow(SentList, S->GetPseudo(StringOf(Entry, TEXT("uid"))), FString(), false, TArray<TPair<FString, TFunction<void()>>>());
	}
}

void UVedgSocialPanel::AddRow(const TSharedPtr<SVerticalBox>& List, const FString& Pseudo, const FString& Detail, bool bOnline, const TArray<TPair<FString, TFunction<void()>>>& Actions)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SBox).WidthOverride(10.f).HeightOverride(10.f)
			[
				SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(bOnline ? OnlineColor : OfflineColor)
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Detail.IsEmpty() ? Pseudo : FString::Printf(TEXT("%s  ·  %s"), *Pseudo, *Detail)))
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		];

	for (const TPair<FString, TFunction<void()>>& Action : Actions)
	{
		TFunction<void()> OnClick = Action.Value;
		Row->AddSlot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(FVedgText::Get(Action.Key))
			]
		];
	}

	List->AddSlot().AutoHeight().Padding(0.f, 2.f)[Row];
}

FString UVedgSocialPanel::PresenceText(const TSharedPtr<FJsonObject>& Entry) const
{
	if (!IsOnlineEntry(Entry))
	{
		return FVedgText::GetString(TEXT("vedg_social_offline"), TEXT("Offline"));
	}
	const FString Game = StringOf(Entry, TEXT("game"));
	if (!Game.IsEmpty() && Game != UVedgSDKSettings::Get()->GetGameSlug())
	{
		return FVedgText::GetString(TEXT("vedg_social_online_in"), TEXT("{game}")).Replace(TEXT("{game}"), *Game);
	}
	return FVedgText::GetString(TEXT("vedg_social_online"), TEXT("Online"));
}
