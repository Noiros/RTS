#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "RTSTypes.h"
#include "ActorScanner.generated.h"

class AUnit;
class ABuilding;
class AResource;

/**
 * World subsystem that maintains live, O(1)-lookup caches of RTS actors by class.
 * Eliminates expensive per-frame / per-timer GetAllActorsOfClass world scans.
 */
UCLASS()
class FERROLITE_API UActorScanner : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void RegisterUnit(AUnit* Unit);
	void UnregisterUnit(AUnit* Unit);
	void RegisterBuilding(ABuilding* Building);
	void UnregisterBuilding(ABuilding* Building);
	void RegisterResource(AResource* Resource);
	void UnregisterResource(AResource* Resource);

	UFUNCTION(BlueprintCallable, Category = "RTS|ActorScanner")
	void GetAllUnits(TArray<AUnit*>& OutUnits);

	UFUNCTION(BlueprintCallable, Category = "RTS|ActorScanner")
	void GetAllBuildings(TArray<ABuilding*>& OutBuildings);

	UFUNCTION(BlueprintCallable, Category = "RTS|ActorScanner")
	void GetAllResources(TArray<AResource*>& OutResources);

	UFUNCTION(BlueprintCallable, Category = "RTS|ActorScanner",
		meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "ActorClass", DynamicOutputParam = "OutActors"))
	static void GetCachedActorsOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RTS|ActorScanner",
		meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "ActorClass"))
	static AActor* GetFirstCachedActorOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass);

	UFUNCTION(BlueprintCallable, Category = "RTS|ActorScanner",
		meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "ActorClass", AdvancedDisplay = "MaxRadius,b2DDistance,IgnoreActor"))
	static AActor* FindNearestCachedActorOfClass(
		const UObject* WorldContextObject,
		TSubclassOf<AActor> ActorClass,
		FVector Origin,
		float& OutDistance,
		float MaxRadius = 0.0f,
		bool b2DDistance = true,
		const AActor* IgnoreActor = nullptr);

	UFUNCTION(BlueprintCallable, Category = "RTS|ActorScanner",
		meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "ActorClass", DynamicOutputParam = "OutActors", AdvancedDisplay = "b2DDistance,IgnoreActor"))
	static void FindCachedActorsOfClassInRadius(
		const UObject* WorldContextObject,
		TSubclassOf<AActor> ActorClass,
		FVector Origin,
		float Radius,
		TArray<AActor*>& OutActors,
		bool b2DDistance = true,
		const AActor* IgnoreActor = nullptr);

	/**
	 * Same rules as BP_UnitBase.FindNearestEnemy: nearest enemy unit (human vs non-human, 3D distance),
	 * else nearest enemy building (raw faction compare, distance minus bounds radius). Null if none within Radius.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTS|ActorScanner", meta = (DefaultToSelf = "Querier"))
	static AActor* FindNearestEnemyForUnit(AActor* Querier, float Radius);

	/** Nearest unit whose Faction differs from Querier's (3D distance), as BP_Building_Turret.TurretScan did. */
	UFUNCTION(BlueprintCallable, Category = "RTS|ActorScanner", meta = (DefaultToSelf = "Querier"))
	static AActor* FindNearestUnitNotOfFaction(AActor* Querier, float Radius);

	/** Reads the Blueprint "Faction" variable (EFaction) of an actor; None if it has none. */
	static EFaction GetActorFaction(const AActor* Actor);

	/** Reads a numeric property of the actor's "Data" asset (e.g. VisionRadius); Default if missing. */
	static double GetDataNumber(const AActor* Actor, FName PropertyName, double Default);

	const TArray<TWeakObjectPtr<AUnit>>& GetUnitsRaw() const { return Units; }
	const TArray<TWeakObjectPtr<ABuilding>>& GetBuildingsRaw() const { return Buildings; }

private:
	static bool IsTrackable(const AActor* Actor);
	void OnActorSpawned(AActor* SpawnedActor);
	void OnLevelAddedToWorld(ULevel* Level, UWorld* World);

	UFUNCTION()
	void HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	void TrackActorForEndPlay(AActor* Actor);
	void AddActorToMatchingBuckets(AActor* Actor);
	void RemoveActorFromAllBuckets(AActor* Actor);
	TArray<TWeakObjectPtr<AActor>>& GetOrSeedBucket(UClass* ActorClass);

	FDelegateHandle ActorSpawnedHandle;
	FDelegateHandle LevelAddedHandle;

	TArray<TWeakObjectPtr<AUnit>> Units;
	TArray<TWeakObjectPtr<ABuilding>> Buildings;
	TArray<TWeakObjectPtr<AResource>> Resources;

	TMap<TObjectKey<UClass>, TArray<TWeakObjectPtr<AActor>>> CachedActorsByClass;
	TSet<TWeakObjectPtr<AActor>> TrackedActors;
};
