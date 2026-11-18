# Yamagi Quake II for the Wii U

Experimental port. It renders with Yamagi Quake II's OpenGL 3.2 renderer
through [gx2gl](https://github.com/ExpansionPak/gx2gl), an OpenGL 3.3 Core
implementation on top of GX2. Needs the Aroma environment.

## Installing

1. Copy `yquake2.wuhb` to `sd:/wiiu/apps/`.
2. Copy the game data to `sd:/wiiu/apps/yquake2/baseq2/`:
   - the full game: the `.pak` files from your copy's `baseq2` folder
     (Steam, GOG or CD), or
   - the free demo: `pak0.pak` from the demo's `baseq2` folder.

The SD card then looks like this:

```
sd:/wiiu/apps/yquake2.wuhb
sd:/wiiu/apps/yquake2/baseq2/pak0.pak
```

Configs, savegames and screenshots are written to
`sd:/wiiu/apps/yquake2/baseq2/`, and the console log of the last run to
`sd:/wiiu/apps/yquake2/yquake2.log`.

## Controls

The GamePad and Pro Controllers work through SDL's game controller
support, with Yamagi Quake II's default gamepad bindings. The image is
shown on the TV and mirrored to the GamePad.

## Not supported yet

- Network play (single player only)
- Touch screen and keyboard
- Stencil shadows (`gl_shadows`) and MSAA

## Building

See the comment at the top of `wiiu/CMakeLists.txt`.
