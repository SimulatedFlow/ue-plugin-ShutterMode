// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "ShutterModeTypes.h"
#include "ShutterModeSettings.generated.h"

class AShutterModeCamera;
class UShutterFilterAsset;

/**
 * Project-wide defaults for ShutterMode.
 * Project Settings > Plugins > ShutterMode; stored in DefaultGame.ini.
 *
 * Everything here is a starting value. The subsystem's setters, the ShutterMode.* console commands
 * and the photo state itself override them at runtime without ever writing to the config.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ShutterMode"))
class SHUTTERMODE_API UShutterModeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UShutterModeSettings();

	//~ Begin UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings interface

	/** Convenience accessor. Never returns null. */
	static const UShutterModeSettings& Get();

	// ---- Session ---------------------------------------------------------------------------

	/**
	 * Pause the game while photo mode is open.
	 * Turn this OFF for network play: pausing is a local, single-player concept and a client that
	 * pauses itself only falls behind the server. Everything else in the plugin works without it.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Session")
	bool bPauseGame = true;

	/**
	 * Stop the player's own pawn from reacting to movement and look input while photo mode is open.
	 * Matters mostly when bPauseGame is off - otherwise the pawn is frozen anyway.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Session")
	bool bDisablePlayerInput = true;

	/** Blend time in seconds when the view moves to the photo camera. 0 is an instant cut. */
	UPROPERTY(Config, EditAnywhere, Category = "Session", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float EnterBlendTime = 0.25f;

	/** Blend time in seconds when the view goes back to the game. */
	UPROPERTY(Config, EditAnywhere, Category = "Session", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float ExitBlendTime = 0.25f;

	/** Show the mouse cursor while photo mode is open (a mouse-driven photo UI wants this). */
	UPROPERTY(Config, EditAnywhere, Category = "Session")
	bool bShowMouseCursor = true;

	/** Which pawn class to spawn as the photo camera. Empty falls back to AShutterModeCamera. */
	UPROPERTY(Config, EditAnywhere, Category = "Session", meta = (AllowAbstract = "false"))
	TSoftClassPtr<AShutterModeCamera> CameraClass;

	// ---- Camera ----------------------------------------------------------------------------

	/** Free fly or orbit when photo mode opens. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	EShutterCameraMode DefaultCameraMode = EShutterCameraMode::FreeFly;

	/**
	 * The leash. How far in centimetres the camera may get from its subject.
	 * Without a leash the player flies out of the level and photographs the back side of the world;
	 * this single number is the difference between a photo mode and a noclip cheat.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0", UIMax = "10000.0"))
	float MaxDistanceFromSubject = 800.0f;

	/** Enforce MaxDistanceFromSubject. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	bool bLeashEnabled = true;

	/** Trace against world geometry so the camera stops at walls instead of passing through them. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	bool bCollisionEnabled = true;

	/** Trace channel used for the wall check. Camera is the sane default. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	TEnumAsByte<ECollisionChannel> CameraCollisionChannel = ECC_Camera;

	/** How far in front of a hit wall the camera parks, in centimetres. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0", UIMax = "100.0"))
	float CollisionPadding = 12.0f;

	/** Flight speed in centimetres per second. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "1.0", UIMax = "5000.0"))
	float MoveSpeed = 400.0f;

	/** Degrees of rotation per unit of look input. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "0.01", UIMax = "10.0"))
	float LookSensitivity = 1.0f;

	/** Degrees of roll per second while roll input is held. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "1.0", UIMax = "180.0"))
	float RollSpeed = 45.0f;

	/** Field-of-view degrees per unit of zoom input. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "0.1", UIMax = "60.0"))
	float ZoomSpeed = 5.0f;

	/** Hard pitch limit in degrees, kept below 90 so the camera never gimbal-flips. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "1.0", ClampMax = "89.9"))
	float MaxPitch = 87.0f;

	/** Closest the orbit camera may sit to its subject, in centimetres. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "10.0", UIMax = "2000.0"))
	float MinOrbitDistance = 120.0f;

	// ---- Optics ------------------------------------------------------------------------------

	/** Field of view a fresh photo session starts with. */
	UPROPERTY(Config, EditAnywhere, Category = "Optics", meta = (ClampMin = "10.0", ClampMax = "120.0"))
	float DefaultFieldOfView = 70.0f;

	/** Aperture a fresh photo session starts with. */
	UPROPERTY(Config, EditAnywhere, Category = "Optics", meta = (ClampMin = "1.2", ClampMax = "22.0"))
	float DefaultAperture = 2.8f;

	/** Start with auto focus on. */
	UPROPERTY(Config, EditAnywhere, Category = "Optics")
	bool bAutoFocusByDefault = true;

	/** How far the auto-focus trace reaches, in centimetres. Nothing hit means focus at infinity. */
	UPROPERTY(Config, EditAnywhere, Category = "Optics", meta = (ClampMin = "100.0", UIMax = "200000.0"))
	float AutoFocusTraceDistance = 50000.0f;

	/** Trace channel for the auto-focus probe out of the centre of frame. */
	UPROPERTY(Config, EditAnywhere, Category = "Optics")
	TEnumAsByte<ECollisionChannel> AutoFocusChannel = ECC_Visibility;

	/**
	 * Simulated sensor width in millimetres. Together with the aperture this decides how strong the
	 * defocus actually is; 36 is a full-frame stills camera, 24.89 is Super 35 cinema.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Optics", meta = (ClampMin = "1.0", UIMax = "100.0"))
	float SensorWidth = 36.0f;

	// ---- Filters -------------------------------------------------------------------------------

	/**
	 * The filter wheel, in order. Soft references: nothing is loaded until photo mode opens for the
	 * first time, so a project that never uses photo mode pays nothing for shipping the filters.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Filters", meta = (AllowedClasses = "/Script/ShutterMode.ShutterFilterAsset"))
	TArray<TSoftObjectPtr<UShutterFilterAsset>> Filters;

	/** Which entry of Filters a fresh photo session starts on. */
	UPROPERTY(Config, EditAnywhere, Category = "Filters", meta = (ClampMin = "0"))
	int32 DefaultFilterIndex = 0;

	// ---- Framing ---------------------------------------------------------------------------------

	/** Start with the composition guides visible. */
	UPROPERTY(Config, EditAnywhere, Category = "Framing")
	bool bGuidesEnabledByDefault = false;

	/** Which guides are drawn when guides are on. */
	UPROPERTY(Config, EditAnywhere, Category = "Framing", meta = (Bitmask, BitmaskEnum = "/Script/ShutterMode.EShutterGuide"))
	int32 DefaultGuideFlags = static_cast<int32>(EShutterGuide::Thirds) | static_cast<int32>(EShutterGuide::CenterCross);

	/** Start with the cinema bars on. */
	UPROPERTY(Config, EditAnywhere, Category = "Framing")
	bool bLetterboxByDefault = false;

	/** Aspect the cinema bars mask down to. 2.39 scope, 1.85 flat, 1.7777 for 16:9. */
	UPROPERTY(Config, EditAnywhere, Category = "Framing", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float DefaultLetterboxRatio = 2.39f;

	/**
	 * Bake the letterbox bars into the saved photo.
	 * Guides never make it into a photo - a guide is a help, not part of the picture - but bars are
	 * a real framing decision, so this one is a choice.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Framing")
	bool bBurnInLetterbox = false;

	/** Draw the little filter / f-stop / focus / FOV read-out with the guides. */
	UPROPERTY(Config, EditAnywhere, Category = "Framing")
	bool bShowStatusLine = true;

	/** Tint of the guide lines. */
	UPROPERTY(Config, EditAnywhere, Category = "Framing")
	FLinearColor GuideColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	// ---- Capture ------------------------------------------------------------------------------------

	/** Folder for saved photos, relative to the project's Saved directory. */
	UPROPERTY(Config, EditAnywhere, Category = "Capture")
	FString PhotoDirectory = TEXT("Photos");

	/** File name prefix. Empty uses the project name. */
	UPROPERTY(Config, EditAnywhere, Category = "Capture")
	FString PhotoFilePrefix;

	/** Resolution multiplier used by the parameterless capture call: 1x, 2x or 4x. */
	UPROPERTY(Config, EditAnywhere, Category = "Capture", meta = (ClampMin = "1", ClampMax = "8"))
	int32 DefaultResolutionMultiplier = 1;

	/**
	 * How many frames to wait between hiding the overlays and firing the shot.
	 * A screenshot captures a frame that has already been drawn, so changing state and triggering
	 * the capture in the same tick photographs the state BEFORE the change - guides and all. One
	 * frame is enough in practice; two is the safe default.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Capture", meta = (ClampMin = "1", ClampMax = "10"))
	int32 CaptureFrameDelay = 2;
};
