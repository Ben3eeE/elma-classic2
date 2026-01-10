project('elma', 'c', 'cpp', 'objcpp', version: '1.4', meson_version: '>=1.2.0', default_options: ['cpp_std=c++20'])

add_project_arguments('-DELMA_VERSION="@0@"'.format(meson.project_version()), language: 'cpp')

if get_option('buildtype') == 'debug'
    add_project_arguments('-DDEBUG', language: 'cpp')
endif

elma_sources = files(
    'abc8.cpp',
    'physics_init.cpp',
    'anim.cpp',
    'physics_move.cpp',
    'best_times.cpp',
    'menu_controls.cpp',
    'editor_dialog.cpp',
    'menu_dialog.cpp',
    'timer.cpp',
    'editor_canvas.cpp',
    'DOBOZ.CPP',
    'D_PIC.CPP',
    'ECSET.CPP',
    'EDITHELP.CPP',
    'EDITMENU.CPP',
    'EDITPLAY.CPP',
    'EDITTOLT.CPP',
    'EDITTOOL.CPP',
    'EDITUJ.CPP',
    'ED_CHECK.CPP',
    'flagtag.cpp',
    'ball_handler.cpp',
    'ball.cpp',
    'ball_collision.cpp',
    'polygon.cpp',
    'H_WAV.CPP',
    'JATEKOS.CPP',
    'KIRAJ320.CPP',
    'affine_pic.cpp',
    'grass.cpp',
    'platform_utils.cpp',
    'LEJATSZO.CPP',
    'LEPTET.CPP',
    'LGRFILE.CPP',
    'LOAD.CPP',
    'main.cpp',
    'MAINMENU.CPP',
    'menu_pic.cpp',
    'menu_options.cpp',
    'PEXTMENU.CPP',
    'pic8.cpp',
    'piclist.cpp',
    'PLAY.CPP',
    'qopen.cpp',
    'RECORDER.CPP',
    'platform_sdl.cpp',
    'SKIP.CPP',
    'SPRITE.CPP',
    'state.cpp',
    'SZAKASZ.CPP',
    'TELJES.CPP',
    'TOPOL.CPP',
    'UTKOZES.CPP',
    'UTKOZES2.CPP',
    'menu_nav.cpp',
    'vect2.cpp',
    'eol_settings.cpp',
    'hhigh.cpp',
    'keys.cpp',
    'fs_utils.cpp',
    'glad.c',
)

# Add Metal renderer on macOS
if host_machine.system() == 'darwin'
    elma_sources += files('metal_renderer.mm')
endif

# Add DirectX renderer on Windows
if host_machine.system() == 'windows'
    elma_sources += files('dx_renderer.cpp')
endif

inc = include_directories('include')

cpp = meson.get_compiler('cpp')
link_args = []
cpp_args = []

win_subsystem = 'windows'
if cpp.get_id() == 'msvc'
    if get_option('buildtype') == 'debug'
        # Allows printf when debugging
        win_subsystem = 'console'
    endif

    if win_subsystem == 'windows'
        # Override windows subsystem entrypoint to main instead of WinMain
        link_args += ['/ENTRY:mainCRTStartup']
    endif

    if host_machine.cpu_family() == 'x86'
        # winmm.lib is automatically linked by meson when 64-bit, but not 32-bit
        link_args += ['winmm.lib']
    endif
else
    cpp_args += ['-Wno-deprecated-declarations']
endif

sdl2_dep = dependency('sdl2',
    static: build_machine.system() == 'windows',
    default_options: {
        'test': false,
        'with_main': false,
    }
)

gl_dep = dependency('gl', required: true)

# Try to find Vulkan, but make it optional
vulkan_dep = dependency('vulkan', required: false)
if vulkan_dep.found()
    add_project_arguments('-DHAVE_VULKAN', language: 'cpp')
endif

nlohmann_json_dep = dependency('nlohmann_json')

# Add Metal framework on macOS
if host_machine.system() == 'darwin'
    metal_dep = dependency('appleframeworks', modules: ['Metal', 'QuartzCore', 'Cocoa'])
else
    metal_dep = disabler()
endif

# Add DirectX 12 on Windows
if host_machine.system() == 'windows'
    d3d12_dep = cpp.find_library('d3d12', required: true)
    dxgi_dep = cpp.find_library('dxgi', required: true)
    d3dcompiler_dep = cpp.find_library('d3dcompiler', required: true)
else
    d3d12_dep = disabler()
    dxgi_dep = disabler()
    d3dcompiler_dep = disabler()
endif

# Build dependency list
deps = [nlohmann_json_dep, gl_dep, sdl2_dep]
if vulkan_dep.found()
    deps += vulkan_dep
endif
if host_machine.system() == 'darwin'
    deps += metal_dep
endif
if host_machine.system() == 'windows'
    deps += [d3d12_dep, dxgi_dep, d3dcompiler_dep]
endif

elma = executable(
    'elma',
    elma_sources,
    include_directories: inc,
    cpp_args: cpp_args,
    link_args: link_args,
    dependencies: deps,
    win_subsystem: win_subsystem,
)
