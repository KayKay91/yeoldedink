project('yeoldedink', 'cpp', 'c',
  version: '0.95', license: 'GPL',
  default_options : ['warning_level=1', 'cpp_std=c++17', 'c_std=gnu17', 'cpp_args=-Wno-write-strings -Wall -O2', 'default_library=shared'])
# GCC FLTO doesn't work due to something with FastFiles
#Static as much as possible for Windows, shared as much as possible for lunix, variety for macOS
extra_args = []
if host_machine.system() == 'windows'
  staticvar = true
  if meson.get_compiler('cpp').get_id() == 'clang'
    extra_args = ['-flto']
  endif
else
  staticvar = false
endif

if host_machine.system() == 'windows' #or host_machine.system() == 'darwin'
  gfxstatic = true
else
  gfxstatic = false
endif

#config.h replacement
as_version = meson.project_version() # set in project() above
ver_arr = as_version.split('.')

as_major_version = ver_arr[0]
as_minor_version = ver_arr[1]

conf = configuration_data()
# Surround the version in quotes to make it a C string
conf.set_quoted('VERSION', as_version)
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_STRING', meson.project_name() + meson.project_version())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_BUGREPORT', 'billy@cock.li')
conf.set_quoted('BUILD_DATA_DIR', get_option('datadir'))
conf.set_quoted('LOCALEDIR', get_option('localedir'))
configure_file(output : 'config.h', configuration : conf)
add_project_arguments('-DHAVE_CONFIG_H', language : 'cpp')

i18n = import('i18n')
i18n.gettext(meson.project_name())

# For making appimages and macos builds, or on windows to build with launcher. do not use for web
#add_project_arguments('-DWELL_BEHAVED_DINK', language: 'cpp')

# included headers
incdir = include_directories('include', 'soloud', 'src/imgui_standard/misc/freetype/')
# library deps, made static to keep DLLs down on Windows, change if you get undefined symbols maybe
cc = meson.get_compiler('cpp')
intl_dep = dependency('intl', required: true, static: staticvar)

#web, libraries are autolinked through ports system for most, all must be static
if host_machine.system() == 'emscripten'
  sdl2_ttf_dep = dependency('', required: false)
  sdl2_gfx_dep = dependency('', required: false)
  ft2_dep = cc.find_library('freetype', static: true)
  #let's chuck bz2 in here
  sdl2_deps = [dependency('SDL2', required: false), dependency('zlib', required: false), dependency('libzip', required: true)]
  thread_dep = dependency('', required: false)
  svg_dep = dependency('plutosvg', required: false)
else
#for normal builds
  sdl2_deps = [dependency('SDL2', static: staticvar, required: true), dependency('SDL2_image', static: true, required: true)]
  sdl2_gfx_dep = dependency('SDL2_gfx', static: gfxstatic, required: true)
  sdl2_ttf_dep = dependency('SDL2_ttf', static: gfxstatic, required: true)
  ft2_dep = dependency('freetype2', static: gfxstatic, required: true)
  thread_dep = dependency('threads')
endif

lua_dep = dependency('lua', static: true, required: true)
cereal_dep = dependency('cereal', static: true, required: true)
font_deps = dependency('fontconfig', static: true, required: false)
m_dep = cc.find_library('m', required : false)

if font_deps.found()
  add_project_arguments('-DHAVE_FONTCONFIG', language : 'cpp')
endif

#If you get undefined symbols related to mixerx, mess about with static true/false, and make sure it was compiled with shared building on
if host_machine.system() == 'emscripten'
  mixerext_opt = dependency('SDL2_mixer_ext', modules: ['SDL2_mixer_ext::SDL2_mixer_ext_Static'], required: false)
else
  mixerext_opt = dependency('SDL2_mixer_ext', required: false, static: false)
endif
# MixerX will only be used if it is installed
if mixerext_opt.found()
  mixer_dep = mixerext_opt
  add_project_arguments('-DSDL_MIXER_X', language : 'cpp')
else
  if host_machine.system() == 'emscripten'
    mixer_dep = dependency('', required: false)
  else
    mixer_dep = dependency('SDL2_mixer', version : '>=2.6.1', required: true, static: false)
  endif
endif

#sdl gpu, optional, to be removed eventually
gl_dep = dependency('', required : false)
cpp = meson.get_compiler('cpp')
sdlgpu_dep = cpp.find_library('SDL2_gpu', required: false)
if sdlgpu_dep.found()
  add_project_arguments('-DHAVE_SDL_GPU', language : 'cpp')
  gl_dep = dependency('GL')
endif

#plutosvg, also optional
svg_dep = dependency('plutosvg', required: false)
if svg_dep.found()
  add_project_arguments('-DIMGUI_ENABLE_FREETYPE_PLUTOSVG', language: 'cpp')
endif

if host_machine.system() == 'emscripten'
  extra_srcs = ['emscripten/extract.c', 'emscripten/importexport.c']
else
  extra_srcs = ['src/whereami.c', 'src/tinyfiledialogs.c']
  if host_machine.system() == 'windows'
    windows = import('windows')
    extra_srcs += windows.compile_resources('woeres.rc')
  endif
endif

#File extension hack, comment out to make web build js file instead
suffix = []
if host_machine.system() == 'emscripten'
    suffix = 'html'
endif

src_common = ['src/app.cpp',
              'src/bgm.cpp',
              'src/sfx.cpp',
              'src/BgTilesetsManager.cpp',
              'src/dinkini.cpp',
              'src/DMod.cpp',
              'src/editor_screen.cpp',
              'src/fastfile.cpp',
              'src/str_util.cpp',
              'src/io_util.cpp',
              'src/EditorMap.cpp',
              'src/editor_screen.cpp',
              'src/gfx.cpp',
              'src/gfx_tiles.cpp',
              'src/gfx_palette.cpp',
              'src/gfx_fonts.cpp',
              'src/gfx_sprites.cpp',
              'src/hardness_tiles.cpp',
              'src/ImageLoader.cpp',
              'src/IOGfxDisplay.cpp',
              'src/IOGfxDisplayGL2.cpp',
              'src/IOGfxDisplaySW.cpp',
              'src/IOGfxSurface.cpp',
              'src/IOGfxSurfaceSW.cpp',
              'src/IOGfxSurfaceGL2.cpp',
              'src/IOGfxPrimitivesSW.cpp',
              'src/FakeIOGfxDisplay.cpp',
              'src/live_screen.cpp',
              'src/live_sprite.cpp',
              'src/live_sprites_manager.cpp',
              'src/rect.cpp',
              'src/resources.cpp',
              'src/input.cpp',
              'src/freedink_xpm.cpp',
              'src/paths.cpp',
              'src/log.cpp',
              'src/vgasys_fon.cpp',
              'src/SDL_android_main.c']

src_soloud = ['soloud/darray.cpp',
           'soloud/dr_impl.cpp',
           'soloud/klatt.cpp',
           'soloud/resonator.cpp',
           'soloud/soloud.cpp',
           'soloud/soloud_audiosource.cpp',
           'soloud/soloud_bassboostfilter.cpp',
           'soloud/soloud_biquadresonantfilter.cpp',
           'soloud/soloud_bus.cpp',
           'soloud/soloud_core_3d.cpp',
           'soloud/soloud_core_basicops.cpp',
           'soloud/soloud_core_faderops.cpp',
           'soloud/soloud_core_filterops.cpp',
           'soloud/soloud_core_getters.cpp',
           'soloud/soloud_core_setters.cpp',
           'soloud/soloud_core_voicegroup.cpp',
           'soloud/soloud_core_voiceops.cpp',
           'soloud/soloud_echofilter.cpp',
           'soloud/soloud_fader.cpp',
           'soloud/soloud_fft.cpp',
           'soloud/soloud_fft_lut.cpp',
           'soloud/soloud_fftfilter.cpp',
           'soloud/soloud_file.cpp',
           'soloud/soloud_coreaudio.cpp',
           'soloud/soloud_filter.cpp',
           'soloud/soloud_freeverbfilter.cpp',
           'soloud/soloud_lofifilter.cpp',
           'soloud/soloud_miniaudio.cpp',
           'soloud/soloud_misc.cpp',
           'soloud/soloud_queue.cpp',
           'soloud/soloud_robotizefilter.cpp',
           'soloud/soloud_sdl2.cpp',
           'soloud/soloud_sdl2_static.cpp',
           'soloud/soloud_speech.cpp',
           'soloud/soloud_thread.cpp',
           'soloud/soloud_wav.cpp',
           'soloud/soloud_waveshaperfilter.cpp',
           'soloud/soloud_wavstream.cpp',
           'soloud/tts.cpp',
           'soloud/stb_vorbis.c',
           'soloud/soloud_midi.cpp']

src_imgui = ['src/imgui_standard/imgui_demo.cpp',
            'src/imgui_standard/imgui_draw.cpp',
            'src/imgui_standard/imgui_impl_opengl3.cpp',
            'src/imgui_standard/imgui_impl_sdl2.cpp',
            'src/imgui_standard/imgui_impl_sdlrenderer2.cpp',
            'src/imgui_standard/imgui_tables.cpp',
            'src/imgui_standard/imgui_widgets.cpp',
            'src/imgui_standard/imgui.cpp',
            'src/imgui_standard/implot_demo.cpp',
            'src/imgui_standard/implot_items.cpp',
            'src/imgui_standard/implot.cpp',
            'src/imgui_standard/misc/freetype/imgui_freetype.cpp']

src_engine = ['src/AppFreeDink.cpp',
              #'src/AStar2.cpp',
              'src/brain_bounce.cpp',
              'src/brain_button.cpp',
              'src/brain_diagonal.cpp',
              'src/brain_lateral.cpp',
              'src/brain_pig.cpp',
              'src/brain_pingpong.cpp',
              'src/IOTouchDragAnywhere.cpp',
              'src/freedink.cpp',
              'src/brain.cpp',
              'src/brain_circular.cpp',
              'src/brain_duck.cpp',
              'src/brain_keyboard.cpp',
              'src/brain_mouse.cpp',
              'src/brain_none.cpp',
              'src/brain_missile.cpp',
              'src/brain_onetime.cpp',
              'src/brain_people.cpp',
              'src/brain_scale.cpp',
              'src/brain_shadow.cpp',
              'src/brain_text.cpp',
              'src/brain_repeat.cpp',
              'src/freedink_main.cpp',
              'src/update_frame.cpp',
              'src/dinkc_bindings.cpp',
              'src/dinkc.cpp',
              'src/dinkc_console.cpp',
              'src/dinklua.cpp',
              'src/dinklua_bindings.cpp',
              'src/script_bindings.cpp',
              'src/scripting.cpp',
              'src/debug.cpp',
              'src/debug_imgui.cpp',
              'src/debug_renderer.cpp',
              'src/game_choice.cpp',
              'src/game_choice_renderer.cpp',
              'src/game_engine.cpp',
              'src/game_state.cpp',
              'src/i18n.cpp',
              'src/inventory.cpp',
              'src/meminfo.cpp',
              'src/savegame.cpp',
              'src/status.cpp',
              'src/text.cpp'
              ]

executable('yeoldedink',
           extra_srcs, src_engine, src_common, src_soloud, src_imgui,
           install : true, dependencies : [sdl2_deps, sdl2_ttf_dep, sdl2_gfx_dep, font_deps, ft2_dep, lua_dep, m_dep, intl_dep, mixer_dep, sdlgpu_dep, gl_dep, svg_dep, cereal_dep, thread_dep], include_directories: incdir, win_subsystem: 'windows', name_suffix: suffix, cpp_args : extra_args)

#Not finished
# executable('yedit', src_editor, src_common, src_soloud, src_imgui, extra_srcs, install : true, dependencies : [sdl2_deps, sdl2_ttf_dep, sdl2_gfx_dep, font_deps, ft2_dep, m_dep, intl_dep, mixer_dep, sdlgpu_dep, gl_dep, luna_dep, cereal_dep, thread_dep], include_directories: incdir, cpp_args : '-DDINKEDIT', implicit_include_directories: true, win_subsystem: 'windows', name_suffix: suffix)
