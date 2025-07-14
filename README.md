This is a mirror of YeOldeDink source code which is found only in its itch.io page https://branleur.itch.io/yeoldedink
-

Introduction
============

This is YeoldeDink, a fork of Beuc's GNU Freedink, derived from [Ghostknights repo](https://codeberg.org/crts/freedink). All of Ghostknight's improved logging features are included in this release. Some of the changes from Freedink 109.x include:

* Improved PNG support. PNG files now work properly without a filename change and without having to resize sprites.
* Debug mode uses Dear Imgui, allowing one to see the entirety of the engine's state as it's happening.
* A completely rewritten audio backend using SoLoud allowing for a variety of audio formats for SFX and BGM such as MP3 and FLAC.
* [Phoenix's DinkLua backend](https://github.com/ilyvion/freedink-lua) has been ported and allows for an actual scripting language to be used in mod-creation.

Installing
----------

Standalone packages are available containing freedink-data and the Martridge frontend for Windows, and AppImages with freedink-data for GNU/Linux that do not require installation.
Otherwise, YeOldeDink needs a set of Dink data as well as its own ancillary data (containing the fonts and config files) in order to run. On Windows, placing the exe and DLLs along with the "yeoldedink" folder next to an existing installation of Dink (so the 'dink' folder is alongside) and then creating shortcuts or selecting the exe for launch in DFarc is enough to get it going. For GNU/Linux, the ancillary data path is hard-coded to ~/.config and a set of Dink data may be installed from the `freedink-data` package on Debuntu. A set of ancillary data is located in "share" in this repo should you require it.

Building
-------

Required packages for building include:

* SDL2 including Mixer, TTF, Image, and GFX.
* SDL Mixer X (optional, but highly recommended)
* Fontconfig (optional, recommended) and Freetype2
* SDL-GPU (optional)
* PlutoSVG (optional, recommended)
* cereal
* libintl/gettext
* Lua 5.4
* Meson

Then from the root of the source tree:
`meson setup builddir --buildtype release`
`meson compile -C builddir`
`builddir/yeoldedink -w`

With the last command being to run it in windowed mode.

GNU/Linux notes:

* Make sure package "build-essential" is installed first
* For Ubuntu/Debian, most of the required packages are prefixed with "lib" and suffixed with "-dev". For example the relevant Lua package is liblua5.4-dev, and for SDL2 it is libsdl2-dev
* Make sure to place your ancillary data (from 'share') into ~/.config after building, and install freedink-data if it's available and you lack existing Dink data.

On Windows:

* Install MSYS2 and install the relevant packages in the Mingw64, Clang64, or UCRT environment.
* Install anything else it complains about lacking with Pacman.
* you should be able to run the above Meson commands and have it produce an exe in the builddir
* Package it up with the relevant 64-bit dlls in {environment prefix}/bin
* Your ancillary data folder (in "share") should be moved next to the exe.

On MacOS:

* Install Homebrew or Macports
* Install SDL2, SDL2_image, SDL2_ttf, fontconfig, gettext, and the freedink package with `brew install` if you need the game data. Otherwise, on Macports it is the package "freedink-data".
* Manually compile and install SDL-GPU, MixerX, and PlutoSVG as per their included instructions.
* Run the compilation commands above.

Web:

* Install and set up the Emscripten SDK as per the instructions on the site.
* Run meson with `meson setup buildweb --cross-file cross/emscripten.txt` and then `meson compile -C buildweb`
* After a very long time it should output js and wasm files.
* Add the JS file to shell_minimal.html in the "script" section down the bottom.
* You will also need to package up freedink-data or similar with `file_packager.py freedink-data.data --js-output=freedink-data-data.js --preload ~/Downloads/freedink-data-1.08.20190120/dink@dink` and add the js to shell_minimal.html in the script section.
* Repeat the process for the ancillary data, and perhaps a d-mod in case you want to run that.
* For A Dmod, add to `arguments_` in yeoldedink.js with `arguments_push("-g//admod");`
* The cross build file contains some additional remarks.

Optional build dependency notes:

* SDL-GPU may require libglew-dev installed before it will compile
* When building and installing SDL-GPU, it may not always install into the proper location for Meson to find it, in which case you will have to specify the install prefix similarly to:
`cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="/ucrt64(or whatever)"`
* MixerX provides the best MIDI functionality with GPL mode enabled such as with:
`cmake -DCMAKE_BUILD_TYPE=Release -DMIXERX_ENABLE_GPL=ON ..`

Known Issues
------------

* Starting in debug mode will cause a crash if the destination for debug.txt is write-protected, such as for the main game.
* Running with the sound off will cause a segfault on exit.
* The OpenGL renderer (-S mode) probably won't work at all.

Licence
-------

    YeOldeDink is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

   YeOldeDink is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
   You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. 
