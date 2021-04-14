# Unnamed SDVX clone ![language: C/C++](https://img.shields.io/badge/language-C%2FC%2B%2B-green.svg) [![Build](https://github.com/Drewol/unnamed-sdvx-clone/workflows/Build/badge.svg)](https://github.com/Drewol/unnamed-sdvx-clone/actions)
A game based on [KShootMania](http://www.kshootmania.com/) and [SDVX](https://remywiki.com/What_is_SOUND_VOLTEX).

### [**Download latest Windows build**](https://drewol.me/Downloads/Game.zip)

### [**FAQ**](https://github.com/Drewol/unnamed-sdvx-clone/wiki/F.-A.-Q.)

#### [**Skinning Documentation**](https://unnamed-sdvx-clone.readthedocs.io/en/latest/index.html)

#### Demo Videos:
[![Gameplay Video](http://img.youtube.com/vi/1GCraT0ktrc/2.jpg)](https://youtu.be/1GCraT0ktrc)
[![Portrait Gameplay](http://img.youtube.com/vi/kP1tD6bkPa4/2.jpg)](https://youtu.be/kP1tD6bkPa4)
[![Various Settings](http://img.youtube.com/vi/_g9Xv5RDwa0/2.jpg)](https://youtu.be/_g9Xv5RDwa0)

### Current features:
- Completely skinnable GUI
- OGG/MP3 Audio streaming (with preloading for gameplay performance)
- Uses KShoot charts (`*.ksh`) (1.6 supported)
- Functional gameplay and scoring
- Saving of local scores
- Autoplay
- Basic controller support (Still some issues with TC+)
- Changeable settings and key mapping in configuration file
- Skinning support
- Supports new sound FX method (real-time sound FX) and old sound FX method (separate NOFX & sound effected music files)
- Song database cache for near-instant game startup (sqlite3)
- Song database searching
- Linux/Windows/macOS support

### Features currently on hold / in progress:
- Song select UI/Controls to change HiSpeed and other game settings

The folder that is scanned for songs can be changed in the "Main.cfg" file (`songfolder = "path to song folder"`).  
Make sure you use a *plain* text editor like Notepad, Notepad++ or gedit; NOT a *rich* text editor like Wordpad or Word!

If something breaks in the song database, delete "maps.db". **Please note this will also wipe saved scores.**

## Controls
### Default bindings (Customizable):
- Start: \[1\]
- BTN (White notes , A/B/C/D): \[D\] \[F\] \[J\] \[K\]
- FX (Yellow notes, L/R): \[C\] \[M\]
- VOL-L (Cyan laser, Move left / right): \[W\] \[E\]
- VOL-R (Magenta laser): \[O\] \[P\]

### Song Select:
- Use the arrow keys or knobs to select a song and difficulty
- Use \[Page Down\]/\[Page Up\] to scroll faster
- Press \[F2\] to select a random song
- Press \[F8\] demo mode (continuously autoplay random songs)
- Press \[F9\] to reload the skin
- Press \[F11\] to open the the currently selected chart in the editor specified by the `EditorPath` setting
- Press \[F12\] to open the directory of the currently selected song in your file explorer
- Press \[Enter\] or \[Start\] to start a song
- Press \[Ctrl\]+\[Start\] to start song with autoplay
- Press \[FX-L\] to open up filter select to filter the displayed songs
- Press \[Start\] when selecting filters to toggle between level and folder filters
- Press \[FX-L\] + \[FX-R\] to open up game settings (Hard gauge, Random, Mirror, etc.)
- Press \[TAB\] to open the Search bar on the top to search for songs

## How to run:
Just run 'usc-game' or 'usc-game_Debug' from within the 'bin' folder. Or, to play a chart immediately:  
#### `{Download Location}/bin> usc-game {path to *.ksh chart} [flags]`

#### Command line flags (all are optional):
- `-notitle` - Skips the title menu launching the game directly into song select.
- `-mute` - Mutes all audio output
- `-autoplay` - Plays chart automatically, no user input required
- `-autobuttons` Like autoplay, but just for the buttons. You only have to control the lasers
- `-autoskip` - Skips beginning of song to the first chart note
- `-debug` - Used to show relevant debug info in game such as hit timings, and scoring debug info
- `-test` - Runs test scene, for development purposes only

## How to build:
Clone the project and then run `git submodule update --init --recursive` to download the required submodules.

### Windows:
It is not required to build from source. A download link to a pre-built copy of the game is located at the beginning of this README.
The recommended Visual Studio version is 2017, if you want to use a different version then you
will need to edit the 'GenerateWin64ProjectFiles.bat' if you want to follow the guide below.

1. Install [CMake](https://cmake.org/download/)
2. Install [vcpkg](https://github.com/microsoft/vcpkg)
3. Install the packages listed in 'build.windows'
4. Run 'GenerateWin64ProjectFiles.bat' from the root of the project
    * If this fails, try using the `-DCMAKE_TOOLCHAIN_FILE=[VCPKG_ROOT]\scripts\buildsystems\vcpkg.cmake` flag that vcpkg should give you on install
5. Build the generated Visual Studio project 'FX.sln'
6. Run the executable made in the 'bin' folder

To run from Visual Studio, go to Properties for Main > Debugging > Working Directory and set it to '$(OutDir)' or '..\\bin'

### Linux:
1. Install [CMake](https://cmake.org/download/)
2. Check 'build.linux' for libraries to install
3. Run `cmake -DCMAKE_BUILD_TYPE=Release .` and then `make` from the root of the project
4. Run the executable made in the 'bin' folder

### macOS:
1. Install dependencies
	* [Homebrew](https://github.com/Homebrew/brew): `brew install cmake freetype libvorbis sdl2 libpng jpeg libarchive libiconv`
2. Run `mac-cmake.sh` and then `make` from the root of the project.
3. Run the executable made in the 'bin' folder.

### Embedded (Raspberry Pi):
1. Install the libraries listed in 'build.linux'
	* For things that are not in the package manager repository or too low of a version you have to download and build yourself
	* SDL2 Can be installed using the instructions found [here](https://wiki.libsdl.org/Installation#Raspberry_Pi)
2. Run `cmake -DEMBEDDED=ON .`
3. If cmake completes succesfully run `make`
4. Run the executable made in the 'bin' folder
