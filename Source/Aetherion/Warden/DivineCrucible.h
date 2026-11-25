// Aetherion: The Incipient Engine

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProgenitorEssence.h"
#include "DivineCrucible.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 *  A Divine Crucible: a forge of the Progenitors where a Warden fuses a second
 *  Essence into their Conflict Essence. Stand close and press Interact to cycle
 *  through the fusions you have attuned to. The flame takes the fusion's colour.
 */
UCLASS()
class ADivineCrucible : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* Base;

	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* Bowl;

	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* Flame;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* FlameMaterial;

public:

	ADivineCrucible();

	/** How close a Warden must stand to use it */
	UPROPERTY(EditAnywhere, Category="Crucible", meta=(Units="cm"))
	float UseRange = 280.0f;

	/** Moves to the next attuned fusion. Returns the one now active. */
	EEssenceFusion CycleFusion();

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category="Crucible")
	TSoftObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY(EditAnywhere, Category="Crucible")
	TSoftObjectPtr<UMaterialInterface> StoneMaterial;

private:

	float Clock = 0.0f;
	float NextRingAt = 0.0f;
	float FlareUntil = 0.0f;

	void Flare();
};
