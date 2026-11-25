// Aetherion: The Incipient Engine

#include "DivineCrucible.h"
#include "AetherionCombatMemory.h"
#include "CleaveEffect.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ADivineCrucible::ADivineCrucible()
{
	PrimaryActorTick.bCanEverTick = true;

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	SetRootComponent(Base);
	Bowl = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bowl"));
	Bowl->SetupAttachment(Base);
	Flame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flame"));
	Flame->SetupAttachment(Base);
	Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flame->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// a squat stone plinth, an inverted cone bowl, and a floating ball of Progenitor fire
	if (Cylinder.Succeeded())
	{
		Base->SetStaticMesh(Cylinder.Object);
		Base->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.9f));
	}
	if (Cone.Succeeded())
	{
		Bowl->SetStaticMesh(Cone.Object);
		Bowl->SetRelativeLocation(FVector(0.0f, 0.0f, 85.0f));
		Bowl->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));
		Bowl->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.6f));
	}
	if (Sphere.Succeeded())
	{
		Flame->SetStaticMesh(Sphere.Object);
		Flame->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
		Flame->SetRelativeScale3D(FVector(0.35f));
	}

	GlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Aetherion/FX/M_AetherionGlow.M_AetherionGlow")));
	StoneMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Aetherion/FX/M_ScarRock.M_ScarRock")));
}

void ADivineCrucible::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInterface* Stone = StoneMaterial.LoadSynchronous())
	{
		Base->SetMaterial(0, Stone);
		Bowl->SetMaterial(0, Stone);
	}
	if (UMaterialInterface* Glow = GlowMaterial.LoadSynchronous())
	{
		FlameMaterial = Flame->CreateDynamicMaterialInstance(0, Glow);
	}
}

EEssenceFusion ADivineCrucible::CycleFusion()
{
	UAetherionCombatMemory* Memory = GetWorld() ? GetWorld()->GetSubsystem<UAetherionCombatMemory>() : nullptr;
	if (!Memory)
	{
		return EEssenceFusion::None;
	}

	// next attuned fusion after the current one, wrapping round to pure Conflict
	const EEssenceFusion Order[] = { EEssenceFusion::None, EEssenceFusion::Umbral, EEssenceFusion::Blood };
	int32 Index = 0;
	for (int32 i = 0; i < 3; ++i)
	{
		if (Order[i] == Memory->GetFusion())
		{
			Index = i;
		}
	}
	for (int32 Step = 1; Step <= 3; ++Step)
	{
		const EEssenceFusion Next = Order[(Index + Step) % 3];
		if (Memory->IsAttuned(Next))
		{
			Memory->SetFusion(Next);
			break;
		}
	}
	Flare();
	return Memory->GetFusion();
}

void ADivineCrucible::Flare()
{
	FlareUntil = Clock + 0.8f;
	UWorld* World = GetWorld();
	UMaterialInterface* Glow = GlowMaterial.LoadSynchronous();
	const UAetherionCombatMemory* Memory = World ? World->GetSubsystem<UAetherionCombatMemory>() : nullptr;
	if (!World || !Glow || !Memory)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ACleaveEffect* Ring = World->SpawnActor<ACleaveEffect>(ACleaveEffect::StaticClass(), GetActorLocation() - FVector(0.0f, 0.0f, 40.0f), FRotator::ZeroRotator, Params))
	{
		Ring->Init(Glow, 600.0f, 360.0f, AetherionEssence::Color(Memory->GetFusion()), 25.0f, 0.8f);
	}
}

void ADivineCrucible::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;

	const UAetherionCombatMemory* Memory = GetWorld() ? GetWorld()->GetSubsystem<UAetherionCombatMemory>() : nullptr;
	const FLinearColor Color = AetherionEssence::Color(Memory ? Memory->GetFusion() : EEssenceFusion::None);

	const float Pulse = 0.5f + 0.5f * FMath::Sin(Clock * 2.0f);
	const float FlareBoost = Clock < FlareUntil ? 3.0f : 1.0f;
	if (FlameMaterial)
	{
		FlameMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		FlameMaterial->SetScalarParameterValue(TEXT("Intensity"), (18.0f + 14.0f * Pulse) * FlareBoost);
	}
	Flame->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f + 8.0f * FMath::Sin(Clock * 1.3f)));
	Flame->SetRelativeScale3D(FVector(0.32f + 0.05f * Pulse) * (Clock < FlareUntil ? 1.4f : 1.0f));

	// a slow ring of light breathes out from the forge
	if (Clock >= NextRingAt)
	{
		NextRingAt = Clock + 2.2f;
		if (UMaterialInterface* Glow = GlowMaterial.LoadSynchronous())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ACleaveEffect* Ring = GetWorld()->SpawnActor<ACleaveEffect>(ACleaveEffect::StaticClass(), GetActorLocation() - FVector(0.0f, 0.0f, 40.0f), FRotator::ZeroRotator, Params))
			{
				Ring->Init(Glow, UseRange, 360.0f, Color, 4.0f, 1.6f);
			}
		}
	}
}
