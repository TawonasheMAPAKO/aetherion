// Aetherion: The Incipient Engine

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProgenitorEssence.h"
#include "EssenceCore.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 *  A crystallised core of a Progenitor's Essence, shed by the fallen.
 *  Floats and pulses where it drops; drifts to a nearby Warden and attunes them,
 *  unlocking that fusion at the Divine Crucible.
 */
UCLASS()
class AEssenceCore : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* Orb;

	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* Halo;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* OrbMaterial;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* HaloMaterial;

public:

	AEssenceCore();

	/** Which Progenitor Essence this core holds */
	UPROPERTY(EditAnywhere, Category="Essence Core")
	EEssenceFusion Kind = EEssenceFusion::Umbral;

	/** Drops a core of the given kind at a spot, with a little toss */
	static AEssenceCore* Drop(UWorld* World, EEssenceFusion Kind, const FVector& Where);

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category="Essence Core")
	TSoftObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY(EditAnywhere, Category="Essence Core", meta=(Units="cm"))
	float MagnetRange = 550.0f;

	UPROPERTY(EditAnywhere, Category="Essence Core", meta=(Units="cm"))
	float CollectRange = 110.0f;

private:

	float Age = 0.0f;
	FVector Velocity = FVector::ZeroVector;
	FVector RestPoint = FVector::ZeroVector;
	bool bResting = false;

	void Collect(class AWardenCharacter* Warden);
};
