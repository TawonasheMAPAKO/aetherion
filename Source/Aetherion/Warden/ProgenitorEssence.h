// Aetherion: The Incipient Engine

#pragma once

#include "CoreMinimal.h"
#include "ProgenitorEssence.generated.h"

/**
 *  A second Progenitor's Essence fused into the Warden's Conflict Essence.
 *  Each fusion transforms Inferno Cleave.
 */
UENUM(BlueprintType)
enum class EEssenceFusion : uint8
{
	None		UMETA(DisplayName = "Pure Conflict"),
	Umbral		UMETA(DisplayName = "Umbral (Conflict + Shadow)"),
	Blood		UMETA(DisplayName = "Blood (Conflict + Vitality)"),
};

namespace AetherionEssence
{
	/** Name of the Cleave a fusion produces */
	inline const TCHAR* CleaveName(EEssenceFusion Fusion)
	{
		switch (Fusion)
		{
		case EEssenceFusion::Umbral: return TEXT("UMBRAL CLEAVE");
		case EEssenceFusion::Blood: return TEXT("BLOOD CLEAVE");
		default: return TEXT("INFERNO CLEAVE");
		}
	}

	/** Name of the Progenitor Essence a fusion uses */
	inline const TCHAR* EssenceName(EEssenceFusion Fusion)
	{
		switch (Fusion)
		{
		case EEssenceFusion::Umbral: return TEXT("SHADOW ESSENCE (VORYNTH)");
		case EEssenceFusion::Blood: return TEXT("VITALITY ESSENCE (ERYTHRA)");
		default: return TEXT("CONFLICT ESSENCE (SERRATH)");
		}
	}

	/** Signature glow colour (linear, for emissive) */
	inline FLinearColor Color(EEssenceFusion Fusion)
	{
		switch (Fusion)
		{
		case EEssenceFusion::Umbral: return FLinearColor(0.42f, 0.08f, 1.0f);
		case EEssenceFusion::Blood: return FLinearColor(1.0f, 0.02f, 0.06f);
		default: return FLinearColor(1.0f, 0.32f, 0.06f);
		}
	}
}
