// Shared RTS enums. In C++ only because the editor tooling cannot author Blueprint enums;
// all gameplay logic using them lives in Blueprint.

#pragma once

#include "CoreMinimal.h"
#include "RTSTypes.generated.h"

UENUM(BlueprintType)
enum class EFaction : uint8
{
	None,
	Human UMETA(DisplayName = "Humain"),
	Alien
};

UENUM(BlueprintType)
enum class EResource : uint8
{
	Ore UMETA(DisplayName = "Minerai"),
	Ice UMETA(DisplayName = "Glace"),
	Crystal UMETA(DisplayName = "Cristaux"),
	Alloy UMETA(DisplayName = "Alliage"),
	Biomass UMETA(DisplayName = "Biomasse")
};

UENUM(BlueprintType)
enum class EOrderType : uint8
{
	None,
	Move,
	Harvest,
	Build,
	Attack,
	Pickup
};

UENUM(BlueprintType)
enum class EBuildingState : uint8
{
	Ghost,
	Construction,
	Active,
	Destroyed
};
