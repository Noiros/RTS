// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "VedgSDKTypes.h"
#include "VedgLoginPanel.generated.h"

class SEditableTextBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVedgOnLoginFinished, const FVedgLoginResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVedgOnSteamTicketRequested);

enum class EVedgLoginView : uint8 { Hidden, Login, Register, Steam };

// Login panel template, mirrors login.tscn + login.gd and its providers' UI
// loops. Drop it in any UMG widget (palette: Vedg) or add TakeWidget() to the
// viewport, then call LoginWithVedg() / LoginWithSteam() and bind
// OnLoginFinished. Session wiring (CompleteLogin) is done before it fires.
UCLASS(meta = (DisplayName = "Vedg Login Panel"))
class VEDGSDK_API UVedgLoginPanel : public UWidget
{
	GENERATED_BODY()

public:
	// Saved credentials / -login CLI first, then the email form.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Login")
	void LoginWithVedg();

	// Fires OnSteamTicketRequested: answer with SubmitSteamTicket(). PersonaName
	// fills the new-account prompt; SteamId is the uid fallback.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Login")
	void LoginWithSteam(const FString& PersonaName, const FString& SteamId);

	// Hex Steam Web API ticket from the game's Steamworks binding ("" = failure, shows retry).
	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Login")
	void SubmitSteamTicket(const FString& TicketHex);

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Login")
	FVedgOnLoginFinished OnLoginFinished;

	UPROPERTY(BlueprintAssignable, Category = "VedgSDK|Login")
	FVedgOnSteamTicketRequested OnSteamTicketRequested;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return NSLOCTEXT("VedgSDK", "Palette", "Vedg"); }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	void ResumeOrShowForm();
	void ShowForm(const FString& Email, const FText& InStatus);
	void SubmitLogin();
	void SubmitRegister();
	void SwitchTo(EVedgLoginView InView);
	void HandleSteamStatus(const FString& TicketHex, const FVedgSteamStatus& SteamStatus, bool bIsNew);
	void SteamPrompt(const FString& TitleKey, const FText& Body, const FString& ButtonKey, TFunction<void()> OnPrimary);
	void SteamRetry(const FText& Body);
	void Finish(const FVedgLoginResult& Result);
	class UVedgSDKSubsystem* Sdk() const;

	TSharedRef<SWidget> MakeField(TSharedPtr<SEditableTextBox>& OutBox, const FString& HintKey, bool bPassword);
	TSharedRef<SWidget> MakeButton(const TAttribute<FText>& Label, TFunction<void()> OnClick);

	EVedgLoginView View = EVedgLoginView::Hidden;
	FText Status;
	bool bBusy = false;
	FText SteamTitle;
	FText SteamBody;
	FText SteamButton;
	TFunction<void()> OnSteamPrimary;
	FString SteamPersona;
	FString SteamFallbackId;

	TSharedPtr<SEditableTextBox> LoginEmail;
	TSharedPtr<SEditableTextBox> LoginPassword;
	TSharedPtr<SEditableTextBox> RegisterPseudo;
	TSharedPtr<SEditableTextBox> RegisterEmail;
	TSharedPtr<SEditableTextBox> RegisterPassword;
	TSharedPtr<SEditableTextBox> RegisterConfirm;
	TSharedPtr<SWidget> Root;
};
