#include "Unit.h"
#include "ActorScanner.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

AUnit::AUnit()
{
	// BP_UnitBase drives CombatTick/WorkTick from Event Tick; both only act every 0.2 s, so don't tick every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bEnablePhysicsInteraction = false;
		MoveComp->bRunPhysicsWithNoController = false;
	}
}

void AUnit::BeginPlay()
{
	Super::BeginPlay();

	// Spread units placed in the same frame so their 4 Hz ticks don't all land on one frame.
	SetActorTickInterval(GetActorTickInterval() + FMath::FRandRange(0.0f, 0.05f));

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

