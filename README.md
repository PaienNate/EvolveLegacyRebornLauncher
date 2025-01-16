# EvolveLegacyReborn Launcher

## Installation
TODO

## Building from source
#### NOTE: THIS TOOL IS ONLY MADE FOR WINDOWS SYSTEMS. 
#### PROTON COMPATIBILITY HAS NOT BEEN TESTED, USE AT YOUR OWN RISK

### Windows:
- clone this repository
- check out N3N-PATCH.md
- clone n3n via wsl (you want unix line-endings, crlf breaks the wsl shell)
- apply the patch as outlined in N3N-PATCH.md
- compile the custom n3n-edge
- in the root folder of this repo, run these commands to create the cmake build config and compile the launcher:
```shell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```
- create a new folder called "bin" in the same spot as your EvolveN3NManager.exe and copy the custom n3n-edge.exe into it
- copy the EvolveN3NManager.exe, EvolveLauncher.exe and the bin folder into your EvolveGame/bin64_SteamRetail folder 
  - (or just run the Launcher and it will try to figure things out itself (TODO))