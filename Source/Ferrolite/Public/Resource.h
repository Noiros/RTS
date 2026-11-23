#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Resource.generated.h"

UCLASS()
class FERROLITE_API AResource : public AActor
{
	GENERATED_BODY()

public:
	AResource();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
