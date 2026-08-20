// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ShutterModeStatics.h"

#include "ShutterModeCamera.h"
#include "ShutterModeSubsystem.h"

UShutterModeSubsystem* UShutterModeStatics::GetShutterMode(const UObject* WorldContextObject)
{
	return UShutterModeSubsystem::Get(WorldContextObject);
}

bool UShutterModeStatics::EnterPhotoMode(const UObject* WorldContextObject, APlayerController* PlayerController)
{
	UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->EnterPhotoMode(PlayerController) : false;
}

void UShutterModeStatics::ExitPhotoMode(const UObject* WorldContextObject)
{
	if (UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject))
	{
		Subsystem->ExitPhotoMode();
	}
}

bool UShutterModeStatics::TogglePhotoMode(const UObject* WorldContextObject, APlayerController* PlayerController)
{
	UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->TogglePhotoMode(PlayerController) : false;
}

bool UShutterModeStatics::IsInPhotoMode(const UObject* WorldContextObject)
{
	const UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->IsInPhotoMode() : false;
}

bool UShutterModeStatics::CapturePhoto(const UObject* WorldContextObject, int32 ResolutionMultiplier)
{
	UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->Capture(ResolutionMultiplier) : false;
}

FShutterModeState UShutterModeStatics::GetState(const UObject* WorldContextObject)
{
	const UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetState() : FShutterModeState();
}

void UShutterModeStatics::SetState(const UObject* WorldContextObject, const FShutterModeState& NewState)
{
	if (UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetState(NewState);
	}
}

void UShutterModeStatics::NextFilter(const UObject* WorldContextObject)
{
	if (UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject))
	{
		Subsystem->NextFilter();
	}
}

void UShutterModeStatics::PreviousFilter(const UObject* WorldContextObject)
{
	if (UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject))
	{
		Subsystem->PreviousFilter();
	}
}

FString UShutterModeStatics::GetActiveFilterName(const UObject* WorldContextObject)
{
	const UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetActiveFilterName() : FString(TEXT("None"));
}

bool UShutterModeStatics::SetFocusFromScreenCenter(const UObject* WorldContextObject)
{
	UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->SetFocusFromScreenCenter() : false;
}

AShutterModeCamera* UShutterModeStatics::GetPhotoCamera(const UObject* WorldContextObject)
{
	const UShutterModeSubsystem* Subsystem = UShutterModeSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetPhotoCamera() : nullptr;
}
