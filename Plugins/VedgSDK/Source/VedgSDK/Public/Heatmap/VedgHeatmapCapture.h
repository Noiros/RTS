// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "VedgHeatmapCapture.generated.h"

class UVedgEventQueue;
class FVedgErrorDevice;

// Player-movement heatmap capture, mirrors heatmap.gd + heatmap_capture.gd:
// 1 accepted sample per second, the path in between supersampled per half
// cell, positions flushed every 5s (direct, loss-tolerant), events through the
// persisted outbox. Owned by the SDK subsystem: GetHeatmap(). Logged errors
// (UE_LOG Error) are tagged as "error" events while a session is active.
UCLASS(BlueprintType)
class VEDGSDK_API UVedgHeatmapCapture : public UObject
{
	GENERATED_BODY()

public:
	void StartUp(UVedgEventQueue* InEventQueue);
	void Shutdown();

	// For the local player only. CellSize must match the map's voxelized cell
	// size; MapVersion is the map's own revision, not the build version.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Heatmap")
	void StartCapture(const FString& MapId, float CellSize, int32 MapVersion);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Heatmap")
	void EndCapture();

	// Restarts "elapsed time" for the current map (e.g. on every respawn).
	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Heatmap")
	void ResetTime();

	// Tags a one-off event at the current moment; its position is resolved
	// server-side from this client's own movement trace.
	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Heatmap")
	void RecordEvent(const FString& Event);

	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Heatmap")
	void RecordEventValue(const FString& Event, float Value);

	// Call every tick with the local player's location (throttled internally).
	UFUNCTION(BlueprintCallable, Category = "VedgSDK|Heatmap")
	void ReportPosition(const FVector& Position);

	UFUNCTION(BlueprintPure, Category = "VedgSDK|Heatmap")
	bool IsActive() const { return !SessionId.IsEmpty(); }

private:
	void Start(const FString& MapId, float CellSize, int32 MapVersion, TFunction<void()> OnDone);
	void Stop(TFunction<void()> OnDone);
	void Record(const FString& Event, const TSharedPtr<FJsonValue>& Value);
	bool FlushTick(float DeltaTime);
	void Flush(const FString& ForSessionId, TFunction<void()> OnDone = nullptr);
	FIntVector VoxelOf(const FVector& Pos) const;
	void EnqueuePath(const FVector& From, const FVector& To, int64 FromTs, int64 ToTs);
	void EnqueueVoxel(const FIntVector& Voxel, int64 Ts);

	UPROPERTY()
	TObjectPtr<UVedgEventQueue> EventQueue;

	FString SessionId;
	FString CurrentMapId;
	int32 CurrentMapVersion = 0;
	float CurrentCellSize = 1.f;
	bool bStarting = false;
	TArray<TSharedPtr<FJsonValue>> Buffer;
	int64 LastSampleAtMs = 0;
	bool bHasLastPosition = false;
	FVector LastPosition = FVector::ZeroVector;
	FIntVector LastVoxel = FIntVector::ZeroValue;

	TSharedPtr<FVedgErrorDevice> ErrorDevice;
	FTSTicker::FDelegateHandle FlushHandle;

	static constexpr int64 SampleIntervalMs = 1000;
	static constexpr float FlushIntervalSec = 5.f;
	static constexpr int32 MaxBufferEntries = 2000;
	// A longer single-tick jump is a teleport (respawn...): not interpolated through
	// walls. 20 m like the Godot addon, in Unreal units (cm).
	static constexpr double MaxInterpolateDistance = 2000.0;
};
