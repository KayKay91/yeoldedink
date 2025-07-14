Changelog
---------

0.2

* Got rid of some windows and moved things into the menu bar
* 8*bit mode is now inaccessible
* Added sprite, variable, and script info viewers
* Moved the DinkC console into the floating info window
* Added the ability to pause the game while the console is active
* The game itself is now viewable in a separate window
* The main renderer can now be switched off too
* Expanded "Ultimate Cheat" and the log window a bit more
* Added "map viewer" window showing which screen you're on
* Quick save/load (slot 1337)

0.3 update:

* Added sysinfo window
* Audio control window for SFX and BGM levels
* added a slow mode
* added controller rumble when hurt
* pressing Tab now increases speed only in normal mode
* Added a speed menu.
* Added "play_mod_order(int order)" for tracker modules to DinkC commands. Requires SDL*Mixer 2.6 and up.
* SoundFonts also seem to work now by specifying the SDL_SOUNDFONTS environment variable on the command line. Confirmation is visible in the audio window.
* Added MP3 playback support in case you want to play Prelude properly.
* Added support for PNG tilescreens (that Seth forgot ;-))

0.4:

* Didn't release and skipped straight to 0.5
* Fixed that mapnuke map.dat loading bug present in FB2 and others with high screen counts
* Fixed a minor issue where text from say() would appear white upon screen load after warping when it was supposed to be another colour

0.5:

* Reintroduced OpenGL renderer and 8*bit mode
* 8-bit mode only works properly with the -S command line flag (software renderer)
* The OpenGL renderer will always use 24-bit display mode
* Changed a few menus around
* Added variable and sprite editors. Remember you can ctrl+click on slider widgets to manually input a value. May need to turn the console on first for keyboard input.
* Pressing the Tab key in mouse mode resets the cursor to the middle of the screen
* The host mouse cursor only shows in debug mode and otherwise is hidden

0.6:

* Added a standardised gamepad mapping using SDL_Controller
* Added a map sprite editor to complement the live sprite editor
* Added an inbuilt launcher with Dmod support
* Menus moved around again
* Updated to the newest ImGui stable
* Fixed a bug with clipped sprites in the software renderer
* The Dink cursor should no longer move with the mouse while in debug mode
* The info window no longer displays by default in debug mode
* Pressing Alt+U will unfreeze sprite 1

0.7

* Added a colour palette viewer
* Bugfix: random() called with zero as base no longer causes a floating point error (thanks drone1400)
* Added an editor sprite override viewer
* Added get_platform() to dinkC with return values corresponding to RTDink
* Added display stretching beyond 4:3
* Map index viewer now can create screens
* Improved font rendering for the debug interface
* Yedink.ini lives in the resources path and may be used to alter a few parameters
* Rewrote the SFX backend using SoLoud
* Various other improvements and changes

0.75

* Added an option for horrible*sounding speech synthesis for all the game's text
* Gold and stats will now lerp up rather than clattering endlessly and blowing out all the audio channels
* Saves will now show with the date and time, unless the info has been manually modified
* Specifying *c on the command line will allow you to provide a path for yedink.ini
* Updated ImGui to the latest stable release

0.8

* OpenGL renderer partially works again. Added dependency SDL*GPU
* More settings are autosaved using Cereal
* Debug messages are coloured using spdlog, and verbose logging now works as expected.
* SFX output now uses miniaudio rather than SDL2

0.85

* Ported DinkLua backend
* Added a jukebox (not on Windows)

0.9

* Build system now uses Meson instead of Autotools
* Sprites are now ranked using a much faster algorithm with the sprite limit now set to 65,535 up from 300
* Added some qol improvements

0.91
* Fixed DinkC console crash
* Made some DinkLua features work properly (thanks Phoenix)
* Added touch controls

0.92
* Fixed some minor bugs
* Added Dink colour scheme
* added Lua serialisation

0.95
* Fixed a bug relating to dinkc return values
* Changed from LunaSVG to PlutoSVG for emojis
