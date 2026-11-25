#include "FogOfWarSubsystem.h"

#include "ActorScanner.h"
#include "Building.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Unit.h"

namespace FogOfWarPrivate
{
	// Same colors the Blueprint drew into the RGBA8 linear render targets.
	const FColor FogUnexplored = FLinearColor(1.f, 1.f, 1.f, 1.f).ToFColor(false);
	const FColor FogVisible = FLinearColor(0.f, 0.f, 0.f, 1.f).ToFColor(false);
	const FColor FogExplored = FLinearColor(0.55f, 0.f, 0.f, 1.f).ToFColor(false);
	const FColor MiniBackground = FLinearColor(0.02f, 0.02f, 0.03f, 1.f).ToFColor(false);
	const FColor MiniVisible = FLinearColor(0.55f, 0.33f, 0.22f, 1.f).ToFColor(false);
	const FColor MiniExplored = FLinearColor(0.28f, 0.17f, 0.12f, 1.f).ToFColor(false);
	const FColor MiniBuildingHuman = FLinearColor(0.f, 0.7f, 1.f, 1.f).ToFColor(false);
	const FColor MiniBuildingAlien = FLinearColor(1.f, 0.15f, 0.1f, 1.f).ToFColor(false);
	const FColor MiniUnitHuman = FLinearColor(0.3f, 1.f, 1.f, 1.f).ToFColor(false);
	const FColor MiniUnitAlien = FLinearColor(1.f, 0.3f, 0.2f, 1.f).ToFColor(false);

	UTexture2D* MakeTexture(int32 Size)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
		Texture->SRGB = false;
		Texture->UpdateResource();
		return Texture;
	}

	void FillRect(TArray<FColor>& Pixels, int32 Size, int32 X, int32 Y, int32 W, int32 H, const FColor& Color)
	{
		for (int32 PY = FMath::Max(Y, 0); PY < FMath::Min(Y + H, Size); ++PY)
		{
			for (int32 PX = FMath::Max(X, 0); PX < FMath::Min(X + W, Size); ++PX)
			{
				Pixels[PY * Size + PX] = Color;
			}
		}
	}
}

bool UFogOfWarSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UFogOfWarSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFogOfWarSubsystem, STATGROUP_Tickables);
}

void UFogOfWarSubsystem::InitGrid(FVector InBoundsMin, FVector BoundsMax, float InCellSize, UTexture2D*& OutFogTexture, UTexture2D*& OutMinimapTexture, int32& OutGridW, int32& OutGridH)
{
	using namespace FogOfWarPrivate;

	BoundsMin = InBoundsMin;
	CellSize = FMath::Max(InCellSize, 1.f);
	GridW = FMath::TruncToInt((BoundsMax.X - BoundsMin.X) / CellSize) + 1;
	GridH = FMath::TruncToInt((BoundsMax.Y - BoundsMin.Y) / CellSize) + 1;

	const int32 NumCells = GridW * GridH;
	ExploredHuman.Init(false, NumCells);
	ExploredAlien.Init(false, NumCells);
	VisibleHuman.Init(false, NumCells);
	VisibleAlien.Init(false, NumCells);

	FogPixels.Init(FogUnexplored, FogTextureSize * FogTextureSize);
	MinimapPixels.Init(MiniBackground, MinimapTextureSize * MinimapTextureSize);
	FogTexture = MakeTexture(FogTextureSize);
	MinimapTexture = MakeTexture(MinimapTextureSize);

	UpdateFog();
	TimeSinceUpdate = 0.f;

	OutFogTexture = FogTexture;
	OutMinimapTexture = MinimapTexture;
	OutGridW = GridW;
	OutGridH = GridH;
}

int32 UFogOfWarSubsystem::CellIndex(const FVector& Pos) const
{
	const int32 X = FMath::FloorToInt((Pos.X - BoundsMin.X) / CellSize);
	const int32 Y = FMath::FloorToInt((Pos.Y - BoundsMin.Y) / CellSize);
	if (X < 0 || X >= GridW || Y < 0 || Y >= GridH)
	{
		return INDEX_NONE;
	}
	return Y * GridW + X;
}

void UFogOfWarSubsystem::MarkSource(FVector Pos, float Radius, bool ForHuman)
{
	if (GridW <= 0)
	{
		return;
	}

	TArray<bool>& Visible = ForHuman ? VisibleHuman : VisibleAlien;
	TArray<bool>& Explored = ForHuman ? ExploredHuman : ExploredAlien;

	const int32 CX = FMath::FloorToInt((Pos.X - BoundsMin.X) / CellSize);
	const int32 CY = FMath::FloorToInt((Pos.Y - BoundsMin.Y) / CellSize);
	const int32 R = FMath::TruncToInt(Radius / CellSize) + 1;
	const double RadiusSq = FMath::Square(static_cast<double>(Radius));

	for (int32 Y = FMath::Max(CY - R, 0); Y <= FMath::Min(CY + R, GridH - 1); ++Y)
	{
		for (int32 X = FMath::Max(CX - R, 0); X <= FMath::Min(CX + R, GridW - 1); ++X)
		{
			const FVector Center(BoundsMin.X + (X + 0.5) * CellSize, BoundsMin.Y + (Y + 0.5) * CellSize, Pos.Z);
			if (FVector::DistSquared2D(Center, Pos) <= RadiusSq)
			{
				const int32 Index = Y * GridW + X;
				Visible[Index] = true;
				Explored[Index] = true;
			}
		}
	}
}

bool UFogOfWarSubsystem::IsActorVisibleTo(bool ForHuman, const AActor* Target) const
{
	const int32 Index = Target ? CellIndex(Target->GetActorLocation()) : INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		return false;
	}
	return ForHuman ? VisibleHuman[Index] : VisibleAlien[Index];
}

FVector UFogOfWarSubsystem::GetNearestUnexplored(bool ForHuman, FVector From) const
{
	const TArray<bool>& Explored = ForHuman ? ExploredHuman : ExploredAlien;
	const double MinDistSq = FMath::Square(CellSize * 2.0);
	double BestDistSq = TNumericLimits<double>::Max();
	FVector Best = From;

	for (int32 Index = 0; Index < Explored.Num(); ++Index)
	{
		if (Explored[Index])
		{
			continue;
		}
		const FVector Center(BoundsMin.X + (Index % GridW + 0.5) * CellSize, BoundsMin.Y + (Index / GridW + 0.5) * CellSize, 0.0);
		const double DistSq = FVector::DistSquared2D(Center, From);
		if (DistSq < BestDistSq && DistSq > MinDistSq)
		{
			BestDistSq = DistSq;
			Best = Center;
		}
	}
	return Best;
}

void UFogOfWarSubsystem::Tick(float DeltaTime)
{
	TimeSinceUpdate += DeltaTime;
	if (TimeSinceUpdate >= UpdateInterval)
	{
		TimeSinceUpdate = 0.f;
		UpdateFog();
	}
}

void UFogOfWarSubsystem::UpdateFog()
{
	UActorScanner* Scanner = GetWorld()->GetSubsystem<UActorScanner>();
	if (!Scanner)
	{
		return;
	}

	FMemory::Memzero(VisibleHuman.GetData(), VisibleHuman.Num() * sizeof(bool));
	FMemory::Memzero(VisibleAlien.GetData(), VisibleAlien.Num() * sizeof(bool));

	for (const TWeakObjectPtr<AUnit>& WeakUnit : Scanner->GetUnitsRaw())
	{
		if (const AUnit* Unit = WeakUnit.Get())
		{
			const float Vision = static_cast<float>(UActorScanner::GetDataNumber(Unit, TEXT("VisionRadius"), 0.0));
			MarkSource(Unit->GetActorLocation(), Vision, UActorScanner::GetActorFaction(Unit) == EFaction::Human);
		}
	}
	for (const TWeakObjectPtr<ABuilding>& WeakBuilding : Scanner->GetBuildingsRaw())
	{
		const ABuilding* Building = WeakBuilding.Get();
		if (Building && !Building->IsGhost())
		{
			MarkSource(Building->GetActorLocation(), BuildingVisionRadius, UActorScanner::GetActorFaction(Building) == EFaction::Human);
		}
	}

	// Hide what the (human) player can't see; only touch actors whose state changes.
	for (const TWeakObjectPtr<AUnit>& WeakUnit : Scanner->GetUnitsRaw())
	{
		AUnit* Unit = WeakUnit.Get();
		if (Unit && UActorScanner::GetActorFaction(Unit) != EFaction::Human)
		{
			const bool bHide = !IsActorVisibleTo(true, Unit);
			if (Unit->IsHidden() != bHide)
			{
				Unit->SetActorHiddenInGame(bHide);
			}
		}
	}
	for (const TWeakObjectPtr<ABuilding>& WeakBuilding : Scanner->GetBuildingsRaw())
	{
		ABuilding* Building = WeakBuilding.Get();
		if (Building && UActorScanner::GetActorFaction(Building) != EFaction::Human)
		{
			const int32 Index = CellIndex(Building->GetActorLocation());
			const bool bHide = Index == INDEX_NONE || !ExploredHuman[Index];
			if (Building->IsHidden() != bHide)
			{
				Building->SetActorHiddenInGame(bHide);
			}
		}
	}

	UploadTextures();
}

void UFogOfWarSubsystem::UploadTextures()
{
	using namespace FogOfWarPrivate;

	UActorScanner* Scanner = GetWorld()->GetSubsystem<UActorScanner>();
	if (!FogTexture || !MinimapTexture || !Scanner)
	{
		return;
	}

	for (FColor& Pixel : MinimapPixels)
	{
		Pixel = MiniBackground;
	}

	for (int32 Index = 0; Index < GridW * GridH; ++Index)
	{
		const int32 X = Index % GridW;
		const int32 Y = Index / GridW;
		const bool bVisible = VisibleHuman[Index];
		const bool bExplored = bVisible || ExploredHuman[Index];
		if (X < FogTextureSize && Y < FogTextureSize)
		{
			FogPixels[Y * FogTextureSize + X] = !bExplored ? FogUnexplored : (bVisible ? FogVisible : FogExplored);
		}
		if (bExplored)
		{
			FillRect(MinimapPixels, MinimapTextureSize, X * 2, Y * 2, 2, 2, bVisible ? MiniVisible : MiniExplored);
		}
	}

	auto ToMini = [this](const FVector& Loc, double Offset) -> FIntPoint
	{
		return FIntPoint(FMath::FloorToInt((Loc.X - BoundsMin.X) / CellSize * 2.0 - Offset), FMath::FloorToInt((Loc.Y - BoundsMin.Y) / CellSize * 2.0 - Offset));
	};

	for (const TWeakObjectPtr<ABuilding>& WeakBuilding : Scanner->GetBuildingsRaw())
	{
		const ABuilding* Building = WeakBuilding.Get();
		if (!Building || Building->IsGhost())
		{
			continue;
		}
		const bool bHuman = UActorScanner::GetActorFaction(Building) == EFaction::Human;
		const int32 Index = CellIndex(Building->GetActorLocation());
		if (bHuman || (Index != INDEX_NONE && ExploredHuman[Index]))
		{
			const FIntPoint P = ToMini(Building->GetActorLocation(), 3.0);
			FillRect(MinimapPixels, MinimapTextureSize, P.X, P.Y, 6, 6, bHuman ? MiniBuildingHuman : MiniBuildingAlien);
		}
	}
	for (const TWeakObjectPtr<AUnit>& WeakUnit : Scanner->GetUnitsRaw())
	{
		const AUnit* Unit = WeakUnit.Get();
		if (!Unit)
		{
			continue;
		}
		const bool bHuman = UActorScanner::GetActorFaction(Unit) == EFaction::Human;
		if (bHuman || IsActorVisibleTo(true, Unit))
		{
			const FIntPoint P = ToMini(Unit->GetActorLocation(), 2.0);
			FillRect(MinimapPixels, MinimapTextureSize, P.X, P.Y, 4, 4, bHuman ? MiniUnitHuman : MiniUnitAlien);
		}
	}

	Upload(FogTexture, FogPixels, FogTextureSize);
	Upload(MinimapTexture, MinimapPixels, MinimapTextureSize);
}

void UFogOfWarSubsystem::Upload(UTexture2D* Texture, const TArray<FColor>& Pixels, int32 Size)
{
	// The render thread reads the copy later, so it owns both the region and the pixel buffer.
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Size, Size);
	const int32 Bytes = Pixels.Num() * sizeof(FColor);
	uint8* Copy = static_cast<uint8*>(FMemory::Malloc(Bytes));
	FMemory::Memcpy(Copy, Pixels.GetData(), Bytes);

	Texture->UpdateTextureRegions(0, 1, Region, Size * sizeof(FColor), sizeof(FColor), Copy,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			FMemory::Free(SrcData);
			delete Regions;
		});
}
