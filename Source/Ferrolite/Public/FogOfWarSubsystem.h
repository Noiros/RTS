#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FogOfWarSubsystem.generated.h"

class UTexture2D;

/**
 * Fog of war grid (one cell = CellSize uu) for both factions, updated natively every UpdateInterval.
 * Writes the fog mask (FogTextureSize px, 1 px per cell) and minimap (2 px per cell + unit/building dots)
 * into transient textures with one upload each, instead of one Canvas draw per cell.
 * Ghost buildings are never vision sources (they are not in ActorScanner, and are skipped explicitly).
 */
UCLASS()
class FERROLITE_API UFogOfWarSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "RTS|FogOfWar")
	void InitGrid(FVector BoundsMin, FVector BoundsMax, float InCellSize, UTexture2D*& OutFogTexture, UTexture2D*& OutMinimapTexture, int32& OutGridW, int32& OutGridH);

	UFUNCTION(BlueprintCallable, Category = "RTS|FogOfWar")
	void MarkSource(FVector Pos, float Radius, bool ForHuman);

	UFUNCTION(BlueprintPure, Category = "RTS|FogOfWar")
	bool IsActorVisibleTo(bool ForHuman, const AActor* Target) const;

	/** Center of the nearest unexplored cell further than 2 cells from From; From itself if none. */
	UFUNCTION(BlueprintCallable, Category = "RTS|FogOfWar")
	FVector GetNearestUnexplored(bool ForHuman, FVector From) const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return GridW > 0; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	int32 CellIndex(const FVector& Pos) const;
	void UpdateFog();
	void UploadTextures();
	static void Upload(UTexture2D* Texture, const TArray<FColor>& Pixels, int32 Size);

	static constexpr int32 FogTextureSize = 64;
	static constexpr int32 MinimapTextureSize = 128;
	static constexpr float UpdateInterval = 0.2f;
	static constexpr float BuildingVisionRadius = 4500.f;

	FVector BoundsMin = FVector::ZeroVector;
	float CellSize = 1000.f;
	int32 GridW = 0;
	int32 GridH = 0;
	float TimeSinceUpdate = 0.f;

	TArray<bool> ExploredHuman, ExploredAlien, VisibleHuman, VisibleAlien;
	TArray<FColor> FogPixels, MinimapPixels;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FogTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MinimapTexture;
};
