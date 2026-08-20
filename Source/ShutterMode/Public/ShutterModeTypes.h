// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "ShutterModeTypes.generated.h"

class AActor;
class APlayerController;
class UWorld;

/** How the photo camera moves. */
UENUM(BlueprintType)
enum class EShutterCameraMode : uint8
{
	/** Fly anywhere inside the leash: forward/right/up are relative to where the camera looks. */
	FreeFly		UMETA(DisplayName = "Free Fly"),

	/** Circle a subject (by default the player's own pawn) at a fixed distance and always look at it. */
	OrbitSubject	UMETA(DisplayName = "Orbit Subject")
};

/** Which composition overlays the guide renderer draws. Combined as a bit mask. */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EShutterGuide : uint8
{
	None			= 0			UMETA(Hidden),
	/** Classic rule-of-thirds grid. */
	Thirds			= 1 << 0,
	/** Small cross in the exact centre of frame - also where the auto focus trace goes out. */
	CenterCross		= 1 << 1,
	/** Golden-ratio (phi) grid, slightly tighter than thirds. */
	GoldenRatio		= 1 << 2,
	/** 90% title-safe rectangle. */
	SafeFrame		= 1 << 3,
	/** Diagonals through the frame corners. */
	Diagonals		= 1 << 4
};
ENUM_CLASS_FLAGS(EShutterGuide);

/**
 * The complete look of one photo: camera optics, colour treatment and framing helpers.
 *
 * This is a single plain struct on purpose. A photo state can be copied, stored in a save game,
 * shipped as a preset by the developer or restored later without touching a single camera object.
 *
 * How the values combine with the selected UShutterFilterAsset:
 *  - Saturation / Contrast are MULTIPLIERS on top of the filter's grade (1.0 = filter unchanged).
 *  - Temperature is an OFFSET in Kelvin added to the filter's white point (0.0 = filter unchanged).
 *  - Vignette / Grain / ChromaticAberration are ABSOLUTE; picking a filter seeds them from the
 *    filter's suggested defaults, after which the player owns them.
 */
USTRUCT(BlueprintType)
struct SHUTTERMODE_API FShutterModeState
{
	GENERATED_BODY()

	// ---- Optics -------------------------------------------------------------------------

	/** Horizontal field of view in degrees. Low = telephoto and compressed, high = wide and dramatic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics", meta = (ClampMin = "10.0", ClampMax = "120.0"))
	float FieldOfView = 70.0f;

	/** Distance in centimetres to the plane that is in focus. Ignored while bAutoFocus is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics", meta = (ClampMin = "1.0", UIMax = "100000.0"))
	float FocusDistance = 1000.0f;

	/** Lens aperture. f/1.2 melts the background, f/22 keeps everything sharp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics", meta = (ClampMin = "1.2", ClampMax = "22.0"))
	float Aperture = 2.8f;

	/** Trace out of the centre of frame every frame and focus on whatever is hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics")
	bool bAutoFocus = true;

	/** Camera roll in degrees - a dutch angle, kept out of the free-look input on purpose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float Roll = 0.0f;

	/** Exposure compensation in stops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics", meta = (ClampMin = "-8.0", ClampMax = "8.0"))
	float ExposureBias = 0.0f;

	// ---- Colour -------------------------------------------------------------------------

	/** Index into the subsystem's filter list. Out-of-range values are wrapped, never clamped away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour", meta = (ClampMin = "0"))
	int32 FilterIndex = 0;

	/** How much of the selected filter is blended in. 0 = untouched image, 1 = full filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FilterIntensity = 1.0f;

	/** Absolute vignette strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Vignette = 0.4f;

	/** Absolute film-grain strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Grain = 0.0f;

	/** Absolute chromatic aberration (scene fringe) strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float ChromaticAberration = 0.0f;

	/** Saturation multiplier on top of the filter's grade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Saturation = 1.0f;

	/** Contrast multiplier on top of the filter's grade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Contrast = 1.0f;

	/** White-point offset in Kelvin on top of the filter. Negative is cooler, positive is warmer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour", meta = (ClampMin = "-4000.0", ClampMax = "4000.0"))
	float Temperature = 0.0f;

	// ---- Framing ------------------------------------------------------------------------

	/** Draw the composition guides selected in GuideFlags. Never burned into the saved photo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing")
	bool bGuides = false;

	/** Which guides to draw while bGuides is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (Bitmask, BitmaskEnum = "/Script/ShutterMode.EShutterGuide"))
	int32 GuideFlags = static_cast<int32>(EShutterGuide::Thirds) | static_cast<int32>(EShutterGuide::CenterCross);

	/** Draw black cinema bars top and bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing")
	bool bLetterbox = false;

	/** Target aspect for the letterbox: 2.39 scope, 1.85 flat, 1.7777 for 16:9. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float LetterboxRatio = 2.39f;

	/** Clamp every value back into its documented range. Cheap, call it after any external write. */
	void Sanitize();
};

/**
 * ===========================================================================================
 *  THE RESTORE POINT - the reason this plugin exists.
 * ===========================================================================================
 *
 * A photo mode is not hard because of the camera. It is hard because it reaches into five
 * unrelated systems at once - view target, pause, cursor, input routing and HUD visibility -
 * and every hand-rolled photo mode eventually forgets to put one of them back. The player ends
 * up with a stuck cursor, a game that stays paused, or a HUD that never comes home.
 *
 * So ShutterMode writes down *everything it is about to touch* before it touches it, in this
 * one struct, and replays exactly that on exit. Nothing is inferred, nothing is guessed, and
 * nothing is "reset to a sensible default" - a default is how you break somebody else's game.
 *
 * Note especially bWasPaused: if the game was ALREADY paused when the player opened photo mode
 * (a pause menu with a "take a picture" button is the common case), leaving photo mode must not
 * un-pause it. That is somebody else's pause and it stays.
 *
 * Every actor reference is weak. Between entering and leaving, the level can stream out, the
 * pawn can die and the player controller can be replaced by seamless travel. In that case the
 * corresponding restore step is skipped instead of dereferencing a dead pointer.
 */
USTRUCT()
struct SHUTTERMODE_API FShutterModeRestorePoint
{
	GENERATED_BODY()

	/** The controller photo mode was entered through. Weak: seamless travel can replace it. */
	UPROPERTY()
	TWeakObjectPtr<APlayerController> PlayerController;

	/** Whatever the controller was looking through before. Weak: the pawn can die meanwhile. */
	UPROPERTY()
	TWeakObjectPtr<AActor> ViewTarget;

	/** The world we entered in. If this no longer matches, the level changed and we bail out. */
	UPROPERTY()
	TWeakObjectPtr<UWorld> World;

	/** Was the game paused by somebody else before we arrived? Then it stays paused afterwards. */
	UPROPERTY()
	bool bWasPaused = false;

	/** Did WE pause the game? Only then are we allowed to un-pause it. */
	UPROPERTY()
	bool bPausedByUs = false;

	// ---- Player controller state --------------------------------------------------------

	UPROPERTY()
	bool bShowMouseCursor = false;

	UPROPERTY()
	bool bEnableClickEvents = false;

	UPROPERTY()
	bool bEnableMouseOverEvents = false;

	UPROPERTY()
	bool bMoveInputIgnored = false;

	UPROPERTY()
	bool bLookInputIgnored = false;

	/** APlayerController::PrimaryActorTick.bTickEvenWhenPaused */
	UPROPERTY()
	bool bControllerTickedWhenPaused = false;

	// ---- Viewport state -------------------------------------------------------------------
	// SetInputMode() is nothing but a writer for these four values, so recording them here is
	// a genuinely complete snapshot of the input mode - not an approximation of it.

	UPROPERTY()
	EMouseCaptureMode MouseCaptureMode = EMouseCaptureMode::CapturePermanently;

	UPROPERTY()
	EMouseLockMode MouseLockMode = EMouseLockMode::DoNotLock;

	UPROPERTY()
	bool bHideCursorDuringCapture = true;

	UPROPERTY()
	bool bViewportIgnoredInput = false;

	/** Objects that were told photo mode started and therefore have to be told when it ends. */
	UPROPERTY()
	TArray<TWeakObjectPtr<UObject>> NotifiedAwareObjects;

	/** False until EnterPhotoMode has filled this in. */
	UPROPERTY()
	bool bValid = false;

	void Reset() { *this = FShutterModeRestorePoint(); }
};
