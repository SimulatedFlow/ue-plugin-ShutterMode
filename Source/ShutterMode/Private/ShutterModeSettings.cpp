// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "ShutterModeSettings.h"

UShutterModeSettings::UShutterModeSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("ShutterMode");
}

FName UShutterModeSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UShutterModeSettings::GetSectionName() const
{
	return TEXT("ShutterMode");
}

const UShutterModeSettings& UShutterModeSettings::Get()
{
	const UShutterModeSettings* Settings = GetDefault<UShutterModeSettings>();
	check(Settings);
	return *Settings;
}
