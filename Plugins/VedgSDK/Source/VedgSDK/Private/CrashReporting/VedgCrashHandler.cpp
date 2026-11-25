// Fill out your copyright notice in the Description page of Project Settings.

#include "CrashReporting/VedgCrashHandler.h"
#include "CrashReporting/VedgWatchdog.h"
#include "Identity/VedgMachineIdentity.h"
#include "VedgSDKModule.h"
#include "VedgSDKSettings.h"
#include "VedgSDKSubsystem.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformOutputDevices.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
	// The handler takes positional args, so an empty value must still occupy its slot.
	FString Arg(const FString& Value)
	{
		return FString::Printf(TEXT("\"%s\""), Value.IsEmpty() ? TEXT("-") : *Value.Replace(TEXT("\""), TEXT("\\\"")));
	}
}

FString FVedgCrashHandler::BinaryPath()
{
#if PLATFORM_WINDOWS
	const FString Path = FPaths::GetPath(FPlatformProcess::ExecutablePath()) / TEXT("handler/vedg_crash_handler.exe");
	return FPaths::FileExists(Path) ? Path : FString();
#else
	return FString();
#endif
}

void FVedgCrashHandler::Launch(const UVedgSDKSubsystem* Sdk)
{
	const FString Binary = BinaryPath();
	if (Binary.IsEmpty())
	{
		UE_LOG(LogVedgSDK, Warning, TEXT("No VedgCrashHandler binary found for this platform - skipping"));
		return;
	}

	const FString ReadyMarker = FPaths::GetPath(Binary) / TEXT("vedg_crash_handler.ready");
	IFileManager::Get().Delete(*ReadyMarker, false, false, true);

	TArray<FString> Args = {
		Sdk->GetApiUrl(),
		Sdk->GetGameSlug(),
		IsRunningDedicatedServer() ? TEXT("server") : TEXT("client"),
		Sdk->GetSessionId(),
		LexToString(FPlatformProcess::GetCurrentProcessId()),
		FPlatformProcess::ExecutablePath(),
		FPaths::ConvertRelativePathToFull(FPlatformOutputDevices::GetAbsoluteLogFilename()),
		UVedgWatchdog::GetSessionFilePath(),
		FVedgMachineIdentity::GetMachineId(),
		UVedgSDKSettings::GetVersion(),
		Sdk->GetEnv(),
	};

	// Trailing args = the game's own command line, replayed by the dialog's restart button.
	const TCHAR* CommandLine = FCommandLine::GetOriginal();
	FString Token;
	while (FParse::Token(CommandLine, Token, false))
	{
		Args.Add(Token);
	}

	FString Params;
	for (const FString& Value : Args)
	{
		Params += Arg(Value) + TEXT(" ");
	}

	FProcHandle Handle = FPlatformProcess::CreateProc(*Binary, *Params, true, true, true, nullptr, 0, *FPaths::GetPath(Binary), nullptr);
	if (!Handle.IsValid())
	{
		UE_LOG(LogVedgSDK, Error, TEXT("VedgCrashHandler failed to start - binary: %s"), *Binary);
		return;
	}
	FPlatformProcess::CloseProc(Handle);

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([ReadyMarker](float)
	{
		if (!FPaths::FileExists(ReadyMarker))
		{
			UE_LOG(LogVedgSDK, Error, TEXT("VedgCrashHandler started but never became ready - crash detection and the restart dialog are NOT active this session (log shipping and the watchdog are unaffected)"));
		}
		return false;
	}), 3.0f);
}
