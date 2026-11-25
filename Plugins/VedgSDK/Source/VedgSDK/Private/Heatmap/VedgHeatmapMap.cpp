// Fill out your copyright notice in the Description page of Project Settings.

#include "Heatmap/VedgHeatmapMap.h"
#include "Heatmap/VedgHeatmapCapture.h"
#include "Http/VedgHttpRequest.h"
#include "VedgSDKModule.h"
#include "VedgSDKSettings.h"
#include "VedgSDKSubsystem.h"
#include "Components/BoxComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/ScopedSlowTask.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "VedgHeatmapMap"

TFunction<void(TFunction<void(const FString&)>)> AVedgHeatmapMap::HeatmapKeyProvider;

namespace
{
	// The API's MAX_HEATMAP_MAP_VOXELS.
	constexpr int32 MaxUploadVoxels = 200000;
	constexpr int64 QueriesPerProgressUpdate = 10000;
}

AVedgHeatmapMap::AVedgHeatmapMap()
{
	PrimaryActorTick.bCanEverTick = false;

	ScanBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ScanBounds"));
	ScanBounds->SetBoxExtent(FVector(5000.f));
	ScanBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ScanBounds->SetGenerateOverlapEvents(false);
	ScanBounds->SetHiddenInGame(true);
	RootComponent = ScanBounds;

	// WorldStatic + WorldDynamic with the default collision profiles.
	CollisionObjectTypes = {ObjectTypeQuery1, ObjectTypeQuery2};
}

bool AVedgHeatmapMap::IsRecordingAllowed(FString* OutReason) const
{
	FString Reason;
	if (!bEnabled)
	{
		Reason = TEXT("bEnabled is false");
	}
	else if (IsRunningDedicatedServer() || GetNetMode() == NM_DedicatedServer)
	{
		Reason = TEXT("dedicated server - the heatmap records from each client's own local player");
	}
	else if (bRequireReleaseBuild && !UE_BUILD_SHIPPING)
	{
		Reason = TEXT("not a Shipping build - untick bRequireReleaseBuild to also record PIE / Development runs");
	}
	if (OutReason)
	{
		*OutReason = Reason;
	}
	return Reason.IsEmpty();
}

void AVedgHeatmapMap::BeginPlay()
{
	Super::BeginPlay();

	FString Reason;
	if (!IsRecordingAllowed(&Reason))
	{
		UE_LOG(LogVedgSDK, Log, TEXT("VedgHeatmapMap: not recording (%s)"), *Reason);
		return;
	}
	if (MapId.IsEmpty())
	{
		UE_LOG(LogVedgSDK, Warning, TEXT("VedgHeatmapMap: MapId is empty, not recording"));
		return;
	}
	const UVedgSDKSubsystem* Sdk = UVedgSDKSubsystem::Get(this);
	if (!Sdk || !Sdk->GetHeatmap())
	{
		return;
	}

	UE_LOG(LogVedgSDK, Log, TEXT("VedgHeatmapMap: recording started for map \"%s\""), *MapId);
	Sdk->GetHeatmap()->StartCapture(MapId, CellSize, MapVersion);
	bRecording = true;
}

void AVedgHeatmapMap::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bRecording)
	{
		bRecording = false;
		const UVedgSDKSubsystem* Sdk = UVedgSDKSubsystem::Get(this);
		if (Sdk && Sdk->GetHeatmap())
		{
			if (EndPlayReason == EEndPlayReason::Quit)
			{
				Sdk->GetHeatmap()->RecordEvent(TEXT("app_quit"));
			}
			Sdk->GetHeatmap()->EndCapture();
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AVedgHeatmapMap::VoxelizeMap()
{
#if WITH_EDITOR
	if (MapId.IsEmpty())
	{
		UE_LOG(LogVedgSDK, Error, TEXT("VedgHeatmapMap: set MapId before voxelizing"));
		return;
	}
	if (!HeatmapKeyProvider)
	{
		UE_LOG(LogVedgSDK, Error, TEXT("VedgHeatmapMap: the VedgSDKEditor module is not loaded"));
		return;
	}

	TWeakObjectPtr<AVedgHeatmapMap> WeakThis(this);
	HeatmapKeyProvider([WeakThis](const FString& Key)
	{
		AVedgHeatmapMap* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (Key.IsEmpty())
		{
			UE_LOG(LogVedgSDK, Error, TEXT("VedgHeatmapMap: no heatmap key - log in from Window > Vedg"));
			return;
		}

		TArray<FIntVector> Voxels;
		if (!Self->Voxelize(Voxels))
		{
			return;
		}
		if (Voxels.Num() == 0)
		{
			UE_LOG(LogVedgSDK, Error, TEXT("VedgHeatmapMap: found no collision inside ScanBounds - check its extent and CollisionObjectTypes"));
			return;
		}
		if (Voxels.Num() > MaxUploadVoxels)
		{
			UE_LOG(LogVedgSDK, Error, TEXT("VedgHeatmapMap: %d voxels, the API accepts at most %d - raise CellSize or shrink ScanBounds"), Voxels.Num(), MaxUploadVoxels);
			return;
		}
		Self->UploadVoxels(Voxels, Key);
	});
#endif
}

#if WITH_EDITOR
bool AVedgHeatmapMap::Voxelize(TArray<FIntVector>& OutVoxels) const
{
	UWorld* World = GetWorld();
	const FCollisionObjectQueryParams ObjectParams(CollisionObjectTypes);
	if (!World || !ObjectParams.IsValid())
	{
		UE_LOG(LogVedgSDK, Error, TEXT("VedgHeatmapMap: no world or no CollisionObjectTypes to scan"));
		return false;
	}

	// Same voxel indexing as UVedgHeatmapCapture::VoxelOf, so traces land on the map's voxels.
	const FBox Bounds = ScanBounds->Bounds.GetBox();
	const FIntVector MinCell(FMath::FloorToInt(Bounds.Min.X / CellSize), FMath::FloorToInt(Bounds.Min.Y / CellSize), FMath::FloorToInt(Bounds.Min.Z / CellSize));
	const FIntVector MaxCell(FMath::FloorToInt(Bounds.Max.X / CellSize), FMath::FloorToInt(Bounds.Max.Y / CellSize), FMath::FloorToInt(Bounds.Max.Z / CellSize));
	const FIntVector CellCount = MaxCell - MinCell + FIntVector(1);

	int32 OctreeSize = 1;
	while (OctreeSize < CellCount.GetMax())
	{
		OctreeSize *= 2;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VedgHeatmapVoxelize));
	QueryParams.AddIgnoredActor(this);

	FScopedSlowTask Task(0, LOCTEXT("Voxelizing", "Voxelizing the map..."));
	Task.MakeDialog(true);

	int64 Queries = 0;
	bool bCancelled = false;
	// Octree descent: one box overlap per node, children only under occupied nodes.
	TFunction<void(const FIntVector&, int32)> Scan = [&](const FIntVector& Cell, int32 Size)
	{
		if (bCancelled)
		{
			return;
		}
		if (++Queries % QueriesPerProgressUpdate == 0)
		{
			Task.EnterProgressFrame(0, FText::Format(LOCTEXT("Progress", "Voxelizing the map... {0} queries, {1} occupied"), Queries, OutVoxels.Num()));
			if (Task.ShouldCancel())
			{
				bCancelled = true;
				return;
			}
		}

		const FVector Center = (FVector(Cell) + Size * 0.5) * CellSize;
		// Slightly shrunk so geometry merely touching a cell's face doesn't mark it.
		const FVector HalfExtent(Size * CellSize * 0.5 - 0.1);
		if (!World->OverlapAnyTestByObjectType(Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeBox(HalfExtent), QueryParams))
		{
			return;
		}
		if (Size == 1)
		{
			OutVoxels.Add(Cell);
			return;
		}
		const int32 Child = Size / 2;
		for (int32 X : {0, Child})
		{
			for (int32 Y : {0, Child})
			{
				for (int32 Z : {0, Child})
				{
					Scan(Cell + FIntVector(X, Y, Z), Child);
				}
			}
		}
	};
	Scan(MinCell, OctreeSize);

	if (bCancelled)
	{
		UE_LOG(LogVedgSDK, Warning, TEXT("VedgHeatmapMap: voxelization cancelled"));
		return false;
	}
	UE_LOG(LogVedgSDK, Log, TEXT("VedgHeatmapMap: %lld physics queries, %d occupied voxels"), Queries, OutVoxels.Num());
	return true;
}

void AVedgHeatmapMap::UploadVoxels(const TArray<FIntVector>& Voxels, const FString& Key) const
{
	const UVedgSDKSettings* Settings = UVedgSDKSettings::Get();

	const TSharedRef<FJsonObject> Origin = MakeShared<FJsonObject>();
	Origin->SetNumberField(TEXT("x"), 0);
	Origin->SetNumberField(TEXT("y"), 0);
	Origin->SetNumberField(TEXT("z"), 0);
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game"), Settings->GetGameSlug());
	Body->SetStringField(TEXT("map_id"), MapId);
	Body->SetNumberField(TEXT("cell_size"), CellSize);
	Body->SetObjectField(TEXT("origin"), Origin);

	// Voxels appended by hand: up to 200k triplets, far cheaper than a FJsonValue per number.
	FString Json;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json));
	Json.LeftChopInline(1);
	Json.Reserve(Json.Len() + Voxels.Num() * 24);
	Json += TEXT(",\"voxels\":[");
	for (int32 i = 0; i < Voxels.Num(); ++i)
	{
		Json += FString::Printf(TEXT("%s[%d,%d,%d]"), i > 0 ? TEXT(",") : TEXT(""), Voxels[i].X, Voxels[i].Y, Voxels[i].Z);
	}
	Json += TEXT("]}");

	const FString Id = MapId;
	const int32 Count = Voxels.Num();
	FVedgHttpRequest::Request(Settings->GetApiUrl() + TEXT("/reports/heatmap/maps"), TEXT("POST"), Json, [Id, Count](FVedgHttpResult Result)
	{
		if (Result.bValid)
		{
			UE_LOG(LogVedgSDK, Log, TEXT("VedgHeatmapMap: uploaded %d voxels for map \"%s\""), Count, *Id);
			return;
		}
		FString Error;
		if (Result.ResponseObject.IsValid())
		{
			Result.ResponseObject->TryGetStringField(TEXT("error"), Error);
		}
		UE_LOG(LogVedgSDK, Error, TEXT("VedgHeatmapMap: upload failed (%d) %s"), Result.StatusCode, *Error);
	}, 60.f, FString(), {{TEXT("X-Heatmap-Key"), Key}});
}
#endif

#undef LOCTEXT_NAMESPACE
