project('kaos-engine',
    ['c', 'cpp'],
    version: '0.1.0',
    license: 'GPL-3.0',
    meson_version: '>=1.1.0',
    default_options: [
        'cpp_std=c++17',
        'buildtype=release',
    ]
)

# ── OS detection ───────────────────────────────────────────────────────────────
os_linux   = host_machine.system() == 'linux'
os_windows = host_machine.system() == 'windows'
os_darwin  = host_machine.system() == 'darwin'

if not (os_linux or os_windows or os_darwin)
    error('Unsupported host OS: ' + host_machine.system())
endif

cc  = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
buildtype = get_option('buildtype')

# ── Base compiler flags ────────────────────────────────────────────────────────
build_flags = [
    '-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1',
    '-DJUCE_DISPLAY_SPLASH_SCREEN=0',  # GPL project — splash not required
    '-DJUCE_REPORT_APP_USAGE=0',
    '-fno-common',
    '-Wall',
    '-Wno-unused-function',
    '-Wno-strict-overflow',
    '-pthread',
]

build_flags_cpp = ['-std=c++17']

if os_linux
    build_flags += ['-DLINUX=1']
elif os_windows
    build_flags += ['-DWINDOWS=1', '-mstackrealign']
    build_flags_cpp += [
        '-fpermissive',
        '-D_NATIVE_WCHAR_T_DEFINED',
        '-D__wchar_t=wchar_t',
        '-Wno-unknown-pragmas',
        '-Wno-class-memaccess',
    ]
elif os_darwin
    # JUCE unity-build .cpp files include Cocoa headers which use @class / @protocol.
    # Compiling them as plain C++ fails; ObjC++ mode is required on macOS.
    # Deployment target 13.0: CGWindowListCreateImage is neither deprecated nor
    # obsoleted at this target (it was removed in macOS 15 SDK at target >= 15.0).
    build_flags += ['-DMAC=1', '-x', 'objective-c++', '-mmacosx-version-min=13.0']
endif

if not os_darwin
    build_flags += ['-Wno-unused-but-set-variable']
endif

if buildtype == 'release'
    build_flags += [
        '-O3',
        '-DNDEBUG=1',
        '-fvisibility=hidden',
        '-fdata-sections',
        '-ffunction-sections',
        '-ftree-vectorize',
        '-funroll-loops',
    ]
    build_flags_cpp += ['-fvisibility-inlines-hidden']
else
    build_flags += ['-O0', '-ggdb', '-DDEBUG=1', '-D_DEBUG=1']
endif

# ── Link flags ─────────────────────────────────────────────────────────────────
link_flags = ['-pthread']

if os_linux
    link_flags += ['-Wl,--as-needed,--no-undefined']
    if buildtype == 'release'
        link_flags += ['-Wl,-O1,--gc-sections,--strip-all']
    endif
elif os_windows
    link_flags += ['-static', '-Wl,--as-needed,--no-undefined']
    if buildtype == 'release'
        link_flags += ['-Wl,-O1,--gc-sections,--strip-all']
    endif
endif

# ── VST3 bundle architecture directory ────────────────────────────────────────
if os_windows
    vst3_arch            = host_machine.cpu_family() + '-win'
    lib_suffix           = '.dll'
    vst3_install_dir     = 'C:/Program Files/Common Files/VST3'
    vst3_inner_lib_ext   = '.vst3'
elif os_darwin
    vst3_arch            = 'MacOS'
    lib_suffix           = '.dylib'
    vst3_install_dir     = '/Library/Audio/Plug-Ins/VST3'
    vst3_inner_lib_ext   = '.vst3'
else
    vst3_arch            = host_machine.cpu_family() + '-linux'
    lib_suffix           = '.so'
    vst3_install_dir     = '/usr/lib/vst3'
    vst3_inner_lib_ext   = '.so'
endif

# ── System dependencies ────────────────────────────────────────────────────────
if os_linux
    system_deps = [
        cc.find_library('dl'),
        cc.find_library('rt'),
        cc.find_library('asound'),
        dependency('freetype2'),
        dependency('gtk+-3.0'),
        dependency('webkit2gtk-4.1'),
        dependency('libcurl'),
        dependency('x11').partial_dependency(link_args: false, links: false),
        dependency('xext').partial_dependency(link_args: false, links: false),
        dependency('gl'),
    ]
elif os_windows
    system_deps = [
        cc.find_library('comdlg32'),
        cc.find_library('gdi32'),
        cc.find_library('imm32'),
        cc.find_library('ole32'),
        cc.find_library('oleaut32'),
        cc.find_library('opengl32'),
        cc.find_library('shlwapi'),
        cc.find_library('uuid'),
        cc.find_library('version'),
        cc.find_library('wininet'),
        cc.find_library('winmm'),
        cc.find_library('ws2_32'),
        cc.find_library('dxgi'),
    ]
elif os_darwin
    system_deps = [
        dependency('appleframeworks', modules: [
            'Accelerate',
            'AudioToolbox',
            'AudioUnit',
            'Cocoa',
            'CoreAudio',
            'CoreAudioKit',
            'CoreFoundation',
            'CoreImage',
            'CoreMIDI',
            'CoreServices',
            'CoreText',
            'Security',
            'IOKit',
            'Metal',
            'MetalKit',
            'QuartzCore',
            'WebKit',
        ]),
    ]
else
    system_deps = []
endif

subdir('third_party')
subdir('src')
