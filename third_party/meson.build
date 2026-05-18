# ── JUCE 7 static library ──────────────────────────────────────────────────────
# Compiles all JUCE modules (except juce_audio_plugin_client, which must be
# compiled together with the plugin code so it can resolve createPluginFilter()).

juce_modules_dir = meson.current_source_dir() / 'JUCE' / 'modules'

juce_srcs = files([
    'JUCE/modules/juce_audio_basics/juce_audio_basics.cpp',
    'JUCE/modules/juce_audio_devices/juce_audio_devices.cpp',
    'JUCE/modules/juce_audio_formats/juce_audio_formats.cpp',
    'JUCE/modules/juce_audio_processors/juce_audio_processors.cpp',
    'JUCE/modules/juce_core/juce_core.cpp',
    'JUCE/modules/juce_cryptography/juce_cryptography.cpp',
    'JUCE/modules/juce_data_structures/juce_data_structures.cpp',
    'JUCE/modules/juce_dsp/juce_dsp.cpp',
    'JUCE/modules/juce_events/juce_events.cpp',
    'JUCE/modules/juce_graphics/juce_graphics.cpp',
    'JUCE/modules/juce_gui_basics/juce_gui_basics.cpp',
    'JUCE/modules/juce_audio_utils/juce_audio_utils.cpp',
    'JUCE/modules/juce_gui_extra/juce_gui_extra.cpp',
])

juce_include_dirs = include_directories([
    'JUCE/modules',
    'JUCE/modules/juce_audio_processors/format_types/VST3_SDK',
])

juce_extra_cpp_args = [
    '-Wno-deprecated-declarations',
    '-Wno-sign-compare',
    '-Wno-char-subscripts',
    '-Wno-non-virtual-dtor',
    '-Wno-free-nonheap-object',
]

# Windows COFF format limits object files to 65535 sections; JUCE's large unity
# build files exceed this. -Wa,-mbig-obj tells the assembler to use the extended
# format that supports up to 4 billion sections.
if os_windows
    juce_extra_cpp_args += ['-Wa,-mbig-obj']
endif

lib_juce = static_library('juce',
    sources: juce_srcs,
    include_directories: juce_include_dirs,
    cpp_args: build_flags_cpp + build_flags + juce_extra_cpp_args,
    dependencies: system_deps,
    pic: true,
    install: false,
)

juce_dep = declare_dependency(
    include_directories: juce_include_dirs,
    link_with: lib_juce,
    dependencies: system_deps,
)

# VST3 plugin client — compiled as part of the plugin shared lib, not the static
# lib, because it references the user-supplied createPluginFilter() at link time.
vst3_client_src = files([
    'JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp',
])
