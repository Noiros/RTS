// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/VedgLoginPanel.h"
#include "Auth/VedgLogin.h"
#include "UI/VedgText.h"
#include "VedgSDKModule.h"
#include "VedgSDKSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor PanelColor(0.012f, 0.012f, 0.016f, 0.97f);
	const FLinearColor StatusColor(1.f, 0.55f, 0.47f);

	TSharedRef<SWidget> Title(const TAttribute<FText>& Text)
	{
		return SNew(STextBlock).Text(Text).Font(FCoreStyle::GetDefaultFontStyle("Bold", 20)).Justification(ETextJustify::Center);
	}
}

UVedgSDKSubsystem* UVedgLoginPanel::Sdk() const
{
	return UVedgSDKSubsystem::Get(this);
}

// ---- Slate layout (login.tscn) ----

TSharedRef<SWidget> UVedgLoginPanel::MakeField(TSharedPtr<SEditableTextBox>& OutBox, const FString& HintKey, bool bPassword)
{
	return SAssignNew(OutBox, SEditableTextBox).HintText(FVedgText::Get(HintKey)).IsPassword(bPassword);
}

TSharedRef<SWidget> UVedgLoginPanel::MakeButton(const TAttribute<FText>& Label, TFunction<void()> OnClick)
{
	return SNew(SButton)
		.HAlign(HAlign_Center)
		.IsEnabled_Lambda([this]() { return !bBusy; })
		.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
		[
			SNew(STextBlock).Text(Label)
		];
}

TSharedRef<SWidget> UVedgLoginPanel::RebuildWidget()
{
	const auto VisibleIn = [this](EVedgLoginView InView)
	{
		return TAttribute<EVisibility>::CreateLambda([this, InView]() { return View == InView ? EVisibility::Visible : EVisibility::Collapsed; });
	};
	const auto StatusText = [this]()
	{
		return SNew(STextBlock).Text_Lambda([this]() { return Status; }).AutoWrapText(true).Justification(ETextJustify::Center).ColorAndOpacity(StatusColor);
	};
	const FMargin Gap(0.f, 4.f);

	Root = SNew(SBox)
		.Visibility_Lambda([this]() { return View == EVedgLoginView::Hidden ? EVisibility::Collapsed : EVisibility::Visible; })
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(420.f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(PanelColor)
				.Padding(24.f)
				[
					SNew(SVerticalBox)

					// login
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SVerticalBox)
						.Visibility(VisibleIn(EVedgLoginView::Login))
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[Title(FVedgText::Get(TEXT("vedg_auth_login_label")))]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[MakeField(LoginEmail, TEXT("vedg_auth_email"), false)]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[MakeField(LoginPassword, TEXT("vedg_auth_pwd"), true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[StatusText()]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[MakeButton(FVedgText::Get(TEXT("vedg_auth_login_btn")), [this]() { SubmitLogin(); })]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[MakeButton(FVedgText::Get(TEXT("vedg_auth_switch_register_btn")), [this]() { SwitchTo(EVedgLoginView::Register); })]
					]

					// register
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SVerticalBox)
						.Visibility(VisibleIn(EVedgLoginView::Register))
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[Title(FVedgText::Get(TEXT("vedg_auth_register_label")))]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[MakeField(RegisterPseudo, TEXT("vedg_auth_pseudo"), false)]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[MakeField(RegisterEmail, TEXT("vedg_auth_email"), false)]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[MakeField(RegisterPassword, TEXT("vedg_auth_pwd"), true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[MakeField(RegisterConfirm, TEXT("vedg_auth_pwd_conf"), true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[StatusText()]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[MakeButton(FVedgText::Get(TEXT("vedg_auth_register_btn")), [this]() { SubmitRegister(); })]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[MakeButton(FVedgText::Get(TEXT("vedg_auth_switch_login_btn")), [this]() { SwitchTo(EVedgLoginView::Login); })]
					]

					// steam
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SVerticalBox)
						.Visibility(VisibleIn(EVedgLoginView::Steam))
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[Title(TAttribute<FText>::CreateLambda([this]() { return SteamTitle; }))]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[SNew(STextBlock).Text_Lambda([this]() { return SteamBody; }).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(Gap)[MakeButton(TAttribute<FText>::CreateLambda([this]() { return SteamButton; }), [this]()
						{
							bBusy = true;
							if (OnSteamPrimary)
							{
								TFunction<void()> Primary = MoveTemp(OnSteamPrimary);
								OnSteamPrimary = nullptr;
								Primary();
							}
						})]
					]
				]
			]
		];
	return Root.ToSharedRef();
}

void UVedgLoginPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
	LoginEmail.Reset();
	LoginPassword.Reset();
	RegisterPseudo.Reset();
	RegisterEmail.Reset();
	RegisterPassword.Reset();
	RegisterConfirm.Reset();
}

// ---- login_firebase.gd ----

void UVedgLoginPanel::LoginWithVedg()
{
	FString User, Password;
	if (!FVedgLogin::GetCliLogin(User, Password))
	{
		ResumeOrShowForm();
		return;
	}

	TWeakObjectPtr<UVedgLoginPanel> WeakThis(this);
	FVedgLogin::Authenticate(Sdk(), User, Password, [WeakThis, User](const FVedgLoginResult& Result)
	{
		UVedgLoginPanel* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (Result.bSuccess)
		{
			FVedgLogin::SaveCredentials(User, Result.Refresh, Result.Pseudo);
			Self->Finish(Result);
			return;
		}
		UE_LOG(LogVedgSDK, Error, TEXT("CLI login failed (%s)"), *Result.Error);
		Self->ResumeOrShowForm();
	});
}

void UVedgLoginPanel::ResumeOrShowForm()
{
	const TSharedPtr<FJsonObject> Saved = FVedgLogin::GetSavedLogin();
	if (!Saved.IsValid())
	{
		ShowForm(FString(), FText::GetEmpty());
		return;
	}

	FString SavedUser;
	Saved->TryGetStringField(TEXT("user"), SavedUser);
	TWeakObjectPtr<UVedgLoginPanel> WeakThis(this);
	FVedgLogin::Resume(Sdk(), Saved.ToSharedRef(), [WeakThis, SavedUser](const FVedgLoginResult& Result)
	{
		UVedgLoginPanel* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (Result.bSuccess)
		{
			Self->Finish(Result);
		}
		else if (Result.Error == TEXT("banned"))
		{
			Self->Finish(FVedgLoginResult::Fail(TEXT("account_banned"), Result.Reason, Result.Until));
		}
		else
		{
			Self->ShowForm(SavedUser, FVedgText::Get(Result.Error));
		}
	});
}

void UVedgLoginPanel::ShowForm(const FString& Email, const FText& InStatus)
{
	TakeWidget();
	SwitchTo(EVedgLoginView::Login);
	bBusy = false;
	LoginEmail->SetText(FText::FromString(Email));
	Status = InStatus;
}

void UVedgLoginPanel::SwitchTo(EVedgLoginView InView)
{
	View = InView;
	Status = FText::GetEmpty();
}

void UVedgLoginPanel::SubmitLogin()
{
	const FString Email = LoginEmail->GetText().ToString();
	bBusy = true;
	Status = FVedgText::Get(TEXT("vedg_auth_connecting"));

	TWeakObjectPtr<UVedgLoginPanel> WeakThis(this);
	FVedgLogin::Authenticate(Sdk(), Email, LoginPassword->GetText().ToString(), [WeakThis, Email](const FVedgLoginResult& Result)
	{
		UVedgLoginPanel* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (Result.bSuccess)
		{
			FVedgLogin::SaveCredentials(Email, Result.Refresh, Result.Pseudo);
			Self->Finish(Result);
		}
		else if (Result.Error == TEXT("banned"))
		{
			Self->Finish(FVedgLoginResult::Fail(TEXT("account_banned"), Result.Reason, Result.Until));
		}
		else
		{
			Self->bBusy = false;
			Self->Status = FVedgText::Get(Result.Error);
		}
	});
}

void UVedgLoginPanel::SubmitRegister()
{
	if (!RegisterPassword->GetText().EqualTo(RegisterConfirm->GetText()))
	{
		Status = FVedgText::Get(TEXT("vedg_auth_pwd_mismatch"));
		return;
	}
	bBusy = true;
	Status = FVedgText::Get(TEXT("vedg_auth_connecting"));

	TWeakObjectPtr<UVedgLoginPanel> WeakThis(this);
	FVedgLogin::Register(Sdk(), RegisterEmail->GetText().ToString(), RegisterPassword->GetText().ToString(), RegisterPseudo->GetText().ToString(), [WeakThis](const FVedgLoginResult& Result)
	{
		UVedgLoginPanel* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (Result.bSuccess)
		{
			Self->Finish(Result);
			return;
		}
		Self->bBusy = false;
		Self->Status = FVedgText::Get(Result.Error);
	});
}

// ---- login_steam.gd ----

void UVedgLoginPanel::LoginWithSteam(const FString& PersonaName, const FString& SteamId)
{
	SteamPersona = PersonaName;
	SteamFallbackId = SteamId;
	OnSteamTicketRequested.Broadcast();
}

void UVedgLoginPanel::SubmitSteamTicket(const FString& TicketHex)
{
	if (TicketHex.IsEmpty())
	{
		SteamRetry(FVedgText::Get(TEXT("steam_error_body")));
		return;
	}
	TWeakObjectPtr<UVedgLoginPanel> WeakThis(this);
	FVedgLogin::CheckSteamStatus(Sdk(), TicketHex, [WeakThis, TicketHex](const FVedgSteamStatus& SteamStatus)
	{
		if (UVedgLoginPanel* Self = WeakThis.Get())
		{
			Self->HandleSteamStatus(TicketHex, SteamStatus, false);
		}
	});
}

void UVedgLoginPanel::HandleSteamStatus(const FString& TicketHex, const FVedgSteamStatus& SteamStatus, bool bIsNew)
{
	TWeakObjectPtr<UVedgLoginPanel> WeakThis(this);

	if (!bIsNew && !SteamStatus.bExists && !SteamStatus.bUnreachable)
	{
		const FText Body = FText::Format(FVedgText::Get(TEXT("steam_new_account_body")), FText::FromString(SteamPersona));
		SteamPrompt(TEXT("steam_new_account_title"), Body, TEXT("steam_new_account_confirm"), [WeakThis, TicketHex]()
		{
			UVedgLoginPanel* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			Self->View = EVedgLoginView::Hidden;
			FVedgLogin::RegisterSteam(TicketHex, [WeakThis, TicketHex](const FVedgLoginResult& Registered)
			{
				UVedgLoginPanel* Panel = WeakThis.Get();
				if (!Panel)
				{
					return;
				}
				if (!Registered.bSuccess)
				{
					Panel->SteamRetry(FVedgText::Get(Registered.Error));
					return;
				}
				FVedgLogin::CheckSteamStatus(Panel->Sdk(), TicketHex, [WeakThis, TicketHex](const FVedgSteamStatus& After)
				{
					if (UVedgLoginPanel* Again = WeakThis.Get())
					{
						Again->HandleSteamStatus(TicketHex, After, true);
					}
				});
			});
		});
		return;
	}

	if (SteamStatus.bBanned)
	{
		Finish(FVedgLoginResult::Fail(SteamStatus.Error == TEXT("banned") ? TEXT("account_banned") : SteamStatus.Error, SteamStatus.Reason, SteamStatus.Until));
		return;
	}

	if (SteamStatus.bUnreachable || !SteamStatus.bExists)
	{
		UE_LOG(LogVedgSDK, Error, TEXT("Cant check account status (%d)"), SteamStatus.Code);
		SteamRetry(FVedgText::Get(TEXT("steam_error_body")));
		return;
	}

	FVedgLoginResult Result;
	Result.bSuccess = true;
	Result.Token = SteamStatus.Token;
	Result.Uid = !SteamStatus.Uid.IsEmpty() ? SteamStatus.Uid : SteamFallbackId;
	Result.bIsNewAccount = bIsNew;
	Finish(Result);
}

void UVedgLoginPanel::SteamRetry(const FText& Body)
{
	TWeakObjectPtr<UVedgLoginPanel> WeakThis(this);
	SteamPrompt(TEXT("steam_error_title"), Body, TEXT("steam_error_retry"), [WeakThis]()
	{
		if (UVedgLoginPanel* Self = WeakThis.Get())
		{
			Self->View = EVedgLoginView::Hidden;
			Self->OnSteamTicketRequested.Broadcast();
		}
	});
}

void UVedgLoginPanel::SteamPrompt(const FString& TitleKey, const FText& Body, const FString& ButtonKey, TFunction<void()> OnPrimary)
{
	SteamTitle = FVedgText::Get(TitleKey);
	SteamBody = Body;
	SteamButton = FVedgText::Get(ButtonKey);
	OnSteamPrimary = MoveTemp(OnPrimary);
	bBusy = false;
	View = EVedgLoginView::Steam;
}

// ---- login.gd ----

void UVedgLoginPanel::Finish(const FVedgLoginResult& Result)
{
	View = EVedgLoginView::Hidden;
	bBusy = false;
	if (UVedgSDKSubsystem* Subsystem = Sdk())
	{
		Subsystem->CompleteLogin(Result);
	}
	OnLoginFinished.Broadcast(Result);
}
