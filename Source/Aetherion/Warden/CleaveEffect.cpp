// Aetherion: The Incipient Engine

#include "CleaveEffect.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

void AetherionFX::BuildArc(UProceduralMeshComponent* Mesh, float InnerRadius, float OuterRadius, float ArcDegrees, int32 Segments)
{
	if (!Mesh)
	{
		return;
	}

	Segments = FMath::Max(Segments, 3);
	const float HalfArc = FMath::DegreesToRadians(FMath::Clamp(ArcDegrees, 1.0f, 360.0f) * 0.5f);

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	for (int32 i = 0; i <= Segments; ++i)
	{
		const float Alpha = static_cast<float>(i) / Segments;
		const float Angle = -HalfArc + 2.0f * HalfArc * Alpha;
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);

		Vertices.Add(Dir * InnerRadius);
		Vertices.Add(Dir * OuterRadius);
		UVs.Add(FVector2D(Alpha, 0.0f));
		UVs.Add(FVector2D(Alpha, 1.0f));
		Normals.Add(FVector::UpVector);
		Normals.Add(FVector::UpVector);
	}

	for (int32 i = 0; i < Segments; ++i)
	{
		const int32 B = i * 2;
		Triangles.Append({ B, B + 3, B + 1 });
		Triangles.Append({ B, B + 2, B + 3 });
	}

	Mesh->ClearAllMeshSections();
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
}

ACleaveEffect::ACleaveEffect()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void ACleaveEffect::Init(UMaterialInterface* GlowMaterial, float Radius, float ArcDegrees, const FLinearColor& Color, float Intensity, float InLifetime)
{
	const bool bFullRing = ArcDegrees >= 359.0f;
	AetherionFX::BuildArc(Mesh, bFullRing ? Radius * 0.55f : Radius * 0.12f, Radius, ArcDegrees, bFullRing ? 72 : 40);

	Lifetime = FMath::Max(InLifetime, 0.05f);
	PeakIntensity = Intensity;
	SetLifeSpan(Lifetime + 0.5f);
	SetActorScale3D(FVector(0.6f, 0.6f, 1.0f));

	if (GlowMaterial)
	{
		Material = Mesh->CreateDynamicMaterialInstance(0, GlowMaterial);
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Material->SetScalarParameterValue(TEXT("Intensity"), PeakIntensity);
		}
	}
}

void ACleaveEffect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Lifetime, 0.0f, 1.0f);

	// fast expand, then hold while it fades
	const float Ease = 1.0f - FMath::Pow(1.0f - T, 3.0f);
	const float Scale = FMath::Lerp(0.6f, 1.05f, Ease);
	SetActorScale3D(FVector(Scale, Scale, 1.0f));

	if (Material)
	{
		Material->SetScalarParameterValue(TEXT("Intensity"), PeakIntensity * FMath::Square(1.0f - T));
	}

	if (Age >= Lifetime)
	{
		Destroy();
	}
}
