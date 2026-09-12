# NOXXA REWIND v2 — ANCHOR MODE

**The player stays. The world rewinds.**

This is a clean rebuild of the original NOXXA REWIND prototype for GTA: San Andreas Android 2.00 (armeabi-v7a / AML).

## Core behavior

- Hold `<<` while on foot.
- The player becomes a fixed **time anchor** instead of being rewound.
- A one-hand-up vanilla animation is played (`IDLE_TAXI` / `PED` by default).
- Nearby peds and vehicles are sampled into an 8-second rolling history.
- While held, those world entities scrub backward through their stored transforms and velocities.
- Release the button and that historical world state becomes the new timeline branch.
- Double-tap performs a quick rewind.

## Why the world uses a bubble

Trying to snapshot the entire GTA map every frame would destroy performance and would also fight streaming. v2 records the nearest configurable entities (default 48) inside a configurable radius (default 45 m). The player remains outside that rewind stream.

## What v2.0 intentionally does not fake

- It does not resurrect despawned/dead entities yet.
- It does not rewind GTA task trees or mission scripts.
- It does not rewind the player itself.
- Ped animation time is not reversed yet; physical motion is. This is a later animation-layer milestone.

Those constraints are deliberate stability boundaries, not hidden limitations.

## Feel pass

v2 adds a lightweight cinematic temporal layer: moving scan streaks, cool veil, breathing vignette, chromatic edge echoes, activation pulse, timeline ribbon, native touch UI, and fully original generated enter/loop/release audio.

No Life is Strange audio or other proprietary assets are copied.
