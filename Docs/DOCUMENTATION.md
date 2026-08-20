# ShutterMode — In-Game Photo Mode

**Documentation · Version 1.0.0 · Unreal Engine 5.8**

A finished, player-facing photo mode. Press a key: the game pauses, the HUD steps aside, a leashed
free camera detaches from the character, and the player frames a shot with real cinematic depth of
field, colour filters, composition guides and a letterbox — then everything goes back exactly the
way it was.

One runtime module. No editor module, no third-party libraries, no Enhanced Input assets, no UMG
dependency. Ships in a cooked Shipping build.

---

## Table of contents

1. [Requirements, engine and platforms](#1-requirements-engine-and-platforms)
2. [Installation](#2-installation)
3. [Quick start — five minutes](#3-quick-start--five-minutes)
4. [Hiding your HUD — `IShutterModeAware`](#4-hiding-your-hud--ishuttermodeaware)
5. [Writing your own filters](#5-writing-your-own-filters)
6. [Depth of field](#6-depth-of-field)
7. [Composition guides and letterbox](#7-composition-guides-and-letterbox)
8. [Capture](#8-capture)
9. [Project settings reference](#9-project-settings-reference)
10. [Class and API overview](#10-class-and-api-overview)
11. [Code examples](#11-code-examples)
12. [Console commands](#12-console-commands)
13. [What the restore point covers](#13-what-the-restore-point-covers)
14. [Limits — read before you buy](#14-limits--read-before-you-buy)
15. [Troubleshooting](#15-troubleshooting)
16. [Support](#16-support)

---

## 1. Requirements, engine and platforms

| | |
|---|---|
| **Engine version** | Unreal Engine **5.8** (`"EngineVersion": "5.8.0"` in the `.uplugin`) |
| **Supported target platforms** | **Win64**, **Mac**, **Linux** (`PlatformAllowList` on the module) |
| **Modules** | one — `ShutterMode`, type `Runtime`, `LoadingPhase: PreDefault` |
| **Module dependencies** | `Core`, `CoreUObject`, `Engine`, `InputCore`, `DeveloperSettings` (public) · `RenderCore` (private) |
| **Third-party code** | none |
| **Editor dependency** | none — no `UnrealEd`, no editor-only Slate. Works in a cooked **Shipping** build. |
| **Language** | C++ and Blueprint. The complete API is Blueprint-exposed; C++ is optional. |
| **Project type** | Works in a Blueprint-only project as well — enabling the plugin adds the C++ classes without requiring project source. |
| **Rendering** | Deferred and Forward. Depth of field uses the engine's own cinematic DOF; `r.DepthOfFieldQuality` must not be 0. |
| **Networking** | Single-player by design. See [Limits](#14-limits--read-before-you-buy) — turn the pause switch off for network play, everything else works unchanged. |

Consoles are not in the `PlatformAllowList`. Nothing in the code is desktop-specific, but console
builds are untested and unsupported.

---

## 2. Installation

### A — Engine plugin (installed from Fab)

1. Fab / Epic Games Launcher > Library > **ShutterMode** > *Install to Engine* (5.8).
2. Open your project, **Edit > Plugins > Camera > ShutterMode**, tick **Enabled**.
3. Restart the editor when prompted.

### B — Project plugin (from the source zip)

1. Copy the `ShutterMode` folder into `<YourProject>/Plugins/` so that the file
   `<YourProject>/Plugins/ShutterMode/ShutterMode.uplugin` exists.
2. Right-click your `.uproject` > **Generate Visual Studio project files** (Windows) or run
   `GenerateProjectFiles` (Mac/Linux).
3. Build your project once — the plugin module compiles with it.
4. **Edit > Plugins > Camera > ShutterMode**, tick **Enabled**, restart.

Or add it to the `.uproject` directly:

```json
"Plugins": [
    { "Name": "ShutterMode", "Enabled": true }
]
```

### Verify the installation

Open the editor, press **Play**, and type in the console (`~`):

```
ShutterMode.Enter
```

The view should detach from your character. `ShutterMode.Exit` puts it back. If the command is not
found, the module did not load — check that the plugin is enabled and that the build succeeded.

---

## 3. Quick start — five minutes

### Step 1 — Open and close photo mode

Anywhere in Blueprint (nodes come from the `ShutterMode` function library — no "get subsystem"
node needed):

```
Toggle Photo Mode (Player Controller = <empty>)     // one binding for the whole feature
Enter Photo Mode  (Player Controller = <empty>)
Exit Photo Mode
```

In C++:

```cpp
#include "ShutterModeSubsystem.h"

if (UShutterModeSubsystem* Shutter = UShutterModeSubsystem::Get(this))
{
    Shutter->TogglePhotoMode(nullptr);   // null = first local player controller
}
```

That is the whole integration. The camera is spawned, the view target is switched, the game is
paused, the cursor comes up and the HUD is asked to hide itself. Exit puts all of it back.

### Step 2 — Wire the camera to your input

The plugin ships **no** Input Actions and **no** Input Mapping Context on purpose — there is nothing
to collide with your project's input setup. Get the camera and call four functions:

```
Get Photo Camera  ->  Add Move Input       (Forward, Right, Up)   // -1..1 axis values
                      Add Look Input       (Yaw, Pitch)           // per-frame mouse delta
                      Add Roll             (Amount)               // dutch angle
                      Zoom By              (Amount)               // field of view
                      Set Speed Multiplier (Multiplier)           // sprint / creep modifier
```

Move input is scaled by delta time inside the pawn; look input is already a per-frame delta and is
deliberately *not* scaled. Calling the same function several times in one frame is fine — input is
accumulated and applied once per tick.

> **Important while the game is paused:** an input binding only fires during a pause if it is marked
> for it. **Enhanced Input:** tick **Trigger When Paused** on the Input Action.
> **Legacy input:** set `bExecuteWhenPaused` on the binding. This is the number-one reason a
> hand-built photo mode has a camera that will not move.

### Step 3 — Add filters

The filter wheel is a list in the project settings and starts **empty**; a fresh install shows
`None` as the filter name until you fill it. Create your looks as data assets — see
[section 5](#5-writing-your-own-filters) — and add them under
**Project Settings > Plugins > ShutterMode > Filters**. The list order *is* the wheel order.

### Step 4 — Take the picture

```
Capture Photo (Resolution Multiplier = 0)    // 0 = project default, otherwise 1, 2 or 4
```

The file is **not** on disk when the call returns. Bind **On Photo Captured** on the subsystem to
get the absolute path:

```
Get Shutter Mode -> On Photo Captured (File Path) -> show a toast / open the folder / upload
```

---

## 4. Hiding your HUD — `IShutterModeAware`

ShutterMode does not know anything about your HUD, and it must not: a plugin that hard-codes
"hide the HUD" breaks in every project whose HUD is not the one it expected.

Anything that should not appear in the picture implements **Shutter Mode Aware** and hides itself.

### Blueprint (a widget)

1. Class Settings > Interfaces > Add > **Shutter Mode Aware**
2. Implement `On Photo Mode Enter` → `Set Visibility (Collapsed)`
3. Implement `On Photo Mode Exit` → `Set Visibility (Visible)`
4. On Construct: `Get Shutter Mode` → **Register Aware Object** (Self)

### Blueprint (an actor)

Steps 1–3 only. **Actors do not need to register.** Every actor in the world that implements the
interface is found and called automatically when photo mode starts.

### C++

```cpp
#include "ShutterModeAware.h"

UCLASS()
class AMyNameplate : public AActor, public IShutterModeAware
{
    GENERATED_BODY()

public:
    virtual void OnPhotoModeEnter_Implementation() override { SetActorHiddenInGame(true);  }
    virtual void OnPhotoModeExit_Implementation()  override { SetActorHiddenInGame(false); }
};
```

Whoever was told photo mode started is remembered in the restore point and told again when it ends —
even if it unregistered in between. Nothing stays hidden by accident.

---

## 5. Writing your own filters

A filter is a data asset, not code. That is deliberate: a customer who wants a "cold morning" look
should be able to build it in the Content Browser without a recompile.

1. Content Browser > right-click > **Miscellaneous > Data Asset** > **Shutter Filter Asset**
2. Fill in `Display Name` (this is what the status line prints), then the grading channels — they
   are the same channels a Post Process Volume uses:
   * `Color Saturation`, `Color Contrast`, `Color Gamma`, `Color Gain` — neutral is `(1,1,1,1)`.
     The **W** component is the global channel; RGB give you a split tone.
   * `Color Offset` — added, not multiplied. Neutral is `(0,0,0,0)`. This is where a lifted-black
     film look comes from.
   * `White Temp` — Kelvin. 6500 is neutral daylight.
3. Optional: `Suggested Vignette / Grain / Chromatic Aberration`. These are copied into the photo
   state the moment the filter is selected; after that the player owns them.
4. Optional: `Post Process Material` + `Post Process Material Weight` — any post-process material,
   blended in on top of the grading, scaled by the state's filter intensity.
5. **Project Settings > Plugins > ShutterMode > Filters** — add the asset to the list.

Filters are stored as **soft references**: nothing is loaded until photo mode is opened for the
first time, so a project that never uses photo mode pays nothing for shipping them.

### Recipes to start from

| Look | Settings |
|---|---|
| **Neutral** | everything at its default. Costs nothing — intensity 0 is a no-op by construction. |
| **Noir** | `Color Saturation W ≈ 0.05`, `Color Contrast W ≈ 1.35`, `Suggested Grain ≈ 0.5`, `Suggested Vignette ≈ 0.8` |
| **Sepia** | `Color Gain (1.15, 1.0, 0.78, 1.0)`, `Color Saturation W ≈ 0.25`, `White Temp ≈ 8200` |
| **Cyberpunk** | `Color Gain (1.15, 0.9, 1.3, 1.0)`, `Color Offset (-0.02, 0.0, 0.04, 0.0)`, `Suggested Chromatic Aberration ≈ 1.5` |
| **Warm Film** | `White Temp ≈ 7800`, `Color Offset (0.015, 0.012, 0.0, 0.0)` (lifted blacks), `Suggested Grain ≈ 0.25` |
| **Bleach Bypass** | `Color Saturation W ≈ 0.35`, `Color Contrast W ≈ 1.5`, `Color Gamma W ≈ 0.9` |

### How the photo state combines with a filter

| State value | Meaning |
|---|---|
| `Saturation`, `Contrast` | **Multipliers** on top of the filter's grade. `1.0` leaves the filter untouched. |
| `Temperature` | **Offset in Kelvin** on top of the filter's white point. `0.0` leaves it untouched. |
| `Vignette`, `Grain`, `ChromaticAberration` | **Absolute.** Seeded from the filter's suggestions, then owned by the player. |
| `FilterIntensity` | `0` = untouched image, `1` = full filter. This is why "Neutral" needs no special case in the code. |

### Unlockable filters

`SetFilters(Array)` replaces the wheel at runtime — hand it a growing array as the player unlocks
looks, and the wheel grows with it. `SetFilterIndex` wraps, so a "next" button never needs a bounds
check.

---

## 6. Depth of field

`Aperture` (f/1.2 … f/22) and `FocusDistance` (centimetres) drive the engine's cinematic depth of
field through the photo camera's own post-process settings — including the `bOverride_…` flags,
without which nothing happens at all.

* **Auto focus** (`bAutoFocus`, on by default) traces out of the centre of frame every frame and
  focuses on whatever it hits. The trace channel and range are in the project settings; nothing hit
  means focus at infinity.
* **Manual focus:** `Set Focus Distance` pins a distance in centimetres and switches auto focus off.
* **Focus pull:** `Set Focus From Screen Center` traces **once**, pins that distance and switches
  auto focus off — aim, tap, recompose. Returns `false` if the trace hit nothing.
* `Sensor Width` (project settings, default **36 mm** full-frame) decides how strong the defocus is
  for a given f-stop. **24.89** gives you Super 35.

If the difference between f/1.4 and f/16 is not visible in your scene, the scene has nothing at
different depths. Depth of field can only show what the level gives it — build your photo spots with
something near the camera, something in the middle and something far away.

---

## 7. Composition guides and letterbox

Drawn on the **Canvas** rather than in UMG: no UMG dependency, nothing for you to style, and they
show up in an editor viewport as well as in a packaged game.

`GuideFlags` is a bit mask:

| Flag | Draws |
|---|---|
| `Thirds` | the classic rule-of-thirds grid |
| `CenterCross` | a small cross in the exact centre of frame — also where the auto-focus trace goes out |
| `GoldenRatio` | the phi grid, slightly tighter than thirds |
| `SafeFrame` | the 90 % title-safe rectangle |
| `Diagonals` | diagonals through the frame corners |

`bGuides` switches the whole overlay, `bLetterbox` + `LetterboxRatio` (2.39 scope / 1.85 flat /
1.7777 for 16:9) draw the cinema bars, and a small read-out prints filter name, f-stop, focus
distance in metres and field of view (**Show Status Line** in the settings). Line colour is
`Guide Color`.

**Guides never end up in a saved photo.** A guide is a help, not part of the picture. The letterbox
*is* a real framing decision, so that one is a choice:
**Project Settings > ShutterMode > Burn In Letterbox**.

---

## 8. Capture

`Capture(ResolutionMultiplier)` — **1x, 2x or 4x** the current viewport resolution; `0` uses the
project default.

* **Output:** `<Project>/Saved/Photos/<Prefix>_<YYYYMMDD-HHMMSS>.png`.
  Folder and prefix are in the project settings; an empty prefix uses the project name.
  `GetPhotoDirectory()` returns the absolute folder and creates it on demand.
* **No HUD and no Slate UI in the image** — the capture is taken with the UI switched off.
* **The frame is respected.** The overlays are hidden, `Capture Frame Delay` frames are allowed to
  draw, and only *then* does the shutter fire. A screenshot captures a frame that has **already been
  drawn**, so changing state and triggering the capture in the same tick would photograph the state
  *before* the change — guides and all. Two frames is the safe default; raise it to 3 if your frame
  pacing is unusual.
* `IsCapturePending()` is true for the duration; use it to grey out your capture button.
* `On Photo Captured(FilePath)` fires when the file is on disk. There is a **watchdog**: if the
  engine never comes back, the overlays are restored anyway and a warning is logged to
  `LogShutterMode`, so a failed capture can never leave the UI in a broken state.

---

## 9. Project settings reference

**Project Settings > Plugins > ShutterMode** (stored in `DefaultGame.ini`).

Everything here is a *starting* value. The subsystem's setters, the `ShutterMode.*` console commands
and the photo state override them at runtime without ever writing back to the config.

| Section | Setting | Default | Notes |
|---|---|---|---|
| Session | `Pause Game` | `true` | **Turn off for network play.** |
| Session | `Disable Player Input` | `true` | Stops the pawn reacting while photo mode is open. |
| Session | `Enter / Exit Blend Time` | `0.25 s` | `0` is an instant cut. |
| Session | `Show Mouse Cursor` | `true` | For a mouse-driven photo UI. |
| Session | `Camera Class` | *(empty)* | Soft class ptr — a Blueprint subclass of `AShutterModeCamera`, if you want one. Empty falls back to the C++ class. |
| Camera | `Default Camera Mode` | `Free Fly` | Or `Orbit Subject`. |
| Camera | **`Max Distance From Subject`** | `800 cm` | **The leash.** The difference between a photo mode and a noclip cheat. |
| Camera | `Leash Enabled` / `Collision Enabled` | `true` | Both can be switched off — your call. |
| Camera | `Camera Collision Channel` | `Camera` | Trace channel for the wall check. |
| Camera | `Collision Padding` | `12 cm` | How far in front of a hit wall the camera parks. |
| Camera | `Move Speed` | `400 cm/s` | |
| Camera | `Look Sensitivity` | `1.0` | Degrees per unit of look input. |
| Camera | `Roll Speed` | `45 °/s` | |
| Camera | `Zoom Speed` | `5` | FOV degrees per unit of zoom input. |
| Camera | `Max Pitch` | `87°` | Kept below 90 so the camera never gimbal-flips. |
| Camera | `Min Orbit Distance` | `120 cm` | Closest the orbit camera may sit to its subject. |
| Optics | `Default Field Of View` | `70°` | |
| Optics | `Default Aperture` | `f/2.8` | |
| Optics | `Auto Focus By Default` | `true` | |
| Optics | `Auto Focus Trace Distance` | `50000 cm` | Nothing hit = focus at infinity. |
| Optics | `Auto Focus Channel` | `Visibility` | |
| Optics | `Sensor Width` | `36 mm` | Full frame. Super 35 is `24.89`. |
| Filters | `Filters` | *(empty)* | The filter wheel, in order. Soft references. |
| Filters | `Default Filter Index` | `0` | |
| Framing | `Guides Enabled By Default` | `false` | |
| Framing | `Default Guide Flags` | `Thirds \| Center Cross` | |
| Framing | `Letterbox By Default` / `Default Letterbox Ratio` | `false` / `2.39` | |
| Framing | `Burn In Letterbox` | `false` | The only overlay that may ever reach the file. |
| Framing | `Show Status Line` | `true` | Filter · f-stop · focus · FOV read-out. |
| Framing | `Guide Color` | white @ 35 % | |
| Capture | `Photo Directory` | `Photos` | Relative to the project's `Saved/`. |
| Capture | `Photo File Prefix` | *(empty)* | Empty uses the project name. |
| Capture | `Default Resolution Multiplier` | `1` | 1–8; 1/2/4 are the intended values. |
| Capture | `Capture Frame Delay` | `2` frames | See [section 8](#8-capture). |

---

## 10. Class and API overview

| Class | Kind | Role |
|---|---|---|
| `UShutterModeSubsystem` | `UTickableWorldSubsystem` | **The state holder.** Session, photo state, filter wheel, capture pipeline, guide overlay and the restore point. The only thing a game has to talk to. |
| `AShutterModeCamera` | `APawn` | The camera the player flies. Leash, collision, free-fly/orbit, ticks while paused. `Blueprintable`. |
| `FShutterModeState` | `USTRUCT`, `BlueprintType` | The complete look of one photo — copyable, storable in a save game, shippable as a preset. |
| `FShutterModeRestorePoint` | `USTRUCT` | Snapshot of everything photo mode is about to change. Written on entry, replayed on exit. |
| `UShutterFilterAsset` | `UPrimaryDataAsset` | One filter, as data: full grading channel set + optional post-process material. |
| `UShutterModeSettings` | `UDeveloperSettings` | Project-wide defaults (`config=Game`, `defaultconfig`). |
| `IShutterModeAware` | `UINTERFACE` | "Photo mode is starting — get out of the picture." |
| `UShutterModeStatics` | `UBlueprintFunctionLibrary` | One-node shortcuts, so a level Blueprint never has to spell out "get world subsystem". |
| `EShutterCameraMode` | `UENUM` | `FreeFly` · `OrbitSubject` |
| `EShutterGuide` | `UENUM` (bitflags) | `Thirds` · `CenterCross` · `GoldenRatio` · `SafeFrame` · `Diagonals` |

### `UShutterModeSubsystem`

| Function | Purpose |
|---|---|
| `static Get(WorldContextObject)` | The subsystem for this context's world, or null outside a game world. |
| `EnterPhotoMode(PC)` → `bool` | Open. Null PC = first local player. False if already open or no controller. |
| `ExitPhotoMode()` | Close and replay the restore point. Safe when nothing is open. |
| `TogglePhotoMode(PC)` → `bool` | One binding for the whole feature. |
| `IsInPhotoMode()` → `bool` | |
| `GetPhotoCamera()` → `AShutterModeCamera*` | |
| `GetState()` / `SetState(State)` | The whole `FShutterModeState`. Values are clamped on the way in. |
| `SetFieldOfView(f)` / `SetAperture(f)` / `SetFocusDistance(f)` / `SetAutoFocus(b)` | Individual optics. `SetFocusDistance` turns auto focus off. |
| `SetFocusFromScreenCenter()` → `bool` | One-shot focus pull. False if nothing was hit. |
| `SetGuidesEnabled(b)` / `SetLetterboxEnabled(b)` | |
| `GetFilters()` / `GetFilterCount()` / `SetFilters(Array)` | The runtime filter wheel. |
| `SetFilterIndex(i)` / `NextFilter()` / `PreviousFilter()` | The index **wraps** — no bounds checks needed. |
| `SetFilterIntensity(f)` | `0`…`1`. |
| `GetActiveFilter()` / `GetActiveFilterName()` | |
| `Capture(Multiplier)` → `bool` | `0` = project default. `true` means the request was accepted, not that the file exists. |
| `IsCapturePending()` / `GetPhotoDirectory()` | |
| `RegisterAwareObject(Obj)` / `UnregisterAwareObject(Obj)` | For non-actors (widgets). Actors are automatic. |
| `OnPhotoModeEntered` / `OnPhotoModeExited` | `BlueprintAssignable`, no parameters. |
| `OnPhotoCaptured(FilePath)` | `BlueprintAssignable`. Fires when the file is on disk. |

### `AShutterModeCamera`

**Input surface:** `AddMoveInput(Forward, Right, Up)`, `AddLookInput(Yaw, Pitch)`, `AddRoll(Amount)`,
`ZoomBy(Amount)`, `SetSpeedMultiplier(Multiplier)`.

**Framing:** `SetCameraMode(Mode)`, `GetCameraMode()`, `SetSubject(Actor)`, `GetSubject()`,
`ResetToSubject()`, `GetDistanceToSubject()`, `TraceFocusDistance()`, `GetPhotoCameraComponent()`.

**Per-session tunables** (seeded from the project settings when the session starts, writable at
runtime, `EditAnywhere` on a Blueprint subclass): `MaxDistanceFromSubject`, `bLeashEnabled`,
`bCollisionEnabled`, `CollisionChannel`, `CollisionPadding`, `MaxPitch`, `MinOrbitDistance`,
`MoveSpeed`, `LookSensitivity`, `RollSpeed`, `ZoomSpeed`, `AutoFocusTraceDistance`,
`AutoFocusChannel`, `SensorWidth`.

### `UShutterModeStatics`

`GetShutterMode`, `EnterPhotoMode`, `ExitPhotoMode`, `TogglePhotoMode`, `IsInPhotoMode`,
`CapturePhoto`, `GetState`, `SetState`, `NextFilter`, `PreviousFilter`, `GetActiveFilterName`,
`SetFocusFromScreenCenter`, `GetPhotoCamera` — all `WorldContext`, all thin forwards to the
subsystem, all no-ops with a sane return value when there is no photo mode in this world.

### `FShutterModeState`

| Group | Fields |
|---|---|
| Optics | `FieldOfView` (10–120) · `FocusDistance` (cm) · `Aperture` (1.2–22) · `bAutoFocus` · `Roll` (±180) · `ExposureBias` (±8 stops) |
| Colour | `FilterIndex` · `FilterIntensity` (0–1) · `Vignette` (0–2) · `Grain` (0–2) · `ChromaticAberration` (0–5) · `Saturation` (0–4) · `Contrast` (0–4) · `Temperature` (±4000 K) |
| Framing | `bGuides` · `GuideFlags` (bitmask of `EShutterGuide`) · `bLetterbox` · `LetterboxRatio` (1–4) |

### `IShutterModeAware`

`OnPhotoModeEnter()`, `OnPhotoModeExit()` — both `BlueprintNativeEvent`.

---

## 11. Code examples

### Toggle from a player controller, with a UI hook

```cpp
// MyPlayerController.cpp
#include "ShutterModeSubsystem.h"

void AMyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (UShutterModeSubsystem* Shutter = UShutterModeSubsystem::Get(this))
    {
        Shutter->OnPhotoModeEntered.AddDynamic(this, &AMyPlayerController::HandlePhotoModeEntered);
        Shutter->OnPhotoModeExited .AddDynamic(this, &AMyPlayerController::HandlePhotoModeExited);
        Shutter->OnPhotoCaptured   .AddDynamic(this, &AMyPlayerController::HandlePhotoCaptured);
    }
}

void AMyPlayerController::TogglePhotoMode()
{
    if (UShutterModeSubsystem* Shutter = UShutterModeSubsystem::Get(this))
    {
        Shutter->TogglePhotoMode(this);
    }
}

void AMyPlayerController::HandlePhotoCaptured(const FString& FilePath)
{
    UE_LOG(LogTemp, Display, TEXT("Photo saved: %s"), *FilePath);
    // ShowToast(FText::Format(NSLOCTEXT("Photo", "Saved", "Saved to {0}"), FText::FromString(FilePath)));
}
```

### Driving the camera from Enhanced Input

```cpp
// Remember: tick "Trigger When Paused" on every one of these Input Actions,
// otherwise nothing fires while photo mode has the game paused.
void AMyPlayerController::PhotoMove(const FInputActionValue& Value)
{
    UShutterModeSubsystem* Shutter = UShutterModeSubsystem::Get(this);
    if (!Shutter || !Shutter->IsInPhotoMode()) { return; }

    if (AShutterModeCamera* Cam = Shutter->GetPhotoCamera())
    {
        const FVector Axis = Value.Get<FVector>();          // X = forward, Y = right, Z = up
        Cam->AddMoveInput(Axis.X, Axis.Y, Axis.Z);
    }
}

void AMyPlayerController::PhotoLook(const FInputActionValue& Value)
{
    UShutterModeSubsystem* Shutter = UShutterModeSubsystem::Get(this);
    if (!Shutter || !Shutter->IsInPhotoMode()) { return; }

    if (AShutterModeCamera* Cam = Shutter->GetPhotoCamera())
    {
        const FVector2D Axis = Value.Get<FVector2D>();
        Cam->AddLookInput(Axis.X, Axis.Y);
    }
}
```

### A "portrait" preset button

```cpp
void AMyPhotoUI::ApplyPortraitPreset()
{
    UShutterModeSubsystem* Shutter = UShutterModeSubsystem::Get(this);
    if (!Shutter) { return; }

    FShutterModeState State = Shutter->GetState();   // start from what the player has now
    State.FieldOfView     = 35.0f;                   // telephoto compression
    State.Aperture        = 1.4f;                    // melt the background
    State.bAutoFocus      = true;
    State.Vignette        = 0.6f;
    State.bGuides         = true;
    State.GuideFlags      = static_cast<int32>(EShutterGuide::Thirds);
    State.bLetterbox      = false;

    Shutter->SetState(State);                        // clamped on the way in
}
```

### Saving and restoring a photo state

`FShutterModeState` is a plain `USTRUCT` — put it in your save game as-is.

```cpp
// Save
MySaveGame->LastPhotoState = Shutter->GetState();

// Restore, later, in a different session
Shutter->SetState(MySaveGame->LastPhotoState);
```

### Unlockable filters

```cpp
void AMyGameMode::RefreshUnlockedFilters()
{
    TArray<UShutterFilterAsset*> Unlocked;
    for (UShutterFilterAsset* Filter : AllFilters)
    {
        if (PlayerHasUnlocked(Filter)) { Unlocked.Add(Filter); }
    }

    if (UShutterModeSubsystem* Shutter = UShutterModeSubsystem::Get(this))
    {
        Shutter->SetFilters(Unlocked);   // the wheel becomes exactly this list, in this order
    }
}
```

### Orbit the character instead of flying

```cpp
if (AShutterModeCamera* Cam = Shutter->GetPhotoCamera())
{
    Cam->SetSubject(MyCharacter);
    Cam->SetCameraMode(EShutterCameraMode::OrbitSubject);
    Cam->ResetToSubject();
}
```

### Capture without the pause (network play)

Switch **Pause Game** off in the project settings. Nothing else changes:

```cpp
Shutter->EnterPhotoMode(nullptr);   // world keeps simulating, camera detaches anyway
Shutter->Capture(2);                // 2x resolution
```

---

## 12. Console commands

All five operate on the first local player's world. Useful for QA, for marketing capture and for
checking the installation without writing a single binding.

| Command | Effect |
|---|---|
| `ShutterMode.Enter` | Open photo mode for the first local player. |
| `ShutterMode.Exit` | Close it and replay the restore point. |
| `ShutterMode.Capture [n]` | Take a photo at *n*× resolution. Without an argument, the project default. |
| `ShutterMode.Filter [i]` | Select filter *i*. Without an argument, advance to the next one. |
| `ShutterMode.Guides [0\|1]` | Show or hide the guides. Without an argument, toggle. |

Log category: `LogShutterMode`.

---

## 13. What the restore point covers

This is the part that matters, and the reason the plugin exists. A photo mode is not hard because of
the camera — it is hard because it reaches into five unrelated systems at once and every hand-rolled
version eventually forgets to put one of them back.

Entering photo mode records, **before touching anything**:

* the current **view target**,
* whether the game was **already paused** — and by whom (`bWasPaused` vs. `bPausedByUs`),
* `bShowMouseCursor`, `bEnableClickEvents`, `bEnableMouseOverEvents`,
* the move-input and look-input ignore state,
* the controller's tick-when-paused flag,
* the viewport's **mouse capture mode**, **mouse lock mode**, hide-cursor-during-capture and
  ignore-input — which together *are* the input mode, since `SetInputMode()` is nothing but a writer
  for those four values. Recording them is a genuinely complete snapshot, not an approximation.
* every object that was told photo mode started.

Exiting replays exactly that. Nothing is inferred, nothing is guessed and nothing is "reset to a
sensible default" — a default is how you break somebody else's game.

**If the game was already paused when photo mode opened** (a pause menu with a "take a picture"
button is the common case), leaving photo mode does **not** un-pause it. That is somebody else's
pause and it stays.

Every reference is **weak**, and each step is individually guarded: if the pawn died, the view falls
back to the current pawn and then to the controller; if the controller is gone, only the pause is
undone; if the level changed, the whole restore point is dropped rather than written into the wrong
world.

---

## 14. Limits — read before you buy

* **Still images only.** There is no video or GIF recorder in here.
* **No character posing**, no facial expressions, no emote control. That is a separate product.
* **No sticker, frame or overlay-graphics library.** Letterbox and composition guides, yes; an art
  pack, no.
* **No multiplayer photo pause.** Pausing is a local, single-player concept — a client that pauses
  itself only falls behind the server. In a networked game, switch **Pause Game** off and use the
  camera, filters, guides and capture without it.
* **Filters are colour grading plus an optional post-process material** — not a shader construction
  kit.
* **Depth of field can only show what your scene gives it.** If nothing in frame is at a different
  distance, no aperture value will look like anything.
* **PNG only.** HDR `.exr` capture is not wired up.
* **No filter assets are shipped.** The filter wheel is yours to fill — see
  [section 5](#5-writing-your-own-filters) for six recipes to start from.
* **Desktop platforms only** (Win64 / Mac / Linux). Nothing in the code is desktop-specific, but
  console platforms are untested and unsupported.

---

## 15. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Camera does not move while paused | The input binding is not marked to fire during a pause. Enhanced Input: **Trigger When Paused** on the Input Action. Legacy: `bExecuteWhenPaused`. |
| The whole view is frozen, but the camera reports moving | Something else took the view target after photo mode started. |
| Aperture does nothing | Nothing in frame is at a different depth, or `r.DepthOfFieldQuality` is `0`. |
| Guides appear in the saved photo | `Capture Frame Delay` is too low for your frame pacing — raise it to `3`. |
| Filter name shows `None` / the wheel is empty | No filter assets are listed in **Project Settings > Plugins > ShutterMode > Filters**. The plugin ships none by design. |
| Photo mode leaves the game paused | Something else paused it *before* photo mode was opened. That pause is deliberately not ours to lift. |
| Nothing happens on `Enter Photo Mode` | No player controller yet, or the call ran on a non-game world (the subsystem only exists in Game and PIE worlds). |
| Camera will not fly far enough | The leash: raise **Max Distance From Subject** or switch **Leash Enabled** off. |
| Camera stops short of where the player aimed | The collision trace found a wall. Adjust **Camera Collision Channel** / **Collision Padding**, or switch **Collision Enabled** off. |
| No file appears after `Capture` | Check `LogShutterMode` — a watchdog warning means the engine never reported the screenshot back. Check write permission on `<Project>/Saved/`. |
| A widget stays hidden after exiting | It implements `IShutterModeAware` but hides itself in a way that its own `OnPhotoModeExit` does not undo. |

---

## 16. Support

* **Documentation & issues:** <https://github.com/SimulatedFlow/ue-plugin-ShutterMode>
* **E-mail:** teufelsilvan@gmail.com

When reporting a problem, please include your engine version, target platform, whether you are in
PIE or a packaged build, and the `LogShutterMode` output.

---

*ShutterMode 1.0.0 — a single runtime module: `Core`, `CoreUObject`, `Engine`, `InputCore`,
`DeveloperSettings`, `RenderCore`. Win64 / Mac / Linux. Unreal Engine 5.8.*
