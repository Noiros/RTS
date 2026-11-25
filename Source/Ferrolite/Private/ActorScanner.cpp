#include "ActorScanner.h"

#include "Building.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Resource.h"
#include "Unit.h"

namespace ActorScannerPrivate
{
	template <typename TActor>
	void CollectValidActors(TArray<TWeakObjectPtr<TActor>>& Source, TArray<TActor*>& OutActors)
	{
		OutActors.Reset(Source.Num());
		for (int32 Index = Source.Num() - 1; Index >= 0; --Index)
		{
			if (TActor* Actor = Source[Index].Get())
			{
				if (IsValid(Actor) && !Actor->IsActorBeingDestroyed())
				{
					OutActors.Add(Actor);
					continue;
				}
			}
			Source.RemoveAtSwap(Index, EAllowShrinking::No);
		}
	}

	UWorld* ResolveWorld(const UObject* WorldContextObject)
	{
		return GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	}
}

void UActorScanner::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		if (World->IsGameWorld())
		{
			ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
				FOnActorSpawned::FDelegate::CreateUObject(this, &UActorScanner::OnActorSpawned));
			LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(
				this, &UActorScanner::OnLevelAddedToWorld);
		}
	}
}

void UActorScanner::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (ActorSpawnedHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
			ActorSpawnedHandle.Reset();
		}
	}

	if (LevelAddedHandle.IsValid())
	{
		FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
		LevelAddedHandle.Reset();
	}

	Units.Reset();
	Buildings.Reset();
	Resources.Reset();
	CachedActorsByClass.Reset();
	TrackedActors.Reset();

	Super::Deinitialize();
}

void UActorScanner::RegisterUnit(AUnit* Unit)
{
	if (!IsValid(Unit))
	{
		return;
	}
	Units.AddUnique(Unit);
	AddActorToMatchingBuckets(Unit);
}

void UActorScanner::UnregisterUnit(AUnit* Unit)
{
	if (!Unit)
	{
		return;
	}
	Units.RemoveSwap(Unit, EAllowShrinking::No);
	RemoveActorFromAllBuckets(Unit);
}

void UActorScanner::RegisterBuilding(ABuilding* Building)
{
	if (!IsValid(Building) || Building->IsGhost())
	{
		return;
	}
	Buildings.AddUnique(Building);
	AddActorToMatchingBuckets(Building);
}

void UActorScanner::UnregisterBuilding(ABuilding* Building)
{
	if (!Building)
	{
		return;
	}
	Buildings.RemoveSwap(Building, EAllowShrinking::No);
	RemoveActorFromAllBuckets(Building);
}

void UActorScanner::RegisterResource(AResource* Resource)
{
	if (!IsValid(Resource))
	{
		return;
	}
	Resources.AddUnique(Resource);
	AddActorToMatchingBuckets(Resource);
}

void UActorScanner::UnregisterResource(AResource* Resource)
{
	if (!Resource)
	{
		return;
	}
	Resources.RemoveSwap(Resource, EAllowShrinking::No);
	RemoveActorFromAllBuckets(Resource);
}

void UActorScanner::GetAllUnits(TArray<AUnit*>& OutUnits)
{
	ActorScannerPrivate::CollectValidActors(Units, OutUnits);
}

void UActorScanner::GetAllBuildings(TArray<ABuilding*>& OutBuildings)
{
	ActorScannerPrivate::CollectValidActors(Buildings, OutBuildings);
}

void UActorScanner::GetAllResources(TArray<AResource*>& OutResources)
{
	ActorScannerPrivate::CollectValidActors(Resources, OutResources);
}

void UActorScanner::GetCachedActorsOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors)
{
	OutActors.Reset();

	UClass* ClassPtr = ActorClass.Get();
	UWorld* World = ClassPtr ? ActorScannerPrivate::ResolveWorld(WorldContextObject) : nullptr;
	if (!World)
	{
		return;
	}

	if (!World->IsGameWorld())
	{
		UGameplayStatics::GetAllActorsOfClass(WorldContextObject, ActorClass, OutActors);
		return;
	}

	UActorScanner* Scanner = World->GetSubsystem<UActorScanner>();
	if (!Scanner)
	{
		UGameplayStatics::GetAllActorsOfClass(WorldContextObject, ActorClass, OutActors);
		return;
	}

	ActorScannerPrivate::CollectValidActors(Scanner->GetOrSeedBucket(ClassPtr), OutActors);
}

AActor* UActorScanner::GetFirstCachedActorOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass)
{
	UClass* ClassPtr = ActorClass.Get();
	UWorld* World = ClassPtr ? ActorScannerPrivate::ResolveWorld(WorldContextObject) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	if (!World->IsGameWorld())
	{
		return UGameplayStatics::GetActorOfClass(WorldContextObject, ActorClass);
	}

	UActorScanner* Scanner = World->GetSubsystem<UActorScanner>();
	if (!Scanner)
	{
		return UGameplayStatics::GetActorOfClass(WorldContextObject, ActorClass);
	}

	TArray<TWeakObjectPtr<AActor>>& Bucket = Scanner->GetOrSeedBucket(ClassPtr);
	for (int32 Index = Bucket.Num() - 1; Index >= 0; --Index)
	{
		if (AActor* Actor = Bucket[Index].Get())
		{
			if (IsValid(Actor) && !Actor->IsActorBeingDestroyed())
			{
				return Actor;
			}
		}
		Bucket.RemoveAtSwap(Index, EAllowShrinking::No);
	}

	return nullptr;
}

AActor* UActorScanner::FindNearestCachedActorOfClass(
	const UObject* WorldContextObject,
	TSubclassOf<AActor> ActorClass,
	FVector Origin,
	float& OutDistance,
	float MaxRadius,
	bool b2DDistance,
	const AActor* IgnoreActor)
{
	OutDistance = 0.0f;

	UClass* ClassPtr = ActorClass.Get();
	UWorld* World = ClassPtr ? ActorScannerPrivate::ResolveWorld(WorldContextObject) : nullptr;
	UActorScanner* Scanner = (World && World->IsGameWorld()) ? World->GetSubsystem<UActorScanner>() : nullptr;
	if (!Scanner)
	{
		return nullptr;
	}

	TArray<TWeakObjectPtr<AActor>>& Bucket = Scanner->GetOrSeedBucket(ClassPtr);

	double BestDistSq = (MaxRadius > 0.0f)
		? static_cast<double>(MaxRadius) * static_cast<double>(MaxRadius)
		: TNumericLimits<double>::Max();
	AActor* BestActor = nullptr;

	for (int32 Index = Bucket.Num() - 1; Index >= 0; --Index)
	{
		AActor* Actor = Bucket[Index].Get();
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
		{
			Bucket.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}
		if (Actor == IgnoreActor)
		{
			continue;
		}

		const FVector Loc = Actor->GetActorLocation();
		const double DistSq = b2DDistance ? FVector::DistSquared2D(Origin, Loc) : FVector::DistSquared(Origin, Loc);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestActor = Actor;
		}
	}

	if (BestActor)
	{
		OutDistance = static_cast<float>(FMath::Sqrt(BestDistSq));
	}

	return BestActor;
}

void UActorScanner::FindCachedActorsOfClassInRadius(
	const UObject* WorldContextObject,
	TSubclassOf<AActor> ActorClass,
	FVector Origin,
	float Radius,
	TArray<AActor*>& OutActors,
	bool b2DDistance,
	const AActor* IgnoreActor)
{
	OutActors.Reset();

	UClass* ClassPtr = ActorClass.Get();
	if (!ClassPtr || Radius <= 0.0f)
	{
		return;
	}

	UWorld* World = ActorScannerPrivate::ResolveWorld(WorldContextObject);
	UActorScanner* Scanner = (World && World->IsGameWorld()) ? World->GetSubsystem<UActorScanner>() : nullptr;
	if (!Scanner)
	{
		return;
	}

	TArray<TWeakObjectPtr<AActor>>& Bucket = Scanner->GetOrSeedBucket(ClassPtr);
	const double RadiusSq = static_cast<double>(Radius) * static_cast<double>(Radius);

	for (int32 Index = Bucket.Num() - 1; Index >= 0; --Index)
	{
		AActor* Actor = Bucket[Index].Get();
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
		{
			Bucket.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}
		if (Actor == IgnoreActor)
		{
			continue;
		}

		const FVector Loc = Actor->GetActorLocation();
		const double DistSq = b2DDistance ? FVector::DistSquared2D(Origin, Loc) : FVector::DistSquared(Origin, Loc);
		if (DistSq <= RadiusSq)
		{
			OutActors.Add(Actor);
		}
	}
}

void UActorScanner::OnActorSpawned(AActor* SpawnedActor)
{
	// Units/Buildings/Resources register themselves in BeginPlay; here we only feed the per-class buckets.
	AddActorToMatchingBuckets(SpawnedActor);
}

void UActorScanner::OnLevelAddedToWorld(ULevel* Level, UWorld* World)
{
	if (!Level || World != GetWorld())
	{
		return;
	}

	for (AActor* Actor : Level->Actors)
	{
		if (IsValid(Actor))
		{
			OnActorSpawned(Actor);
		}
	}
}

void UActorScanner::HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	if (!Actor)
	{
		return;
	}

	// Typed arrays are cleaned by the actors' own EndPlay (Unregister*).
	RemoveActorFromAllBuckets(Actor);
}

void UActorScanner::TrackActorForEndPlay(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	bool bAlreadyTracked = false;
	TrackedActors.Add(Actor, &bAlreadyTracked);
	if (!bAlreadyTracked)
	{
		Actor->OnEndPlay.AddUniqueDynamic(this, &UActorScanner::HandleActorEndPlay);
	}
}

void UActorScanner::AddActorToMatchingBuckets(AActor* Actor)
{
	// Already tracked means already in every matching bucket (later buckets are seeded from the world).
	if (!IsTrackable(Actor) || TrackedActors.Contains(Actor))
	{
		return;
	}

	bool bMatchedAny = false;
	for (TPair<TObjectKey<UClass>, TArray<TWeakObjectPtr<AActor>>>& Pair : CachedActorsByClass)
	{
		if (UClass* BucketClass = Pair.Key.ResolveObjectPtr())
		{
			if (Actor->IsA(BucketClass))
			{
				Pair.Value.Add(Actor);
				bMatchedAny = true;
			}
		}
	}

	if (bMatchedAny)
	{
		TrackActorForEndPlay(Actor);
	}
}

void UActorScanner::RemoveActorFromAllBuckets(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	TrackedActors.Remove(Actor);

	for (TPair<TObjectKey<UClass>, TArray<TWeakObjectPtr<AActor>>>& Pair : CachedActorsByClass)
	{
		if (UClass* BucketClass = Pair.Key.ResolveObjectPtr())
		{
			if (Actor->IsA(BucketClass))
			{
				Pair.Value.RemoveSwap(Actor, EAllowShrinking::No);
			}
		}
	}
}

TArray<TWeakObjectPtr<AActor>>& UActorScanner::GetOrSeedBucket(UClass* ActorClass)
{
	const TObjectKey<UClass> Key(ActorClass);
	if (TArray<TWeakObjectPtr<AActor>>* Existing = CachedActorsByClass.Find(Key))
	{
		return *Existing;
	}

	TArray<TWeakObjectPtr<AActor>>& NewBucket = CachedActorsByClass.Add(Key);

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World, ActorClass); It; ++It)
		{
			AActor* Actor = *It;
			if (IsTrackable(Actor) && !Actor->IsActorBeingDestroyed())
			{
				NewBucket.Add(Actor);
				TrackActorForEndPlay(Actor);
			}
		}
	}

	return NewBucket;
}

bool UActorScanner::IsTrackable(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}
	const ABuilding* Building = Cast<ABuilding>(Actor);
	return !(Building && Building->IsGhost());
}

EFaction UActorScanner::GetActorFaction(const AActor* Actor)
{
	// shortcut: Faction is a Blueprint variable read by reflection; move it to a C++ UPROPERTY on AUnit/ABuilding if this shows up in profiles.
	if (!Actor)
	{
		return EFaction::None;
	}
	const FProperty* Prop = Actor->GetClass()->FindPropertyByName(TEXT("Faction"));
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		return static_cast<EFaction>(EnumProp->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Actor)));
	}
	if (const FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
	{
		return static_cast<EFaction>(ByteProp->GetPropertyValue_InContainer(Actor));
	}
	return EFaction::None;
}

double UActorScanner::GetDataNumber(const AActor* Actor, FName PropertyName, double Default)
{
	const FObjectPropertyBase* DataProp = Actor ? CastField<FObjectPropertyBase>(Actor->GetClass()->FindPropertyByName(TEXT("Data"))) : nullptr;
	const UObject* Data = DataProp ? DataProp->GetObjectPropertyValue_InContainer(Actor) : nullptr;
	if (!Data)
	{
		return Default;
	}
	const FNumericProperty* NumProp = CastField<FNumericProperty>(Data->GetClass()->FindPropertyByName(PropertyName));
	if (!NumProp)
	{
		return Default;
	}
	const void* Value = NumProp->ContainerPtrToValuePtr<void>(Data);
	return NumProp->IsFloatingPoint() ? NumProp->GetFloatingPointPropertyValue(Value) : static_cast<double>(NumProp->GetSignedIntPropertyValue(Value));
}

AActor* UActorScanner::FindNearestEnemyForUnit(AActor* Querier, float Radius)
{
	UWorld* World = Querier ? Querier->GetWorld() : nullptr;
	UActorScanner* Scanner = World ? World->GetSubsystem<UActorScanner>() : nullptr;
	if (!Scanner)
	{
		return nullptr;
	}

	const EFaction SelfFaction = GetActorFaction(Querier);
	const bool bSelfHuman = SelfFaction == EFaction::Human;
	double BestDist = Radius;
	AActor* Best = nullptr;

	for (const TWeakObjectPtr<AUnit>& WeakUnit : Scanner->Units)
	{
		AUnit* Unit = WeakUnit.Get();
		if (!IsValid(Unit) || Unit->IsActorBeingDestroyed() || (GetActorFaction(Unit) == EFaction::Human) == bSelfHuman)
		{
			continue;
		}
		const double Dist = Querier->GetDistanceTo(Unit);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Unit;
		}
	}
	if (Best)
	{
		return Best;
	}

	for (const TWeakObjectPtr<ABuilding>& WeakBuilding : Scanner->Buildings)
	{
		ABuilding* Building = WeakBuilding.Get();
		if (!IsValid(Building) || Building->IsActorBeingDestroyed() || GetActorFaction(Building) == SelfFaction)
		{
			continue;
		}
		FVector Origin, Extent;
		Building->GetActorBounds(true, Origin, Extent);
		const double Dist = Querier->GetDistanceTo(Building) - FMath::Max(Extent.X, Extent.Y);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Building;
		}
	}
	return Best;
}

AActor* UActorScanner::FindNearestUnitNotOfFaction(AActor* Querier, float Radius)
{
	UWorld* World = Querier ? Querier->GetWorld() : nullptr;
	UActorScanner* Scanner = World ? World->GetSubsystem<UActorScanner>() : nullptr;
	if (!Scanner)
	{
		return nullptr;
	}

	const EFaction SelfFaction = GetActorFaction(Querier);
	double BestDist = Radius;
	AActor* Best = nullptr;
	for (const TWeakObjectPtr<AUnit>& WeakUnit : Scanner->Units)
	{
		AUnit* Unit = WeakUnit.Get();
		if (!IsValid(Unit) || Unit->IsActorBeingDestroyed() || GetActorFaction(Unit) == SelfFaction)
		{
			continue;
		}
		const double Dist = Querier->GetDistanceTo(Unit);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Unit;
		}
	}
	return Best;
}
