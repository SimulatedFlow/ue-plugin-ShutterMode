// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Engine/EngineTypes.h"
#include "ShutterModeTypes.h"
#include "ShutterModeCamera.generated.h"

class UCameraComponent;
class UShutterFilterAsset;
class UShutterModeSubsystem;

/**
 * The camera the player flies while photo mode is open.
 *
 * Two deliberate design decisions:
 *
 * 1) NO INPUT BINDINGS IN C++. This pawn exposes AddMoveInput / AddLookInput / AddRoll / ZoomBy as
 *    BlueprintCallable and nothing else. It ships no Input Actions, no Input Mapping Context and no
 *    axis names, so dropping the plugin into a project cannot collide with that project's input
 *    setup - which is exactly what makes most camera plugins painful to integrate. Wire the four
 *    functions to whatever the game already uses.
 *
 * 2) IT MOVES WHILE THE GAME IS PAUSED. See the constructor: bTickEvenWhenPaused plus
 *    SetTickableWhenPaused. Forget either one and the camera is frozen the instant photo mode
 *    pauses the game - the single most common bug in a hand-built photo mode.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Shutter Mode Camera"))
class SHUTTERMODE_API AShutterModeCamera : public APawn
{
	GENERATED_BODY()

public:
	AShutterModeCamera();

	//~ Begin AActor interface
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	// ---- Input surface (call these from Blueprint) ---------------------------------------

	/**
	 * Move the camera. Values are -1..1 style axis input relative to where the camera looks;
	 * they are accumulated and applied once per tick, so calling this several times a frame is fine.
	 */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Camera")
	void AddMoveInput(float Forward, float Right, float Up);

	/** Turn the camera. Look input is already a per-frame delta, so it is NOT scaled by delta time. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Camera")
	void AddLookInput(float Yaw, float Pitch);

	/** Roll the camera for a dutch angle. Scaled by RollSpeed and delta time. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Camera")
	void AddRoll(float Amount);

	/** Zoom by changing field of view. Positive zooms in (narrower FOV). */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Camera")
	void ZoomBy(float Amount);

	/** Temporary speed factor for a sprint or creep modifier key. Reset to 1 when released. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Camera")
	void SetSpeedMultiplier(float Multiplier);

	// ---- Framing ----------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Camera")
	void SetCameraMode(EShutterCameraMode NewMode);

	UFUNCTION(BlueprintPure, Category = "ShutterMode|Camera")
	EShutterCameraMode GetCameraMode() const { return CameraMode; }

	/** Set the actor the leash is anchored to and the orbit turns around. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Camera")
	void SetSubject(AActor* NewSubject);

	UFUNCTION(BlueprintPure, Category = "ShutterMode|Camera")
	AActor* GetSubject() const { return Subject.Get(); }

	/** Snap back to where the subject is, at the current orbit distance. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Camera")
	void ResetToSubject();

	/** How far the camera currently is from its subject, in centimetres. -1 without a subject. */
	UFUNCTION(BlueprintPure, Category = "ShutterMode|Camera")
	float GetDistanceToSubject() const;

	/** Distance in centimetres to whatever is in the centre of frame, or -1 if nothing was hit. */
	UFUNCTION(BlueprintPure, Category = "ShutterMode|Camera")
	float TraceFocusDistance() const;

	UFUNCTION(BlueprintPure, Category = "ShutterMode|Camera")
	UCameraComponent* GetPhotoCameraComponent() const { return CameraComponent; }

	/** Push a photo state (and its filter) into the actual camera component. */
	void ApplyPhotoState(const FShutterModeState& State, const UShutterFilterAsset* Filter);

	/** Place the camera at a starting point and hand it its owning subsystem. */
	void InitializeForSession(UShutterModeSubsystem* InOwner, const FVector& Location, const FRotator& Rotation, AActor* InSubject);

	// ---- Tunables (seeded from UShutterModeSettings when the session starts) ----------------

	/** THE LEASH: how far the camera may get from its subject, in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Limits", meta = (ClampMin = "0.0"))
	float MaxDistanceFromSubject = 800.0f;

	/** Enforce the leash at all. Off means the player can fly anywhere - your call, not ours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Limits")
	bool bLeashEnabled = true;

	/** Stop at walls instead of flying through them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Limits")
	bool bCollisionEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Limits")
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Limits", meta = (ClampMin = "0.0"))
	float CollisionPadding = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Limits", meta = (ClampMin = "1.0", ClampMax = "89.9"))
	float MaxPitch = 87.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Limits", meta = (ClampMin = "10.0"))
	float MinOrbitDistance = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Movement", meta = (ClampMin = "1.0"))
	float MoveSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Movement", meta = (ClampMin = "0.01"))
	float LookSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Movement", meta = (ClampMin = "1.0"))
	float RollSpeed = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Movement", meta = (ClampMin = "0.1"))
	float ZoomSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Optics", meta = (ClampMin = "100.0"))
	float AutoFocusTraceDistance = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Optics")
	TEnumAsByte<ECollisionChannel> AutoFocusChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShutterMode|Optics", meta = (ClampMin = "1.0"))
	float SensorWidth = 36.0f;

protected:
	/** Root, so the camera component can be offset in a Blueprint subclass without moving the pawn. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShutterMode")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShutterMode")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShutterMode")
	EShutterCameraMode CameraMode = EShutterCameraMode::FreeFly;

	UPROPERTY()
	TWeakObjectPtr<AActor> Subject;

	UPROPERTY()
	TWeakObjectPtr<UShutterModeSubsystem> OwningSubsystem;

	/** Where the leash is anchored: the subject if there is one, otherwise where we started. */
	FVector GetAnchorLocation() const;

	/** Clamp Desired to the leash sphere and trace it against the world. */
	FVector ConstrainDesiredLocation(const FVector& Desired) const;

private:
	/** Accumulated input, consumed and zeroed every tick. */
	FVector PendingMove = FVector::ZeroVector;
	FVector2D PendingLook = FVector2D::ZeroVector;
	float PendingRoll = 0.0f;
	float PendingZoom = 0.0f;
	float SpeedMultiplier = 1.0f;

	/** Yaw/pitch the player has aimed, kept separate from the actor rotation so roll cannot leak in. */
	FRotator ViewRotation = FRotator::ZeroRotator;

	/** Fallback leash anchor for the case where there is no subject at all. */
	FVector FallbackAnchor = FVector::ZeroVector;

	float OrbitDistance = 400.0f;

	void TickFreeFly(float DeltaSeconds);
	void TickOrbit(float DeltaSeconds);
};
