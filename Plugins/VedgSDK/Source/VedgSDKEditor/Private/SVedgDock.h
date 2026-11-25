// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FMonitoredProcess;
class FJsonObject;
class SEditableTextBox;
class SMultiLineEditableTextBox;
class SCheckBox;
template<typename OptionType> class SComboBox;

// The "Vedg" editor tab (Window > Vedg): studio account, game environment and game server deploy.
// Mirrors the Godot dock (vedg_dock, vedg_account_tab, vedg_game_env_field_editor, vedg_deploy_dock).
// Other settings are edited in Project Settings > Vedg SDK.
class SVedgDock : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVedgDock) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SVedgDock() override;

private:
	TSharedRef<SWidget> BuildAccount();
	TSharedRef<SWidget> BuildGameEnv();
	TSharedRef<SWidget> BuildDeploy();

	FReply OnLogin();
	FReply OnLogout();

	void FetchEnvs();
	void RefreshEnvOptions(const TArray<FString>& Envs);
	FString GameEnv() const;

	FReply OnDeploy();
	void StartDeploy(const TSharedPtr<FJsonObject>& Config);
	void OnDeployFinished(int32 ExitCode);
	void RegisterBuild();
	void DeployRequest(const FString& Verb, const FString& Path, const FString& Body, TFunction<void(TSharedPtr<FJsonObject>)> OnDone, int32 Attempt = 0);
	void LogLine(const FString& Line, bool bError = false);

	TSharedPtr<SEditableTextBox> EmailBox;
	TSharedPtr<SEditableTextBox> PasswordBox;
	FText AccountStatus;

	TArray<TSharedPtr<FString>> EnvOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> EnvCombo;

	TSharedPtr<SCheckBox> ActivateCheck;
	TSharedPtr<SMultiLineEditableTextBox> DeployLog;
	TSharedPtr<FMonitoredProcess> DeployProcess;
	FString DeployKey;
	FString PushedImage;
	bool bDeploying = false;

	FDelegateHandle AccessHandle;
};
