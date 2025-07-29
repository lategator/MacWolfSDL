# MacWolfSDL

A source port of Macintosh Wolfenstein 3D to modern platforms using SDL3.
Preserves compatibility with Third Encounter scenarios.

## Running

The Mac binary `Wolfenstein 3D` and the `Levels/` subdirectory need to be taken
from the original game install and placed in the same folder as the MacWolfSDL
executable. All the necessary files can be downloaded at:
https://archive.org/details/macintosh-wolfenstein-3d-macbinary

Alternatively the files can be placed in a platform-specific config folder:

- Windows: `C:\Users\<USER>\AppData\Roaming\macwolfsdl\`
- macOS: `/Users/<USER>/Library/Application Support/macwolfsdl/`
- Linux: `/home/<USER>/.local/share/macwolfsdl/`

Where `<USER>` is your local username. If you're extracting the files yourself,
they can be any of these formats: MacBinary II, AppleSingle, AppleDouble, raw
resource fork.

Additional custom scenarios can be loaded using `File → Load Scenario` in the
menu, or can be placed in the `Levels/` folder just as in the original Third
Encounter release.

More custom scenarios can be downloaded at:

- https://archive.org/details/dermuda-wolfenstein-3d-loose-scenarios-macbinary
- https://archive.org/details/dermuda-wolfenstein-3d-scenario-packs-macbinary

## Playing

### Default Controls

| Key              | Action            |
| ---------------- | ----------------- |
| Arrow Keys       | Move & Turn       |
| Z, X             | Strafe Left/Right |
| Left Ctrl        | Attack            |
| Space            | Use Door/Switch   |
| Left Shift       | Run               |
| Left Alt         | Strafe            |
| Tab              | Cycle Weapons     |
| 1, 2, 3, 4, 5, 6 | Select Weapon     |
| /                | Auto Map          |
| Escape           | Pause/Menu        |

The menu can also be accessed by pressing Escape at the title screen. In-game
controls can be adjusted using `Options → Configure Keyboard` in the menu.

## Building

Source code is available at: https://github.com/LateGator/MacWolfSDL/

CMake and SDL3 are required to build. SDL3 will be downloaded automatically if
it is not present on the system.

```bash
cmake -B build . && cmake --build build
```

## Links

- [Original Wolf3D-Mac Source Release](https://github.com/Blzut3/Wolf3D-Mac/)
- [Wolfenstein 3D - Macintosh Garden](https://macintoshgarden.org/games/wolfenstein-3d)
- [Dermuda's Wolfenstein 3D Archive - Macintosh Garden](https://macintoshgarden.org/games/dermudas-wolfenstein-3d-archive)
