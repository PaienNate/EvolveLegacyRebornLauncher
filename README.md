# EvolveLegacyReborn Launcher

## Installation
TODO

## Building from source
#### NOTE: THIS TOOL IS ONLY MADE FOR WINDOWS SYSTEMS. 
#### PROTON COMPATIBILITY HAS NOT BEEN TESTED, USE AT YOUR OWN RISK

### Windows:
#### Make sure you have QT 6.8.1 with sources installed!
#### Make sure you have Visual Studio 2022 Buildtools for x64 Architecture installed (cmake, cl, ninja, etc)!

First, we have to build QT from source with static linking enabled
Open the "x64 Native Tools Command Prompt for VS 2022"
Navigate to the folder where your QT install is (the one where the Src folder is) (in my case this is "C:\Qt\6.8.1")
Make sure to replace the prefix with the path where you want the static QT files to be installed to!

#### WARNING: The initial QT build will take a LONG time and probably cause your PC to lag, this is normal, QT has roughly 16k files!
#### QT is heavy if you compile it yourself (around 93GB for me for just the build files). Make sure you have enough disk space BEFORE you compile it!
#### You can in theory get rid of the build folder once you ran `ninja install`. I wouldn't recommend to do so unless you know you won't need to compile it again later!
#### You can skip this step, QT will be linked dynamically in that case and might require extra dlls to run!
#### If you get missing header files you are likely missing a library. Just search for the missing header file on google and install the lib with the Visual Studio installer
#### Make sure to relaunch your terminal after installing missing libraries!
#### Don't worry if the build does fail, you only have to compile the files you haven't successfully compiled again
```shell
mkdir build
cd build
"../Src/configure.bat" -prefix "C:\Qt\6.8.1\MSVC-Static" -debug-and-release -static -opensource -confirm-license -platform win32-msvc -qt-zlib -qt-libpng -qt-libjpeg -nomake examples -nomake tests -no-opengl -skip qtscript -skip qtquick3d -skip qtgraphs -skip qtquick3dphysics -cmake-generator "Ninja Multi-Config"
cmake --build . --parallel
ninja install
```

- clone this repository
- check out N3N-PATCH.md
- clone n3n via wsl (you want unix line-endings, crlf breaks the wsl shell)
- apply the patch as outlined in N3N-PATCH.md
- compile the custom n3n-edge
- create a new folder called "bin" in the root folder of this repo and copy the custom n3n-edge.exe into it
- in the root folder of this repo, run these commands to create the cmake build config and compile the launcher
  - Make sure to set the CMAKE_PREFIX_PATH to the place where you have installed QT on your system:
  - you might to run this inside the "x64 Native Tools Command Prompt for VS 2022" as well in case things like WMF can't be found
```shell
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH="C:\Qt\6.8.1\MSVC-Static" ..
cmake --build . --config Release
```
- run the launcher and point it to your bin64_SteamRetail folder via the Settings, it will then copy all the necessary files into the correct places