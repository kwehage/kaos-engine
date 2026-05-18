standalone_srcs = files([
    '../effects/distortion/distortion_processor.cpp',
    '../plugin/distortion_plugin.cpp',
    '../plugin/distortion_editor.cpp',
    '../plugin/look_and_feel.cpp',
    '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
])

standalone_inc = include_directories([
    '../plugin',
    '../effects/distortion',
])

standalone_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=1',
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
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

standalone_cpp_args = build_flags_cpp + build_flags + standalone_defines + [
    '-Wno-deprecated-declarations',
    '-Wno-non-virtual-dtor',
    '-Wno-sign-compare',
]

# Windows GUI subsystem — no console window
standalone_link_flags = link_flags
if os_windows
    standalone_link_flags += ['-mwindows']
endif

executable('kaos-engine-distortion',
    sources: standalone_srcs,
    include_directories: [standalone_inc, juce_include_dirs],
    cpp_args: standalone_cpp_args,
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Delay standalone ───────────────────────────────────────────────────────────
delay_standalone_srcs = files([
    '../effects/delay/delay_processor.cpp',
    '../plugin/delay_plugin.cpp',
    '../plugin/delay_editor.cpp',
    '../plugin/look_and_feel.cpp',
    '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
])

delay_standalone_inc = include_directories([
    '../plugin',
    '../effects/delay',
])

delay_standalone_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=1',
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
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

executable('kaos-engine-delay',
    sources: delay_standalone_srcs,
    include_directories: [delay_standalone_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + delay_standalone_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Reverb standalone ──────────────────────────────────────────────────────────
reverb_standalone_srcs = files([
    '../effects/reverb/reverb_processor.cpp',
    '../plugin/reverb_plugin.cpp',
    '../plugin/reverb_editor.cpp',
    '../plugin/look_and_feel.cpp',
    '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
])

reverb_standalone_inc = include_directories([
    '../plugin',
    '../effects/reverb',
])

reverb_standalone_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=1',
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
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

executable('kaos-engine-reverb',
    sources: reverb_standalone_srcs,
    include_directories: [reverb_standalone_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + reverb_standalone_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Shifter standalone ─────────────────────────────────────────────────────────
pitch_shifter_standalone_srcs = files([
    '../effects/pitch_shifter/pitch_shifter_processor.cpp',
    '../plugin/pitch_shifter_plugin.cpp',
    '../plugin/pitch_shifter_editor.cpp',
    '../plugin/look_and_feel.cpp',
    '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
])

pitch_shifter_standalone_inc = include_directories([
    '../plugin',
    '../effects/pitch_shifter',
])

pitch_shifter_standalone_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=1',
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
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

executable('kaos-engine-pitch-shifter',
    sources: pitch_shifter_standalone_srcs,
    include_directories: [pitch_shifter_standalone_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + pitch_shifter_standalone_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Modulator standalone ───────────────────────────────────────────────────────
modulator_standalone_srcs = files([
    '../effects/modulator/modulator_processor.cpp',
    '../plugin/modulator_plugin.cpp',
    '../plugin/modulator_editor.cpp',
    '../plugin/look_and_feel.cpp',
    '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
])

modulator_standalone_inc = include_directories([
    '../plugin',
    '../effects/modulator',
])

modulator_standalone_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=1',
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
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

executable('kaos-engine-modulator',
    sources: modulator_standalone_srcs,
    include_directories: [modulator_standalone_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + modulator_standalone_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)
