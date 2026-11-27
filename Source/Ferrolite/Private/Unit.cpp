#include "Unit.h"
#include "ActorScanner.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "Navigation/CrowdFollowingComponent.h"

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

	// Done here (not ctor) because BP_UnitBase's CDO overrides constructor defaults.
	// RVO fights DetourCrowd (AIC_UnitBase) and causes bouncing; crowd does the avoidance.
	GetCharacterMovement()->SetAvoidanceEnabled(false);
	// Pawn-vs-pawn blocking makes units shove each other; spacing comes from crowd separation instead.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

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

void AUnit::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	const AAIController* AIC = Cast<AAIController>(NewController);
	if (UCrowdFollowingComponent* Crowd = AIC ? Cast<UCrowdFollowingComponent>(AIC->GetPathFollowingComponent()) : nullptr)
	{
		Crowd->SetCrowdSeparation(true);
		Crowd->SetCrowdSeparationWeight(4.f);
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::High);
	}
}

void AUnit::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

