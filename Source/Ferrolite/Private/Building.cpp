#include "Building.h"
#include "ActorScanner.h"
#include "Engine/World.h"

ABuilding::ABuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ABuilding::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UActorScanner* Scanner = World->GetSubsystem<UActorScanner>())
		{
			Scanner->RegisterBuilding(this);
		}
	}
}

void ABuilding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UActorScanner* Scanner = World->GetSubsystem<UActorScanner>())
		{
			Scanner->UnregisterBuilding(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

