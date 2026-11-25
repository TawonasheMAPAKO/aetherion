// Aetherion: The Incipient Engine

#include "WardenHUD.h"
#include "WardenCharacter.h"
#include "EssenceComponent.h"
#include "AetherionCombatMemory.h"
#include "FluxChampion.h"
#include "DivineCrucible.h"
#include "ProgenitorEssence.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScarWorldDirector.h"

namespace WardenPalette
{
	// sRGB values from the browser prototype
	static const FLinearColor Gold		= FLinearColor(FColor(230, 198, 124));
	static const FLinearColor Ember		= FLinearColor(FColor(255, 106, 43));
	static const FLinearColor EmberHot	= FLinearColor(FColor(255, 179, 107));
	static const FLinearColor Bone		= FLinearColor(FColor(239, 230, 218));
	static const FLinearColor BoneDim	= FLinearColor(FColor(168, 156, 149));
	static const FLinearColor Vital		= FLinearColor(FColor(217, 71, 90));
	static const FLinearColor AshEdge	= FLinearColor(FColor(59, 47, 52));
	static const FLinearColor Panel		= FLinearColor(FColor(18, 14, 16, 200));
	static const FLinearColor Track		= FLinearColor(FColor(35, 27, 31, 230));

	static FLinearColor WithAlpha(FLinearColor C, float A) { C.A *= A; return C; }
}


void AWardenHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !GetWorld())
	{
		return;
	}

	const float S = FMath::Clamp(Canvas->ClipY / 1080.0f, 0.6f, 2.0f);
	const AWardenCharacter* Warden = Cast<AWardenCharacter>(GetOwningPawn());

	if (Warden)
	{
		DrawStatusPanel(Warden, S);
		DrawFusionPrompts(Warden, GetWorld()->GetSubsystem<UAetherionCombatMemory>(), S);

		// lock-on marker: ember brackets around the target's chest
		if (const AActor* Target = Warden->GetLockTarget())
		{
			const FVector Screen = Project(Target->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f), true);
			if (Screen.Z > 0.0f)
			{
				const float R = 22.0f * S, L = 9.0f * S, T = 2.0f * S;
				const float X = static_cast<float>(Screen.X), Y = static_cast<float>(Screen.Y);
				const FLinearColor C = WardenPalette::EmberHot;
				static const float Corners[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
				for (const auto& Corner : Corners)
				{
					const float CX = X + Corner[0] * R, CY = Y + Corner[1] * R;
					DrawLine(CX, CY, CX - Corner[0] * L, CY, C, T);
					DrawLine(CX, CY, CX, CY - Corner[1] * L, C, T);
				}
				DrawRect(C, X - 3.0f * S, Y - 3.0f * S, 6.0f * S, 6.0f * S);
			}
		}
	}

	// what the Cult has learned about you, and how alert it is
	if (UAetherionCombatMemory* Memory = GetWorld()->GetSubsystem<UAetherionCombatMemory>())
	{
		FString Insight;
		while (Memory->PopInsight(Insight))
		{
			PushCallout(Insight, FLinearColor(0.72f, 0.45f, 1.0f), 1.1f, 2.6f);
		}

		DrawFluxMeter(Memory, S);
	}
	DrawWorldClock(S);

	if (const AFluxChampion* Champion = AFluxChampion::GetActive())
	{
		DrawBossBar(Champion, S);
	}

	DrawCallouts(S, GetWorld()->GetRealTimeSeconds());
}

void AWardenHUD::PushCallout(const FString& Text, const FLinearColor& Color, float Scale, float Duration)
{
	if (!GetWorld())
	{
		return;
	}

	FCallout C;
	C.Text = Text;
	C.Color = Color;
	C.Scale = Scale;
	C.Duration = Duration;
	C.Born = GetWorld()->GetRealTimeSeconds();
	Callouts.Insert(C, 0);

	if (Callouts.Num() > 4)
	{
		Callouts.SetNum(4);
	}
}

void AWardenHUD::DrawOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness)
{
	DrawLine(X, Y, X + W, Y, Color, Thickness);
	DrawLine(X + W, Y, X + W, Y + H, Color, Thickness);
	DrawLine(X + W, Y + H, X, Y + H, Color, Thickness);
	DrawLine(X, Y + H, X, Y, Color, Thickness);
}

void AWardenHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, int32 Segments)
{
	DrawRect(WardenPalette::Track, X, Y, W, H);
	DrawRect(Fill, X, Y, W * FMath::Clamp(Fraction, 0.0f, 1.0f), H);

	// segment dividers: one per Cleave's worth of Essence
	for (int32 i = 1; i < Segments; ++i)
	{
		const float SX = X + W * i / Segments;
		DrawRect(WardenPalette::Panel, SX - 1.5f, Y, 3.0f, H);
	}

	DrawOutline(X, Y, W, H, WardenPalette::AshEdge, 1.0f);
}

void AWardenHUD::DrawStatusPanel(const AWardenCharacter* Warden, float S)
{
	UFont* Font = GEngine->GetSmallFont();
	const float TextScale = 1.25f * S;
	const float X = 36.0f * S;
	float Y = 36.0f * S;
	const float BarW = 330.0f * S;
	const float BarH = 13.0f * S;
	float TW = 0.0f, TH = 0.0f;

	DrawRect(WardenPalette::Panel, X - 16.0f * S, Y - 14.0f * S, BarW + 32.0f * S, 200.0f * S);

	// Vitality
	DrawText(TEXT("VITALITY"), WardenPalette::BoneDim, X, Y, Font, TextScale);
	const FString HPText = FString::Printf(TEXT("%.0f / %.0f"), FMath::Max(0.0f, Warden->GetCurrentHP()), Warden->GetMaxHP());
	GetTextSize(HPText, TW, TH, Font, TextScale);
	DrawText(HPText, WardenPalette::Bone, X + BarW - TW, Y, Font, TextScale);
	Y += 20.0f * S;
	DrawBar(X, Y, BarW, BarH, Warden->GetHealthFraction(), WardenPalette::Vital, 0);
	Y += 28.0f * S;

	// Essence
	if (const UEssenceComponent* Essence = Warden->GetEssence())
	{
		const int32 Segments = Warden->GetCleaveCost() > 0.0f ? FMath::RoundToInt(Essence->GetMaxEssence() / Warden->GetCleaveCost()) : 0;
		DrawText(FString::Printf(TEXT("ESSENCE  -  %.0f PER CLEAVE"), Warden->GetCleaveCost()), WardenPalette::BoneDim, X, Y, Font, TextScale);
		const FString EssText = FString::Printf(TEXT("%.0f"), Essence->GetEssence());
		GetTextSize(EssText, TW, TH, Font, TextScale);
		DrawText(EssText, WardenPalette::Bone, X + BarW - TW, Y, Font, TextScale);
		Y += 20.0f * S;
		const bool bCanCleave = Essence->HasEssence(Warden->GetCleaveCost());
		const FLinearColor EssenceColor = Warden->GetFusion() == EEssenceFusion::None ? WardenPalette::Ember : AetherionEssence::Color(Warden->GetFusion());
		DrawBar(X, Y, BarW, BarH, Essence->GetEssenceFraction(), bCanCleave ? EssenceColor : WardenPalette::WithAlpha(EssenceColor, 0.55f), Segments);
		Y += 30.0f * S;
	}

	// Cleave chain
	DrawText(TEXT("CLEAVE CHAIN"), WardenPalette::BoneDim, X, Y + 4.0f * S, Font, TextScale);
	static const TCHAR* Roman[] = { TEXT("I"), TEXT("II"), TEXT("III") };
	const float PipW = 44.0f * S, PipH = 26.0f * S;
	float PX = X + BarW - 3.0f * PipW - 2.0f * 8.0f * S;
	for (int32 i = 0; i < 3; ++i)
	{
		const bool bLit = i < Warden->GetCleaveStage();
		DrawRect(bLit ? WardenPalette::Ember : WardenPalette::Track, PX, Y, PipW, PipH);
		DrawOutline(PX, Y, PipW, PipH, bLit ? WardenPalette::EmberHot : WardenPalette::AshEdge, 1.0f);
		GetTextSize(Roman[i], TW, TH, Font, TextScale);
		DrawText(Roman[i], bLit ? FLinearColor(FColor(18, 14, 16)) : WardenPalette::BoneDim, PX + (PipW - TW) * 0.5f, Y + (PipH - TH) * 0.5f, Font, TextScale);
		PX += PipW + 8.0f * S;
	}

	// the fused Progenitor
	const EEssenceFusion Fusion = Warden->GetFusion();
	const FLinearColor FusionColor = Fusion == EEssenceFusion::None ? WardenPalette::EmberHot : AetherionEssence::Color(Fusion);
	const float FY = Y + PipH + 10.0f * S;
	DrawRect(FusionColor, X, FY + 3.0f * S, 4.0f * S, 14.0f * S);
	DrawText(AetherionEssence::CleaveName(Fusion), FusionColor, X + 12.0f * S, FY, Font, TextScale);
}

void AWardenHUD::DrawFusionPrompts(const AWardenCharacter* Warden, const UAetherionCombatMemory* Memory, float S)
{
	UFont* Font = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	float TW = 0.0f, TH = 0.0f;

	// at the Crucible: what pressing F will forge
	if (Warden->GetNearbyCrucible())
	{
		const FString Line = TEXT("[F]  FUSE AT THE DIVINE CRUCIBLE");
		const FString Sub = FString::Printf(TEXT("Now forged: %s"), AetherionEssence::CleaveName(Warden->GetFusion()));
		GetTextSize(Line, TW, TH, Font, 1.0f * S);
		const float X = (Canvas->ClipX - TW) * 0.5f;
		const float Y = Canvas->ClipY * 0.66f;
		DrawRect(WardenPalette::Panel, X - 20.0f * S, Y - 12.0f * S, TW + 40.0f * S, 64.0f * S);
		DrawText(Line, WardenPalette::Gold, X, Y, Font, 1.0f * S);
		float SW = 0.0f, SH = 0.0f;
		GetTextSize(Sub, SW, SH, Small, 1.1f * S);
		DrawText(Sub, AetherionEssence::Color(Warden->GetFusion()), (Canvas->ClipX - SW) * 0.5f, Y + 26.0f * S, Small, 1.1f * S);
	}

	// Umbral cloak
	if (Memory && Memory->IsPlayerCloaked())
	{
		const FString Line = FString::Printf(TEXT("SHADOWED  %.1f"), Memory->GetCloakRemaining());
		GetTextSize(Line, TW, TH, Small, 1.3f * S);
		DrawText(Line, AetherionEssence::Color(EEssenceFusion::Umbral), (Canvas->ClipX - TW) * 0.5f, Canvas->ClipY * 0.80f, Small, 1.3f * S);
	}
}

void AWardenHUD::DrawFluxMeter(const UAetherionCombatMemory* Memory, float S)
{
	// ease the shown value so jumps read as a surge rather than a flicker
	const float Target = Memory->GetMastery();
	const float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	ShownAwareness = ShownAwareness < 0.0f ? Target : FMath::FInterpTo(ShownAwareness, Target, Delta, 2.0f);

	UFont* Small = GEngine->GetSmallFont();
	UFont* Large = GEngine->GetMediumFont();
	const int32 Tier = Memory->GetTier();
	const FLinearColor Violet(0.58f, 0.32f, 1.0f);
	const FLinearColor Hot = Tier >= 9 ? FLinearColor(1.0f, 0.18f, 0.25f) : Violet;

	const float W = 260.0f * S;
	const float X = Canvas->ClipX - W - 36.0f * S;
	const float Y = 36.0f * S;
	DrawRect(WardenPalette::Panel, X - 16.0f * S, Y - 14.0f * S, W + 32.0f * S, 88.0f * S);

	DrawText(TEXT("THE FLUX SEES YOU AS"), WardenPalette::BoneDim, X, Y, Small, 1.1f * S);
	DrawText(Memory->GetTierName(), Hot, X, Y + 16.0f * S, Large, 1.0f * S);

	// ten tier pips; the current one fills by progress through it
	const int32 Pips = 10;
	const float Gap = 4.0f * S;
	const float PipW = (W - (Pips - 1) * Gap) / Pips;
	const float PipH = 7.0f * S;
	const float PY = Y + 50.0f * S;
	for (int32 i = 0; i < Pips; ++i)
	{
		const float PX = X + i * (PipW + Gap);
		DrawRect(WardenPalette::Track, PX, PY, PipW, PipH);
		const float Fill = FMath::Clamp(ShownAwareness * Pips - i, 0.0f, 1.0f);
		if (Fill > 0.0f)
		{
			DrawRect(i == Pips - 1 ? FLinearColor(1.0f, 0.18f, 0.25f) : Violet, PX, PY, PipW * Fill, PipH);
		}
		DrawOutline(PX, PY, PipW, PipH, i == Tier ? Hot : WardenPalette::AshEdge, 1.0f);
	}
}

void AWardenHUD::DrawBossBar(const AFluxChampion* Champion, float S)
{
	UFont* Small = GEngine->GetSmallFont();
	UFont* Large = GEngine->GetMediumFont();

	const float W = FMath::Min(760.0f * S, Canvas->ClipX - 120.0f * S);
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = 150.0f * S;
	const float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;

	// the trailing chunk shows how much the last hits took
	const float HP = Champion->GetHealthFraction();
	ShownBossHealth = ShownBossHealth < 0.0f || HP > ShownBossHealth ? HP : FMath::FInterpTo(ShownBossHealth, HP, Delta, 2.5f);

	DrawRect(WardenPalette::Panel, X - 18.0f * S, Y - 40.0f * S, W + 36.0f * S, 92.0f * S);

	const FString Name = Champion->GetChampionName();
	const FString Title = Champion->GetChampionTitle();
	float NW = 0.0f, NH = 0.0f;
	GetTextSize(Name, NW, NH, Large, 1.1f * S);
	DrawText(Name, WardenPalette::Gold, X, Y - 32.0f * S, Large, 1.1f * S);
	DrawText(Title, WardenPalette::BoneDim, X + NW + 12.0f * S, Y - 24.0f * S, Small, 1.1f * S);

	const FString PhaseText = Champion->IsStaggered() ? TEXT("STAGGERED") : Champion->GetPhase() == 2 ? TEXT("PHASE II") : TEXT("PHASE I");
	float PW = 0.0f, PH = 0.0f;
	GetTextSize(PhaseText, PW, PH, Small, 1.1f * S);
	DrawText(PhaseText, Champion->IsStaggered() ? FLinearColor(1.0f, 0.8f, 0.3f) : WardenPalette::Bone, X + W - PW, Y - 24.0f * S, Small, 1.1f * S);

	// health
	const float H = 16.0f * S;
	DrawRect(WardenPalette::Track, X, Y, W, H);
	DrawRect(FLinearColor(1.0f, 0.85f, 0.6f, 0.6f), X, Y, W * ShownBossHealth, H);
	DrawRect(Champion->GetPhase() == 2 ? FLinearColor(0.75f, 0.1f, 0.35f) : WardenPalette::Vital, X, Y, W * HP, H);
	DrawRect(WardenPalette::Bone, X + W * 0.5f - 1.0f * S, Y - 3.0f * S, 2.0f * S, H + 6.0f * S);
	DrawOutline(X, Y, W, H, WardenPalette::AshEdge, 1.0f);

	// poise: fill it to stagger
	const float PY = Y + H + 8.0f * S;
	const float PHt = 6.0f * S;
	DrawRect(WardenPalette::Track, X, PY, W, PHt);
	DrawRect(FLinearColor(1.0f, 0.78f, 0.35f), X, PY, W * (Champion->IsStaggered() ? 1.0f : Champion->GetPoiseFraction()), PHt);
	DrawText(TEXT("POISE"), WardenPalette::BoneDim, X, PY + 9.0f * S, Small, 0.95f * S);
}

void AWardenHUD::DrawCallouts(float S, float Now)
{
	UFont* Font = GEngine->GetLargeFont();
	float Y = Canvas->ClipY * 0.30f;

	for (int32 i = Callouts.Num() - 1; i >= 0; --i)
	{
		if (Now - Callouts[i].Born > Callouts[i].Duration)
		{
			Callouts.RemoveAt(i);
		}
	}

	for (const FCallout& C : Callouts)
	{
		const float Age = Now - C.Born;
		const float K = Age / C.Duration;
		const float Alpha = FMath::Clamp(FMath::Min(K * 8.0f, (1.0f - K) * 3.0f), 0.0f, 1.0f);
		const float Scale = C.Scale * S * (1.0f + 0.15f * FMath::Max(0.0f, 1.0f - K * 6.0f));
		float TW = 0.0f, TH = 0.0f;
		GetTextSize(C.Text, TW, TH, Font, Scale);
		const float X = Canvas->ClipX * 0.5f - TW * 0.5f;
		const float CY = Y - Age * 30.0f * S;
		DrawText(C.Text, FLinearColor(0.0f, 0.0f, 0.0f, 0.7f * Alpha), X + 2.0f * S, CY + 2.0f * S, Font, Scale);
		DrawText(C.Text, WardenPalette::WithAlpha(C.Color, Alpha), X, CY, Font, Scale);
		Y += TH + 6.0f * S;
	}
}

void AWardenHUD::DrawWorldClock(float S)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!WorldDirector.IsValid())
	{
		if (World->GetTimeSeconds() < NextDirectorSearchAt)
		{
			return;
		}
		NextDirectorSearchAt = World->GetTimeSeconds() + 2.0f;
		TActorIterator<AScarWorldDirector> FirstDirector(World);
		if (FirstDirector)
		{
			WorldDirector = *FirstDirector;
		}
		if (!WorldDirector.IsValid())
		{
			return;
		}
	}

	const AScarWorldDirector* Director = WorldDirector.Get();
	const float Hour = Director->GetHour();
	const int32 H = FMath::Clamp(FMath::FloorToInt(Hour), 0, 23);
	const int32 M = FMath::Clamp(FMath::FloorToInt((Hour - H) * 60.0f), 0, 59);
	const TCHAR* Part = Hour < 5.0f ? TEXT("NIGHT") : (Hour < 7.0f ? TEXT("DAWN") : (Hour < 17.5f ? TEXT("DAY") : (Hour < 19.5f ? TEXT("DUSK") : TEXT("NIGHT"))));

	FString Sky = TEXT("CLEAR SKIES");
	FLinearColor SkyColor = WardenPalette::BoneDim;
	switch (Director->GetWeather())
	{
	case EScarWeather::Ashfall:
		Sky = TEXT("ASHFALL");
		SkyColor = FLinearColor(0.7f, 0.7f, 0.72f);
		break;
	case EScarWeather::EmberStorm:
		Sky = TEXT("EMBER STORM  +Essence");
		SkyColor = FLinearColor(1.0f, 0.45f, 0.12f);
		break;
	case EScarWeather::VoidTide:
		Sky = TEXT("VOID TIDE  low gravity");
		SkyColor = FLinearColor(0.62f, 0.36f, 1.0f);
		break;
	default:
		break;
	}

	UFont* Small = GEngine->GetSmallFont();
	const float W = 260.0f * S;
	const float X = Canvas->ClipX - W - 36.0f * S;
	const float Y = 36.0f * S + 86.0f * S;
	DrawRect(WardenPalette::Panel, X - 16.0f * S, Y - 6.0f * S, W + 32.0f * S, 44.0f * S);
	DrawText(FString::Printf(TEXT("%02d:%02d  %s"), H, M, Part), WardenPalette::BoneDim, X, Y, Small, 1.1f * S);
	DrawText(Sky, SkyColor, X, Y + 16.0f * S, Small, 1.1f * S);
}
