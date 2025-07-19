# MacWolfSDL

A source port of Macintosh Wolfenstein 3D to modern platforms using SDL3. Preserves compatibility with Third Encounter scenarios.

## Building

CMake and SDL3 are required to build. SDL3 will be downloaded automatically if it is not present on the system.

```bash
cmake -B build . && cmake --build build
```

## Running

The resource fork from the Mac binary needs to be extracted with the name `Wolf3D.rsrc` and placed either in the same folder as the macwolfsdl executable, or in a platform-specific config folder:

- Windows: `C:\Users\<USER>\AppData\Roaming\macwolfsdl\`
- macOS: `/Users/<USER>/Library/Application Support/macwolfsdl/`
- Linux: `/home/<USER>/.local/share/macwolfsdl/`

Where `<USER>` is your local username.

Resource forks from scenarios also need to be extracted and placed within a `/Levels/` subfolder of the config folder. A fully extracted Third Encounter will look like this:

```bash
macwolfsdl/Wolf3D.rsrc
macwolfsdl/Levels/ Second_Encounter (30 Levels).rsrc
macwolfsdl/Levels/1 Escape From Wolfenstein.rsrc
macwolfsdl/Levels/2 Operation Eisenfaust.rsrc
macwolfsdl/Levels/3 Die, Fuhrer, Die!.rsrc
macwolfsdl/Levels/4 A Dark Secret.rsrc
macwolfsdl/Levels/5 Trail of the Madman.rsrc
macwolfsdl/Levels/6 Confrontation.rsrc
```

Additional custom scenarios can be placed in the `/Levels/` folder and can be played just as in the original Third Encounter release.
