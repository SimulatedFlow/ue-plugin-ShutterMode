// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "ShutterModeTypes.h"
#include "ShutterModeSubsystem.generated.h"

class AShutterModeCamera;
class APlayerController;
class UCanvas;
class UShutterFilterAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FShutterModeSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShutterPhotoCapturedSignature, const FString&, FilePath);

/** Internal steps of the capture sequence. */
enum class EShutterCaptureStage : uint8
{
	Idle,
	/** Overlays are off, waiting for a clean frame to be drawn before the shutter fires. */
	Settling,
	/** The engine has the request and we are waiting for the file to hit the disk. */
	Waiting
};

/**
 * The state holder and the only thing a game has to talk to.
 *
 * Owns the photo state, the filter list, the photo camera, the composition overlay and - the part
 * that actually matters - the restore point. See FShutterModeRestorePoint: entering photo mode
 * writes down every single thing it is about to change, and leaving replays exactly that. Enter and
 * exit as often as you like, in any order, across pawn deaths and level changes; the game comes
 * back the way it was or the step is skipped, never guessed.
 *
 * Console commands (all of them operate on the first local player's world):
 *   ShutterMode.Enter, ShutterMode.Exit, ShutterMode.Capture [multiplier],
 *   ShutterMode.Filter <index>, ShutterMode.Guides <0|1>
 */
UCLASS(meta = (DisplayName = "Shutter Mode Subsystem"))
class SHUTTERMODE_API UShutterModeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	/** The capture sequence has to keep counting frames while the game is paused - it always is. */
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject interface

	/** Shortcut from anything with a world. Returns null outside a game world. */
	static UShutterModeSubsystem* Get(const UObject* WorldContextObject);

	// ---- Session ---------------------------------------------------------------------------

	/**
	 * Open photo mode. Pass null to use the first local player controller.
	 * Returns false if photo mode is already open or there is no controller to work with.
	 */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode")
	bool EnterPhotoMode(APlayerController* PlayerController);

	/** Close photo mode and replay the restore point. Safe to call when nothing is open. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode")
	void ExitPhotoMode();

	UFUNCTION(BlueprintPure, Category = "ShutterMode")
	bool IsInPhotoMode() const { return bInPhotoMode; }

	/** Enter if closed, exit if open - one key binding for the whole feature. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode")
	bool TogglePhotoMode(APlayerController* PlayerController);

	UFUNCTION(BlueprintPure, Category = "ShutterMode")
	AShutterModeCamera* GetPhotoCamera() const { return PhotoCamera; }

	// ---- Photo state ------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "ShutterMode|State")
	FShutterModeState GetState() const { return State; }

	/** Overwrite the whole photo state, e.g. to restore a preset. Values are clamped on the way in. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|State")
	void SetState(const FShutterModeState& NewState);

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|State")
	void SetFieldOfView(float NewFieldOfView);

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|State")
	void SetAperture(float NewAperture);

	/** Turn auto focus off and pin the focus plane to a distance in centimetres. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|State")
	void SetFocusDistance(float NewFocusDistance);

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|State")
	void SetAutoFocus(bool bEnabled);

	/** Trace out of the centre of frame once and pin the focus there. Returns false if nothing was hit. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|State")
	bool SetFocusFromScreenCenter();

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|State")
	void SetGuidesEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|State")
	void SetLetterboxEnabled(bool bEnabled);

	/** Non-UFUNCTION write access for the camera pawn, which owns FOV, roll and auto focus. */
	FShutterModeState& GetMutableState() { return State; }

	// ---- Filters -------------------------------------------------------------------------------

	/** The filter wheel in wheel order. Loaded from the project settings the first time it is needed. */
	UFUNCTION(BlueprintPure, Category = "ShutterMode|Filters")
	TArray<UShutterFilterAsset*> GetFilters() const;

	UFUNCTION(BlueprintPure, Category = "ShutterMode|Filters")
	int32 GetFilterCount() const { return Filters.Num(); }

	/** Replace the filter wheel at runtime, e.g. to unlock filters as the player progresses. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Filters")
	void SetFilters(const TArray<UShutterFilterAsset*>& NewFilters);

	/** Select a filter. The index wraps, so a "next" button never needs bounds checks. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Filters")
	void SetFilterIndex(int32 NewIndex);

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Filters")
	void NextFilter();

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Filters")
	void PreviousFilter();

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Filters")
	void SetFilterIntensity(float NewIntensity);

	UFUNCTION(BlueprintPure, Category = "ShutterMode|Filters")
	UShutterFilterAsset* GetActiveFilter() const;

	UFUNCTION(BlueprintPure, Category = "ShutterMode|Filters")
	FString GetActiveFilterName() const;

	// ---- Capture ---------------------------------------------------------------------------------

	/**
	 * Take the picture. ResolutionMultiplier is 1, 2 or 4; 0 uses the project default.
	 *
	 * Overlays are hidden first, a frame is allowed to draw, and only then does the shutter fire -
	 * so guides and letterbox are never baked into a photo (unless Burn In Letterbox is on).
	 * The finished path arrives through OnPhotoCaptured, not through the return value: the file
	 * does not exist yet when this returns.
	 */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Capture")
	bool Capture(int32 ResolutionMultiplier = 0);

	UFUNCTION(BlueprintPure, Category = "ShutterMode|Capture")
	bool IsCapturePending() const { return CaptureStage != EShutterCaptureStage::Idle; }

	/** Absolute folder photos are written to. Created on demand. */
	UFUNCTION(BlueprintPure, Category = "ShutterMode|Capture")
	FString GetPhotoDirectory() const;

	// ---- HUD hand-off --------------------------------------------------------------------------

	/**
	 * Register a non-actor object (a UMG widget, typically) that implements IShutterModeAware.
	 * Actors are found automatically and do not need this.
	 */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|HUD")
	void RegisterAwareObject(UObject* Object);

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|HUD")
	void UnregisterAwareObject(UObject* Object);

	// ---- Delegates ------------------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "ShutterMode|Events")
	FShutterModeSignature OnPhotoModeEntered;

	UPROPERTY(BlueprintAssignable, Category = "ShutterMode|Events")
	FShutterModeSignature OnPhotoModeExited;

	/** Fired once the file is on disk, with its absolute path. */
	UPROPERTY(BlueprintAssignable, Category = "ShutterMode|Events")
	FShutterPhotoCapturedSignature OnPhotoCaptured;

protected:
	//~ Begin UWorldSubsystem interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem interface

	/** Spawn (or reuse) the photo camera and put it where the player was looking from. */
	AShutterModeCamera* SpawnPhotoCamera(APlayerController* PlayerController);

	/** Take the snapshot of everything we are about to change. */
	void RecordRestorePoint(APlayerController* PlayerController);

	/** Replay the snapshot. Every step is individually guarded - see the struct's comment. */
	void ApplyRestorePoint();

	/** Tell every IShutterModeAware actor and registered object that photo mode started/ended. */
	void NotifyAwareObjects(bool bEntering);

	/** Keep the view alive while the world is paused - see the comment on the implementation. */
	void PumpCameraManagerWhilePaused();

	/** Load the filter wheel from the project settings, once. */
	void EnsureFiltersLoaded();

	/** Copy the filter's suggested vignette/grain/fringe into the state. */
	void ApplyFilterDefaultsToState();

	// ---- Composition overlay --------------------------------------------------------------------

	void RegisterGuideRenderer();
	void UnregisterGuideRenderer();
	void DrawGuides(UCanvas* Canvas, APlayerController* PlayerController);

	// ---- Capture pipeline -----------------------------------------------------------------------

	void IssueScreenshotRequest();
	void FinishCapture(bool bSucceeded);
	void HandleScreenshotProcessed();

private:
	UPROPERTY(Transient)
	TObjectPtr<AShutterModeCamera> PhotoCamera = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UShutterFilterAsset>> Filters;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UObject>> RegisteredAwareObjects;

	UPROPERTY(Transient)
	FShutterModeState State;

	/** THE restore point. Written on enter, replayed on exit, never partially applied. */
	UPROPERTY(Transient)
	FShutterModeRestorePoint RestorePoint;

	bool bInPhotoMode = false;
	bool bFiltersLoaded = false;

	FDelegateHandle GuideDrawHandle;
	FDelegateHandle ScreenshotProcessedHandle;

	EShutterCaptureStage CaptureStage = EShutterCaptureStage::Idle;
	int32 CaptureFramesRemaining = 0;
	int32 CaptureWatchdogFrames = 0;
	int32 PendingResolutionMultiplier = 1;
	FString PendingPhotoPath;

	/** Overlay flags stashed for the duration of a capture and put back afterwards. */
	bool bGuidesBeforeCapture = false;
	bool bLetterboxBeforeCapture = false;
};
