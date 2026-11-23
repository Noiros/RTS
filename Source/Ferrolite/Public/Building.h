#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Building.generated.h"

UCLASS()
class FERROLITE_API ABuilding : public AActor
{
	GENERATED_BODY()
	
public:	
	ABuilding();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

};
