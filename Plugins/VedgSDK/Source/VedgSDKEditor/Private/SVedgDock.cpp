// Fill out your copyright notice in the Description page of Project Settings.

#include "SVedgDock.h"
#include "VedgEditorAccount.h"
#include "Http/VedgHttpRequest.h"
#include "VedgSDKModule.h"
#include "VedgSDKSettings.h"
#include "VedgSDKTypes.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/MonitoredProcess.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VedgDock"

namespace
{
	const FString ImageMarker = TEXT("VEDG_IMAGE=");

	FString Slug()
	{
		return FGenericPlatformHttp::UrlEncode(UVedgSDKSettings::Get()->GetGameSlug());
	}

	// "" = no override: the runtime falls back to its default game env.
	FText EnvLabel(const FString& Env)
	{
		return FText::FromString(Env.IsEmpty() ? TEXT("prod (default)") : Env);
	}

	EVisibility LoggedIn()
	{
		return FVedgEditorAccount::IsLoggedIn() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	TSharedRef<SWidget> Heading(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	}
}

void SVedgDock::Construct(const FArguments&)
{
	AccessHandle = FVedgEditorAccount::OnAccessChanged.AddSP(this, &SVedgDock::FetchEnvs);

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(12)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[BuildAccount()]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 0)
			[
				SNew(SBox).Visibility_Static(&LoggedIn)[BuildGameEnv()]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 0)
			[
				SNew(SBox).Visibility_Static(&LoggedIn)[BuildDeploy()]
			]
		]
	];

	FetchEnvs();
}

SVedgDock::~SVedgDock()
{
	FVedgEditorAccount::OnAccessChanged.Remove(AccessHandle);
}

TSharedRef<SWidget> SVedgDock::BuildAccount()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Heading(LOCTEXT("Account", "Vedg account"))]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([] { return FVedgEditorAccount::IsLoggedIn() ? EVisibility::Collapsed : EVisibility::Visible; })
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SAssignNew(EmailBox, SEditableTextBox).HintText(LOCTEXT("Email", "Email")).Text(FText::FromString(FVedgEditorAccount::Email()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SAssignNew(PasswordBox, SEditableTextBox).HintText(LOCTEXT("Password", "Password")).IsPassword(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4).HAlign(HAlign_Left)
			[
				SNew(SButton).Text(LOCTEXT("Login", "Log in")).OnClicked(this, &SVedgDock::OnLogin)
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			.Visibility_Static(&LoggedIn)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([] { return FText::Format(LOCTEXT("LoggedAs", "Logged in as {0}"), FText::FromString(FVedgEditorAccount::Email())); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("Logout", "Log out")).OnClicked(this, &SVedgDock::OnLogout)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
		[
			SNew(STextBlock).Text_Lambda([this] { return AccountStatus; }).AutoWrapText(true)
		];
}

FReply SVedgDock::OnLogin()
{
	AccountStatus = LOCTEXT("LoggingIn", "Logging in...");
	TWeakPtr<SVedgDock> Weak = SharedThis(this);
	FVedgEditorAccount::Login(EmailBox->GetText().ToString().TrimStartAndEnd(), PasswordBox->GetText().ToString(), [Weak](const FString& Error)
	{
		if (const TSharedPtr<SVedgDock> Self = Weak.Pin())
		{
			Self->AccountStatus = FText::FromString(Error);
			Self->PasswordBox->SetText(FText::GetEmpty());
		}
	});
	return FReply::Handled();
}

FReply SVedgDock::OnLogout()
{
	FVedgEditorAccount::Logout();
	AccountStatus = FText::GetEmpty();
	return FReply::Handled();
}

TSharedRef<SWidget> SVedgDock::BuildGameEnv()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Heading(LOCTEXT("GameEnv", "Game environment"))]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SAssignNew(EnvCombo, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&EnvOptions)
			.ToolTipText(LOCTEXT("GameEnvTip", "Remote config, matchmaking queues and game server build. Environments are managed in Vedg Admin > Game Settings."))
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(EnvLabel(*Item)); })
			.OnSelectionChanged_Lambda([](TSharedPtr<FString> Item, ESelectInfo::Type Info)
			{
				if (!Item.IsValid() || Info == ESelectInfo::Direct)
				{
					return;
				}
				UVedgSDKSettings* Settings = GetMutableDefault<UVedgSDKSettings>();
				Settings->GameEnv = *Item;
				Settings->TryUpdateDefaultConfigFile();
			})
			[
				SNew(STextBlock).MinDesiredWidth(140).Text_Lambda([this] { return EnvLabel(GameEnv()); })
			]
		];
}

FString SVedgDock::GameEnv() const
{
	return UVedgSDKSettings::Get()->GameEnv;
}

void SVedgDock::FetchEnvs()
{
	RefreshEnvOptions({});
	if (!FVedgEditorAccount::IsLoggedIn() || UVedgSDKSettings::Get()->GetGameSlug().IsEmpty())
	{
		return;
	}
	TWeakPtr<SVedgDock> Weak = SharedThis(this);
	const FString Url = FString::Printf(TEXT("%s/matchmaking/%s/envs"), *UVedgSDKSettings::Get()->GetApiUrl(), *Slug());
	FVedgHttpRequest::Request(Url, TEXT("GET"), FString(), [Weak](FVedgHttpResult Result)
	{
		const TSharedPtr<SVedgDock> Self = Weak.Pin();
		const TArray<TSharedPtr<FJsonValue>>* Envs = nullptr;
		if (!Self || !Result.bValid || !Result.ResponseObject.IsValid() || !Result.ResponseObject->TryGetArrayField(TEXT("envs"), Envs))
		{
			return;
		}
		TArray<FString> Slugs;
		for (const TSharedPtr<FJsonValue>& Env : *Envs)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Env->TryGetObject(Object))
			{
				Slugs.Add((*Object)->GetStringField(TEXT("slug")));
			}
		}
		Self->RefreshEnvOptions(Slugs);
	});
}

void SVedgDock::RefreshEnvOptions(const TArray<FString>& Envs)
{
	TArray<FString> Options = Envs.Num() > 0 ? Envs : TArray<FString>{TEXT("dev"), TEXT("prod")};
	if (!GameEnv().IsEmpty())
	{
		Options.AddUnique(GameEnv());
	}
	EnvOptions.Reset();
	EnvOptions.Add(MakeShared<FString>());
	for (const FString& Option : Options)
	{
		EnvOptions.Add(MakeShared<FString>(Option));
	}
	if (EnvCombo.IsValid())
	{
		EnvCombo->RefreshOptions();
	}
}

TSharedRef<SWidget> SVedgDock::BuildDeploy()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Heading(LOCTEXT("Deploy", "Game server deploy"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			SNew(STextBlock).Text_Lambda([] { return FText::Format(LOCTEXT("DeployEnv", "Target: {0}"), FText::FromString(UVedgSDKSettings::Get()->ResolveGameEnv())); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			SAssignNew(ActivateCheck, SCheckBox).IsChecked(ECheckBoxState::Checked)
			[
				SNew(STextBlock).Text(LOCTEXT("Activate", "Activate the build for the game environment"))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4).HAlign(HAlign_Left)
		[
			SNew(SButton)
			.Text(LOCTEXT("DeployButton", "Deploy game server"))
			.IsEnabled_Lambda([this] { return !bDeploying; })
			.OnClicked(this, &SVedgDock::OnDeploy)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
		[
			SNew(SBox).HeightOverride(320)
			[
				SAssignNew(DeployLog, SMultiLineEditableTextBox).IsReadOnly(true).AlwaysShowScrollbars(true)
			]
		];
}

FReply SVedgDock::OnDeploy()
{
	const FString GameEnvName = UVedgSDKSettings::Get()->ResolveGameEnv();
	const bool bActivate = ActivateCheck->IsChecked();
	const FText Question = FText::Format(LOCTEXT("DeployConfirm", "Build the game server and {0} for the game environment \"{1}\"?"),
		bActivate ? LOCTEXT("PushActivate", "push and activate it") : LOCTEXT("Push", "push it"), FText::FromString(GameEnvName));
	if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	bDeploying = true;
	PushedImage.Reset();
	DeployLog->SetText(FText::GetEmpty());

	TWeakPtr<SVedgDock> Weak = SharedThis(this);
	FVedgEditorAccount::DeployKey([Weak](const FString& Key)
	{
		const TSharedPtr<SVedgDock> Self = Weak.Pin();
		if (!Self)
		{
			return;
		}
		if (Key.IsEmpty())
		{
			Self->LogLine(TEXT("No deploy key: log in with an account allowed to manage the infrastructure"), true);
			Self->bDeploying = false;
			return;
		}
		Self->DeployKey = Key;
		Self->LogLine(TEXT("Reading the image repository from Vedg ..."));
		Self->DeployRequest(TEXT("GET"), TEXT("config"), FString(), [Weak](TSharedPtr<FJsonObject> Config)
		{
			if (const TSharedPtr<SVedgDock> Self = Weak.Pin())
			{
				Self->StartDeploy(Config);
			}
		});
	});
	return FReply::Handled();
}

void SVedgDock::StartDeploy(const TSharedPtr<FJsonObject>& Config)
{
	FString Repository, Registry, Region;
	if (!Config.IsValid() || !Config->TryGetStringField(TEXT("repository"), Repository))
	{
		LogLine(TEXT("Could not read the deploy configuration from Vedg"), true);
		bDeploying = false;
		return;
	}
	Config->TryGetStringField(TEXT("registry"), Registry);
	Config->TryGetStringField(TEXT("region"), Region);

	// The project's own deploy.bat wins; the SDK one builds the project's Dockerfile.
	const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const FString ProjectScript = ProjectRoot / TEXT("deploy.bat");
	const FString Script = FPaths::FileExists(ProjectScript) ? ProjectScript
		: FPaths::ConvertRelativePathToFull(IPluginManager::Get().FindPlugin(TEXT("VedgSDK"))->GetBaseDir() / TEXT("Resources/deploy_server.bat"));
	LogLine(FString::Printf(TEXT("Deploying to %s with %s"), *Repository, *FPaths::GetCleanFilename(Script)));

	FString RootArg = ProjectRoot;
	FString ScriptArg = Script;
	FPaths::MakePlatformFilename(RootArg);
	FPaths::MakePlatformFilename(ScriptArg);
	const FString Params = FString::Printf(TEXT("/c chcp 65001>nul && cd /d \"%s\" && \"%s\" \"%s\" \"%s\" \"%s\" 2>&1"),
		*RootArg, *ScriptArg, *Repository, *Registry, *Region);
	DeployProcess = MakeShared<FMonitoredProcess>(TEXT("cmd.exe"), Params, true, true);

	// FMonitoredProcess calls back from its own thread.
	TWeakPtr<SVedgDock> Weak = SharedThis(this);
	DeployProcess->OnOutput().BindLambda([Weak](FString Line)
	{
		AsyncTask(ENamedThreads::GameThread, [Weak, Line]()
		{
			if (const TSharedPtr<SVedgDock> Self = Weak.Pin())
			{
				const FString Trimmed = Line.TrimStartAndEnd();
				if (Trimmed.StartsWith(ImageMarker))
				{
					Self->PushedImage = Trimmed.RightChop(ImageMarker.Len());
				}
				Self->LogLine(Line);
			}
		});
	});
	DeployProcess->OnCompleted().BindLambda([Weak](int32 ExitCode)
	{
		AsyncTask(ENamedThreads::GameThread, [Weak, ExitCode]()
		{
			if (const TSharedPtr<SVedgDock> Self = Weak.Pin())
			{
				Self->OnDeployFinished(ExitCode);
			}
		});
	});

	if (!DeployProcess->Launch())
	{
		LogLine(FString::Printf(TEXT("Failed to start %s"), *Script), true);
		DeployProcess.Reset();
		bDeploying = false;
	}
}

void SVedgDock::OnDeployFinished(int32 ExitCode)
{
	LogLine(FString::Printf(TEXT("Finished with exit code %d"), ExitCode), ExitCode != 0);
	if (ExitCode == 0)
	{
		RegisterBuild();
		return;
	}
	bDeploying = false;
}

void SVedgDock::RegisterBuild()
{
	if (PushedImage.IsEmpty())
	{
		LogLine(FString::Printf(TEXT("The deploy script did not print a %s line, the build was not registered"), *ImageMarker), true);
		bDeploying = false;
		return;
	}

	const FString GameEnvName = UVedgSDKSettings::Get()->ResolveGameEnv();
	const bool bActivate = ActivateCheck->IsChecked();
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("image"), PushedImage);
	Body->SetStringField(TEXT("label"), UVedgSDKSettings::GetVersion() + TEXT("-") + GameEnvName);
	Body->SetStringField(TEXT("env"), GameEnvName);
	Body->SetBoolField(TEXT("activate"), bActivate);
	FString BodyText;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&BodyText));

	LogLine(FString::Printf(TEXT("Registering %s in Vedg ..."), *PushedImage));
	TWeakPtr<SVedgDock> Weak = SharedThis(this);
	DeployRequest(TEXT("POST"), TEXT("builds"), BodyText, [Weak, bActivate, GameEnvName](TSharedPtr<FJsonObject> Response)
	{
		const TSharedPtr<SVedgDock> Self = Weak.Pin();
		if (!Self)
		{
			return;
		}
		Self->bDeploying = false;
		const TSharedPtr<FJsonObject>* Build = nullptr;
		if (!Response.IsValid() || !Response->TryGetObjectField(TEXT("build"), Build))
		{
			Self->LogLine(FString::Printf(TEXT("Register %s manually in Vedg Admin > Builds"), *Self->PushedImage), true);
			return;
		}
		const FString Id = (*Build)->GetStringField(TEXT("id"));
		Self->LogLine(bActivate ? FString::Printf(TEXT("Build #%s registered and active in \"%s\""), *Id, *GameEnvName) : FString::Printf(TEXT("Build #%s registered"), *Id));
	});
}

void SVedgDock::DeployRequest(const FString& Verb, const FString& Path, const FString& Body, TFunction<void(TSharedPtr<FJsonObject>)> OnDone, int32 Attempt)
{
	const FString Url = FString::Printf(TEXT("%s/deploy/%s/%s"), *UVedgSDKSettings::Get()->GetApiUrl(), *Slug(), *Path);
	TWeakPtr<SVedgDock> Weak = SharedThis(this);
	FVedgHttpRequest::Request(Url, Verb, Body, [Weak, Verb, Path, Body, OnDone, Attempt, Url](FVedgHttpResult Result)
	{
		const TSharedPtr<SVedgDock> Self = Weak.Pin();
		if (!Self)
		{
			return;
		}
		if (Result.bValid)
		{
			OnDone(Result.ResponseObject);
			return;
		}
		Self->LogLine(FString::Printf(TEXT("Vedg answered %d on %s: %s"), Result.StatusCode, *Url, *FVedgEditorAccount::ErrorOf(Result, TEXT("request failed"))), true);
		// Like the Godot dock: retry only what can recover (no connection, API restarting, route not deployed yet).
		const bool bRetry = Attempt < 2 && (Result.StatusCode == 0 || Result.StatusCode == 404 || Result.StatusCode >= 500);
		if (!bRetry)
		{
			OnDone(nullptr);
			return;
		}
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, Verb, Path, Body, OnDone, Attempt](float)
		{
			if (const TSharedPtr<SVedgDock> Self = Weak.Pin())
			{
				Self->DeployRequest(Verb, Path, Body, OnDone, Attempt + 1);
			}
			return false;
		}), 10.f);
	}, 120.f, FString(), {{TEXT("X-Deploy-Key"), DeployKey}});
}

void SVedgDock::LogLine(const FString& Line, bool bError)
{
	if (Line.IsEmpty())
	{
		return;
	}
	const FString Text = (bError ? TEXT("ERROR: ") : TEXT("")) + Line.TrimEnd();
	UE_LOG(LogVedgSDK, Display, TEXT("Deploy: %s"), *Text);
	DeployLog->SetText(FText::FromString(DeployLog->GetText().ToString() + Text + TEXT("\n")));
}

#undef LOCTEXT_NAMESPACE
