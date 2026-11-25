// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UVedgSDKSubsystem;

// Launches the native Vedg-Crash-Handler (same binary as the Godot addon) next to the game process.
// It watches our PID, posts session end, and on an abnormal exit shows the branded crash dialog
// (restart + send logs). Staged by VedgSDK.Build.cs to <exe dir>/handler/. Mirrors crash_handler.gd.
class VEDGSDK_API FVedgCrashHandler
{
public:
	static void Launch(const UVedgSDKSubsystem* Sdk);

	static FString BinaryPath();
};
