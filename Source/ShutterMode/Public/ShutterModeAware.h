// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ShutterModeAware.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable, meta = (DisplayName = "Shutter Mode Aware"))
class UShutterModeAware : public UInterface
{
	GENERATED_BODY()
};

/**
 * "Photo mode is starting - get out of the picture."
 *
 * ShutterMode knows nothing about your HUD, and that is the whole point. Every foreign photo mode
 * that ships with a hard-coded "hide the HUD" step breaks the moment your project's HUD is not the
 * one it expected. Instead, anything that should disappear from the frame implements this
 * interface and hides itself - a UMG widget, a nameplate actor, a quest marker, a minimap.
 *
 * Two ways to get called:
 *  - Actors: found automatically when photo mode starts, no registration needed.
 *  - Anything else (widgets are the common case, they are not actors): register the object once
 *    with UShutterModeSubsystem::RegisterAwareObject.
 *
 * Whoever was told that photo mode started is remembered in the restore point and told again when
 * it ends - even if it was unregistered in between. Nothing stays hidden by accident.
 */
class SHUTTERMODE_API IShutterModeAware
{
	GENERATED_BODY()

public:
	/** Photo mode just started. Hide anything that is not part of the picture. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ShutterMode")
	void OnPhotoModeEnter();

	/** Photo mode ended. Put yourself back exactly the way you were. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ShutterMode")
	void OnPhotoModeExit();
};
