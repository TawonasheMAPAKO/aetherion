// Aetherion: The Incipient Engine

#pragma once

#include "CoreMinimal.h"
#include "CleaveTiming.generated.h"

/** How well a Cleave was timed against the swing before it */
UENUM(BlueprintType)
enum class ECleaveTiming : uint8
{
	Off		UMETA(DisplayName = "Too Early"),
	Good	UMETA(DisplayName = "Good"),
	Perfect	UMETA(DisplayName = "Perfect")
};
