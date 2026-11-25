// Aetherion: The Incipient Engine

#include "EmberField.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEmberField::AEmberField()
{
	PrimaryActorTick.bCanEverTick = true;

	Embers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Embers"));
	SetRootComponent(Embers);
	Embers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Embers->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Embers->SetStaticMesh(Sphere.Object);
	}

	GlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Aetherion/FX/M_AetherionGlow.M_AetherionGlow")));
}

void AEmberField::Respawn(FEmber& Ember, bool bAnywhere) const
{
	Ember.Position = FVector(
		FMath::FRandRange(-Extent.X, Extent.X),
		FMath::FRandRange(-Extent.Y, Extent.Y),
		bAnywhere ? FMath::FRandRange(-Extent.Z, Extent.Z) : -Extent.Z);
	Ember.Speed = FMath::FRandRange(RiseSpeed.X, RiseSpeed.Y);
	Ember.Size = FMath::FRandRange(Size.X, Size.Y);
	Ember.Phase = FMath::FRandRange(0.0f, 2.0f * PI);
	Ember.Sway = FMath::FRandRange(20.0f, 70.0f);
}

void AEmberField::BeginPlay()
{
	Super::BeginPlay();

	Embers->ClearInstances();
	Motes.SetNum(Count);
	Transforms.SetNum(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		Respawn(Motes[i], true);
		Transforms[i] = FTransform(FRotator::ZeroRotator, Motes[i].Position, FVector(Motes[i].Size / 100.0f));
	}
	Embers->AddInstances(Transforms, false);

	if (UMaterialInterface* Material = GlowMaterial.LoadSynchronous())
	{
		EmberMaterial = Embers->CreateDynamicMaterialInstance(0, Material);
		if (EmberMaterial)
		{
			EmberMaterial->SetVectorParameterValue(TEXT("Color"), Color);
			EmberMaterial->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		}
	}
}

void AEmberField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Motes.Num() == 0)
	{
		return;
	}

	Clock += DeltaSeconds;

	for (int32 i = 0; i < Motes.Num(); ++i)
	{
		FEmber& E = Motes[i];
		E.Position.Z += E.Speed * DeltaSeconds;
		if (E.Position.Z > Extent.Z)
		{
			Respawn(E, false);
		}

		// sway sideways and shrink as it rises, like a cooling spark
		const float Height = (E.Position.Z + Extent.Z) / (2.0f * Extent.Z);
		const FVector Drift(
			FMath::Sin(Clock * 0.9f + E.Phase) * E.Sway,
			FMath::Cos(Clock * 0.7f + E.Phase * 1.3f) * E.Sway,
			0.0f);
		const float Scale = E.Size * FMath::Clamp(1.2f - Height, 0.15f, 1.0f) / 100.0f;
		Transforms[i] = FTransform(FRotator::ZeroRotator, E.Position + Drift, FVector(Scale));
	}

	Embers->BatchUpdateInstancesTransforms(0, Transforms, false, true, true);
}
