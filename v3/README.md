# NOXXA REWIND v3.1 — CAUSAL REWIND

V3.1 changes the meaning of rewind. The player remains the time anchor, but nearby world entities are no longer merely dragged through old transforms while their AI continues to live in the present.

## Core idea

**V3.0:** historical transform rollback.

**V3.1:** historical simulation playback + causal commit.

While rewind is active, GTA simulation time is frozen (`TimeScale=0.00` by default) and the rewind cursor continues using `steady_clock` real time. This prevents ambient AI/physics from advancing into the future while old world states are being replayed.

Every captured ambient ped now includes:
- transform / move speed / turn speed
- health + armour
- ped state + move states + animation group
- selected weapon slot, ammo and weapon state when the live slot still matches
- attack/weapon timing stored as relative delays
- last-damage timing stored as historical age
- selected behaviour flags (look, aim, fire, duck, stay, wanted/chased)
- active task type and temporary/non-temporary event-response task types (IDs only; never raw task pointers)

Every captured vehicle now also includes steering, throttle, brake, gear, engine, handbrake and lights state.

## Causal commit

On release, the exact interpolated historical world is materialised before the future branch is discarded. For ambient GAME-created pedestrians, V3.1 compares the live event-response task types against the selected historical frame. If the task response belongs to the discarded future, `ClearTaskEventResponse()` is used; the whole task tree is never flushed and `FlushImmediately()` is never used.

Historical timers are rebased onto the new present instead of copying stale absolute GTA timestamps. This prevents old attack/weapon timers from instantly expiring after rewind.

Mission/script peds are deliberately protected from task-tree rewriting. Their transforms/state may be replayed if captured, but V3.1 will not clear their scripted primary task tree.

## Safety model

- fixed-capacity timeline; no hot-path heap growth
- pool ref + model-index identity validation
- finite transform checks and >20m discontinuity rejection
- no stored `CTask*`, entity pointers, or animation task pointers in history
- no `Flush()` / `FlushImmediately()`
- two-frame input arm barrier remains
- pose system stays disabled by default (`PoseEnabled=0`)

## Current limitations

V3.1 still does not resurrect already-dead/despawned peds, rewind projectiles/explosions, recreate destroyed entities, or fully reconstruct arbitrary scripted task internals. It intentionally prefers a safe historical reset over pretending raw engine pointers can be serialized.

The default rewind window remains exactly 13 seconds and `rewind_user.wav` remains a single callback-free OpenSL buffer.
