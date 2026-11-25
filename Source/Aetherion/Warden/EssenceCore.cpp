// Aetherion: The Incipient Engine

#include "EssenceCore.h"
#include "WardenCharacter.h"
#include "AetherionCombatMemory.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEssenceCore::AEssenceCore()
{
	PrimaryActorTick.bCanEverTick = true;

	Orb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Orb"));
	SetRootComponent(Orb);
	Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Orb->SetCastShadow(false);
	Orb->SetRelativeScale3D(FVector(0.28f));

	Halo = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Halo"));
	Halo->SetupAttachment(Orb);
	Halo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Halo->SetCastShadow(false);
	Halo->SetRelativeScale3D(FVector(2.2f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Orb->SetStaticMesh(Sphere.Object);
		Halo->SetStaticMesh(Sphere.Object);
	}

	GlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Aetherion/FX/M_AetherionGlow.M_AetherionGlow")));
	InitialLifeSpan = 90.0f;
}

AEssenceCore* AEssenceCore::Drop(UWorld* World, EEssenceFusion Kind, const FVector& Where)
{
	if (!World || Kind == EEssenceFusion::None)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEssenceCore* Core = World->SpawnActorDeferred<AEssenceCore>(AEssenceCore::StaticClass(), FTransform(Where), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Core)
	{
		Core->Kind = Kind;
		Core->FinishSpawning(FTransform(Where));
	}
	return Core;
}

void AEssenceCore::BeginPlay()
{
	Super::BeginPlay();

	const FLinearColor Color = AetherionEssence::Color(Kind);
	if (UMaterialInterface* Material = GlowMaterial.LoadSynchronous())
	{
		OrbMaterial = Orb->CreateDynamicMaterialInstance(0, Material);
		HaloMaterial = Halo->CreateDynamicMaterialInstance(0, Material);
		if (OrbMaterial)
		{
			OrbMaterial->SetVectorParameterValue(TEXT("Color"), Color);
			OrbMaterial->SetScalarParameterValue(TEXT("Intensity"), 40.0f);
		}
		if (HaloMaterial)
		{
			HaloMaterial->SetVectorParameterValue(TEXT("Color"), Color);
			HaloMaterial->SetScalarParameterValue(TEXT("Intensity"), 1.5f);
		}
	}

	// tossed up out of the body, settling at chest height
	Velocity = FVector(FMath::FRandRange(-150.0f, 150.0f), FMath::FRandRange(-150.0f, 150.0f), 520.0f);
	RestPoint = GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
}

void AEssenceCore::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	AWardenCharacter* Warden = Cast<AWardenCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const FVector Here = GetActorLocation();

	if (Warden && Age > 0.6f)
	{
		const FVector Chest = Warden->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
		const float Dist = FVector::Dist(Here, Chest);
		if (Dist < CollectRange)
		{
			Collect(Warden);
			return;
		}
		if (Dist < MagnetRange)
		{
			// drift in, faster the closer it gets
			const float Pull = FMath::Lerp(1800.0f, 400.0f, Dist / MagnetRange);
			SetActorLocation(Here + (Chest - Here).GetSafeNormal() * Pull * DeltaSeconds);
			return;
		}
	}

	if (Age < 0.6f)
	{
		Velocity.Z -= 1400.0f * DeltaSeconds;
		SetActorLocation(Here + Velocity * DeltaSeconds);
		if (Age + DeltaSeconds >= 0.6f)
		{
			RestPoint = FVector(GetActorLocation().X, GetActorLocation().Y, RestPoint.Z);
		}
	}
	else
	{
		// bob and pulse
		const FVector Target = RestPoint + FVector(0.0f, 0.0f, FMath::Sin(Age * 2.4f) * 14.0f);
		SetActorLocation(FMath::VInterpTo(Here, Target, DeltaSeconds, 4.0f));
	}

	const float Pulse = 0.5f + 0.5f * FMath::Sin(Age * 4.0f);
	if (OrbMaterial)
	{
		OrbMaterial->SetScalarParameterValue(TEXT("Intensity"), 25.0f + 30.0f * Pulse);
	}
	Halo->SetRelativeScale3D(FVector(2.0f + 0.6f * Pulse));
}

void AEssenceCore::Collect(AWardenCharacter* Warden)
{
	if (UAetherionCombatMemory* Memory = GetWorld()->GetSubsystem<UAetherionCombatMemory>())
	{
		if (Memory->Attune(Kind))
		{
			Memory->Announce(FString::Printf(TEXT("%s ATTUNED"), AetherionEssence::EssenceName(Kind)));
			Memory->Announce(TEXT("Fuse it at the Divine Crucible"));
		}
	}
	Warden->OnEssenceCoreCollected(Kind);
	Destroy();
}
