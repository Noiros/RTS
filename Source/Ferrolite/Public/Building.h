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

	/** Placement preview: not tracked by ActorScanner, so it never reveals fog, gets health bars or is targeted. */
	UFUNCTION(BlueprintCallable, Category = "RTS|Building")
	void SetGhost(bool bInGhost);

	UFUNCTION(BlueprintPure, Category = "RTS|Building")
	bool IsGhost() const { return bIsGhost; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "RTS|Building")
	bool bIsGhost = false;
};
