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

clap_link_flags = link_flags
if os_darwin
    clap_link_flags += ['-Wl,-exported_symbol,_clap_entry']
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
            vst3_bundle_contents, vst3_inner_lib_ext),
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
            delay_vst3_bundle_contents, vst3_inner_lib_ext),
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
            reverb_vst3_bundle_contents, vst3_inner_lib_ext),
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
            pitch_shifter_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── Modulator VST3 ─────────────────────────────────────────────────────────────
modulator_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::modulator"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654d64',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Modulation"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

modulator_plugin_inc = include_directories(['.', '../effects/modulator'])

modulator_plugin_srcs = files([
    'modulator_plugin.cpp',
    'modulator_editor.cpp',
    'look_and_feel.cpp',
])

modulator_all_srcs = modulator_plugin_srcs + modulator_srcs + vst3_client_src

modulator_vst3_lib = shared_library('kaos-engine-modulator-vst3',
    name_prefix: '',
    sources: modulator_all_srcs,
    include_directories: [modulator_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + modulator_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

modulator_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-modulator.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-modulator-vst3-bundle',
    output: 'kaos-engine-modulator.vst3',
    input: modulator_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-modulator@1@"'.format(
            modulator_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── Frequency Shifter VST3 ────────────────────────────────────────────────────
freq_shifter_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::frequency-shifter"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654671',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Modulation"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

freq_shifter_plugin_inc = include_directories(['.', '../effects/frequency_shifter'])

freq_shifter_plugin_srcs = files([
    'frequency_shifter_plugin.cpp',
    'frequency_shifter_editor.cpp',
    'look_and_feel.cpp',
])

freq_shifter_all_srcs = freq_shifter_plugin_srcs + frequency_shifter_srcs + vst3_client_src

freq_shifter_vst3_lib = shared_library('kaos-engine-frequency-shifter-vst3',
    name_prefix: '',
    sources: freq_shifter_all_srcs,
    include_directories: [freq_shifter_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + freq_shifter_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

freq_shifter_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-frequency-shifter.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-frequency-shifter-vst3-bundle',
    output: 'kaos-engine-frequency-shifter.vst3',
    input: freq_shifter_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-frequency-shifter@1@"'.format(
            freq_shifter_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── EQ VST3 ───────────────────────────────────────────────────────────────────
eq_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::eq"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654571',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|EQ"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

eq_plugin_inc = include_directories(['.', '../effects/eq'])

eq_plugin_srcs = files([
    'eq_plugin.cpp',
    'eq_editor.cpp',
    'look_and_feel.cpp',
])

eq_all_srcs = eq_plugin_srcs + eq_srcs + vst3_client_src

eq_vst3_lib = shared_library('kaos-engine-eq-vst3',
    name_prefix: '',
    sources: eq_all_srcs,
    include_directories: [eq_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + eq_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

eq_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-eq.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-eq-vst3-bundle',
    output: 'kaos-engine-eq.vst3',
    input: eq_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-eq@1@"'.format(
            eq_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── Compressor VST3 ───────────────────────────────────────────────────────────
comp_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::compressor"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e65436f',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Dynamics"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

comp_plugin_inc = include_directories(['.', '../effects/compressor'])

comp_plugin_srcs = files([
    'compressor_plugin.cpp',
    'compressor_editor.cpp',
    'look_and_feel.cpp',
])

comp_all_srcs = comp_plugin_srcs + compressor_srcs + vst3_client_src

comp_vst3_lib = shared_library('kaos-engine-compressor-vst3',
    name_prefix: '',
    sources: comp_all_srcs,
    include_directories: [comp_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + comp_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

comp_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-compressor.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-compressor-vst3-bundle',
    output: 'kaos-engine-compressor.vst3',
    input: comp_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-compressor@1@"'.format(
            comp_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── Gate VST3 ─────────────────────────────────────────────────────────────────
gate_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::gate"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654774',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Dynamics"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

gate_plugin_inc = include_directories(['.', '../effects/gate'])

gate_plugin_srcs = files([
    'gate_plugin.cpp',
    'gate_editor.cpp',
    'look_and_feel.cpp',
])

gate_all_srcs = gate_plugin_srcs + gate_srcs + vst3_client_src

gate_vst3_lib = shared_library('kaos-engine-gate-vst3',
    name_prefix: '',
    sources: gate_all_srcs,
    include_directories: [gate_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + gate_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

gate_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-gate.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-gate-vst3-bundle',
    output: 'kaos-engine-gate.vst3',
    input: gate_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-gate@1@"'.format(
            gate_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── Envelope Follower VST3 ────────────────────────────────────────────────────
env_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::envelope-follower"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654566',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=1',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Tools"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

env_plugin_inc = include_directories(['.', '../framework/envelope_follower'])

env_plugin_srcs = files([
    'envelope_follower_plugin.cpp',
    'envelope_follower_editor.cpp',
    'look_and_feel.cpp',
])

env_all_srcs = env_plugin_srcs + envelope_follower_srcs + vst3_client_src

env_vst3_lib = shared_library('kaos-engine-envelope-follower-vst3',
    name_prefix: '',
    sources: env_all_srcs,
    include_directories: [env_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + env_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

env_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-envelope-follower.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-envelope-follower-vst3-bundle',
    output: 'kaos-engine-envelope-follower.vst3',
    input: env_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-envelope-follower@1@"'.format(
            env_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── Stochastic VST3 ───────────────────────────────────────────────────────────
stoch_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::stochastic"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e655374',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=1',
    '-DJucePlugin_ProducesMidiOutput=1',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Tools"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

stoch_plugin_inc = include_directories(['.', '../framework/stochastic'])

stoch_plugin_srcs = files([
    'stochastic_plugin.cpp',
    'stochastic_editor.cpp',
    'look_and_feel.cpp',
])

stoch_all_srcs = stoch_plugin_srcs + stochastic_srcs + vst3_client_src

stoch_vst3_lib = shared_library('kaos-engine-stochastic-vst3',
    name_prefix: '',
    sources: stoch_all_srcs,
    include_directories: [stoch_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + stoch_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

stoch_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-stochastic.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-stochastic-vst3-bundle',
    output: 'kaos-engine-stochastic.vst3',
    input: stoch_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-stochastic@1@"'.format(
            stoch_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── Filter VST3 ───────────────────────────────────────────────────────────────
filter_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::filter"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654669',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Filter"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

filter_plugin_inc = include_directories(['.', '../effects/filter'])

filter_plugin_srcs = files([
    'filter_plugin.cpp',
    'filter_editor.cpp',
    'look_and_feel.cpp',
])

filter_all_srcs = filter_plugin_srcs + filter_srcs + vst3_client_src

filter_vst3_lib = shared_library('kaos-engine-filter-vst3',
    name_prefix: '',
    sources: filter_all_srcs,
    include_directories: [filter_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + filter_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

filter_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-filter.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-filter-vst3-bundle',
    output: 'kaos-engine-filter.vst3',
    input: filter_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-filter@1@"'.format(
            filter_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── LFO VST3 ──────────────────────────────────────────────────────────────────
lfo_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::lfo"',
    '-DJucePlugin_Desc="LFO modulator with MIDI CC and Audio CV outputs"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654c66',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=1',
    '-DJucePlugin_ProducesMidiOutput=1',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Tools"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

lfo_plugin_inc = include_directories(['.', '../effects', '../framework/lfo'])

lfo_plugin_srcs = files([
    'lfo_plugin.cpp',
    'lfo_editor.cpp',
    'look_and_feel.cpp',
])

lfo_all_srcs = lfo_plugin_srcs + lfo_srcs + vst3_client_src

lfo_vst3_lib = shared_library('kaos-engine-lfo-vst3',
    name_prefix: '',
    sources: lfo_all_srcs,
    include_directories: [lfo_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + lfo_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

lfo_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-lfo.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-lfo-vst3-bundle',
    output: 'kaos-engine-lfo.vst3',
    input: lfo_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-lfo@1@"'.format(
            lfo_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── LFO CLAP ──────────────────────────────────────────────────────────────────
# Same plugin code compiled against the clap-juce-extensions wrapper instead of
# the VST3 client.  The output is a .clap shared library (DLL renamed on Windows).
lfo_clap_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::lfo"',
    '-DJucePlugin_Desc="LFO modulator with MIDI CC and Audio CV outputs"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654c66',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=1',
    '-DJucePlugin_ProducesMidiOutput=1',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Tools"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
    '-DCLAP_ID="org.kaos-engine.lfo"',
    '-DCLAP_FEATURES="utility","modulator"',
    '-DCLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES=64',
    '-DCLAP_MISBEHAVIOUR_HANDLER_LEVEL=Ignore',
    '-DCLAP_CHECKING_LEVEL=Minimal',
    '-DCLAP_MANUAL_URL=""',
    '-DCLAP_SUPPORT_URL=""',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

# ── Noise VST3 ────────────────────────────────────────────────────────────────
noise_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::noise"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654e6f',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Generator"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

noise_plugin_inc = include_directories(['.', '../effects/noise'])

noise_plugin_srcs = files([
    'noise_plugin.cpp',
    'noise_editor.cpp',
    'look_and_feel.cpp',
])

noise_all_srcs = noise_plugin_srcs + noise_srcs + vst3_client_src

noise_vst3_lib = shared_library('kaos-engine-noise-vst3',
    name_prefix: '',
    sources: noise_all_srcs,
    include_directories: [noise_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + noise_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

noise_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-noise.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-noise-vst3-bundle',
    output: 'kaos-engine-noise.vst3',
    input: noise_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-noise@1@"'.format(
            noise_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── Spectrogram VST3 ──────────────────────────────────────────────────────────
spectrogram_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::spectrogram"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e655367',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Analyzer"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

spectrogram_plugin_inc = include_directories(['.', '../effects/spectrogram'])

spectrogram_plugin_srcs = files([
    'spectrogram_plugin.cpp',
    'spectrogram_editor.cpp',
    'look_and_feel.cpp',
])

spectrogram_all_srcs = spectrogram_plugin_srcs + spectrogram_srcs + vst3_client_src

spectrogram_vst3_lib = shared_library('kaos-engine-spectrogram-vst3',
    name_prefix: '',
    sources: spectrogram_all_srcs,
    include_directories: [spectrogram_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + spectrogram_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

spectrogram_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-spectrogram.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-spectrogram-vst3-bundle',
    output: 'kaos-engine-spectrogram.vst3',
    input: spectrogram_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-spectrogram@1@"'.format(
            spectrogram_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── Looper VST3 ──────────────────────────────────────────────────────────────
looper_plugin_defines = [
    '-DJucePlugin_Build_VST3=1',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::looper"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654c70',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=1',
    '-DJucePlugin_ProducesMidiOutput=0',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Tools"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
]

looper_plugin_inc = include_directories(['.', '../effects/looper'])

looper_plugin_srcs = files([
    'looper_plugin.cpp',
    'looper_editor.cpp',
    'look_and_feel.cpp',
])

looper_all_srcs = looper_plugin_srcs + looper_srcs + vst3_client_src

looper_vst3_lib = shared_library('kaos-engine-looper-vst3',
    name_prefix: '',
    sources: looper_all_srcs,
    include_directories: [looper_plugin_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + looper_plugin_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: plugin_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

looper_vst3_bundle_contents = meson.current_build_dir() / 'kaos-engine-looper.vst3' / 'Contents' / vst3_arch

custom_target('kaos-engine-looper-vst3-bundle',
    output: 'kaos-engine-looper.vst3',
    input: looper_vst3_lib,
    command: [
        'bash', '-c',
        'mkdir -p "@0@" && cp "@INPUT@" "@0@/kaos-engine-looper@1@"'.format(
            looper_vst3_bundle_contents, vst3_inner_lib_ext),
    ],
    build_by_default: true,
)

# ── VST3 install ──────────────────────────────────────────────────────────────
py = import('python').find_installation(required: true)
meson.add_install_script(
    py,
    meson.project_source_root() / 'scripts' / 'install-vst3.py',
    meson.current_build_dir(),
    vst3_install_dir,
)

lfo_clap_lib = shared_library('kaos-engine-lfo',
    name_prefix: '',
    name_suffix: 'clap',
    sources: lfo_plugin_srcs + lfo_srcs + clap_client_srcs,
    include_directories: [lfo_plugin_inc, juce_include_dirs, clap_include_dirs],
    cpp_args: build_flags_cpp + build_flags + lfo_clap_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
        '-Wno-unused-parameter',
    ],
    link_args: clap_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

# ── Stochastic CLAP ───────────────────────────────────────────────────────────
stoch_clap_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::stochastic"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e655374',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=1',
    '-DJucePlugin_ProducesMidiOutput=1',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Tools"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
    '-DJucePlugin_Desc="Stochastic modulation controller with MIDI CC and Audio CV outputs"',
    '-DCLAP_ID="org.kaos-engine.stochastic"',
    '-DCLAP_FEATURES="utility","modulator"',
    '-DCLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES=64',
    '-DCLAP_MISBEHAVIOUR_HANDLER_LEVEL=Ignore',
    '-DCLAP_CHECKING_LEVEL=Minimal',
    '-DCLAP_MANUAL_URL=""',
    '-DCLAP_SUPPORT_URL=""',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

stoch_clap_lib = shared_library('kaos-engine-stochastic',
    name_prefix: '',
    name_suffix: 'clap',
    sources: stoch_plugin_srcs + stochastic_srcs + clap_client_srcs,
    include_directories: [stoch_plugin_inc, juce_include_dirs, clap_include_dirs],
    cpp_args: build_flags_cpp + build_flags + stoch_clap_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
        '-Wno-unused-parameter',
    ],
    link_args: clap_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)

# ── Envelope Follower CLAP ────────────────────────────────────────────────────
env_clap_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=0',
    '-DJucePlugin_Build_LV2=0',
    '-DJucePlugin_Build_Unity=0',
    '-DJucePlugin_Name="kaos-engine::envelope-follower"',
    '-DJucePlugin_Manufacturer="kaos-engine"',
    '-DJucePlugin_ManufacturerWebsite=""',
    '-DJucePlugin_ManufacturerEmail=""',
    '-DJucePlugin_ManufacturerCode=0x4e75456e',
    '-DJucePlugin_PluginCode=0x4e654566',
    '-DJucePlugin_IsSynth=0',
    '-DJucePlugin_WantsMidiInput=0',
    '-DJucePlugin_ProducesMidiOutput=1',
    '-DJucePlugin_IsMidiEffect=0',
    '-DJucePlugin_EditorRequiresKeyboardFocus=0',
    '-DJucePlugin_Version=0x010000',
    '-DJucePlugin_VersionString="1.0.0"',
    '-DJucePlugin_VersionCode=0x010000',
    '-DJucePlugin_Vst3Category="Fx|Tools"',
    '-DJUCE_VST3_CAN_REPLACE_VST2=0',
    '-DJucePlugin_VSTUniqueID=JucePlugin_PluginCode',
    '-DJucePlugin_Desc="Envelope follower with MIDI CC and Audio CV outputs"',
    '-DCLAP_ID="org.kaos-engine.envelope-follower"',
    '-DCLAP_FEATURES="utility","analyzer"',
    '-DCLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES=64',
    '-DCLAP_MISBEHAVIOUR_HANDLER_LEVEL=Ignore',
    '-DCLAP_CHECKING_LEVEL=Minimal',
    '-DCLAP_MANUAL_URL=""',
    '-DCLAP_SUPPORT_URL=""',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

env_clap_lib = shared_library('kaos-engine-envelope-follower',
    name_prefix: '',
    name_suffix: 'clap',
    sources: env_plugin_srcs + envelope_follower_srcs + clap_client_srcs,
    include_directories: [env_plugin_inc, juce_include_dirs, clap_include_dirs],
    cpp_args: build_flags_cpp + build_flags + env_clap_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
        '-Wno-unused-parameter',
    ],
    link_args: clap_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    install: false,
)
