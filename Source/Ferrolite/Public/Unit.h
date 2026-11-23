#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Unit.generated.h"

UCLASS()
class FERROLITE_API AUnit : public ACharacter
{
	GENERATED_BODY()

public:
	AUnit();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
