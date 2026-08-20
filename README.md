# ShutterMode — In-Game Photo Mode

A finished player photo mode for **Unreal Engine 5.8**: press a key, the game pauses, the HUD steps
aside, a leashed free camera detaches from the character, and the player frames a shot with real
depth of field, colour filters, composition guides and a letterbox — then everything goes back
exactly the way it was.

The value is not in any single feature. It is that the five things a photo mode touches — camera,
pause, HUD, post-processing and file output — are all cleanly put back. That is where hand-built
photo modes fall over.

## Quick start

```cpp
#include "ShutterModeSubsystem.h"

if (UShutterModeSubsystem* Shutter = UShutterModeSubsystem::Get(this))
{
    Shutter->TogglePhotoMode(nullptr);   // null = first local player controller
}
```

Then wire the camera to your input (`Get Photo Camera` → `Add Move Input` / `Add Look Input` /
`Add Roll` / `Zoom By`) and call `Capture Photo`. The plugin ships no Input Actions of its own, so
there is nothing to collide with your project's input setup.

## What is in the box

| Class | Role |
|---|---|
| `UShutterModeSubsystem` | State holder, session, capture, guides — **and the restore point**. |
| `AShutterModeCamera` | Leashed, collision-aware free-fly / orbit camera that moves while paused. |
| `UShutterFilterAsset` | A filter as data: full grading channel set + optional post-process material. |
| `UShutterModeSettings` | Project defaults (Project Settings > Plugins > ShutterMode). |
| `IShutterModeAware` | "Photo mode is starting — get out of the picture." |
| `UShutterModeStatics` | One-node Blueprint shortcuts. |
| `FShutterModeState` | The complete look of one photo, copyable and saveable. |

## Console commands

`ShutterMode.Enter` · `ShutterMode.Exit` · `ShutterMode.Capture [n]` · `ShutterMode.Filter [i]` ·
`ShutterMode.Guides 0|1`

## Documentation

* [`Docs/DOCUMENTATION.md`](Docs/DOCUMENTATION.md) — five-minute integration, authoring filters,
  settings reference, full API, troubleshooting, limits.
* [`Docs/Fab-Store-Description.md`](Docs/Fab-Store-Description.md) — store listing text.

## Requirements

Unreal Engine 5.8 · Win64 / Mac / Linux · one Runtime module · no third-party libraries.

---

© 2026 Silvan Teufel. All Rights Reserved.
