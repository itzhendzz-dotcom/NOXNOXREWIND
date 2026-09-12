# NOXXA REWIND v3

V3 is a stability-first rewrite of the world-rewind branch for GTA SA Android v2.00 / AML 1.3+.

## Design
The player is the time anchor. Nearby pedestrians and vehicles are recorded into a fixed-capacity timeline and played backward while the player remains in the present position. The default rewind window is exactly 13 seconds.

## V3 changes
- explicit Idle -> Arming -> Rewinding -> Recovering state machine
- two-frame safety barrier before any world/audio mutation
- release lockout so holding after auto-end cannot instantly retrigger
- model-index validation on every historical entity restore
- finite/position sanity checks before writing transforms
- >20m per-snapshot discontinuities are rejected instead of teleported
- matrix basis interpolation for smoother vehicle/ped rotation
- missing history samples are skipped instead of popping entities into existence
- fixed timeline allocation; no hot-path heap growth
- compact top-left vector UI with charge/remaining indicator
- neutral memory-smear FX with soft vignette and a 13-second segmented timeline
- exact 13.00-second gameplay cap
- the user-provided `rewind_user.wav` remains a single callback-free OpenSL buffer

## Animation safety
The old crash-on-press correlated with the animation/task-tree path. V3 contains a non-clearing one-shot focus gesture, but `PoseEnabled=0` by default. The core/world/audio path is intentionally kept independent from animation tasks.
