// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ShutterModeTypes.h"

void FShutterModeState::Sanitize()
{
	FieldOfView = FMath::Clamp(FieldOfView, 10.0f, 120.0f);
	FocusDistance = FMath::Max(FocusDistance, 1.0f);
	Aperture = FMath::Clamp(Aperture, 1.2f, 22.0f);
	Roll = FMath::UnwindDegrees(Roll);
	ExposureBias = FMath::Clamp(ExposureBias, -8.0f, 8.0f);

	FilterIndex = FMath::Max(FilterIndex, 0);
	FilterIntensity = FMath::Clamp(FilterIntensity, 0.0f, 1.0f);
	Vignette = FMath::Clamp(Vignette, 0.0f, 2.0f);
	Grain = FMath::Clamp(Grain, 0.0f, 2.0f);
	ChromaticAberration = FMath::Clamp(ChromaticAberration, 0.0f, 5.0f);
	Saturation = FMath::Clamp(Saturation, 0.0f, 4.0f);
	Contrast = FMath::Clamp(Contrast, 0.0f, 4.0f);
	Temperature = FMath::Clamp(Temperature, -4000.0f, 4000.0f);

	LetterboxRatio = FMath::Clamp(LetterboxRatio, 1.0f, 4.0f);
}
