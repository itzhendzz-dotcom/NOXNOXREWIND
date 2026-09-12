# Install

Target: GTA SA Android 2.00, 32-bit / armeabi-v7a, AML 1.3+

Copy:

```text
Android/data/com.rockstargames.gtasa/mods/
├── libNoxxaRewind.so
└── NoxxaRewind/
    ├── rewind_enter.wav
    ├── rewind_loop.wav
    └── rewind_release.wav
```

Optional: copy `NoxxaRewind.ini.example` as your AML config if you want to tune radius/history/button placement.

When the game starts you should see:

`NOXXA REWIND v2 loaded - player is the time anchor`

Rewind is intentionally disabled while the player is inside a vehicle in v2.0.
