// Aetherion: The Incipient Engine

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "WardenHUD.generated.h"

class AWardenCharacter;
class UFont;
class UAetherionCombatMemory;
class AFluxChampion;

/**
 *  The Shard Warden's combat HUD, drawn directly to the canvas:
 *  - Vitality and Essence bars (Essence split into one segment per Cleave)
 *  - Cleave chain pips I, II, III
 *  - Callouts such as "PERFECT III"
 */
UCLASS()
class AWardenHUD : public AHUD
{
	GENERATED_BODY()

public:

	virtual void DrawHUD() override;

	/** Shows a short message in the middle of the screen */
	void PushCallout(const FString& Text, const FLinearColor& Color, float Scale = 1.0f, float Duration = 0.9f);

private:

	struct FCallout
	{
		FString Text;
		FLinearColor Color;
		float Scale = 1.0f;
		float Born = 0.0f;
		float Duration = 1.0f;
	};

	TArray<FCallout> Callouts;

	void DrawStatusPanel(const AWardenCharacter* Warden, float S);
	void DrawCallouts(float S, float Now);
	void DrawFluxMeter(const UAetherionCombatMemory* Memory, float S);

	float ShownAwareness = -1.0f;

	void DrawBossBar(const AFluxChampion* Champion, float S);
	void DrawFusionPrompts(const AWardenCharacter* Warden, const UAetherionCombatMemory* Memory, float S);

	/** Time of day and weather, when the level has a World Director */
	void DrawWorldClock(float S);
	TWeakObjectPtr<class AScarWorldDirector> WorldDirector;
	float NextDirectorSearchAt = 0.0f;
	float ShownBossHealth = -1.0f;
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, int32 Segments);
	void DrawOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness);
};
