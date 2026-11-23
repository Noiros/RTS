#include "Unit.h"
#include "ActorScanner.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

AUnit::AUnit()
{
	// BP_UnitBase currently drives CombatTick/WorkTick from Event Tick; keep ticking enabled until moved to timers.
	PrimaryActorTick.bCanEverTick = true;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bEnablePhysicsInteraction = false;
		MoveComp->bRunPhysicsWithNoController = false;
	}
}

void AUnit::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UActorScanner* Scanner = World->GetSubsystem<UActorScanner>())
		{
			Scanner->RegisterUnit(this);
		}
	}
}

void AUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UActorScanner* Scanner = World->GetSubsystem<UActorScanner>())
		{
			Scanner->UnregisterUnit(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AUnit::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

