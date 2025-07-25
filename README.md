# MacWolfSDL

A source port of Macintosh Wolfenstein 3D to modern platforms using SDL3. Preserves compatibility with Third Encounter scenarios.

## Building

CMake and SDL3 are required to build. SDL3 will be downloaded automatically if it is not present on the system.

```bash
cmake -B build . && cmake --build build
```

## Running

The Mac binary or its resource fork need to be taken from the original game install and placed either in the same folder as the macwolfsdl executable, or in a platform-specific config folder:

- Windows: `C:\Users\<USER>\AppData\Roaming\macwolfsdl\`
- macOS: `/Users/<USER>/Library/Application Support/macwolfsdl/`
- Linux: `/home/<USER>/.local/share/macwolfsdl/`

Where `<USER>` is your local username. The file can be any of these formats: MacBinary II, AppleSingle, AppleDouble, raw resource fork.

Scenario files accept the same formats and need to be placed within a `/Levels/` subfolder of the config folder. A fully set-up Third Encounter will look like this:

```bash
macwolfsdl/Wolfenstein 3D™
macwolfsdl/Levels/ Second Encounter (30 Levels)
macwolfsdl/Levels/1 Escape From Wolfenstein
macwolfsdl/Levels/2 Operation Eisenfaust
macwolfsdl/Levels/3 Die, Fuhrer, Die!
macwolfsdl/Levels/4 A Dark Secret
macwolfsdl/Levels/5 Trail of the Madman
macwolfsdl/Levels/6 Confrontation
```

Additional custom scenarios can be placed in the `/Levels/` folder and can be played just as in the original Third Encounter release.
