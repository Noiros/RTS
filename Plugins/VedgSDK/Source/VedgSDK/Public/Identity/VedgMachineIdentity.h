// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UVedgEventQueue;

// Two distinct stable identifiers, mirroring VedgMachine + the identity
// helper from the Godot addon: MachineId (OS-level hardware id) and
// DeviceUid (an app-generated UUID persisted once, shared with the studio's
// Godot titles on the same machine -- same identity.json subfolder).
class VEDGSDK_API FVedgMachineIdentity
{
public:
	static FString GetMachineId();
	static FString GetDeviceUid();

	static void AssignToUser(UVedgEventQueue* EventQueue, const FString& GameSlug, const FString& Uid);

	// One-shot hardware snapshot (os, os_version, cpu, gpu, ram_mb, unique_id) shared
	// by the boot report and the watchdog's abnormal-termination report, so both
	// build the same object instead of duplicating field-by-field construction.
	static TSharedRef<FJsonObject> CollectHardwareSnapshot();

	// Posts CollectHardwareSnapshot() to POST /reports/machine {game, machine_id,
	// hardware}. Intentionally redundant with the native crash-handler binary's own
	// one-shot /reports/machine POST (see FVedgCrashHandlerLauncher) - this report
	// doesn't depend on that binary being present or alive.
	static void PostHardwareSnapshot(UVedgEventQueue* EventQueue, const FString& GameSlug);

private:
	static FString LoadOrCreateDeviceUid();
};
