// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "VedgHeatmapMap.generated.h"

class UBoxComponent;

// Drop one per level: records a heatmap session while the level is played, and voxelizes the level's
// collision (inside the ScanBounds box) for the admin's heatmap view. Mirrors VedgHeatmapMap (heatmap_map.gd).
// Positions still come from the game (UVedgHeatmapCapture::ReportPosition), like in Godot.
UCLASS(meta = (DisplayName = "Vedg Heatmap Map"))
class VEDGSDK_API AVedgHeatmapMap : public AActor
{
	GENERATED_BODY()

public:
	AVedgHeatmapMap();

	UPROPERTY(EditAnywhere, Category = "Vedg Heatmap")
	FString MapId;

	// Revision of the map's geometry (not the build version): bump it when you re-voxelize a changed map.
	UPROPERTY(EditAnywhere, Category = "Vedg Heatmap", meta = (ClampMin = 1))
	int32 MapVersion = 1;

	// In Unreal units (cm). Must match between voxelization and capture.
	UPROPERTY(EditAnywhere, Category = "Vedg Heatmap", meta = (ClampMin = 10, Units = "cm"))
	float CellSize = 100.f;

	UPROPERTY(EditAnywhere, Category = "Vedg Heatmap")
	bool bEnabled = true;

	// Only record from Shipping builds (untick to also record PIE / Development runs).
	UPROPERTY(EditAnywhere, Category = "Vedg Heatmap")
	bool bRequireReleaseBuild = true;

	// Object types that count as solid when voxelizing.
	UPROPERTY(EditAnywhere, Category = "Vedg Heatmap")
	TArray<TEnumAsByte<EObjectTypeQuery>> CollisionObjectTypes;

	// Volume scanned by "Voxelize Map": fit it around the playable area (not the sky sphere).
	UPROPERTY(VisibleAnywhere, Category = "Vedg Heatmap")
	TObjectPtr<UBoxComponent> ScanBounds;

	// Scans the level's collision and uploads the occupied voxels (needs the editor's Vedg login for the heatmap key).
	UFUNCTION(CallInEditor, Category = "Vedg Heatmap", meta = (DisplayName = "Voxelize Map"))
	void VoxelizeMap();

	bool IsRecordingAllowed(FString* OutReason = nullptr) const;

	// Bound by the VedgSDKEditor module: fetches the game's heatmap key with the editor's Vedg account.
	static TFunction<void(TFunction<void(const FString&)>)> HeatmapKeyProvider;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool bRecording = false;

#if WITH_EDITOR
	bool Voxelize(TArray<FIntVector>& OutVoxels) const;
	void UploadVoxels(const TArray<FIntVector>& Voxels, const FString& Key) const;
#endif
};
