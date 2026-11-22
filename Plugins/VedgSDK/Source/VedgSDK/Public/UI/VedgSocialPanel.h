// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "VedgSocialPanel.generated.h"

class SEditableTextBox;
class SVerticalBox;
class UVedgSocial;

// Friends + group panel template, mirrors social_panel.tscn + social_panel.gd.
// Drop it in any UMG widget (palette: Vedg). Needs the matchmaking socket open
// (GetMatchmaking()->Open(token)).
UCLASS(meta = (DisplayName = "Vedg Social Panel"))
class VEDGSDK_API UVedgSocialPanel : public UWidget
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return NSLOCTEXT("VedgSDK", "Palette", "Vedg"); }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UFUNCTION()
	void Redraw();

	UFUNCTION()
	void HandleInvite(const FString& Uid, const FString& Pseudo);

	UFUNCTION()
	void HandleAction(const FString& Action, bool bSuccess, const FText& ErrorText);

	void AddFriendPressed();
	void AddRow(const TSharedPtr<SVerticalBox>& List, const FString& Pseudo, const FString& Detail, bool bOnline, const TArray<TPair<FString, TFunction<void()>>>& Actions);
	FString PresenceText(const TSharedPtr<FJsonObject>& Entry) const;

	UPROPERTY()
	TObjectPtr<UVedgSocial> Social;

	FText Status;
	bool bAddPending = false;

	TSharedPtr<SEditableTextBox> PseudoBox;
	TSharedPtr<SVerticalBox> GroupList;
	TSharedPtr<SVerticalBox> InvitesList;
	TSharedPtr<SVerticalBox> RequestsList;
	TSharedPtr<SVerticalBox> FriendsList;
	TSharedPtr<SVerticalBox> SentList;
};
