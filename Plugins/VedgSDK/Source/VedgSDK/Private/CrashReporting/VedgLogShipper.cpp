// Fill out your copyright notice in the Description page of Project Settings.

#include "CrashReporting/VedgLogShipper.h"
#include "Http/VedgEventQueue.h"
#include "Identity/VedgMachineIdentity.h"
#include "VedgSDKSettings.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformOutputDevices.h"

void UVedgLogShipper::StartUp(UVedgEventQueue* InEventQueue, const FString& InSessionId)
{
	EventQueue = InEventQueue;
	SessionId = InSessionId;
	LogFilePath = FPlatformOutputDevices::GetAbsoluteLogFilename();
	ReadOffset = 0;

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVedgLogShipper::Tick), FlushInterval);
}

void UVedgLogShipper::Shutdown()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Flush();
}

bool UVedgLogShipper::Tick(float)
{
	Flush();
	return true;
}

void UVedgLogShipper::Flush()
{
	if (LogFilePath.IsEmpty() || !EventQueue)
	{
		return;
	}

	const TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*LogFilePath));
	if (!Reader.IsValid())
	{
		return;
	}

	const int64 FileSize = Reader->TotalSize();
	if (FileSize < ReadOffset)
	{
		// Log file was rotated/truncated since our last read.
		ReadOffset = 0;
	}
	if (FileSize <= ReadOffset)
	{
		return;
	}

	const int64 BytesToRead = FileSize - ReadOffset;
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(BytesToRead);
	Reader->Seek(ReadOffset);
	Reader->Serialize(Buffer.GetData(), BytesToRead);
	ReadOffset = FileSize;

	// The engine's log file is UTF-8 encoded; decode explicitly rather than
	// via the ANSI-assuming StringCast<TCHAR> overload.
	const FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Buffer.GetData()), BytesToRead);
	const FString NewText(Converter.Length(), Converter.Get());

	TArray<FString> NewLines;
	NewText.ParseIntoArrayLines(NewLines, false);

	for (const FString& Line : NewLines)
	{
		if (Line.IsEmpty())
		{
			continue;
		}
		PendingLines.Add(Line);
		if (PendingLines.Num() > MaxBufferedLines)
		{
			PendingLines.RemoveAt(0);
		}
	}

	if (PendingLines.Num() == 0)
	{
		return;
	}

	TArray<TSharedPtr<FJsonValue>> LogsJson;
	for (const FString& Line : PendingLines)
	{
		LogsJson.Add(MakeShared<FJsonValueString>(Line));
	}

	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("logs"), LogsJson);

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game"), UVedgSDKSettings::Get()->GetGameSlug());
	Body->SetStringField(TEXT("type"), TEXT("log_batch"));
	Body->SetStringField(TEXT("session_id"), SessionId);
	Body->SetStringField(TEXT("machine_id"), FVedgMachineIdentity::GetMachineId());
	Body->SetObjectField(TEXT("data"), Data);

	EventQueue->Enqueue(TEXT("/reports/event"), Body);
	PendingLines.Empty();
}
