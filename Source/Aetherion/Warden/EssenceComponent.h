// Aetherion: The Incipient Engine

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EssenceComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEssenceChanged, float, CurrentEssence, float, MaxEssence);

/**
 *  Essence: the divine resource harvested in combat and spent on abilities.
 *  Lives on any actor that banks Essence (Wardens now, Sanctums later).
 */
UCLASS(ClassGroup=(Aetherion), meta=(BlueprintSpawnableComponent))
class UEssenceComponent : public UActorComponent
{
	GENERATED_BODY()

protected:

	/** Most Essence this actor can hold */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Essence", meta=(ClampMin=1))
	float MaxEssence = 100.0f;

	/** Essence held when play begins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Essence", meta=(ClampMin=0))
	float StartingEssence = 25.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Essence")
	float CurrentEssence = 0.0f;

public:

	UEssenceComponent();

	/** Fires whenever the amount changes */
	UPROPERTY(BlueprintAssignable, Category="Essence")
	FOnEssenceChanged OnEssenceChanged;

	/** Adds Essence, capped at the maximum. Returns the amount actually gained. */
	UFUNCTION(BlueprintCallable, Category="Essence")
	float GainEssence(float Amount);

	/** Spends Essence only if there is enough. Returns true if it was spent. */
	UFUNCTION(BlueprintCallable, Category="Essence")
	bool TrySpendEssence(float Amount);

	UFUNCTION(BlueprintPure, Category="Essence")
	bool HasEssence(float Amount) const { return CurrentEssence >= Amount; }

	UFUNCTION(BlueprintPure, Category="Essence")
	float GetEssence() const { return CurrentEssence; }

	UFUNCTION(BlueprintPure, Category="Essence")
	float GetMaxEssence() const { return MaxEssence; }

	UFUNCTION(BlueprintPure, Category="Essence")
	float GetEssenceFraction() const { return MaxEssence > 0.0f ? CurrentEssence / MaxEssence : 0.0f; }

	/** Restores the starting amount (used on respawn) */
	UFUNCTION(BlueprintCallable, Category="Essence")
	void ResetEssence();

protected:

	virtual void BeginPlay() override;
};
