// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

VEDGSDK_API DECLARE_LOG_CATEGORY_EXTERN(LogVedgSDK, Log, All);

class FVedgSDKModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
