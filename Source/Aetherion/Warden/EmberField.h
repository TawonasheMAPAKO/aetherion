// Aetherion: The Incipient Engine

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmberField.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 *  Drifting embers rising out of Serrath's Scar.
 *  Hundreds of tiny glowing motes in a box around the actor, each rising, swaying
 *  and fading back in at the bottom. Drawn as one instanced mesh, so it is cheap.
 */
UCLASS()
class AEmberField : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	UInstancedStaticMeshComponent* Embers;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* EmberMaterial;

protected:

	UPROPERTY(EditAnywhere, Category="Embers", meta=(ClampMin=1, ClampMax=3000))
	int32 Count = 600;

	/** Half-size of the box the embers live in */
	UPROPERTY(EditAnywhere, Category="Embers", meta=(Units="cm"))
	FVector Extent = FVector(3500.0f, 3500.0f, 900.0f);

	UPROPERTY(EditAnywhere, Category="Embers", meta=(Units="cm/s"))
	FVector2D RiseSpeed = FVector2D(40.0f, 140.0f);

	UPROPERTY(EditAnywhere, Category="Embers", meta=(Units="cm"))
	FVector2D Size = FVector2D(2.0f, 6.0f);

	UPROPERTY(EditAnywhere, Category="Embers")
	FLinearColor Color = FLinearColor(1.0f, 0.32f, 0.06f);

	UPROPERTY(EditAnywhere, Category="Embers", meta=(ClampMin=0))
	float Intensity = 40.0f;

	/** Additive glow material (created by Scripts/setup_feedback.py) */
	UPROPERTY(EditAnywhere, Category="Embers")
	TSoftObjectPtr<UMaterialInterface> GlowMaterial;

public:

	AEmberField();

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

private:

	struct FEmber
	{
		FVector Position;
		float Speed = 0.0f;
		float Size = 1.0f;
		float Phase = 0.0f;
		float Sway = 0.0f;
	};

	TArray<FEmber> Motes;
	TArray<FTransform> Transforms;
	float Clock = 0.0f;

	void Respawn(FEmber& Ember, bool bAnywhere) const;
};
