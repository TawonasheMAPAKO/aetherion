// Aetherion: The Incipient Engine

#include "WardenCharacter.h"
#include "Components/WidgetComponent.h"
#include "EssenceComponent.h"
#include "CombatEnemy.h"
#include "CombatDamageable.h"
#include "CombatLifeBar.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "WardenHUD.h"
#include "AetherionCombatMemory.h"
#include "DivineCrucible.h"
#include "EngineUtils.h"
#include "CleaveEffect.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/App.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "TimerManager.h"
#include "InputAction.h"
#include "GameFramework/CharacterMovementComponent.h"

AWardenCharacter::AWardenCharacter()
{
	Essence = CreateDefaultSubobject<UEssenceComponent>(TEXT("Essence"));

	auto Path = [](const TCHAR* Name) { return FSoftObjectPath(FString::Printf(TEXT("/Game/Aetherion/%s"), Name)); };
	GlowMaterial = TSoftObjectPtr<UMaterialInterface>(Path(TEXT("FX/M_AetherionGlow.M_AetherionGlow")));
	CleaveSound = TSoftObjectPtr<USoundBase>(Path(TEXT("Audio/S_Cleave.S_Cleave")));
	PerfectSound = TSoftObjectPtr<USoundBase>(Path(TEXT("Audio/S_CleavePerfect.S_CleavePerfect")));
	FinisherSound = TSoftObjectPtr<USoundBase>(Path(TEXT("Audio/S_CleaveFinisher.S_CleaveFinisher")));
	DeniedSound = TSoftObjectPtr<USoundBase>(Path(TEXT("Audio/S_CleaveDenied.S_CleaveDenied")));
	EssenceSound = TSoftObjectPtr<USoundBase>(Path(TEXT("Audio/S_Essence.S_Essence")));

	// hero: Paragon Sun Wukong
	const FString Hero = TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/");
	auto HeroPath = [&Hero](const TCHAR* Rel) { return FSoftObjectPath(Hero + Rel); };
	HeroMesh = TSoftObjectPtr<USkeletalMesh>(HeroPath(TEXT("Skins/Infernal/Meshes/Wukong_Infernal.Wukong_Infernal")));
	HeroAnimClass = TSoftClassPtr<UAnimInstance>(HeroPath(TEXT("SunWukong_AnimBlueprint.SunWukong_AnimBlueprint_C")));
	for (const TCHAR* Letter : { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D") })
	{
		HeroComboMontages.Add(TSoftObjectPtr<UAnimMontage>(HeroPath(*FString::Printf(TEXT("Animations/Primary_Melee_%s_Slow_Montage.Primary_Melee_%s_Slow_Montage"), Letter, Letter))));
	}
	HeroCleaveMontages.Add(TSoftObjectPtr<UAnimMontage>(HeroPath(TEXT("Animations/Primary_Melee_C_Slow_Montage.Primary_Melee_C_Slow_Montage"))));
	HeroCleaveMontages.Add(TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Aetherion/Hero/AM_Wukong_Cleave_I.AM_Wukong_Cleave_I"))));
	HeroCleaveMontages.Add(TSoftObjectPtr<UAnimMontage>(HeroPath(TEXT("Animations/Primary_Melee_D_Slow_Montage.Primary_Melee_D_Slow_Montage"))));
	HeroCleaveMontages.Add(TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Aetherion/Hero/AM_Wukong_Cleave_III.AM_Wukong_Cleave_III"))));
	DodgeForwardMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Aetherion/Hero/AM_Wukong_Dodge_Fwd.AM_Wukong_Dodge_Fwd")));
	DodgeBackMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Aetherion/Hero/AM_Wukong_Dodge_Bwd.AM_Wukong_Dodge_Bwd")));
	HeroDeathAnimation = TSoftObjectPtr<UAnimSequenceBase>(HeroPath(TEXT("Animations/Death.Death")));
	HeroMontageEnded.BindUObject(this, &AWardenCharacter::OnHeroMontageEnded);

	HitCameraShake = TSoftClassPtr<UCameraShakeBase>(FSoftObjectPath(TEXT("/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Enemy.BP_CameraShake_Hit_Enemy_C")));
}

void AWardenCharacter::StartHitStop(float Duration)
{
	UWorld* World = GetWorld();
	if (!World || Duration <= 0.0f)
	{
		return;
	}

	UGameplayStatics::SetGlobalTimeDilation(this, HitStopTimeScale);
	TimeScaleRestoreAt = FMath::Max(TimeScaleRestoreAt, World->GetRealTimeSeconds() + Duration);
}

void AWardenCharacter::PlayImpactShake(float Scale) const
{
	if (Scale <= 0.0f || HitCameraShake.IsNull())
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UClass* ShakeClass = HitCameraShake.LoadSynchronous())
		{
			PC->ClientStartCameraShake(ShakeClass, Scale);
		}
	}
}

void AWardenCharacter::UpdateGameFeel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// end hit-stop, possibly rolling straight into finisher slow-mo
	if (TimeScaleRestoreAt > 0.0f && World->GetRealTimeSeconds() >= TimeScaleRestoreAt)
	{
		if (PendingSlowMoDuration > 0.0f)
		{
			UGameplayStatics::SetGlobalTimeDilation(this, FinisherSlowMoTimeScale);
			TimeScaleRestoreAt = World->GetRealTimeSeconds() + PendingSlowMoDuration;
			PendingSlowMoDuration = 0.0f;
		}
		else
		{
			UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
			TimeScaleRestoreAt = -1.0f;
		}
	}

	// ease the FOV punch back using real frame time
	if (UCameraComponent* Camera = GetFollowCamera())
	{
		FOVKick = FMath::FInterpTo(FOVKick, 0.0f, static_cast<float>(FApp::GetDeltaTime()), 4.0f);
		Camera->SetFieldOfView(BaseFOV + FOVKick);
	}
}

void AWardenCharacter::RemoveWardenInput(AController* FromController)
{
	// take our key mappings with us, or a dead Warden's copy keeps swallowing the keys after respawn
	const APlayerController* PC = Cast<APlayerController>(FromController);
	if (!WardenMappingContext || !PC || !PC->GetLocalPlayer())
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->RemoveMappingContext(WardenMappingContext);
	}
}

void AWardenCharacter::UnPossessed()
{
	RemoveWardenInput(GetController());
	Super::UnPossessed();
}

void AWardenCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveWardenInput(GetController());

	if (TimeScaleRestoreAt > 0.0f)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
		TimeScaleRestoreAt = -1.0f;
	}

	Super::EndPlay(EndPlayReason);
}

void AWardenCharacter::PlaySfx(const TSoftObjectPtr<USoundBase>& Sound, float Volume) const
{
	if (!IsLocallyControlled() || Sound.IsNull())
	{
		return;
	}

	if (USoundBase* Loaded = Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(this, Loaded, Volume);
	}
}

void AWardenCharacter::SpawnCleaveEffect(int32 Stage, ECleaveTiming Timing, float Radius, float ArcDegrees)
{
	UMaterialInterface* Material = GlowMaterial.LoadSynchronous();
	UWorld* World = GetWorld();
	if (!Material || !World)
	{
		return;
	}

	FLinearColor Color(0.55f, 0.48f, 0.42f);
	float Intensity = 3.0f;
	if (Stage > 0)
	{
		Color = Timing == ECleaveTiming::Perfect ? FLinearColor(1.0f, 0.45f, 0.12f) : FLinearColor(1.0f, 0.32f, 0.06f);
		if (GetFusion() != EEssenceFusion::None)
		{
			Color = AetherionEssence::Color(GetFusion()) * (Timing == ECleaveTiming::Perfect ? 1.3f : 1.0f);
		}
		Intensity = Stage == 3 ? 22.0f : (Timing == ECleaveTiming::Perfect ? 14.0f : 9.0f);
	}

	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.0f);

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ACleaveEffect* Effect = World->SpawnActor<ACleaveEffect>(ACleaveEffect::StaticClass(), Feet, FRotator(0.0f, GetActorRotation().Yaw, 0.0f), Params))
	{
		Effect->Init(Material, Radius, ArcDegrees, Color, Intensity, Stage == 3 ? 0.6f : 0.35f);
	}
}


float AWardenCharacter::StageValue(const TArray<float>& Values, int32 Stage, float Fallback) const
{
	return Values.IsValidIndex(Stage) ? Values[Stage] : Fallback;
}

bool AWardenCharacter::ApplyHeroModel()
{
	if (!bUseHero || HeroMesh.IsNull() || HeroAnimClass.IsNull())
	{
		return false;
	}

	USkeletalMesh* Mesh = HeroMesh.LoadSynchronous();
	UClass* AnimClass = HeroAnimClass.LoadSynchronous();
	if (!Mesh || !AnimClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Aetherion] Hero mesh or animation blueprint not found, keeping the mannequin."));
		return false;
	}

	if (GetMesh()->GetSkeletalMeshAsset() != Mesh)
	{
		GetMesh()->SetSkeletalMeshAsset(Mesh);
	}
	GetMesh()->SetAnimInstanceClass(AnimClass);
	return true;
}

void AWardenCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// show the hero in the editor too
	ApplyHeroModel();
}

void AWardenCharacter::BeginPlay()
{
	// swap the model before the template saves the mesh transform
	bHeroActive = ApplyHeroModel();
	if (bHeroActive)
	{
		MeleeTraceDistance = HeroTraceDistance;
		MeleeTraceRadius = HeroTraceRadius;
	}

	Super::BeginPlay();

	// the Warden's vitality is on the HUD; the template's floating bar over his head is not needed
	if (UWidgetComponent* FloatingBar = FindComponentByClass<UWidgetComponent>())
	{
		FloatingBar->SetHiddenInGame(true);
	}

	// a wider, more cinematic camera than the template's
	GetCameraBoom()->TargetArmLength = WardenCameraDistance;
	GetCameraBoom()->SocketOffset = WardenCameraOffset;
	if (const UCameraComponent* Camera = GetFollowCamera())
	{
		BaseFOV = Camera->FieldOfView;
	}
}

void AWardenCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnsureRuntimeInput();
		EnhancedInput->BindAction(DodgeAction, ETriggerEvent::Started, this, &AWardenCharacter::DoDodge);
		EnhancedInput->BindAction(LockOnAction, ETriggerEvent::Started, this, &AWardenCharacter::ToggleLockOn);
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AWardenCharacter::DoInteract);

		if (InfernoCleaveAction)
		{
			EnhancedInput->BindAction(InfernoCleaveAction, ETriggerEvent::Started, this, &AWardenCharacter::DoInfernoCleave);
			bCleaveInputBound = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Aetherion] BP_Warden has no Inferno Cleave Action set, so E will do nothing."));
		}
	}
}

void AWardenCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->GetLocalPlayer())
	{
		return;
	}

	// swap in the Warden HUD
	if (!Cast<AWardenHUD>(PC->GetHUD()))
	{
		PC->ClientSetHUD(AWardenHUD::StaticClass());
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (!WardenMappingContext)
		{
			EnsureRuntimeInput();
			WardenMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_WardenRuntime"));
			if (InfernoCleaveAction)
			{
				WardenMappingContext->MapKey(InfernoCleaveAction, EKeys::E);
				WardenMappingContext->MapKey(InfernoCleaveAction, EKeys::Gamepad_FaceButton_Top);
			}
			WardenMappingContext->MapKey(DodgeAction, EKeys::LeftShift);
			WardenMappingContext->MapKey(DodgeAction, EKeys::B);
			WardenMappingContext->MapKey(DodgeAction, EKeys::Gamepad_FaceButton_Right);
			WardenMappingContext->MapKey(LockOnAction, EKeys::Q);
			WardenMappingContext->MapKey(LockOnAction, EKeys::MiddleMouseButton);
			WardenMappingContext->MapKey(LockOnAction, EKeys::Gamepad_RightThumbstick);
			WardenMappingContext->MapKey(InteractAction, EKeys::F);
			WardenMappingContext->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Left);
		}

		// higher priority than the template contexts, so nothing else can swallow these keys
		Subsystem->RemoveMappingContext(WardenMappingContext);
		Subsystem->AddMappingContext(WardenMappingContext, 10);
	}
}

void AWardenCharacter::DoLook(float Yaw, float Pitch)
{
	// the lock-on camera owns the view while a target is held
	if (bIsLockedOn)
	{
		return;
	}
	Super::DoLook(Yaw * LookSensitivity, Pitch * LookSensitivity);
}

void AWardenCharacter::DoMove(float Right, float Forward)
{
	LastMoveInput = FVector2D(Right, Forward);
	Super::DoMove(Right, Forward);
}

void AWardenCharacter::EnsureRuntimeInput()
{
	if (!DodgeAction)
	{
		DodgeAction = NewObject<UInputAction>(this, TEXT("IA_WardenDodge"));
	}
	if (!LockOnAction)
	{
		LockOnAction = NewObject<UInputAction>(this, TEXT("IA_WardenLockOn"));
	}
	if (!InteractAction)
	{
		InteractAction = NewObject<UInputAction>(this, TEXT("IA_WardenInteract"));
	}
}

EEssenceFusion AWardenCharacter::GetFusion() const
{
	const UAetherionCombatMemory* Memory = GetWorld() ? GetWorld()->GetSubsystem<UAetherionCombatMemory>() : nullptr;
	return Memory ? Memory->GetFusion() : EEssenceFusion::None;
}

float AWardenCharacter::GetCleaveCost() const
{
	return CleaveCost + (GetFusion() == EEssenceFusion::Umbral ? UmbralCostBonus : 0.0f);
}

void AWardenCharacter::DoInteract()
{
	ADivineCrucible* Crucible = NearbyCrucible.Get();
	if (!Crucible || CurrentHP <= 0.0f)
	{
		return;
	}
	const EEssenceFusion Fusion = Crucible->CycleFusion();
	ShowFeedback(FString::Printf(TEXT("FORGED: %s"), AetherionEssence::CleaveName(Fusion)), AetherionEssence::Color(Fusion).ToFColor(true), 1.6f, 1.5f);
	PlaySfx(PerfectSound, 0.8f);
	FOVKick = FMath::Max(FOVKick, PerfectFOVKick);
}

void AWardenCharacter::OnEssenceCoreCollected(EEssenceFusion Kind)
{
	Essence->GainEssence(10.0f);
	PlaySfx(EssenceSound, 1.0f);
	PlaySfx(PerfectSound, 0.4f);
}

void AWardenCharacter::UmbralRift()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// the void opens at the Warden's feet and drags the Cult in
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(UmbralRift), false, this);
	World->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, Objects, FCollisionShape::MakeSphere(UmbralPullRadius), Query);

	TSet<AActor*> Pulled;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACombatEnemy* Enemy = Cast<ACombatEnemy>(Overlap.GetActor());
		if (!Enemy || Pulled.Contains(Enemy) || Enemy->CurrentHP <= 0.0f)
		{
			continue;
		}
		Pulled.Add(Enemy);
		const FVector ToMe = GetActorLocation() - Enemy->GetActorLocation();
		const float Dist = ToMe.Size2D();
		const float Strength = UmbralPullStrength * FMath::Clamp(Dist / UmbralPullRadius, 0.35f, 1.0f);
		Enemy->LaunchCharacter(ToMe.GetSafeNormal2D() * Strength + FVector(0.0f, 0.0f, 180.0f), true, true);
	}

	if (UMaterialInterface* Material = GlowMaterial.LoadSynchronous())
	{
		const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.0f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 i = 0; i < 3; ++i)
		{
			if (ACleaveEffect* Ring = World->SpawnActor<ACleaveEffect>(ACleaveEffect::StaticClass(), Feet + FVector(0.0f, 0.0f, i * 3.0f), FRotator(0.0f, i * 40.0f, 0.0f), Params))
			{
				Ring->Init(Material, UmbralPullRadius * (1.0f - 0.25f * i), 360.0f, AetherionEssence::Color(EEssenceFusion::Umbral), 16.0f - 4.0f * i, 0.7f + 0.15f * i);
			}
		}
	}
	if (Pulled.Num() > 0)
	{
		ShowFeedback(FString::Printf(TEXT("THE VOID TAKES %d"), Pulled.Num()), AetherionEssence::Color(EEssenceFusion::Umbral).ToFColor(true), 1.0f, 1.3f);
	}
}

void AWardenCharacter::UpdateFusionWorld()
{
	UWorld* World = GetWorld();
	if (!World || !IsLocallyControlled())
	{
		return;
	}
	const float Now = World->GetTimeSeconds();

	// find a Crucible within reach
	if (Now >= NextCrucibleScan)
	{
		NextCrucibleScan = Now + 0.25f;
		ADivineCrucible* Best = nullptr;
		float BestDist = FLT_MAX;
		for (TActorIterator<ADivineCrucible> It(World); It; ++It)
		{
			const float Dist = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
			if (Dist < It->UseRange && Dist < BestDist)
			{
				Best = *It;
				BestDist = Dist;
			}
		}
		NearbyCrucible = Best;
	}

	// Umbral cloak: wisps of void trail the hidden Warden
	const UAetherionCombatMemory* Memory = World->GetSubsystem<UAetherionCombatMemory>();
	if (Memory && Memory->IsPlayerCloaked() && Now >= NextCloakRingAt)
	{
		NextCloakRingAt = Now + 0.3f;
		if (UMaterialInterface* Material = GlowMaterial.LoadSynchronous())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.0f);
			if (ACleaveEffect* Ring = World->SpawnActor<ACleaveEffect>(ACleaveEffect::StaticClass(), Feet, FRotator::ZeroRotator, Params))
			{
				Ring->Init(Material, 120.0f, 360.0f, AetherionEssence::Color(EEssenceFusion::Umbral), 5.0f, 0.5f);
			}
		}
	}
}

void AWardenCharacter::DoDodge()
{
	UWorld* World = GetWorld();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	UE_LOG(LogTemp, Log, TEXT("[Aetherion] Dodge pressed"));
	if (!World || !Move || CurrentHP <= 0.0f || bDodging || Move->IsFalling())
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now < NextDodgeAt)
	{
		return;
	}

	// steer with movement input relative to the camera; standing still steps back
	const bool bHasInput = LastMoveInput.SizeSquared() > 0.04f;
	const FRotator ViewYaw(0.0f, GetControlRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(ViewYaw).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(ViewYaw).GetUnitAxis(EAxis::Y);
	DodgeDirection = bHasInput ? (Forward * LastMoveInput.Y + Right * LastMoveInput.X).GetSafeNormal2D() : -GetActorForwardVector();
	if (DodgeDirection.IsNearlyZero())
	{
		DodgeDirection = -GetActorForwardVector();
	}

	// break out of any swing
	StopHeroCombo();
	CachedAttackInputTime = 0.0f;

	// flip forward in the dodge direction, or back-flip away
	const bool bBackwards = !bHasInput || FVector::DotProduct(DodgeDirection, GetActorForwardVector()) < -0.3f;
	if (!bBackwards && !LockTarget.IsValid())
	{
		SetActorRotation(FRotator(0.0f, DodgeDirection.Rotation().Yaw, 0.0f));
	}
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		if (UAnimMontage* Flip = (bBackwards ? DodgeBackMontage : DodgeForwardMontage).LoadSynchronous())
		{
			Anim->Montage_Play(Flip, DodgePlayRate, EMontagePlayReturnType::MontageLength, 0.0f, true);
		}
	}

	// frictionless burst, restored in EndDodge
	SavedGroundFriction = Move->GroundFriction;
	SavedBrakingDeceleration = Move->BrakingDecelerationWalking;
	SavedMaxWalkSpeed = Move->MaxWalkSpeed;
	Move->GroundFriction = 0.0f;
	Move->BrakingDecelerationWalking = 0.0f;
	Move->MaxWalkSpeed = DodgeSpeed;

	bDodging = true;
	bPerfectDodgeAvailable = true;
	if (UAetherionCombatMemory* Memory = World->GetSubsystem<UAetherionCombatMemory>())
	{
		Memory->RecordPlayerDodge(DodgeDirection);
	}
	DodgeStartedAt = Now;
	InvulnerableUntil = Now + DodgeInvulnerability;
	NextDodgeAt = Now + DodgeDuration + DodgeCooldown;
	UpdateDodge();
}

void AWardenCharacter::UpdateDodge()
{
	if (!bDodging || !GetWorld())
	{
		return;
	}

	const float T = (GetWorld()->GetTimeSeconds() - DodgeStartedAt) / DodgeDuration;
	if (T >= 1.0f)
	{
		EndDodge();
		return;
	}

	// fast out, easing off toward the end
	const float Speed = DodgeSpeed * (1.0f - 0.7f * T * T);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->Velocity = FVector(DodgeDirection.X * Speed, DodgeDirection.Y * Speed, Move->Velocity.Z);
}

void AWardenCharacter::EndDodge()
{
	bDodging = false;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->GroundFriction = SavedGroundFriction;
		Move->BrakingDecelerationWalking = SavedBrakingDeceleration;
		Move->MaxWalkSpeed = SavedMaxWalkSpeed;
		Move->Velocity *= 0.25f;
	}
}

void AWardenCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	if (GetWorld() && GetWorld()->GetTimeSeconds() < InvulnerableUntil)
	{
		// dodged through an attack: reward it once per dodge
		if (bPerfectDodgeAvailable && DamageCauser && DamageCauser != this)
		{
			bPerfectDodgeAvailable = false;
			Essence->GainEssence(PerfectDodgeEssence);
			if (UAetherionCombatMemory* Memory = GetWorld()->GetSubsystem<UAetherionCombatMemory>())
			{
				Memory->RecordPerfectDodge();
			}
			ShowFeedback(FString::Printf(TEXT("PERFECT DODGE  +%.0f"), PerfectDodgeEssence), FColor(230, 198, 124), 1.0f, 1.5f);
			PlaySfx(PerfectSound, 0.6f);
			FOVKick = FMath::Max(FOVKick, PerfectFOVKick);
			PendingSlowMoDuration = PerfectDodgeSlowMo;
			StartHitStop(0.02f);
		}
		return;
	}

	if (CurrentHP > 0.0f && Damage > 0.0f && GetWorld())
	{
		if (UAetherionCombatMemory* Memory = GetWorld()->GetSubsystem<UAetherionCombatMemory>())
		{
			Memory->RecordPlayerHit(Damage);
		}
	}
	Super::ApplyDamage(Damage * (GetFusion() == EEssenceFusion::Blood ? BloodDamageTakenMultiplier : 1.0f), DamageCauser, DamageLocation, DamageImpulse);
}

bool AWardenCharacter::IsLockable(const AActor* Candidate) const
{
	const ACombatEnemy* Enemy = Cast<ACombatEnemy>(Candidate);
	if (!IsValid(Enemy))
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
	return Capsule && Capsule->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
}

AActor* AWardenCharacter::FindLockTarget(const AActor* Exclude, float MinFacing, float RangeScale) const
{
	const UCameraComponent* Camera = GetFollowCamera();
	if (!Camera)
	{
		return nullptr;
	}

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(this, ACombatEnemy::StaticClass(), Enemies);

	const FVector Eye = Camera->GetComponentLocation();
	const FVector Look = Camera->GetForwardVector();
	AActor* Best = nullptr;
	float BestScore = -FLT_MAX;

	for (AActor* Candidate : Enemies)
	{
		if (Candidate == Exclude || !IsLockable(Candidate))
		{
			continue;
		}
		const float Distance = FVector::Dist(GetActorLocation(), Candidate->GetActorLocation());
		if (Distance > LockOnRange * RangeScale)
		{
			continue;
		}
		const float Facing = FVector::DotProduct(Look, (Candidate->GetActorLocation() - Eye).GetSafeNormal());
		if (Facing < MinFacing)
		{
			continue;
		}
		// favour what you are looking at, then what is close
		const float Score = Facing * 2.0f - Distance / (LockOnRange * RangeScale);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

void AWardenCharacter::SetLockTarget(AActor* NewTarget)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bWasLocked = bIsLockedOn;

	// listen for the target's death so the lock can hop straight to the next enemy
	if (ACombatEnemy* Old = Cast<ACombatEnemy>(LockTarget.Get()))
	{
		Old->OnEnemyDied.RemoveDynamic(this, &AWardenCharacter::HandleLockTargetDied);
	}
	if (ACombatEnemy* New = Cast<ACombatEnemy>(NewTarget))
	{
		New->OnEnemyDied.AddUniqueDynamic(this, &AWardenCharacter::HandleLockTargetDied);
	}

	LockTarget = NewTarget;
	bIsLockedOn = NewTarget != nullptr;

	if (NewTarget && !bWasLocked && Move)
	{
		// face the target and strafe around it
		bSavedOrientToMovement = Move->bOrientRotationToMovement;
		bSavedUseControllerRotation = Move->bUseControllerDesiredRotation;
		Move->bOrientRotationToMovement = false;
		Move->bUseControllerDesiredRotation = true;
	}
	else if (!NewTarget && bWasLocked && Move)
	{
		Move->bOrientRotationToMovement = bSavedOrientToMovement;
		Move->bUseControllerDesiredRotation = bSavedUseControllerRotation;
	}
}

void AWardenCharacter::RetargetLock(const AActor* Exclude)
{
	// prefer what you are facing, then anything nearby
	AActor* Next = FindLockTarget(Exclude, 0.0f, 1.5f);
	if (!Next)
	{
		Next = FindLockTarget(Exclude, -1.0f, 1.5f);
	}

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(this, ACombatEnemy::StaticClass(), Enemies);
	int32 Alive = 0;
	for (const AActor* E : Enemies)
	{
		Alive += (E != Exclude && IsLockable(E)) ? 1 : 0;
	}
	UE_LOG(LogTemp, Log, TEXT("[Aetherion] Lock-on: target lost, %d enemies alive, switching to %s"),
		Alive, Next ? *Next->GetName() : TEXT("none (out of range)"));

	SetLockTarget(Next);
	if (!Next)
	{
		ShowFeedback(TEXT("No more targets nearby"), FColor(168, 156, 149), 0.8f, 0.9f);
	}
}

void AWardenCharacter::HandleLockTargetDied()
{
	if (bIsLockedOn)
	{
		RetargetLock(LockTarget.Get());
	}
}

void AWardenCharacter::ToggleLockOn()
{
	if (bIsLockedOn)
	{
		SetLockTarget(nullptr);
		return;
	}

	if (AActor* Target = FindLockTarget(nullptr, 0.3f))
	{
		SetLockTarget(Target);
	}
	else
	{
		ShowFeedback(TEXT("No target in sight"), FColor(168, 156, 149), 0.7f, 0.9f);
	}
}

void AWardenCharacter::UpdateLockOn()
{
	if (!bIsLockedOn)
	{
		return;
	}

	AActor* Target = LockTarget.Get();

	// target destroyed, dead or out of range: hop to the next enemy, or let go
	if (!Target || !IsLockable(Target) || FVector::Dist(GetActorLocation(), Target->GetActorLocation()) > LockOnRange * 1.5f)
	{
		RetargetLock(Target);
		Target = LockTarget.Get();
		if (!Target)
		{
			return;
		}
	}

	AController* PC = GetController();
	if (!PC)
	{
		return;
	}

	const FVector From = GetCameraBoom()->GetComponentLocation();
	const FVector Aim = Target->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
	FRotator Desired = (Aim - From).Rotation();
	Desired.Pitch = FMath::Clamp(Desired.Pitch - 8.0f, -35.0f, 5.0f);
	Desired.Roll = 0.0f;
	PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Desired, static_cast<float>(FApp::GetDeltaTime()), LockOnTurnSpeed));
}

void AWardenCharacter::DoComboAttackStart()
{
	if (UAetherionCombatMemory* Memory = GetWorld()->GetSubsystem<UAetherionCombatMemory>())
	{
		Memory->RecordPlayerAttack(false, 0, ECleaveTiming::Good);
	}

	if (bHeroActive && HeroComboMontages.Num() > 0)
	{
		HeroComboPressed();
		return;
	}

	Super::DoComboAttackStart();
}

void AWardenCharacter::FaceCamera()
{
	if (const AController* PC = GetController())
	{
		SetActorRotation(FRotator(0.0f, PC->GetControlRotation().Yaw, 0.0f));
	}
}

void AWardenCharacter::HeroComboPressed()
{
	if (CurrentHP <= 0.0f)
	{
		return;
	}

	if (!bHeroAttacking)
	{
		PlayHeroCombo(0);
	}
	else if (bHeroPastChainPoint)
	{
		PlayHeroCombo(HeroComboIndex + 1);
	}
	else
	{
		bHeroQueued = true;
	}
}

void AWardenCharacter::PlayHeroCombo(int32 Index)
{
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	if (!Anim || HeroComboMontages.Num() == 0)
	{
		return;
	}

	const int32 Slot = Index % HeroComboMontages.Num();
	UAnimMontage* Montage = HeroComboMontages[Slot].LoadSynchronous();
	if (!Montage)
	{
		return;
	}

	FaceCamera();
	const float Duration = Anim->Montage_Play(Montage, HeroAttackPlayRate, EMontagePlayReturnType::Duration, 0.0f, true);
	if (Duration <= 0.0f)
	{
		StopHeroCombo();
		return;
	}
	Anim->Montage_SetEndDelegate(HeroMontageEnded, Montage);

	bHeroAttacking = true;
	bHeroQueued = false;
	bHeroPastChainPoint = false;
	HeroComboIndex = Slot;

	NotifyEnemiesOfIncomingAttack();

	FTimerManager& Timers = GetWorldTimerManager();
	Timers.SetTimer(HeroHitTimer, this, &AWardenCharacter::HeroHitPoint, FMath::Max(Duration * HeroHitFraction, 0.01f), false);
	Timers.SetTimer(HeroChainTimer, this, &AWardenCharacter::HeroChainPoint, FMath::Max(Duration * HeroChainFraction, 0.02f), false);
}

void AWardenCharacter::HeroHitPoint()
{
	DoAttackTrace(HeroTraceBone);
}

void AWardenCharacter::HeroChainPoint()
{
	bHeroPastChainPoint = true;
	if (bHeroQueued)
	{
		PlayHeroCombo(HeroComboIndex + 1);
	}
}

void AWardenCharacter::OnHeroMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// interruptions come from the next swing or a Cleave, which set their own state
	if (!bInterrupted)
	{
		StopHeroCombo();
	}
}

void AWardenCharacter::StopHeroCombo()
{
	GetWorldTimerManager().ClearTimer(HeroHitTimer);
	GetWorldTimerManager().ClearTimer(HeroChainTimer);
	bHeroAttacking = false;
	bHeroQueued = false;
	bHeroPastChainPoint = false;
	HeroComboIndex = 0;
}

void AWardenCharacter::HandleDeath()
{
	StopHeroCombo();
	if (bDodging)
	{
		EndDodge();
	}
	SetLockTarget(nullptr);
	Super::HandleDeath();

	// the hero has no full ragdoll, so play its death animation instead
	if (bHeroActive)
	{
		GetMesh()->SetSimulatePhysics(false);
		if (UAnimSequenceBase* DeathAnim = HeroDeathAnimation.LoadSynchronous())
		{
			GetMesh()->PlayAnimation(DeathAnim, false);
		}
	}
}

void AWardenCharacter::OnMeleeHit(AActor* HitActor, float Damage)
{
	Essence->GainEssence(EssencePerHit);
	PlaySfx(EssenceSound, 0.35f);
	StartHitStop(HitStopDuration);
	PlayImpactShake(0.35f);
	TrackKill(HitActor);
}

void AWardenCharacter::TrackKill(AActor* Victim)
{
	if (ACombatEnemy* Enemy = Cast<ACombatEnemy>(Victim))
	{
		Enemy->OnEnemyDied.AddUniqueDynamic(this, &AWardenCharacter::HandleEnemyKilled);
	}
}

void AWardenCharacter::HandleEnemyKilled()
{
	Essence->GainEssence(EssencePerKill);
	if (UAetherionCombatMemory* Memory = GetWorld()->GetSubsystem<UAetherionCombatMemory>())
	{
		Memory->RecordKill();
	}
	PlaySfx(EssenceSound, 0.7f);
	ShowFeedback(FString::Printf(TEXT("Kill  +%.0f Essence"), EssencePerKill), FColor(255, 179, 107), 1.0f);
}

void AWardenCharacter::DoInfernoCleave()
{
	UE_LOG(LogTemp, Log, TEXT("[Aetherion] Inferno Cleave pressed"));

	if (CurrentHP <= 0.0f)
	{
		return;
	}

	if (!Essence->TrySpendEssence(GetCleaveCost()))
	{
		ShowFeedback(FString::Printf(TEXT("Need %.0f Essence"), GetCleaveCost()), FColor(168, 156, 149), 1.0f, 0.9f);
		PlaySfx(DeniedSound, 0.6f);
		BP_OnCleaveDenied();
		return;
	}

	// judge the press against the previous swing: Perfect as its blow lands, too early if mashed
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bChainOpen = CleaveStage > 0 && CleaveStage < 3 && Now <= NextPerfectTime + ChainWindow;

	ECleaveTiming Timing = ECleaveTiming::Good;
	int32 Stage = 1;
	if (bChainOpen)
	{
		if (Now < NextPerfectTime - TooEarlyMargin)
		{
			Timing = ECleaveTiming::Off;
			Stage = 0;
		}
		else
		{
			Timing = FMath::Abs(Now - NextPerfectTime) <= PerfectWindow ? ECleaveTiming::Perfect : ECleaveTiming::Good;
			Stage = CleaveStage + 1;
		}
	}

	CleaveStage = Stage;
	if (UAetherionCombatMemory* Memory = GetWorld()->GetSubsystem<UAetherionCombatMemory>())
	{
		Memory->RecordPlayerAttack(true, Stage, Timing);
	}

	// swing where the camera is looking
	if (const AController* PC = GetController())
	{
		SetActorRotation(FRotator(0.0f, PC->GetControlRotation().Yaw, 0.0f));
	}

	// play the swing, without letting a cached basic attack interrupt it
	UAnimMontage* SwingMontage = CleaveMontage;
	float SwingStart = 0.0f;
	if (bHeroActive && HeroCleaveMontages.IsValidIndex(Stage))
	{
		if (UAnimMontage* HeroSwing = HeroCleaveMontages[Stage].LoadSynchronous())
		{
			SwingMontage = HeroSwing;
			SwingStart = HeroCleaveStartTime;
		}
	}
	// the next press is Perfect at this swing's sweet spot
	float SweetSpot = ComboSweetSpotFallback;
	if (SwingMontage)
	{
		SweetSpot = FMath::Max(0.0f, SwingMontage->GetPlayLength() - SwingStart) * ComboSweetSpotFraction / 1.1f;
	}
	NextPerfectTime = Now + SweetSpot;

	if (SwingMontage)
	{
		CachedAttackInputTime = 0.0f;
		StopHeroCombo();
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(SwingMontage, 1.1f, EMontagePlayReturnType::MontageLength, SwingStart, true);
		}
	}

	NotifyEnemiesOfIncomingAttack();

	float Damage = StageValue(StageDamage, Stage, 1.0f);
	if (Timing == ECleaveTiming::Perfect)
	{
		Damage *= PerfectDamageMultiplier;
	}

	const int32 Hits = ApplyCleaveDamage(Stage, Damage);

	// the fused Progenitor reshapes the Cleave
	const EEssenceFusion Fusion = GetFusion();
	if (Fusion == EEssenceFusion::Umbral)
	{
		if (Stage == 3)
		{
			UmbralRift();
		}
		if (Timing == ECleaveTiming::Perfect)
		{
			if (UAetherionCombatMemory* Memory = GetWorld()->GetSubsystem<UAetherionCombatMemory>())
			{
				Memory->CloakPlayer(UmbralCloakDuration);
			}
			ShowFeedback(TEXT("SHADOWED"), AetherionEssence::Color(EEssenceFusion::Umbral).ToFColor(true), UmbralCloakDuration, 1.2f);
		}
	}
	else if (Fusion == EEssenceFusion::Blood && Hits > 0)
	{
		ApplyHealing(Hits * BloodDrainPerHit + (Stage == 3 ? Hits * BloodFinisherHealPerHit : 0.0f), this);
	}

	// impact: freeze, shake, and for the finisher a slow-motion beat with a camera punch
	if (Hits > 0)
	{
		StartHitStop(CleaveHitStopDuration + 0.015f * Stage);
	}
	PlayImpactShake(Hits > 0 ? 0.6f + 0.35f * Stage : 0.25f);
	if (Timing == ECleaveTiming::Perfect)
	{
		FOVKick = FMath::Max(FOVKick, PerfectFOVKick);
	}
	if (Stage == 3)
	{
		FOVKick = FinisherFOVKick;
		PendingSlowMoDuration = FinisherSlowMoDuration;
		if (Hits == 0)
		{
			StartHitStop(0.01f);
		}
	}

	SpawnCleaveEffect(Stage, Timing, StageValue(StageRadius, Stage, 300.0f), StageValue(StageArcDegrees, Stage, 120.0f));

	PlaySfx(CleaveSound, Stage == 0 ? 0.6f : 0.9f);
	if (Timing == ECleaveTiming::Perfect)
	{
		PlaySfx(PerfectSound, 0.7f);
	}

	if (Stage == 3)
	{
		PlaySfx(FinisherSound, 1.0f);
		ApplyHealing(FinisherHeal, this);
	}

	// feedback
	static const TCHAR* Roman[] = { TEXT(""), TEXT("I"), TEXT("II"), TEXT("III") };
	switch (Timing)
	{
	case ECleaveTiming::Perfect:
		ShowFeedback(FString::Printf(TEXT("PERFECT  %s"), Roman[Stage]), FColor(255, 140, 70), 0.9f, Stage == 3 ? 2.0f : 1.6f);
		break;
	case ECleaveTiming::Good:
		ShowFeedback(FString::Printf(TEXT("CLEAVE  %s"), Roman[Stage]), FColor(230, 198, 124), 0.9f, Stage == 3 ? 1.8f : 1.4f);
		break;
	default:
		ShowFeedback(TEXT("TOO EARLY"), FColor(168, 156, 149), 0.8f, 1.2f);
		break;
	}

	BP_OnInfernoCleave(Stage, Timing, StageValue(StageRadius, Stage, 300.0f), StageValue(StageArcDegrees, Stage, 120.0f));
}

int32 AWardenCharacter::ApplyCleaveDamage(int32 Stage, float Damage)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	const FVector Origin = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	const float Radius = StageValue(StageRadius, Stage, 300.0f);
	const float ArcDegrees = StageValue(StageArcDegrees, Stage, 120.0f);
	const float Knockback = StageValue(StageKnockback, Stage, 400.0f);
	const bool bFullRing = ArcDegrees >= 359.0f;
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(ArcDegrees * 0.5f));

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InfernoCleave), false, this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	TSet<AActor*> AlreadyHit;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Target == this || AlreadyHit.Contains(Target))
		{
			continue;
		}

		ICombatDamageable* Damageable = Cast<ICombatDamageable>(Target);
		if (!Damageable)
		{
			// loose rock and debris caught in the arc is thrown clear
			UPrimitiveComponent* Loose = Overlap.GetComponent();
			if (Loose && Loose->IsSimulatingPhysics())
			{
				FVector Out = Loose->GetComponentLocation() - Origin;
				Out.Z = 0.0f;
				const FVector Throw = Out.IsNearlyZero() ? Forward : Out.GetSafeNormal();
				if (bFullRing || FVector::DotProduct(Forward, Throw) >= MinDot)
				{
					AlreadyHit.Add(Target);
					Loose->AddImpulse(Throw * Knockback * 1.5f + FVector::UpVector * (260.0f + 90.0f * Stage), NAME_None, true);
				}
			}
			continue;
		}

		FVector ToTarget = Target->GetActorLocation() - Origin;
		ToTarget.Z = 0.0f;
		const FVector Direction = ToTarget.IsNearlyZero() ? Forward : ToTarget.GetSafeNormal();

		if (!bFullRing && FVector::DotProduct(Forward, Direction) < MinDot)
		{
			continue;
		}

		AlreadyHit.Add(Target);
		TrackKill(Target);

		const FVector Impulse = Direction * Knockback + FVector::UpVector * MeleeLaunchImpulse;
		Damageable->ApplyDamage(Damage, this, Target->GetActorLocation(), Impulse);
		DealtDamage(Damage, Target->GetActorLocation());
	}

#if ENABLE_DRAW_DEBUG
	if (bShowDebugFeedback)
	{
		const FColor ArcColor = Stage == 0 ? FColor(168, 156, 149) : FColor(255, 106, 43);
		const FVector Floor = Origin - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.0f);
		const int32 Segments = 24;
		const float Half = bFullRing ? 180.0f : ArcDegrees * 0.5f;
		FVector Prev = Floor + Forward.RotateAngleAxis(-Half, FVector::UpVector) * Radius;
		if (!bFullRing)
		{
			DrawDebugLine(World, Floor, Prev, ArcColor, false, 0.35f, 0, 4.0f);
		}
		for (int32 i = 1; i <= Segments; ++i)
		{
			const float Angle = -Half + (2.0f * Half) * i / Segments;
			const FVector Next = Floor + Forward.RotateAngleAxis(Angle, FVector::UpVector) * Radius;
			DrawDebugLine(World, Prev, Next, ArcColor, false, 0.35f, 0, 4.0f);
			Prev = Next;
		}
		if (!bFullRing)
		{
			DrawDebugLine(World, Floor, Prev, ArcColor, false, 0.35f, 0, 4.0f);
		}
	}
#endif

	return AlreadyHit.Num();
}

void AWardenCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	if (CurrentHP <= 0.0f || Healing <= 0.0f)
	{
		return;
	}

	CurrentHP = FMath::Min(MaxHP, CurrentHP + Healing);
	if (LifeBarWidget)
	{
		LifeBarWidget->SetLifePercentage(CurrentHP / MaxHP);
	}
	ShowFeedback(FString::Printf(TEXT("+%.1f Vitality"), Healing), FColor(217, 71, 90), 1.0f);
}

void AWardenCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateGameFeel();
	UpdateDodge();
	UpdateLockOn();
	UpdateFusionWorld();

	// fell off the Shard
	if (CurrentHP > 0.0f && GetActorLocation().Z < VoidDeathZ)
	{
		InvulnerableUntil = -1.0f;
		ApplyDamage(1000.0f, nullptr, GetActorLocation(), FVector::ZeroVector);
	}

	// let an unfinished chain expire once it falls out of the window
	if (CleaveStage > 0 && GetWorld()->GetTimeSeconds() > NextPerfectTime + ChainWindow)
	{
		CleaveStage = 0;
	}

	if (!IsLocallyControlled())
	{
		return;
	}

#if ENABLE_DRAW_DEBUG
	if (bShowDebugFeedback && GEngine)
	{
		const FString Chain = CleaveStage == 0 ? TEXT("-") : FString::Printf(TEXT("%d / 3"), CleaveStage);
		GEngine->AddOnScreenDebugMessage(9001, 0.0f, FColor(230, 198, 124),
			FString::Printf(TEXT("Essence %.0f / %.0f    Chain %s    Cleave key: %s"),
				Essence->GetEssence(), Essence->GetMaxEssence(), *Chain,
				bCleaveInputBound ? TEXT("E ready") : TEXT("NOT CONNECTED")));
	}
#endif
}

void AWardenCharacter::ShowFeedback(const FString& Message, const FColor& Color, float Duration, float Scale) const
{
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AWardenHUD* WardenHUD = Cast<AWardenHUD>(PC->GetHUD()))
		{
			WardenHUD->PushCallout(Message, FLinearColor(Color), Scale, Duration);
		}
	}

	if (bShowDebugFeedback && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, Color, Message, false, FVector2D(1.4f, 1.4f));
	}
}
