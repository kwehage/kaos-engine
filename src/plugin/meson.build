# ── Plugin characteristics passed to the JUCE VST3 wrapper ────────────────────
plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::distortion"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654469',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Distortion"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

plugin_inc = include_directories([
    '.',
    '../effects/distortion',
])

plugin_srcs = files([
    'distortion_plugin.cpp',
    'distortion_editor.cpp',
    'look_and_feel.cpp',
])

all_srcs = plugin_srcs + distortion_srcs + vst3_client_src

plugin_cpp_args = build_flags_cpp + build_flags + plugin_defines + [
    '-Wno-deprecated-declarations',
    '-Wno-non-virtual-dtor',
    '-Wno-sign-compare',
]

# ── Link flags per platform ────────────────────────────────────────────────────
plugin_link_flags = link_flags

if os_linux
    plugin_link_flags += [
        '-Wl,--version-script=' + meson.project_source_root() / 'scripts/plugin-symbols-vst3.version',
    ]
elif os_darwin
    plugin_link_flags += [
        '-Wl,-exported_symbol,_GetPluginFactory',
        '-Wl,-exported_symbol,_bundleEntry',
        '-Wl,-exported_symbol,_bundleExit',
    ]
endif

# ── VST3 shared library ────────────────────────────────────────────────────────
vst3_lib = shared_library('kaos-engine-distortion-vst3',
    name_prefix: '',
    sources: all_srcs,
    include_directories: [plugin_inc, juce_include_dirs],
    cpp_args: plugin_cpp_args,
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

# ── Bundle directory (Contents/<arch>/) ───────────────────────────────────────
vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-distortion.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-distortion-vst3-bundle',
    output: 'kaos-engine-distortion.vst3',
    input: vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-distortion@1@"'.format(
            vst3_bundle_contents, lib_suffix),
    ],
    build_by_default: true,
)

# ── Delay VST3 ────────────────────────────────────────────────────────────────
delay_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::delay"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e65446c',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Delay"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

delay_plugin_inc = include_directories(['.', '../effects/delay'])

delay_plugin_srcs = files([
    'delay_plugin.cpp',
    'delay_editor.cpp',
    'look_and_feel.cpp',
])

delay_all_srcs = delay_plugin_srcs + delay_srcs + vst3_client_src

delay_vst3_lib = shared_library('kaos-engine-delay-vst3',
    name_prefix: '',
    sources: delay_all_srcs,
    include_directories: [delay_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + delay_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

delay_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-delay.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-delay-vst3-bundle',
    output: 'kaos-engine-delay.vst3',
    input: delay_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-delay@1@"'.format(
            delay_vst3_bundle_contents, lib_suffix),
    ],
    build_by_default: true,
)

# ── Reverb VST3 ───────────────────────────────────────────────────────────────
reverb_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::reverb"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e655276',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Reverb"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

reverb_plugin_inc = include_directories(['.', '../effects/reverb'])

reverb_plugin_srcs = files([
    'reverb_plugin.cpp',
    'reverb_editor.cpp',
    'look_and_feel.cpp',
])

reverb_all_srcs = reverb_plugin_srcs + reverb_srcs + vst3_client_src

reverb_vst3_lib = shared_library('kaos-engine-reverb-vst3',
    name_prefix: '',
    sources: reverb_all_srcs,
    include_directories: [reverb_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + reverb_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

reverb_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-reverb.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-reverb-vst3-bundle',
    output: 'kaos-engine-reverb.vst3',
    input: reverb_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-reverb@1@"'.format(
            reverb_vst3_bundle_contents, lib_suffix),
    ],
    build_by_default: true,
)

# ── Shifter VST3 ──────────────────────────────────────────────────────────────
pitch_shifter_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::pitch-shifter"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e655368',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|PitchShift"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

pitch_shifter_plugin_inc = include_directories(['.', '../effects/pitch_shifter'])

pitch_shifter_plugin_srcs = files([
    'pitch_shifter_plugin.cpp',
    'pitch_shifter_editor.cpp',
    'look_and_feel.cpp',
])

pitch_shifter_all_srcs = pitch_shifter_plugin_srcs + pitch_shifter_srcs + vst3_client_src

pitch_shifter_vst3_lib = shared_library('kaos-engine-pitch-shifter-vst3',
    name_prefix: '',
    sources: pitch_shifter_all_srcs,
    include_directories: [pitch_shifter_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + pitch_shifter_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

pitch_shifter_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-pitch-shifter.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-pitch-shifter-vst3-bundle',
    output: 'kaos-engine-pitch-shifter.vst3',
    input: pitch_shifter_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-pitch-shifter@1@"'.format(
            pitch_shifter_vst3_bundle_contents, lib_suffix),
    ],
    build_by_default: true,
)
