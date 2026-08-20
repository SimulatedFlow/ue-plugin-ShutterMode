// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShutterModeTypes.h"
#include "ShutterModeStatics.generated.h"

class APlayerController;
class AShutterModeCamera;
class UShutterFilterAsset;
class UShutterModeSubsystem;

/**
 * One-node shortcuts into the photo mode, so a level or player Blueprint never has to spell out
 * "get world subsystem" first. Everything here is a thin forward to UShutterModeSubsystem; nothing
 * has behaviour of its own, and every call is a no-op with a sane return value when there is no
 * photo mode in this world.
 */
UCLASS()
class SHUTTERMODE_API UShutterModeStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The photo mode for this context's world, or null outside a game world. */
	UFUNCTION(BlueprintPure, Category = "ShutterMode", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Shutter Mode"))
	static UShutterModeSubsystem* GetShutterMode(const UObject* WorldContextObject);

	/** Open photo mode. Pass null for the first local player controller. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode", meta = (WorldContext = "WorldContextObject"))
	static bool EnterPhotoMode(const UObject* WorldContextObject, APlayerController* PlayerController = nullptr);

	/** Close photo mode and put the game back the way it was. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode", meta = (WorldContext = "WorldContextObject"))
	static void ExitPhotoMode(const UObject* WorldContextObject);

	/** One binding for the whole feature: enter if closed, exit if open. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode", meta = (WorldContext = "WorldContextObject"))
	static bool TogglePhotoMode(const UObject* WorldContextObject, APlayerController* PlayerController = nullptr);

	UFUNCTION(BlueprintPure, Category = "ShutterMode", meta = (WorldContext = "WorldContextObject"))
	static bool IsInPhotoMode(const UObject* WorldContextObject);

	/**
	 * Take a photo. 0 uses the project's default resolution multiplier.
	 * The file is not on disk when this returns - listen to On Photo Captured for the path.
	 */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode", meta = (WorldContext = "WorldContextObject"))
	static bool CapturePhoto(const UObject* WorldContextObject, int32 ResolutionMultiplier = 0);

	UFUNCTION(BlueprintPure, Category = "ShutterMode|State", meta = (WorldContext = "WorldContextObject"))
	static FShutterModeState GetState(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|State", meta = (WorldContext = "WorldContextObject"))
	static void SetState(const UObject* WorldContextObject, const FShutterModeState& NewState);

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Filters", meta = (WorldContext = "WorldContextObject"))
	static void NextFilter(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "ShutterMode|Filters", meta = (WorldContext = "WorldContextObject"))
	static void PreviousFilter(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "ShutterMode|Filters", meta = (WorldContext = "WorldContextObject"))
	static FString GetActiveFilterName(const UObject* WorldContextObject);

	/** Focus on whatever is in the centre of frame and switch auto focus off. */
	UFUNCTION(BlueprintCallable, Category = "ShutterMode|State", meta = (WorldContext = "WorldContextObject"))
	static bool SetFocusFromScreenCenter(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "ShutterMode", meta = (WorldContext = "WorldContextObject"))
	static AShutterModeCamera* GetPhotoCamera(const UObject* WorldContextObject);
};
