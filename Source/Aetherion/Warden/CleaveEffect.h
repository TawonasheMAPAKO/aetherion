// Aetherion: The Incipient Engine

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CleaveEffect.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

namespace AetherionFX
{
	/** Builds a flat arc (or full ring) in the XY plane, facing +X. UV.Y runs 0 at the inner edge to 1 at the outer edge. */
	void BuildArc(UProceduralMeshComponent* Mesh, float InnerRadius, float OuterRadius, float ArcDegrees, int32 Segments);
}

/**
 *  A short-lived glowing arc of fire on the ground, spawned by each Inferno Cleave.
 *  Expands outward and fades. Needs no art assets: the shape is generated and the
 *  look comes from the M_AetherionGlow material (Color, Intensity parameters).
 */
UCLASS()
class ACleaveEffect : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	UProceduralMeshComponent* Mesh;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* Material;

	float Age = 0.0f;
	float Lifetime = 0.4f;
	float PeakIntensity = 10.0f;

public:

	ACleaveEffect();

	/** Shapes and colors the effect. Call right after spawning. */
	void Init(UMaterialInterface* GlowMaterial, float Radius, float ArcDegrees, const FLinearColor& Color, float Intensity, float InLifetime);

	virtual void Tick(float DeltaSeconds) override;
};
