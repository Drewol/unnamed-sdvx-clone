# Unnamed SDVX clone ![language: C/C++](https://img.shields.io/badge/language-C%2FC%2B%2B-green.svg) [![Windows Build Status](https://ci.appveyor.com/api/projects/status/github/drewol/unnamed-sdvx-clone?branch=master&svg=true&retina=true)](https://ci.appveyor.com/project/drewol/unnamed-sdvx-clone) [![Linux Build Status](https://travis-ci.org/Drewol/unnamed-sdvx-clone.svg?branch=master)](https://travis-ci.org/Drewol/unnamed-sdvx-clone)
A game based on [KShootMania](http://www.kshootmania.com/) and [SDVX](https://remywiki.com/What_is_SOUND_VOLTEX).

### [**Download latest Windows build**](https://drewol.me/Downloads/Game.zip)

#### Demo Videos:
[![Gameplay Video](http://img.youtube.com/vi/uMlAj9xfSEA/2.jpg)](https://youtu.be/uMlAj9xfSEA)
[![Laser Color Settings](http://img.youtube.com/vi/tJXVr1KdZsc/2.jpg)](https://youtu.be/tJXVr1KdZsc)

### Current features:
- Basic GUI (Buttons, Sliders, Scroll Boxes)
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
- GUI remake
- Song select UI/Controls to change HiSpeed and other game settings

The folder that is scanned for songs can be changed in the "Main.cfg" file (`songfolder = "path to song folder"`).  
Make sure you use a *plain* text editor like Notepad, Notepad++ or gedit; NOT a *rich* text editor like Wordpad or Word!

If something breaks in the song database, delete "maps.db". **Please note this will also wipe saved scores.**

## Controls
### Game (Customizable, read 'Readme_Input.txt'):
- BTN (White notes): \[D\] \[F\] \[J\] \[K\]
- FX (Yellow notes) = \[C\] \[M\]
- VOL-L (Cyan laser, Move left / right) = \[W\] \[E\]
- VOL-R (Magenta laser) = \[O\] \[P\]
- Quit song & go back to song selection \[Esc\]

### Song Select:
- Use the arrow keys to select a song and difficulty
- Use \[Page Down\]/\[Page Up\] to scroll faster
- Press \[Enter\] to start a song
- Press \[Ctrl\]+\[Enter\] to start song with autoplay
- Use the Search bar on the top to search for songs

## How to run:
Just run 'Main_Release' or 'Main_Debug' from within the 'bin' folder. Or, to play a chart immediately:  
#### `{Download Location}/bin> Main_{Release or Debug} {path to *.ksh chart} [flags]`

#### Command line flags (all are optional):
- `-notitle` - Skips the title menu launching the game directly into song select.
- `-mute` - Mutes all audio output
- `-autoplay` - Plays chart automatically, no user input required
- `-autobuttons` Like autoplay, but just for the buttons. You only have to control the lasers
- `-autoskip` - Skips beginning of song to the first chart note
- `-convertmaps` - Allows converting of `*.ksh` charts to a binary format that loads faster (experimental, feature not complete)
- `-debug` - Used to show relevant debug info in game such as hit timings, and scoring debug info
- `-test` - Runs test scene, for development purposes only

## How to build:

### Windows:
It is not required to build from source. A download link to a pre-built copy of the game is located at the beginning of this README. But, if you must:
1. Install [CMake](https://cmake.org/download/)
2. Run `cmake .` from the root of the project
3. Build the generated Visual Studio project 'FX.sln'
4. Run the executable made in the 'bin' folder

To run from Visual Studio, go to Properties for Main > Debugging > Working Directory and set it to '$(OutDir)' or '..\\bin'

### Linux:
1. Install [CMake](https://cmake.org/download/)
2. Check 'build.linux' for libraries to install
3. Run `cmake .` and then `make` from the root of the project
4. Run the executable made in the 'bin' folder

### macOS
1. Install dependencies
  * [Homebrew](https://github.com/Homebrew/brew): `brew install cmake freetype libvorbis sdl2 libpng jpeg`
2. Run `cmake .` and then `make` from the root of the project.
3. Run the executable made in the 'bin' folder.