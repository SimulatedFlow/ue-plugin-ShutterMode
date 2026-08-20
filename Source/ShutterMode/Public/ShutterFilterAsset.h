// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Scene.h"
#include "ShutterFilterAsset.generated.h"

class UMaterialInterface;

/**
 * One photo filter, as data.
 *
 * Filters are deliberately NOT code. A customer who wants a "cold morning" look should be able to
 * right-click > Miscellaneous > Data Asset > Shutter Filter Asset, drag the numbers around and see
 * it in the filter wheel - without opening a C++ file, without a recompile and without asking us.
 *
 * The colour section maps one-to-one onto the engine's post-process grading channels, so anything
 * an artist can do in a Post Process Volume can be shipped as a filter. If a look needs more than
 * grading, hang a post-process material on PostProcessMaterial and it is blended in on top.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Shutter Filter Asset"))
class SHUTTERMODE_API UShutterFilterAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UShutterFilterAsset();

	/** Shown in the filter wheel and in the guide overlay's status line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter")
	FText DisplayName;

	/** One line for a tooltip or a filter picker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Filter", meta = (MultiLine = "true"))
	FText Description;

	// ---- Colour grading (RGB + luminance in W, exactly like a Post Process Volume) --------

	/** 1,1,1,1 is neutral. Push W down for a desaturated look, per channel for a split tone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grading")
	FVector4 ColorSaturation = FVector4(1.0, 1.0, 1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grading")
	FVector4 ColorContrast = FVector4(1.0, 1.0, 1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grading")
	FVector4 ColorGamma = FVector4(1.0, 1.0, 1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grading")
	FVector4 ColorGain = FVector4(1.0, 1.0, 1.0, 1.0);

	/** Added, not multiplied - this is where a lifted-black film look comes from. Neutral is 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grading")
	FVector4 ColorOffset = FVector4(0.0, 0.0, 0.0, 0.0);

	/** White point in Kelvin. 6500 is neutral daylight; the photo state adds its own offset on top. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grading", meta = (ClampMin = "1500.0", ClampMax = "15000.0"))
	float WhiteTemp = 6500.0f;

	// ---- Suggested defaults ---------------------------------------------------------------
	// Applied to the photo state the moment the filter is selected, then the player owns them.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suggested Defaults", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SuggestedVignette = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suggested Defaults", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SuggestedGrain = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suggested Defaults", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float SuggestedChromaticAberration = 0.0f;

	// ---- Optional material ------------------------------------------------------------------

	/** Optional post-process material, blended in on top of the grading. May be null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TObjectPtr<UMaterialInterface> PostProcessMaterial = nullptr;

	/** Blend weight of PostProcessMaterial, scaled by the state's filter intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PostProcessMaterialWeight = 1.0f;

	/**
	 * Write this filter's grade into OutSettings, blended from neutral by Intensity.
	 * Intensity 0 leaves the image alone, which is what makes a "Neutral" filter free rather than
	 * a special case in the code.
	 */
	void ApplyGrading(FPostProcessSettings& OutSettings, float Intensity) const;

	/** DisplayName if set, otherwise the asset name - the overlay always has something to print. */
	FString GetFilterLabel() const;

	//~ Begin UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset interface
};
