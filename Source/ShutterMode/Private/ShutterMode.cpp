// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ShutterMode.h"
#include "ShutterModeLog.h"

DEFINE_LOG_CATEGORY(LogShutterMode);

#define LOCTEXT_NAMESPACE "FShutterModeModule"

void FShutterModeModule::StartupModule()
{
	UE_LOG(LogShutterMode, Log, TEXT("ShutterMode started."));
}

void FShutterModeModule::ShutdownModule()
{
	UE_LOG(LogShutterMode, Log, TEXT("ShutterMode shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FShutterModeModule, ShutterMode)
