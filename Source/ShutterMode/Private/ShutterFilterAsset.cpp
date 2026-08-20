// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "ShutterFilterAsset.h"
#include "Materials/MaterialInterface.h"

UShutterFilterAsset::UShutterFilterAsset()
{
}

void UShutterFilterAsset::ApplyGrading(FPostProcessSettings& OutSettings, float Intensity) const
{
	const float Alpha = FMath::Clamp(Intensity, 0.0f, 1.0f);

	static const FVector4 NeutralOne(1.0, 1.0, 1.0, 1.0);
	static const FVector4 NeutralZero(0.0, 0.0, 0.0, 0.0);

	OutSettings.bOverride_ColorSaturation = true;
	OutSettings.ColorSaturation = FMath::Lerp(NeutralOne, ColorSaturation, static_cast<double>(Alpha));

	OutSettings.bOverride_ColorContrast = true;
	OutSettings.ColorContrast = FMath::Lerp(NeutralOne, ColorContrast, static_cast<double>(Alpha));

	OutSettings.bOverride_ColorGamma = true;
	OutSettings.ColorGamma = FMath::Lerp(NeutralOne, ColorGamma, static_cast<double>(Alpha));

	OutSettings.bOverride_ColorGain = true;
	OutSettings.ColorGain = FMath::Lerp(NeutralOne, ColorGain, static_cast<double>(Alpha));

	OutSettings.bOverride_ColorOffset = true;
	OutSettings.ColorOffset = FMath::Lerp(NeutralZero, ColorOffset, static_cast<double>(Alpha));

	OutSettings.bOverride_WhiteTemp = true;
	OutSettings.WhiteTemp = FMath::Lerp(6500.0f, WhiteTemp, Alpha);

	if (PostProcessMaterial)
	{
		const float Weight = FMath::Clamp(PostProcessMaterialWeight, 0.0f, 1.0f) * Alpha;
		if (Weight > KINDA_SMALL_NUMBER)
		{
			OutSettings.AddBlendable(PostProcessMaterial, Weight);
		}
	}
}

FString UShutterFilterAsset::GetFilterLabel() const
{
	return DisplayName.IsEmpty() ? GetName() : DisplayName.ToString();
}

FPrimaryAssetId UShutterFilterAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ShutterFilter"), GetFName());
}
