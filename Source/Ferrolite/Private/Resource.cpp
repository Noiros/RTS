#include "Resource.h"
#include "ActorScanner.h"
#include "Engine/World.h"

AResource::AResource()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AResource::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UActorScanner* Scanner = World->GetSubsystem<UActorScanner>())
		{
			Scanner->RegisterResource(this);
		}
	}
}

void AResource::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UActorScanner* Scanner = World->GetSubsystem<UActorScanner>())
		{
			Scanner->UnregisterResource(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}
