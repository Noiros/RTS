#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Unit.generated.h"

UCLASS()
class RTS_API AUnit : public ACharacter
{
	GENERATED_BODY()

public:
	AUnit();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
