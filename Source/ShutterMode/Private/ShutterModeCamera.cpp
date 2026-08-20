// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "ShutterModeCamera.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "ShutterFilterAsset.h"
#include "ShutterModeLog.h"
#include "ShutterModeSubsystem.h"

AShutterModeCamera::AShutterModeCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// ===================================================================================
	//  GOTCHA #1 - THE CAMERA HAS TO MOVE WHILE THE GAME IS PAUSED.
	// ===================================================================================
	// Photo mode pauses the game. A paused world does not tick its actors, so a camera with the
	// default tick settings is frozen the moment the player opens photo mode - they can look at
	// their own frozen screenshot and nothing else. BOTH of these are needed: the flag on the tick
	// function, and SetTickableWhenPaused, which also propagates to the component tick functions.
	// This is the number-one reason hand-built photo modes get thrown away.
	PrimaryActorTick.bTickEvenWhenPaused = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("PhotoCamera"));
	CameraComponent->SetupAttachment(SceneRoot);
	CameraComponent->bUsePawnControlRotation = false;
	CameraComponent->SetConstraintAspectRatio(false);

	// The photo camera is a viewpoint, not a physical object: it does its own wall check with a
	// line trace, so real collision would only fight the leash.
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);

	bReplicates = false;
	SetReplicatingMovement(false);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
}

void AShutterModeCamera::InitializeForSession(UShutterModeSubsystem* InOwner, const FVector& Location, const FRotator& Rotation, AActor* InSubject)
{
	OwningSubsystem = InOwner;
	Subject = InSubject;
	FallbackAnchor = Location;

	ViewRotation = FRotator(FMath::ClampAngle(Rotation.Pitch, -MaxPitch, MaxPitch), Rotation.Yaw, 0.0f);
	SetActorLocationAndRotation(Location, ViewRotation);

	OrbitDistance = FMath::Max(GetDistanceToSubject(), MinOrbitDistance);

	// The counterpart to bTickEvenWhenPaused above - see the constructor comment.
	SetTickableWhenPaused(true);
}

void AShutterModeCamera::AddMoveInput(float Forward, float Right, float Up)
{
	PendingMove += FVector(Forward, Right, Up);
}

void AShutterModeCamera::AddLookInput(float Yaw, float Pitch)
{
	PendingLook += FVector2D(Yaw, Pitch);
}

void AShutterModeCamera::AddRoll(float Amount)
{
	PendingRoll += Amount;
}

void AShutterModeCamera::ZoomBy(float Amount)
{
	PendingZoom += Amount;
}

void AShutterModeCamera::SetSpeedMultiplier(float Multiplier)
{
	SpeedMultiplier = FMath::Clamp(Multiplier, 0.01f, 100.0f);
}

void AShutterModeCamera::SetCameraMode(EShutterCameraMode NewMode)
{
	if (CameraMode == NewMode)
	{
		return;
	}

	CameraMode = NewMode;

	if (CameraMode == EShutterCameraMode::OrbitSubject)
	{
		OrbitDistance = FMath::Max(GetDistanceToSubject(), MinOrbitDistance);
	}
}

void AShutterModeCamera::SetSubject(AActor* NewSubject)
{
	Subject = NewSubject;

	if (NewSubject)
	{
		FallbackAnchor = NewSubject->GetActorLocation();
	}
}

void AShutterModeCamera::ResetToSubject()
{
	const FVector Anchor = GetAnchorLocation();
	const FVector Offset = ViewRotation.Vector() * -FMath::Max(OrbitDistance, MinOrbitDistance);
	SetActorLocation(ConstrainDesiredLocation(Anchor + Offset));
}

float AShutterModeCamera::GetDistanceToSubject() const
{
	const AActor* SubjectActor = Subject.Get();
	if (!SubjectActor)
	{
		return -1.0f;
	}

	return FVector::Dist(GetActorLocation(), SubjectActor->GetActorLocation());
}

float AShutterModeCamera::TraceFocusDistance() const
{
	const UWorld* World = GetWorld();
	if (!World || !CameraComponent)
	{
		return -1.0f;
	}

	const FVector Start = CameraComponent->GetComponentLocation();
	const FVector End = Start + CameraComponent->GetForwardVector() * FMath::Max(AutoFocusTraceDistance, 100.0f);

	FCollisionQueryParams Params(TEXT("ShutterModeAutoFocus"), /*bTraceComplex=*/true, this);
	Params.bReturnPhysicalMaterial = false;

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, AutoFocusChannel, Params))
	{
		return FMath::Max(FVector::Dist(Start, Hit.ImpactPoint), 1.0f);
	}

	return -1.0f;
}

FVector AShutterModeCamera::GetAnchorLocation() const
{
	if (const AActor* SubjectActor = Subject.Get())
	{
		return SubjectActor->GetActorLocation();
	}

	return FallbackAnchor;
}

FVector AShutterModeCamera::ConstrainDesiredLocation(const FVector& Desired) const
{
	FVector Result = Desired;
	const FVector Anchor = GetAnchorLocation();

	// --- The leash. Without it the player flies straight out of the level and photographs the
	//     back faces of the world. Clamping to a sphere around the subject keeps the shot inside
	//     the space the level artist actually built.
	if (bLeashEnabled && MaxDistanceFromSubject > 0.0f)
	{
		const FVector FromAnchor = Result - Anchor;
		const float DistanceSq = FromAnchor.SizeSquared();
		if (DistanceSq > FMath::Square(MaxDistanceFromSubject))
		{
			Result = Anchor + FromAnchor.GetSafeNormal() * MaxDistanceFromSubject;
		}
	}

	// --- The wall check. Traced from the anchor outwards, so the camera can never end up on the
	//     far side of a wall from its subject - which is the other half of "photographing the
	//     back of the world".
	if (bCollisionEnabled)
	{
		if (const UWorld* World = GetWorld())
		{
			FCollisionQueryParams Params(TEXT("ShutterModeCameraCollision"), /*bTraceComplex=*/false, this);
			if (const AActor* SubjectActor = Subject.Get())
			{
				Params.AddIgnoredActor(SubjectActor);
			}

			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Anchor, Result, CollisionChannel, Params))
			{
				Result = Hit.ImpactPoint + Hit.ImpactNormal * CollisionPadding;
			}
		}
	}

	return Result;
}

void AShutterModeCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// While the world is paused it hands out a delta time of zero, so every speed multiplied by it
	// collapses and the camera stops. Fall back to the real frame time in that case.
	const UWorld* World = GetWorld();
	const bool bPaused = World && World->IsPaused();
	const float EffectiveDelta = (bPaused || DeltaSeconds <= 0.0f) ? FApp::GetDeltaTime() : DeltaSeconds;

	if (CameraMode == EShutterCameraMode::OrbitSubject && Subject.IsValid())
	{
		TickOrbit(EffectiveDelta);
	}
	else
	{
		TickFreeFly(EffectiveDelta);
	}

	float Roll = 0.0f;

	if (UShutterModeSubsystem* Session = OwningSubsystem.Get())
	{
		FShutterModeState& State = Session->GetMutableState();

		if (!FMath::IsNearlyZero(PendingZoom))
		{
			State.FieldOfView = FMath::Clamp(State.FieldOfView - PendingZoom * ZoomSpeed, 10.0f, 120.0f);
		}

		if (!FMath::IsNearlyZero(PendingRoll))
		{
			State.Roll = FMath::UnwindDegrees(State.Roll + PendingRoll * RollSpeed * EffectiveDelta);
		}

		if (State.bAutoFocus)
		{
			const float Traced = TraceFocusDistance();
			State.FocusDistance = (Traced > 0.0f) ? Traced : FMath::Max(AutoFocusTraceDistance, 100.0f);
		}

		Roll = State.Roll;
		ApplyPhotoState(State, Session->GetActiveFilter());
	}

	SetActorRotation(FRotator(ViewRotation.Pitch, ViewRotation.Yaw, Roll));

	PendingMove = FVector::ZeroVector;
	PendingLook = FVector2D::ZeroVector;
	PendingRoll = 0.0f;
	PendingZoom = 0.0f;
}

void AShutterModeCamera::TickFreeFly(float DeltaSeconds)
{
	ViewRotation.Yaw += PendingLook.X * LookSensitivity;
	ViewRotation.Pitch = FMath::ClampAngle(ViewRotation.Pitch + PendingLook.Y * LookSensitivity, -MaxPitch, MaxPitch);
	ViewRotation.Roll = 0.0f;

	if (!PendingMove.IsNearlyZero())
	{
		const FRotator FlatRotation(ViewRotation.Pitch, ViewRotation.Yaw, 0.0f);
		const FVector Forward = FlatRotation.Vector();
		const FVector Right = FRotationMatrix(FRotator(0.0f, ViewRotation.Yaw, 0.0f)).GetUnitAxis(EAxis::Y);

		const FVector Direction =
			Forward * PendingMove.X +
			Right * PendingMove.Y +
			FVector::UpVector * PendingMove.Z;

		const FVector Desired = GetActorLocation() + Direction.GetClampedToMaxSize(1.0f) * MoveSpeed * SpeedMultiplier * DeltaSeconds;
		SetActorLocation(ConstrainDesiredLocation(Desired));
	}
	else if (bLeashEnabled && Subject.IsValid())
	{
		// The subject can move underneath a standing camera (an unpaused, networked session), so the
		// leash is re-checked even on a frame with no input.
		SetActorLocation(ConstrainDesiredLocation(GetActorLocation()));
	}
}

void AShutterModeCamera::TickOrbit(float DeltaSeconds)
{
	const FVector Anchor = GetAnchorLocation();

	ViewRotation.Yaw += PendingLook.X * LookSensitivity;
	ViewRotation.Pitch = FMath::ClampAngle(ViewRotation.Pitch + PendingLook.Y * LookSensitivity, -MaxPitch, MaxPitch);
	ViewRotation.Roll = 0.0f;

	// Forward/back dollies in and out, up/down lifts the orbit, strafe is meaningless on a circle.
	const float DistanceDelta = -PendingMove.X * MoveSpeed * SpeedMultiplier * DeltaSeconds;
	const float MaxOrbit = bLeashEnabled ? FMath::Max(MaxDistanceFromSubject, MinOrbitDistance) : 100000.0f;
	OrbitDistance = FMath::Clamp(OrbitDistance + DistanceDelta, MinOrbitDistance, MaxOrbit);

	const FVector Offset = ViewRotation.Vector() * -OrbitDistance + FVector::UpVector * (PendingMove.Z * MoveSpeed * SpeedMultiplier * DeltaSeconds);
	const FVector Desired = Anchor + Offset;

	SetActorLocation(ConstrainDesiredLocation(Desired));

	// Always keep the subject in frame - that is what the mode is for.
	ViewRotation = (Anchor - GetActorLocation()).Rotation();
	ViewRotation.Pitch = FMath::ClampAngle(ViewRotation.Pitch, -MaxPitch, MaxPitch);
	ViewRotation.Roll = 0.0f;
}

void AShutterModeCamera::ApplyPhotoState(const FShutterModeState& State, const UShutterFilterAsset* Filter)
{
	if (!CameraComponent)
	{
		return;
	}

	CameraComponent->SetFieldOfView(FMath::Clamp(State.FieldOfView, 10.0f, 120.0f));
	CameraComponent->PostProcessBlendWeight = 1.0f;

	// Rebuilt from scratch each frame: a stale bOverride_ flag from a previous look is impossible
	// to spot in a screenshot and impossible to explain to a customer.
	FPostProcessSettings& PP = CameraComponent->PostProcessSettings;
	PP = FPostProcessSettings();

	// ===================================================================================
	//  GOTCHA #2 - THE bOverride_ FLAGS.
	// ===================================================================================
	// FPostProcessSettings carries one override bit per value. Writing DepthOfFieldFstop without
	// setting bOverride_DepthOfFieldFstop changes precisely nothing, silently, and the aperture
	// slider looks broken. Every single value written below therefore sets its flag first.
	PP.bOverride_DepthOfFieldFocalDistance = true;
	PP.DepthOfFieldFocalDistance = FMath::Max(State.FocusDistance, 1.0f);

	PP.bOverride_DepthOfFieldFstop = true;
	PP.DepthOfFieldFstop = FMath::Clamp(State.Aperture, 1.2f, 22.0f);

	PP.bOverride_DepthOfFieldSensorWidth = true;
	PP.DepthOfFieldSensorWidth = FMath::Max(SensorWidth, 1.0f);

	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = State.ExposureBias;

	if (Filter)
	{
		Filter->ApplyGrading(PP, State.FilterIntensity);
	}
	else
	{
		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(1.0, 1.0, 1.0, 1.0);
		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(1.0, 1.0, 1.0, 1.0);
		PP.bOverride_WhiteTemp = true;
		PP.WhiteTemp = 6500.0f;
	}

	// The player's own trims ride on top of the filter. W is the global channel of those vectors,
	// so scaling only W leaves an intentional per-channel split tone in the filter intact.
	PP.ColorSaturation.W *= static_cast<double>(FMath::Max(State.Saturation, 0.0f));
	PP.ColorContrast.W *= static_cast<double>(FMath::Max(State.Contrast, 0.0f));
	PP.WhiteTemp = FMath::Clamp(PP.WhiteTemp + State.Temperature, 1500.0f, 15000.0f);

	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = FMath::Max(State.Vignette, 0.0f);

	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = FMath::Max(State.Grain, 0.0f);

	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = FMath::Max(State.ChromaticAberration, 0.0f);

	// A photo is a still. Motion blur on a paused frame is only ever a smear.
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.0f;
}
