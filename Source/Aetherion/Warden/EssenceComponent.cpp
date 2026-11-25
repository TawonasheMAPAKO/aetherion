// Aetherion: The Incipient Engine

#include "EssenceComponent.h"

UEssenceComponent::UEssenceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEssenceComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetEssence();
}

float UEssenceComponent::GainEssence(float Amount)
{
	if (Amount <= 0.0f)
	{
		return 0.0f;
	}

	const float Before = CurrentEssence;
	CurrentEssence = FMath::Min(MaxEssence, CurrentEssence + Amount);

	if (CurrentEssence != Before)
	{
		OnEssenceChanged.Broadcast(CurrentEssence, MaxEssence);
	}
	return CurrentEssence - Before;
}

bool UEssenceComponent::TrySpendEssence(float Amount)
{
	if (Amount < 0.0f || CurrentEssence < Amount)
	{
		return false;
	}

	CurrentEssence -= Amount;
	OnEssenceChanged.Broadcast(CurrentEssence, MaxEssence);
	return true;
}

void UEssenceComponent::ResetEssence()
{
	CurrentEssence = FMath::Clamp(StartingEssence, 0.0f, MaxEssence);
	OnEssenceChanged.Broadcast(CurrentEssence, MaxEssence);
}
