// Fill out your copyright notice in the Description page of Project Settings.

#include "SVedgDock.h"
#include "VedgEditorAccount.h"
#include "Heatmap/VedgHeatmapMap.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "VedgSDKEditor"

static const FName VedgTabName(TEXT("VedgDock"));

class FVedgSDKEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(VedgTabName, FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SVedgDock)];
		}))
		.SetDisplayName(LOCTEXT("TabTitle", "Vedg"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Vedg account, game environment and game server deploy"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

		AVedgHeatmapMap::HeatmapKeyProvider = [](TFunction<void(const FString&)> OnKey) { FVedgEditorAccount::HeatmapKey(MoveTemp(OnKey)); };

		// Keeps Saved/VedgSDK/build_key (compiled in by VedgSDK.Build.cs) in sync with the game's current key.
		if (FVedgEditorAccount::IsLoggedIn())
		{
			FVedgEditorAccount::RefreshBuildKey();
		}
	}

	virtual void ShutdownModule() override
	{
		AVedgHeatmapMap::HeatmapKeyProvider = nullptr;
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(VedgTabName);
		}
	}
};

IMPLEMENT_MODULE(FVedgSDKEditorModule, VedgSDKEditor)

#undef LOCTEXT_NAMESPACE
