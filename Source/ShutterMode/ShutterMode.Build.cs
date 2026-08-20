// Copyright 2026 Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class ShutterMode : ModuleRules
{
	public ShutterMode(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Runtime only. No UnrealEd, no editor-only Slate: a photo mode is a shipping feature,
		// so every line in here has to survive a cooked Shipping build.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// GWhiteTexture for the composition-guide canvas tiles.
			"RenderCore",
		});
	}
}
