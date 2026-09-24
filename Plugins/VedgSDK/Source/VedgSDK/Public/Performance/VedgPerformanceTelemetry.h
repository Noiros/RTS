// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "VedgPerformanceTelemetry.generated.h"

class UVedgEventQueue;

// Samples frame time every frame, flushes one aggregate FPS sample every 30s
// (and once on Shutdown) via the event queue. Mirrors VedgPerformance from
// the Godot addon, including its fps_min/fps_max derivation quirk (fps_min
// comes from the MAX frame time, fps_max from the MIN frame time).
UCLASS()
class VEDGSDK_API UVedgPerformanceTelemetry : public UObject
{
	GENERATED_BODY()

public:
	void StartUp(UVedgEventQueue* InEventQueue, const FString& InSessionId);
	void Shutdown();

private:
	bool Tick(float DeltaTime);
	void OnEndFrame();
	void Flush();

	UPROPERTY()
	TObjectPtr<UVedgEventQueue> EventQueue;

	FString SessionId;

	int32 FrameCount = 0;
	double FrameTimeSum = 0.0;
	double FrameTimeMin = 0.0;
	double FrameTimeMax = 0.0;

	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle EndFrameHandle;

	static constexpr double FlushInterval = 30.0;
};
