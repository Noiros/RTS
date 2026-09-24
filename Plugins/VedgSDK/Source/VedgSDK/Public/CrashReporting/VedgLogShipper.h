// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "VedgLogShipper.generated.h"

class UVedgEventQueue;

// Tails the engine's own log file every 3s by byte offset and ships new
// lines through the event queue. Mirrors VedgLogShipper from the Godot
// addon; the offset is kept in memory only (the Godot original persists a
// .offset sidecar file to disk but never reads it back at boot -- dead
// code, not worth porting).
UCLASS()
class VEDGSDK_API UVedgLogShipper : public UObject
{
	GENERATED_BODY()

public:
	void StartUp(UVedgEventQueue* InEventQueue, const FString& InSessionId);
	void Shutdown();

private:
	bool Tick(float DeltaTime);
	void Flush();

	UPROPERTY()
	TObjectPtr<UVedgEventQueue> EventQueue;

	FString SessionId;
	FString LogFilePath;
	int64 ReadOffset = 0;

	TArray<FString> PendingLines;

	FTSTicker::FDelegateHandle TickerHandle;

	static constexpr double FlushInterval = 3.0;
	static constexpr int32 MaxBufferedLines = 1000;
};
