// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "ShutterModeSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "ShutterFilterAsset.h"
#include "ShutterModeAware.h"
#include "ShutterModeCamera.h"
#include "ShutterModeLog.h"
#include "ShutterModeSettings.h"
#include "UnrealClient.h"

namespace ShutterModeConsole
{
	/** All console commands operate on the world of the first local player. */
	static UShutterModeSubsystem* Resolve(UWorld* World)
	{
		return World ? World->GetSubsystem<UShutterModeSubsystem>() : nullptr;
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdEnter(
		TEXT("ShutterMode.Enter"),
		TEXT("Open photo mode for the first local player."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UShutterModeSubsystem* Subsystem = Resolve(World))
			{
				Subsystem->EnterPhotoMode(nullptr);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdExit(
		TEXT("ShutterMode.Exit"),
		TEXT("Close photo mode and restore the game exactly as it was."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UShutterModeSubsystem* Subsystem = Resolve(World))
			{
				Subsystem->ExitPhotoMode();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdCapture(
		TEXT("ShutterMode.Capture"),
		TEXT("Take a photo. Optional argument: resolution multiplier (1, 2 or 4)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UShutterModeSubsystem* Subsystem = Resolve(World))
			{
				const int32 Multiplier = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
				Subsystem->Capture(Multiplier);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdFilter(
		TEXT("ShutterMode.Filter"),
		TEXT("Select a filter by index. Without an argument, advances to the next one."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UShutterModeSubsystem* Subsystem = Resolve(World))
			{
				if (Args.Num() > 0)
				{
					Subsystem->SetFilterIndex(FCString::Atoi(*Args[0]));
				}
				else
				{
					Subsystem->NextFilter();
				}
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdGuides(
		TEXT("ShutterMode.Guides"),
		TEXT("Show (1) or hide (0) the composition guides."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UShutterModeSubsystem* Subsystem = Resolve(World))
			{
				const bool bEnabled = Args.Num() > 0 ? (FCString::Atoi(*Args[0]) != 0) : !Subsystem->GetState().bGuides;
				Subsystem->SetGuidesEnabled(bEnabled);
			}
		}));
}

// =====================================================================================
//  Subsystem lifetime
// =====================================================================================

void UShutterModeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UShutterModeSettings& Settings = UShutterModeSettings::Get();

	State.FieldOfView = Settings.DefaultFieldOfView;
	State.Aperture = Settings.DefaultAperture;
	State.bAutoFocus = Settings.bAutoFocusByDefault;
	State.FilterIndex = Settings.DefaultFilterIndex;
	State.bGuides = Settings.bGuidesEnabledByDefault;
	State.GuideFlags = Settings.DefaultGuideFlags;
	State.bLetterbox = Settings.bLetterboxByDefault;
	State.LetterboxRatio = Settings.DefaultLetterboxRatio;
	State.Sanitize();
}

void UShutterModeSubsystem::Deinitialize()
{
	// The world is going away - a level change, or the game shutting down. There is nothing left to
	// restore into, so tear our own hooks down without touching the (possibly already dead) player
	// controller. ExitPhotoMode's restore path deliberately does NOT run here.
	UnregisterGuideRenderer();

	if (ScreenshotProcessedHandle.IsValid())
	{
		FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessedHandle);
		ScreenshotProcessedHandle.Reset();
	}

	CaptureStage = EShutterCaptureStage::Idle;
	bInPhotoMode = false;
	PhotoCamera = nullptr;
	RestorePoint.Reset();

	Super::Deinitialize();
}

bool UShutterModeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UShutterModeSubsystem* UShutterModeSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UShutterModeSubsystem>() : nullptr;
}

TStatId UShutterModeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UShutterModeSubsystem, STATGROUP_Tickables);
}

bool UShutterModeSubsystem::IsTickable() const
{
	return bInPhotoMode || CaptureStage != EShutterCaptureStage::Idle;
}

void UShutterModeSubsystem::Tick(float DeltaTime)
{
	PumpCameraManagerWhilePaused();

	switch (CaptureStage)
	{
	case EShutterCaptureStage::Settling:
		// Waiting for a frame WITHOUT the overlays to be drawn. See the comment in Capture().
		if (--CaptureFramesRemaining <= 0)
		{
			IssueScreenshotRequest();
		}
		break;

	case EShutterCaptureStage::Waiting:
		// The engine owns the request now. If it never comes back (a viewport that stopped drawing,
		// a failed write) the overlays would stay hidden forever, so time out and clean up.
		if (--CaptureWatchdogFrames <= 0)
		{
			UE_LOG(LogShutterMode, Warning, TEXT("Capture timed out waiting for the screenshot to be written."));
			FinishCapture(false);
		}
		break;

	default:
		break;
	}
}

void UShutterModeSubsystem::PumpCameraManagerWhilePaused()
{
	// ===================================================================================
	//  GOTCHA #3 - A PAUSED WORLD STOPS UPDATING THE CAMERA MANAGER.
	// ===================================================================================
	// UWorld::Tick only calls UpdateCameraManager while the game is running (or for a controller
	// with full-tick-when-paused, which is engine-protected and not ours to set). So the photo
	// camera would move perfectly and the player would still be looking at the frozen frame from
	// the moment the pause started - and the view-target blend would never finish either.
	// Pumping the camera manager ourselves, from a tickable that is allowed to run while paused,
	// is what makes a paused photo mode actually a photo mode.
	if (!bInPhotoMode)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || !World->IsPaused())
	{
		return;
	}

	APlayerController* PlayerController = RestorePoint.PlayerController.Get();
	if (!IsValid(PlayerController) || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	// Real frame time: the world hands out zero while paused, and a blend driven with zero never ends.
	PlayerController->PlayerCameraManager->UpdateCamera(FApp::GetDeltaTime());
}

// =====================================================================================
//  Enter / exit
// =====================================================================================

bool UShutterModeSubsystem::EnterPhotoMode(APlayerController* PlayerController)
{
	if (bInPhotoMode)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (!PlayerController)
	{
		PlayerController = World->GetFirstPlayerController();
	}

	if (!IsValid(PlayerController))
	{
		UE_LOG(LogShutterMode, Warning, TEXT("EnterPhotoMode: no player controller to work with."));
		return false;
	}

	const UShutterModeSettings& Settings = UShutterModeSettings::Get();

	EnsureFiltersLoaded();

	// --- 1. Write down everything we are about to touch, BEFORE touching any of it.
	RecordRestorePoint(PlayerController);

	// --- 2. Camera.
	AShutterModeCamera* Camera = SpawnPhotoCamera(PlayerController);
	if (!Camera)
	{
		RestorePoint.Reset();
		return false;
	}

	PlayerController->SetViewTargetWithBlend(Camera, Settings.EnterBlendTime);

	// --- 3. Pause, but only if nobody else already did. Un-pausing somebody else's pause menu on
	//        the way out is the classic way to break a game with a photo mode bolted on.
	if (Settings.bPauseGame && !RestorePoint.bWasPaused)
	{
		PlayerController->SetPause(true);
		RestorePoint.bPausedByUs = true;
	}

	// --- 4. Input. The controller has to keep ticking through the pause or none of the player's
	//        photo-mode input ever reaches the camera. (Input bindings themselves still have to be
	//        marked "execute when paused" on the game's side - that part is not ours to decide.)
	PlayerController->SetTickableWhenPaused(true);
	PlayerController->bShowMouseCursor = Settings.bShowMouseCursor;

	if (Settings.bDisablePlayerInput)
	{
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
	}

	// --- 5. Hand the HUD over to whoever implements IShutterModeAware.
	NotifyAwareObjects(true);

	// --- 6. Overlays.
	RegisterGuideRenderer();

	bInPhotoMode = true;

	UE_LOG(LogShutterMode, Log, TEXT("Photo mode entered (paused before: %s)."), RestorePoint.bWasPaused ? TEXT("yes") : TEXT("no"));
	OnPhotoModeEntered.Broadcast();
	return true;
}

void UShutterModeSubsystem::ExitPhotoMode()
{
	if (!bInPhotoMode)
	{
		return;
	}

	// A capture in flight would otherwise leave the overlay flags stashed and never put back.
	if (CaptureStage != EShutterCaptureStage::Idle)
	{
		FinishCapture(false);
	}

	UnregisterGuideRenderer();
	NotifyAwareObjects(false);
	ApplyRestorePoint();

	if (IsValid(PhotoCamera))
	{
		PhotoCamera->Destroy();
	}
	PhotoCamera = nullptr;

	RestorePoint.Reset();
	bInPhotoMode = false;

	UE_LOG(LogShutterMode, Log, TEXT("Photo mode exited."));
	OnPhotoModeExited.Broadcast();
}

bool UShutterModeSubsystem::TogglePhotoMode(APlayerController* PlayerController)
{
	if (bInPhotoMode)
	{
		ExitPhotoMode();
		return false;
	}

	return EnterPhotoMode(PlayerController);
}

void UShutterModeSubsystem::RecordRestorePoint(APlayerController* PlayerController)
{
	UWorld* World = GetWorld();

	RestorePoint.Reset();
	RestorePoint.PlayerController = PlayerController;
	RestorePoint.World = World;
	RestorePoint.ViewTarget = PlayerController->GetViewTarget();
	RestorePoint.bWasPaused = World ? World->IsPaused() : false;
	RestorePoint.bPausedByUs = false;

	RestorePoint.bShowMouseCursor = PlayerController->bShowMouseCursor;
	RestorePoint.bEnableClickEvents = PlayerController->bEnableClickEvents;
	RestorePoint.bEnableMouseOverEvents = PlayerController->bEnableMouseOverEvents;
	RestorePoint.bMoveInputIgnored = PlayerController->IsMoveInputIgnored();
	RestorePoint.bLookInputIgnored = PlayerController->IsLookInputIgnored();
	RestorePoint.bControllerTickedWhenPaused = PlayerController->PrimaryActorTick.bTickEvenWhenPaused;

	// SetInputMode() is nothing but a writer for these four viewport values, so snapshotting them
	// is a genuinely complete record of the input mode rather than a guess at which preset was used.
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		RestorePoint.MouseCaptureMode = Viewport->GetMouseCaptureMode();
		RestorePoint.MouseLockMode = Viewport->GetMouseLockMode();
		RestorePoint.bHideCursorDuringCapture = Viewport->HideCursorDuringCapture();
		RestorePoint.bViewportIgnoredInput = Viewport->IgnoreInput();
	}

	RestorePoint.bValid = true;
}

void UShutterModeSubsystem::ApplyRestorePoint()
{
	if (!RestorePoint.bValid)
	{
		return;
	}

	UWorld* World = GetWorld();

	// The level can have changed underneath us. Anything recorded in a different world is stale by
	// definition, so drop it rather than write it into the wrong game.
	if (RestorePoint.World.Get() != World)
	{
		UE_LOG(LogShutterMode, Warning, TEXT("The world changed while photo mode was open; the restore point was dropped."));
		return;
	}

	APlayerController* PlayerController = RestorePoint.PlayerController.Get();

	// Pause first: it is the one piece of state a player notices immediately, and it can be undone
	// through the world even if the controller that set it is gone.
	if (RestorePoint.bPausedByUs && World && World->IsPaused())
	{
		if (IsValid(PlayerController))
		{
			PlayerController->SetPause(false);
		}
		else if (APlayerController* AnyController = World->GetFirstPlayerController())
		{
			AnyController->SetPause(false);
		}
	}

	if (!IsValid(PlayerController))
	{
		UE_LOG(LogShutterMode, Warning, TEXT("The player controller went away while photo mode was open; only the pause was restored."));
		return;
	}

	// View target. If the pawn died in the meantime the recorded target is gone - fall back to the
	// controller's current pawn, and to the controller itself if there is not even one of those.
	// Never leave the player looking through a camera that is about to be destroyed.
	AActor* ViewTarget = RestorePoint.ViewTarget.Get();
	if (!IsValid(ViewTarget))
	{
		ViewTarget = PlayerController->GetPawn();
	}
	if (!IsValid(ViewTarget))
	{
		ViewTarget = PlayerController;
	}
	PlayerController->SetViewTargetWithBlend(ViewTarget, UShutterModeSettings::Get().ExitBlendTime);

	PlayerController->bShowMouseCursor = RestorePoint.bShowMouseCursor;
	PlayerController->bEnableClickEvents = RestorePoint.bEnableClickEvents;
	PlayerController->bEnableMouseOverEvents = RestorePoint.bEnableMouseOverEvents;
	PlayerController->SetTickableWhenPaused(RestorePoint.bControllerTickedWhenPaused);

	// SetIgnoreMoveInput/SetIgnoreLookInput keep a counter, so these have to be balanced, not forced.
	if (PlayerController->IsMoveInputIgnored() != RestorePoint.bMoveInputIgnored)
	{
		PlayerController->SetIgnoreMoveInput(RestorePoint.bMoveInputIgnored);
	}
	if (PlayerController->IsLookInputIgnored() != RestorePoint.bLookInputIgnored)
	{
		PlayerController->SetIgnoreLookInput(RestorePoint.bLookInputIgnored);
	}

	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		Viewport->SetMouseCaptureMode(RestorePoint.MouseCaptureMode);
		Viewport->SetMouseLockMode(RestorePoint.MouseLockMode);
		Viewport->SetHideCursorDuringCapture(RestorePoint.bHideCursorDuringCapture);
		Viewport->SetIgnoreInput(RestorePoint.bViewportIgnoredInput);
	}
}

AShutterModeCamera* UShutterModeSubsystem::SpawnPhotoCamera(APlayerController* PlayerController)
{
	UWorld* World = GetWorld();
	if (!World || !PlayerController)
	{
		return nullptr;
	}

	const UShutterModeSettings& Settings = UShutterModeSettings::Get();

	UClass* CameraClass = Settings.CameraClass.IsNull() ? AShutterModeCamera::StaticClass() : Settings.CameraClass.LoadSynchronous();
	if (!CameraClass)
	{
		CameraClass = AShutterModeCamera::StaticClass();
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.Owner = PlayerController;

	AShutterModeCamera* Camera = World->SpawnActor<AShutterModeCamera>(CameraClass, ViewLocation, ViewRotation, SpawnParams);
	if (!Camera)
	{
		UE_LOG(LogShutterMode, Error, TEXT("Could not spawn the photo camera (class %s)."), *CameraClass->GetName());
		return nullptr;
	}

	Camera->MaxDistanceFromSubject = Settings.MaxDistanceFromSubject;
	Camera->bLeashEnabled = Settings.bLeashEnabled;
	Camera->bCollisionEnabled = Settings.bCollisionEnabled;
	Camera->CollisionChannel = Settings.CameraCollisionChannel;
	Camera->CollisionPadding = Settings.CollisionPadding;
	Camera->MaxPitch = Settings.MaxPitch;
	Camera->MinOrbitDistance = Settings.MinOrbitDistance;
	Camera->MoveSpeed = Settings.MoveSpeed;
	Camera->LookSensitivity = Settings.LookSensitivity;
	Camera->RollSpeed = Settings.RollSpeed;
	Camera->ZoomSpeed = Settings.ZoomSpeed;
	Camera->AutoFocusTraceDistance = Settings.AutoFocusTraceDistance;
	Camera->AutoFocusChannel = Settings.AutoFocusChannel;
	Camera->SensorWidth = Settings.SensorWidth;

	// The leash is anchored to the player's own pawn by default - the thing the picture is about.
	Camera->InitializeForSession(this, ViewLocation, ViewRotation, PlayerController->GetPawn());
	Camera->SetCameraMode(Settings.DefaultCameraMode);

	PhotoCamera = Camera;
	return Camera;
}

// =====================================================================================
//  HUD hand-off
// =====================================================================================

void UShutterModeSubsystem::RegisterAwareObject(UObject* Object)
{
	if (!Object || !Object->GetClass()->ImplementsInterface(UShutterModeAware::StaticClass()))
	{
		return;
	}

	RegisteredAwareObjects.AddUnique(Object);

	// Registered while photo mode is already open? Then it has missed the announcement, so make it
	// up immediately - and remember it, so it is told about the exit as well.
	if (bInPhotoMode)
	{
		IShutterModeAware::Execute_OnPhotoModeEnter(Object);
		RestorePoint.NotifiedAwareObjects.AddUnique(Object);
	}
}

void UShutterModeSubsystem::UnregisterAwareObject(UObject* Object)
{
	RegisteredAwareObjects.RemoveAll([Object](const TWeakObjectPtr<UObject>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Object;
	});
}

void UShutterModeSubsystem::NotifyAwareObjects(bool bEntering)
{
	if (bEntering)
	{
		RestorePoint.NotifiedAwareObjects.Reset();

		// Actors are found automatically; a customer's nameplate or quest marker only has to
		// implement the interface and it is picked up with no registration step at all.
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				if (IsValid(Actor) && Actor->GetClass()->ImplementsInterface(UShutterModeAware::StaticClass()))
				{
					IShutterModeAware::Execute_OnPhotoModeEnter(Actor);
					RestorePoint.NotifiedAwareObjects.AddUnique(Actor);
				}
			}
		}

		// Widgets are not actors, so they register themselves.
		for (const TWeakObjectPtr<UObject>& Entry : RegisteredAwareObjects)
		{
			if (UObject* Object = Entry.Get())
			{
				if (!RestorePoint.NotifiedAwareObjects.Contains(Object))
				{
					IShutterModeAware::Execute_OnPhotoModeEnter(Object);
					RestorePoint.NotifiedAwareObjects.Add(Object);
				}
			}
		}

		return;
	}

	// Leaving: tell exactly the objects that were told we were entering - not the ones that happen
	// to be around now. An object that unregistered in between still gets its exit call, because it
	// was WE who hid it, and something that was hidden has to be un-hidden.
	for (const TWeakObjectPtr<UObject>& Entry : RestorePoint.NotifiedAwareObjects)
	{
		if (UObject* Object = Entry.Get())
		{
			IShutterModeAware::Execute_OnPhotoModeExit(Object);
		}
	}

	RestorePoint.NotifiedAwareObjects.Reset();
}

// =====================================================================================
//  Photo state
// =====================================================================================

void UShutterModeSubsystem::SetState(const FShutterModeState& NewState)
{
	State = NewState;
	State.Sanitize();
	SetFilterIndex(State.FilterIndex);
}

void UShutterModeSubsystem::SetFieldOfView(float NewFieldOfView)
{
	State.FieldOfView = FMath::Clamp(NewFieldOfView, 10.0f, 120.0f);
}

void UShutterModeSubsystem::SetAperture(float NewAperture)
{
	State.Aperture = FMath::Clamp(NewAperture, 1.2f, 22.0f);
}

void UShutterModeSubsystem::SetFocusDistance(float NewFocusDistance)
{
	State.bAutoFocus = false;
	State.FocusDistance = FMath::Max(NewFocusDistance, 1.0f);
}

void UShutterModeSubsystem::SetAutoFocus(bool bEnabled)
{
	State.bAutoFocus = bEnabled;
}

bool UShutterModeSubsystem::SetFocusFromScreenCenter()
{
	if (!IsValid(PhotoCamera))
	{
		return false;
	}

	const float Distance = PhotoCamera->TraceFocusDistance();
	State.bAutoFocus = false;

	if (Distance > 0.0f)
	{
		State.FocusDistance = Distance;
		return true;
	}

	State.FocusDistance = FMath::Max(PhotoCamera->AutoFocusTraceDistance, 100.0f);
	return false;
}

void UShutterModeSubsystem::SetGuidesEnabled(bool bEnabled)
{
	State.bGuides = bEnabled;
}

void UShutterModeSubsystem::SetLetterboxEnabled(bool bEnabled)
{
	State.bLetterbox = bEnabled;
}

// =====================================================================================
//  Filters
// =====================================================================================

void UShutterModeSubsystem::EnsureFiltersLoaded()
{
	if (bFiltersLoaded)
	{
		return;
	}

	bFiltersLoaded = true;

	const UShutterModeSettings& Settings = UShutterModeSettings::Get();
	for (const TSoftObjectPtr<UShutterFilterAsset>& SoftFilter : Settings.Filters)
	{
		if (SoftFilter.IsNull())
		{
			continue;
		}

		if (UShutterFilterAsset* Filter = SoftFilter.LoadSynchronous())
		{
			Filters.Add(Filter);
		}
		else
		{
			UE_LOG(LogShutterMode, Warning, TEXT("Filter '%s' from the project settings could not be loaded."), *SoftFilter.ToString());
		}
	}

	SetFilterIndex(State.FilterIndex);
}

TArray<UShutterFilterAsset*> UShutterModeSubsystem::GetFilters() const
{
	TArray<UShutterFilterAsset*> Result;
	Result.Reserve(Filters.Num());
	for (const TObjectPtr<UShutterFilterAsset>& Filter : Filters)
	{
		Result.Add(Filter);
	}
	return Result;
}

void UShutterModeSubsystem::SetFilters(const TArray<UShutterFilterAsset*>& NewFilters)
{
	Filters.Reset();
	for (UShutterFilterAsset* Filter : NewFilters)
	{
		if (Filter)
		{
			Filters.Add(Filter);
		}
	}

	bFiltersLoaded = true;
	SetFilterIndex(State.FilterIndex);
}

void UShutterModeSubsystem::SetFilterIndex(int32 NewIndex)
{
	EnsureFiltersLoaded();

	if (Filters.Num() == 0)
	{
		State.FilterIndex = 0;
		return;
	}

	// Wrap instead of clamp: a "next filter" button then needs no bounds check of its own, and a
	// saved state from a build with more filters cannot select nothing at all.
	State.FilterIndex = ((NewIndex % Filters.Num()) + Filters.Num()) % Filters.Num();
	ApplyFilterDefaultsToState();
}

void UShutterModeSubsystem::NextFilter()
{
	SetFilterIndex(State.FilterIndex + 1);
}

void UShutterModeSubsystem::PreviousFilter()
{
	SetFilterIndex(State.FilterIndex - 1);
}

void UShutterModeSubsystem::SetFilterIntensity(float NewIntensity)
{
	State.FilterIntensity = FMath::Clamp(NewIntensity, 0.0f, 1.0f);
}

UShutterFilterAsset* UShutterModeSubsystem::GetActiveFilter() const
{
	return Filters.IsValidIndex(State.FilterIndex) ? Filters[State.FilterIndex] : nullptr;
}

FString UShutterModeSubsystem::GetActiveFilterName() const
{
	const UShutterFilterAsset* Filter = GetActiveFilter();
	return Filter ? Filter->GetFilterLabel() : TEXT("None");
}

void UShutterModeSubsystem::ApplyFilterDefaultsToState()
{
	if (const UShutterFilterAsset* Filter = GetActiveFilter())
	{
		State.Vignette = Filter->SuggestedVignette;
		State.Grain = Filter->SuggestedGrain;
		State.ChromaticAberration = Filter->SuggestedChromaticAberration;
	}
}

// =====================================================================================
//  Capture
// =====================================================================================

FString UShutterModeSubsystem::GetPhotoDirectory() const
{
	const UShutterModeSettings& Settings = UShutterModeSettings::Get();
	const FString Folder = Settings.PhotoDirectory.IsEmpty() ? TEXT("Photos") : Settings.PhotoDirectory;
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / Folder);
}

bool UShutterModeSubsystem::Capture(int32 ResolutionMultiplier)
{
	if (CaptureStage != EShutterCaptureStage::Idle)
	{
		UE_LOG(LogShutterMode, Verbose, TEXT("Capture ignored: one is already in flight."));
		return false;
	}

	const UShutterModeSettings& Settings = UShutterModeSettings::Get();

	PendingResolutionMultiplier = FMath::Clamp(
		ResolutionMultiplier > 0 ? ResolutionMultiplier : Settings.DefaultResolutionMultiplier, 1, 8);

	const FString Directory = GetPhotoDirectory();
	IFileManager::Get().MakeDirectory(*Directory, /*Tree=*/true);

	const FString Prefix = Settings.PhotoFilePrefix.IsEmpty() ? FString(FApp::GetProjectName()) : Settings.PhotoFilePrefix;
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));

	FString BasePath = Directory / FString::Printf(TEXT("%s_%s"), *Prefix, *Stamp);
	int32 Suffix = 1;
	while (IFileManager::Get().FileExists(*(BasePath + TEXT(".png"))))
	{
		BasePath = Directory / FString::Printf(TEXT("%s_%s_%02d"), *Prefix, *Stamp, Suffix++);
	}
	PendingPhotoPath = BasePath;

	// ===================================================================================
	//  Guides are a help, not part of the picture.
	// ===================================================================================
	// A screenshot photographs a frame that has ALREADY been drawn, so hiding the overlays and
	// firing the shutter in the same tick captures the frame from before they were hidden - guides
	// and all. Hide them here, let CaptureFrameDelay frames go by, and only then fire.
	bGuidesBeforeCapture = State.bGuides;
	bLetterboxBeforeCapture = State.bLetterbox;

	State.bGuides = false;
	if (!Settings.bBurnInLetterbox)
	{
		State.bLetterbox = false;
	}

	CaptureFramesRemaining = FMath::Max(Settings.CaptureFrameDelay, 1);
	CaptureStage = EShutterCaptureStage::Settling;
	return true;
}

void UShutterModeSubsystem::IssueScreenshotRequest()
{
	UWorld* World = GetWorld();
	UGameViewportClient* ViewportClient = World ? World->GetGameViewport() : nullptr;
	FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;

	if (!Viewport)
	{
		UE_LOG(LogShutterMode, Warning, TEXT("Capture failed: there is no game viewport to photograph."));
		FinishCapture(false);
		return;
	}

	const FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		FinishCapture(false);
		return;
	}

	// The high-res path is used for every multiplier, 1x included: it renders off-screen with the
	// Slate UI switched off, which is precisely the "no HUD in my photo" guarantee.
	FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
	Config.bMaskEnabled = false;
	Config.bCaptureHDR = false;
	Config.bDateTimeBasedNaming = false;
	Config.SetFilename(PendingPhotoPath);

	if (!Config.SetResolution(ViewportSize.X * PendingResolutionMultiplier, ViewportSize.Y * PendingResolutionMultiplier))
	{
		UE_LOG(LogShutterMode, Warning, TEXT("Capture failed: %dx is larger than this GPU's maximum texture size."), PendingResolutionMultiplier);
		FinishCapture(false);
		return;
	}

	ScreenshotProcessedHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &UShutterModeSubsystem::HandleScreenshotProcessed);

	if (!Viewport->TakeHighResScreenShot())
	{
		FinishCapture(false);
		return;
	}

	CaptureStage = EShutterCaptureStage::Waiting;
	CaptureWatchdogFrames = 600;
}

void UShutterModeSubsystem::HandleScreenshotProcessed()
{
	if (CaptureStage != EShutterCaptureStage::Waiting)
	{
		return;
	}

	FinishCapture(true);
}

void UShutterModeSubsystem::FinishCapture(bool bSucceeded)
{
	if (ScreenshotProcessedHandle.IsValid())
	{
		FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessedHandle);
		ScreenshotProcessedHandle.Reset();
	}

	// Whatever happened, the overlays come back.
	State.bGuides = bGuidesBeforeCapture;
	State.bLetterbox = bLetterboxBeforeCapture;

	CaptureStage = EShutterCaptureStage::Idle;
	CaptureFramesRemaining = 0;
	CaptureWatchdogFrames = 0;

	if (bSucceeded)
	{
		const FString FinalPath = PendingPhotoPath + TEXT(".png");
		UE_LOG(LogShutterMode, Log, TEXT("Photo saved: %s"), *FinalPath);
		OnPhotoCaptured.Broadcast(FinalPath);
	}

	PendingPhotoPath.Reset();
}

// =====================================================================================
//  Composition overlay
// =====================================================================================

void UShutterModeSubsystem::RegisterGuideRenderer()
{
	if (GuideDrawHandle.IsValid())
	{
		return;
	}

	// Canvas rather than UMG on purpose: no UMG dependency for the plugin, no widget for the
	// customer to style, and it draws in an editor viewport as well as in a packaged game.
	GuideDrawHandle = UDebugDrawService::Register(
		TEXT("Game"),
		FDebugDrawDelegate::CreateUObject(this, &UShutterModeSubsystem::DrawGuides));
}

void UShutterModeSubsystem::UnregisterGuideRenderer()
{
	if (GuideDrawHandle.IsValid())
	{
		UDebugDrawService::Unregister(GuideDrawHandle);
		GuideDrawHandle.Reset();
	}
}

void UShutterModeSubsystem::DrawGuides(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (!Canvas || !bInPhotoMode)
	{
		return;
	}

	const UShutterModeSettings& Settings = UShutterModeSettings::Get();

	const float Width = static_cast<float>(Canvas->SizeX);
	const float Height = static_cast<float>(Canvas->SizeY);
	if (Width <= 0.0f || Height <= 0.0f)
	{
		return;
	}

	// --- Letterbox first, so guide lines stay visible on top of the bars.
	if (State.bLetterbox)
	{
		const float TargetHeight = Width / FMath::Max(State.LetterboxRatio, 0.1f);
		const float BarHeight = FMath::Max((Height - TargetHeight) * 0.5f, 0.0f);
		if (BarHeight > 1.0f)
		{
			FCanvasTileItem TopBar(FVector2D(0.0f, 0.0f), GWhiteTexture, FVector2D(Width, BarHeight), FLinearColor::Black);
			TopBar.BlendMode = SE_BLEND_Opaque;
			Canvas->DrawItem(TopBar);

			FCanvasTileItem BottomBar(FVector2D(0.0f, Height - BarHeight), GWhiteTexture, FVector2D(Width, BarHeight), FLinearColor::Black);
			BottomBar.BlendMode = SE_BLEND_Opaque;
			Canvas->DrawItem(BottomBar);
		}
	}

	if (State.bGuides)
	{
		const FLinearColor Color = Settings.GuideColor;
		const int32 Flags = State.GuideFlags;

		auto HasGuide = [Flags](EShutterGuide Guide)
		{
			return (Flags & static_cast<int32>(Guide)) != 0;
		};

		if (HasGuide(EShutterGuide::Thirds))
		{
			for (int32 Index = 1; Index <= 2; ++Index)
			{
				const float X = Width * Index / 3.0f;
				const float Y = Height * Index / 3.0f;
				Canvas->K2_DrawLine(FVector2D(X, 0.0f), FVector2D(X, Height), 1.0f, Color);
				Canvas->K2_DrawLine(FVector2D(0.0f, Y), FVector2D(Width, Y), 1.0f, Color);
			}
		}

		if (HasGuide(EShutterGuide::GoldenRatio))
		{
			// 1/phi = 0.381966..., the classic phi grid.
			const float Phi = 0.3819660f;
			const float Xs[2] = { Width * Phi, Width * (1.0f - Phi) };
			const float Ys[2] = { Height * Phi, Height * (1.0f - Phi) };
			for (int32 Index = 0; Index < 2; ++Index)
			{
				Canvas->K2_DrawLine(FVector2D(Xs[Index], 0.0f), FVector2D(Xs[Index], Height), 1.0f, Color);
				Canvas->K2_DrawLine(FVector2D(0.0f, Ys[Index]), FVector2D(Width, Ys[Index]), 1.0f, Color);
			}
		}

		if (HasGuide(EShutterGuide::Diagonals))
		{
			Canvas->K2_DrawLine(FVector2D(0.0f, 0.0f), FVector2D(Width, Height), 1.0f, Color);
			Canvas->K2_DrawLine(FVector2D(Width, 0.0f), FVector2D(0.0f, Height), 1.0f, Color);
		}

		if (HasGuide(EShutterGuide::SafeFrame))
		{
			const float MarginX = Width * 0.05f;
			const float MarginY = Height * 0.05f;
			Canvas->K2_DrawBox(FVector2D(MarginX, MarginY), FVector2D(Width - 2.0f * MarginX, Height - 2.0f * MarginY), 1.0f, Color);
		}

		if (HasGuide(EShutterGuide::CenterCross))
		{
			const FVector2D Center(Width * 0.5f, Height * 0.5f);
			const float Arm = FMath::Min(Width, Height) * 0.02f;
			Canvas->K2_DrawLine(Center - FVector2D(Arm, 0.0f), Center + FVector2D(Arm, 0.0f), 1.5f, Color);
			Canvas->K2_DrawLine(Center - FVector2D(0.0f, Arm), Center + FVector2D(0.0f, Arm), 1.5f, Color);
		}
	}

	// --- The read-out: filter, aperture, focus in metres, field of view.
	if (Settings.bShowStatusLine && State.bGuides)
	{
		if (UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr)
		{
			const FString Line = FString::Printf(
				TEXT("%s   f/%.1f   %s %.1f m   %.0f mm-eq FOV %.0f deg"),
				*GetActiveFilterName(),
				State.Aperture,
				State.bAutoFocus ? TEXT("AF") : TEXT("MF"),
				State.FocusDistance * 0.01f,
				36.0f / (2.0f * FMath::Tan(FMath::DegreesToRadians(State.FieldOfView) * 0.5f)),
				State.FieldOfView);

			Canvas->K2_DrawText(Font, Line, FVector2D(Width * 0.05f + 8.0f, Height * 0.95f - 24.0f),
				FVector2D(1.0f, 1.0f), FLinearColor::White, 0.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.75f), FVector2D(1.0f, 1.0f));
		}
	}
}
