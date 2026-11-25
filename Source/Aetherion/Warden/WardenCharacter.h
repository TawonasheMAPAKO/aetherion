// Aetherion: The Incipient Engine

#pragma once

#include "CoreMinimal.h"
#include "CombatCharacter.h"
#include "CleaveTiming.h"
#include "ProgenitorEssence.h"
#include "WardenCharacter.generated.h"

class UEssenceComponent;
class UInputAction;
class USoundBase;
class UInputMappingContext;
class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UCameraShakeBase;
class USkeletalMesh;
class UAnimInstance;
class UAnimSequenceBase;

/**
 *  A Shard Warden channeling Serrath, Progenitor of Conflict and Ruin.
 *  Builds on the template combat character and adds:
 *  - Essence, earned from melee hits and kills
 *  - Inferno Cleave: costs Essence, chains I -> II -> III. Pressing again as the
 *    previous swing lands is Perfect; mashing too early breaks the chain
 */
UCLASS(abstract)
class AWardenCharacter : public ACombatCharacter
{
	GENERATED_BODY()

	/** Essence bank */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UEssenceComponent* Essence;

protected:

	/** Inferno Cleave input */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InfernoCleaveAction;

	/** Multiplies all camera look input. Lower = slower mouse look. */
	UPROPERTY(EditAnywhere, Category="Input", meta=(ClampMin=0.05, ClampMax=3))
	float LookSensitivity = 0.35f;

	/** Runtime mapping that guarantees E / gamepad top button reach Inferno Cleave */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> WardenMappingContext;

	/** True once the Cleave input has been bound to this character */
	bool bCleaveInputBound = false;

	/** Essence spent per Cleave */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave", meta=(ClampMin=0))
	float CleaveCost = 25.0f;

	/** Optional montage to play on each Cleave. Damage lands immediately either way. */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave")
	UAnimMontage* CleaveMontage;

	/** How far through the previous Cleave's swing (0-1) the next press is Perfect: the moment the blow lands */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave|Combo", meta=(ClampMin=0, ClampMax=1))
	float ComboSweetSpotFraction = 0.55f;

	/** Sweet spot in seconds when a Cleave has no montage to measure */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave|Combo", meta=(ClampMin=0, Units="s"))
	float ComboSweetSpotFallback = 0.45f;

	/** Seconds either side of the sweet spot that count as Perfect */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave|Combo", meta=(ClampMin=0, Units="s"))
	float PerfectWindow = 0.1f;

	/** Pressing this long before the sweet spot is mashing: a weak Cleave that breaks the chain */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave|Combo", meta=(ClampMin=0, Units="s"))
	float TooEarlyMargin = 0.25f;

	/** The chain stays open this long after the sweet spot */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave|Combo", meta=(ClampMin=0, Units="s"))
	float ChainWindow = 0.8f;

	/** Per stage, index 0 = mistimed Cleave, 1-3 = chain stages I, II, III */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave|Stages")
	TArray<float> StageRadius = { 250.0f, 300.0f, 350.0f, 450.0f };

	/** Width of the swing per stage. 360 = full ring. */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave|Stages")
	TArray<float> StageArcDegrees = { 120.0f, 140.0f, 220.0f, 360.0f };

	/** Damage per stage, in the template's HP units (enemies have 3 HP, a basic hit deals 1) */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave|Stages")
	TArray<float> StageDamage = { 0.75f, 1.5f, 2.0f, 3.0f };

	/** Outward knockback per stage */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave|Stages")
	TArray<float> StageKnockback = { 300.0f, 450.0f, 550.0f, 800.0f };

	/** Damage multiplier for a Perfect Cleave */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave|Stages", meta=(ClampMin=1))
	float PerfectDamageMultiplier = 1.25f;

	/** HP restored by the stage III ring of fire */
	UPROPERTY(EditAnywhere, Category="Inferno Cleave|Stages", meta=(ClampMin=0))
	float FinisherHeal = 1.0f;

	/** Essence for each basic attack hit */
	UPROPERTY(EditAnywhere, Category="Essence", meta=(ClampMin=0))
	float EssencePerHit = 5.0f;

	/** Essence for each enemy you hit that dies */
	UPROPERTY(EditAnywhere, Category="Essence", meta=(ClampMin=0))
	float EssencePerKill = 10.0f;

	/** Additive glow used by the Cleave arcs (created by Scripts/setup_feedback.py) */
	UPROPERTY(EditAnywhere, Category="Feedback")
	TSoftObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY(EditAnywhere, Category="Feedback|Sound")
	TSoftObjectPtr<USoundBase> CleaveSound;

	/** Layered on top of the Cleave sound when it lands Perfect */
	UPROPERTY(EditAnywhere, Category="Feedback|Sound")
	TSoftObjectPtr<USoundBase> PerfectSound;

	/** Layered on top for the stage III ring of fire */
	UPROPERTY(EditAnywhere, Category="Feedback|Sound")
	TSoftObjectPtr<USoundBase> FinisherSound;

	UPROPERTY(EditAnywhere, Category="Feedback|Sound")
	TSoftObjectPtr<USoundBase> DeniedSound;

	UPROPERTY(EditAnywhere, Category="Feedback|Sound")
	TSoftObjectPtr<USoundBase> EssenceSound;

	/** Camera distance behind the Warden. The template uses 100, which sits right on the shoulder. */
	UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin=50, ClampMax=1000, Units="cm"))
	float WardenCameraDistance = 340.0f;

	/** Camera offset: right, up */
	UPROPERTY(EditAnywhere, Category="Camera")
	FVector WardenCameraOffset = FVector(0.0f, 45.0f, 70.0f);

	/** Freeze on each basic hit, in real seconds */
	UPROPERTY(EditAnywhere, Category="Game Feel", meta=(ClampMin=0, Units="s"))
	float HitStopDuration = 0.04f;

	/** Freeze when a Cleave connects (grows with the chain stage) */
	UPROPERTY(EditAnywhere, Category="Game Feel", meta=(ClampMin=0, Units="s"))
	float CleaveHitStopDuration = 0.07f;

	/** How slow the world runs during a hit-stop */
	UPROPERTY(EditAnywhere, Category="Game Feel", meta=(ClampMin=0.001, ClampMax=1))
	float HitStopTimeScale = 0.03f;

	/** Slow motion after the stage III finisher, in real seconds */
	UPROPERTY(EditAnywhere, Category="Game Feel", meta=(ClampMin=0, Units="s"))
	float FinisherSlowMoDuration = 0.45f;

	UPROPERTY(EditAnywhere, Category="Game Feel", meta=(ClampMin=0.05, ClampMax=1))
	float FinisherSlowMoTimeScale = 0.3f;

	/** Camera shake played on impacts */
	UPROPERTY(EditAnywhere, Category="Game Feel")
	TSoftClassPtr<UCameraShakeBase> HitCameraShake;

	/** Field of view punch on a Perfect Cleave and on the finisher, in degrees */
	UPROPERTY(EditAnywhere, Category="Game Feel", meta=(ClampMin=0))
	float PerfectFOVKick = 5.0f;

	UPROPERTY(EditAnywhere, Category="Game Feel", meta=(ClampMin=0))
	float FinisherFOVKick = 14.0f;

	/** Use the hero model and animations below instead of the template mannequin */
	UPROPERTY(EditAnywhere, Category="Hero")
	bool bUseHero = true;

	/** Hero skeletal mesh (default: Paragon Sun Wukong, Infernal skin) */
	UPROPERTY(EditAnywhere, Category="Hero")
	TSoftObjectPtr<USkeletalMesh> HeroMesh;

	/** Animation blueprint made for the hero's skeleton */
	UPROPERTY(EditAnywhere, Category="Hero")
	TSoftClassPtr<UAnimInstance> HeroAnimClass;

	/** Basic attack string, played in order while you keep pressing attack */
	UPROPERTY(EditAnywhere, Category="Hero")
	TArray<TSoftObjectPtr<UAnimMontage>> HeroComboMontages;

	/** Swing per Cleave stage: 0 = off-beat, 1-3 = chain stages I, II, III */
	UPROPERTY(EditAnywhere, Category="Hero")
	TArray<TSoftObjectPtr<UAnimMontage>> HeroCleaveMontages;

	/** Played on death instead of a ragdoll */
	UPROPERTY(EditAnywhere, Category="Hero")
	TSoftObjectPtr<UAnimSequenceBase> HeroDeathAnimation;

	UPROPERTY(EditAnywhere, Category="Hero", meta=(ClampMin=0.2, ClampMax=3))
	float HeroAttackPlayRate = 1.15f;

	/** Point in each basic swing (0-1) where the hit lands */
	UPROPERTY(EditAnywhere, Category="Hero", meta=(ClampMin=0, ClampMax=1))
	float HeroHitFraction = 0.4f;

	/** Point in each basic swing (0-1) from which a queued press rolls into the next swing */
	UPROPERTY(EditAnywhere, Category="Hero", meta=(ClampMin=0, ClampMax=1))
	float HeroChainFraction = 0.55f;

	/** Cleave swings start this far in, so the impact lands right on your press */
	UPROPERTY(EditAnywhere, Category="Hero", meta=(ClampMin=0, Units="s"))
	float HeroCleaveStartTime = 0.12f;

	/** Bone the staff hit is traced from, and the reach of the staff */
	UPROPERTY(EditAnywhere, Category="Hero")
	FName HeroTraceBone = TEXT("hand_r");

	UPROPERTY(EditAnywhere, Category="Hero", meta=(Units="cm"))
	float HeroTraceDistance = 170.0f;

	UPROPERTY(EditAnywhere, Category="Hero", meta=(Units="cm"))
	float HeroTraceRadius = 100.0f;

	/** Dodge burst speed */
	UPROPERTY(EditAnywhere, Category="Dodge", meta=(Units="cm/s"))
	float DodgeSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin=0.05, Units="s"))
	float DodgeDuration = 0.35f;

	/** Untouchable for this long from the start of a dodge */
	UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin=0, Units="s"))
	float DodgeInvulnerability = 0.3f;

	UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin=0, Units="s"))
	float DodgeCooldown = 0.4f;

	/** Essence for dodging through an attack */
	UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin=0))
	float PerfectDodgeEssence = 10.0f;

	/** Slow motion after a Perfect Dodge, in real seconds */
	UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin=0, Units="s"))
	float PerfectDodgeSlowMo = 0.5f;

	UPROPERTY(EditAnywhere, Category="Dodge")
	TSoftObjectPtr<UAnimMontage> DodgeForwardMontage;

	UPROPERTY(EditAnywhere, Category="Dodge")
	TSoftObjectPtr<UAnimMontage> DodgeBackMontage;

	UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin=0.2, ClampMax=4))
	float DodgePlayRate = 1.6f;

	/** How far away an enemy can be locked onto */
	UPROPERTY(EditAnywhere, Category="Lock On", meta=(Units="cm"))
	float LockOnRange = 2500.0f;

	/** Falling below this height, into the void beneath the Shard, is death */
	UPROPERTY(EditAnywhere, Category="World", meta=(Units="cm"))
	float VoidDeathZ = -15000.0f;

	/** Umbral: extra Essence each Cleave costs */
	UPROPERTY(EditAnywhere, Category="Fusion|Umbral", meta=(ClampMin=0))
	float UmbralCostBonus = 5.0f;

	/** Umbral: the stage III rift drags enemies in from this far */
	UPROPERTY(EditAnywhere, Category="Fusion|Umbral", meta=(Units="cm"))
	float UmbralPullRadius = 750.0f;

	UPROPERTY(EditAnywhere, Category="Fusion|Umbral", meta=(Units="cm/s"))
	float UmbralPullStrength = 1100.0f;

	/** Umbral: a Perfect Cleave hides you from the Cult this long */
	UPROPERTY(EditAnywhere, Category="Fusion|Umbral", meta=(Units="s"))
	float UmbralCloakDuration = 2.5f;

	/** Blood: HP drained from each enemy a Cleave hits */
	UPROPERTY(EditAnywhere, Category="Fusion|Blood", meta=(ClampMin=0))
	float BloodDrainPerHit = 0.2f;

	/** Blood: extra HP per enemy caught by the stage III finisher */
	UPROPERTY(EditAnywhere, Category="Fusion|Blood", meta=(ClampMin=0))
	float BloodFinisherHealPerHit = 0.5f;

	/** Blood: the price of recklessness, more damage taken */
	UPROPERTY(EditAnywhere, Category="Fusion|Blood", meta=(ClampMin=1))
	float BloodDamageTakenMultiplier = 1.2f;

	/** How quickly the camera swings to keep the target framed */
	UPROPERTY(EditAnywhere, Category="Lock On", meta=(ClampMin=1))
	float LockOnTurnSpeed = 10.0f;

	/** Debug lines and engine text, as used before the HUD existed */
	UPROPERTY(EditAnywhere, Category="Feedback")
	bool bShowDebugFeedback = false;

	/** Current chain stage (0 = no chain) */
	int32 CleaveStage = 0;

	/** Game time at which the next Cleave press is Perfect (the previous swing's sweet spot) */
	float NextPerfectTime = -100.0f;

	/** Real time at which the current hit-stop or slow-mo ends (-1 = none) */
	float TimeScaleRestoreAt = -1.0f;

	/** Slow-mo queued to start when the current hit-stop ends */
	float PendingSlowMoDuration = 0.0f;

	/** Current FOV punch, decays to zero */
	float FOVKick = 0.0f;

	float BaseFOV = 90.0f;

	/** Inputs created in code, so no editor setup is needed for them */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DodgeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LockOnAction;

	FVector2D LastMoveInput = FVector2D::ZeroVector;

	bool bDodging = false;
	bool bPerfectDodgeAvailable = false;
	FVector DodgeDirection = FVector::ForwardVector;
	float DodgeStartedAt = 0.0f;
	float InvulnerableUntil = -1.0f;
	float NextDodgeAt = 0.0f;
	float SavedGroundFriction = 8.0f;
	float SavedBrakingDeceleration = 2048.0f;
	float SavedMaxWalkSpeed = 400.0f;

	TWeakObjectPtr<AActor> LockTarget;
	bool bIsLockedOn = false;
	bool bSavedOrientToMovement = true;
	bool bSavedUseControllerRotation = false;

	/** True once the hero mesh and animation blueprint are in place */
	bool bHeroActive = false;
	bool bHeroAttacking = false;
	bool bHeroQueued = false;
	bool bHeroPastChainPoint = false;
	int32 HeroComboIndex = 0;
	FTimerHandle HeroHitTimer;
	FTimerHandle HeroChainTimer;
	FOnMontageEnded HeroMontageEnded;

public:

	AWardenCharacter();

	/** Performs an Inferno Cleave. Callable from touch UI as well as input. */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoInfernoCleave();

	UFUNCTION(BlueprintPure, Category="Inferno Cleave")
	int32 GetCleaveStage() const { return CleaveStage; }

	UFUNCTION(BlueprintPure, Category="Essence")
	UEssenceComponent* GetEssence() const { return Essence; }

	float GetCurrentHP() const { return CurrentHP; }
	float GetMaxHP() const { return MaxHP; }
	float GetHealthFraction() const { return MaxHP > 0.0f ? FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f) : 0.0f; }
	float GetCleaveCost() const;

	/** The Progenitor Essence fused into the Warden's Conflict Essence */
	EEssenceFusion GetFusion() const;

	/** The Crucible within reach, if any (for the HUD prompt) */
	class ADivineCrucible* GetNearbyCrucible() const { return NearbyCrucible.Get(); }

	/** Called by an Essence Core when the Warden absorbs it */
	void OnEssenceCoreCollected(EEssenceFusion Kind);

	/** Uses whatever is in reach (the Divine Crucible) */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoInteract();
	/** Records the basic attack for the Combat Memory before running the template combo */
	virtual void DoComboAttackStart() override;

	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	virtual void Tick(float DeltaSeconds) override;

	/** Scales look input by LookSensitivity */
	virtual void DoLook(float Yaw, float Pitch) override;

	/** Remembers movement input so a dodge goes where you are steering */
	virtual void DoMove(float Right, float Forward) override;

	/** Ignores hits during a dodge's invincibility and rewards a Perfect Dodge */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Dodge in the direction you are moving, or step back if standing still */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoDodge();

	/** Locks the camera onto the enemy you are facing, or releases it */
	UFUNCTION(BlueprintCallable, Category="Input")
	void ToggleLockOn();

	/** The locked-on enemy, if any */
	AActor* GetLockTarget() const { return LockTarget.Get(); }

	/** Adds the Warden's own key mapping once a local player controls it */
	virtual void PawnClientRestart() override;

protected:

	virtual void BeginPlay() override;

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void HandleDeath() override;

	virtual void UnPossessed() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called by the template melee trace for each actor hit */
	virtual void OnMeleeHit(AActor* HitActor, float Damage) override;

	/** Blueprint hook for Cleave VFX and sound */
	UFUNCTION(BlueprintImplementableEvent, Category="Inferno Cleave")
	void BP_OnInfernoCleave(int32 Stage, ECleaveTiming Timing, float Radius, float ArcDegrees);

	/** Blueprint hook for when a Cleave fails for lack of Essence */
	UFUNCTION(BlueprintImplementableEvent, Category="Inferno Cleave")
	void BP_OnCleaveDenied();

	UFUNCTION()
	void HandleEnemyKilled();

	/** The locked-on enemy died: move the lock to the next one */
	UFUNCTION()
	void HandleLockTargetDied();

private:

	/** Plays a 2D sound if it has been set up */
	void PlaySfx(const TSoftObjectPtr<USoundBase>& Sound, float Volume = 1.0f) const;

	/** Swaps in the hero mesh and animation blueprint. Returns true if both loaded. */
	bool ApplyHeroModel();

	void HeroComboPressed();
	void PlayHeroCombo(int32 Index);
	void HeroHitPoint();
	void HeroChainPoint();
	void OnHeroMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void StopHeroCombo();

	void EnsureRuntimeInput();
	void UmbralRift();
	void UpdateFusionWorld();

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	TWeakObjectPtr<class ADivineCrucible> NearbyCrucible;
	float NextCrucibleScan = 0.0f;
	float NextCloakRingAt = 0.0f;
	void RemoveWardenInput(AController* FromController);
	void UpdateDodge();
	void EndDodge();
	bool IsLockable(const AActor* Candidate) const;
	AActor* FindLockTarget(const AActor* Exclude, float MinFacing, float RangeScale = 1.0f) const;

	/** Moves the lock to the best remaining enemy, or releases it if none are near */
	void RetargetLock(const AActor* Exclude);
	void SetLockTarget(AActor* NewTarget);
	void UpdateLockOn();

	/** Turns the Warden to face where the camera looks */
	void FaceCamera();

	/** Briefly slows the whole world to sell an impact */
	void StartHitStop(float Duration);

	/** Plays the impact camera shake */
	void PlayImpactShake(float Scale) const;

	/** Restores time scale and eases the FOV punch back, using real time */
	void UpdateGameFeel();

	/** Spawns the glowing arc for a Cleave */
	void SpawnCleaveEffect(int32 Stage, ECleaveTiming Timing, float Radius, float ArcDegrees);

	/** Hits every damageable actor inside the stage's arc */
	int32 ApplyCleaveDamage(int32 Stage, float Damage);

	/** Listens for the death of an enemy we hit so the kill pays out Essence */
	void TrackKill(AActor* Victim);

	void ShowFeedback(const FString& Message, const FColor& Color, float Duration = 1.2f, float Scale = 1.0f) const;

	float StageValue(const TArray<float>& Values, int32 Stage, float Fallback) const;
};
