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

# ── Frequency Shifter standalone ───────────────────────────────────────────────
freq_shifter_standalone_srcs = files([
    '../effects/frequency_shifter/frequency_shifter_processor.cpp',
    '../plugin/frequency_shifter_plugin.cpp',
    '../plugin/frequency_shifter_editor.cpp',
    '../plugin/look_and_feel.cpp',
    '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
])

freq_shifter_standalone_inc = include_directories([
    '../plugin',
    '../effects/frequency_shifter',
])

freq_shifter_standalone_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=1',
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
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

executable('kaos-engine-frequency-shifter',
    sources: freq_shifter_standalone_srcs,
    include_directories: [freq_shifter_standalone_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + freq_shifter_standalone_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── EQ standalone ─────────────────────────────────────────────────────────────
eq_standalone_srcs = files([
    '../effects/eq/eq_processor.cpp',
    '../plugin/eq_plugin.cpp',
    '../plugin/eq_editor.cpp',
    '../plugin/look_and_feel.cpp',
    '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
])

eq_standalone_inc = include_directories([
    '../plugin',
    '../effects/eq',
])

eq_standalone_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=1',
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
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

executable('kaos-engine-eq',
    sources: eq_standalone_srcs,
    include_directories: [eq_standalone_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + eq_standalone_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Compressor standalone ─────────────────────────────────────────────────────
comp_standalone_srcs = files([
    '../effects/compressor/compressor_processor.cpp',
    '../plugin/compressor_plugin.cpp',
    '../plugin/compressor_editor.cpp',
    '../plugin/look_and_feel.cpp',
    '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
])

comp_standalone_inc = include_directories([
    '../plugin',
    '../effects/compressor',
])

comp_standalone_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=1',
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
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

executable('kaos-engine-compressor',
    sources: comp_standalone_srcs,
    include_directories: [comp_standalone_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + comp_standalone_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Gate standalone ────────────────────────────────────────────────────────────
gate_standalone_srcs = files([
    '../effects/gate/gate_processor.cpp',
    '../plugin/gate_plugin.cpp',
    '../plugin/gate_editor.cpp',
    '../plugin/look_and_feel.cpp',
    '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
])

gate_standalone_inc = include_directories([
    '../plugin',
    '../effects/gate',
])

gate_standalone_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=1',
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
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

executable('kaos-engine-gate',
    sources: gate_standalone_srcs,
    include_directories: [gate_standalone_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + gate_standalone_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Envelope Follower standalone ──────────────────────────────────────────────
executable('kaos-engine-envelope-follower',
    sources: files([
        '../framework/envelope_follower/envelope_follower_processor.cpp',
        '../plugin/envelope_follower_plugin.cpp',
        '../plugin/envelope_follower_editor.cpp',
        '../plugin/look_and_feel.cpp',
        '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
    ]),
    include_directories: [
        include_directories(['../plugin', '../framework/envelope_follower']),
        juce_include_dirs,
    ],
    cpp_args: build_flags_cpp + build_flags + [
        '-DJucePlugin_Build_VST3=0',
        '-DJucePlugin_Build_VST=0',
        '-DJucePlugin_Build_AU=0',
        '-DJucePlugin_Build_AUv3=0',
        '-DJucePlugin_Build_AAX=0',
        '-DJucePlugin_Build_Standalone=1',
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
        '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
        '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Stochastic standalone ─────────────────────────────────────────────────────
executable('kaos-engine-stochastic',
    sources: files([
        '../framework/stochastic/stochastic_processor.cpp',
        '../plugin/stochastic_plugin.cpp',
        '../plugin/stochastic_editor.cpp',
        '../plugin/look_and_feel.cpp',
        '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
    ]),
    include_directories: [
        include_directories(['../plugin', '../framework/stochastic']),
        juce_include_dirs,
    ],
    cpp_args: build_flags_cpp + build_flags + [
        '-DJucePlugin_Build_VST3=0',
        '-DJucePlugin_Build_VST=0',
        '-DJucePlugin_Build_AU=0',
        '-DJucePlugin_Build_AUv3=0',
        '-DJucePlugin_Build_AAX=0',
        '-DJucePlugin_Build_Standalone=1',
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
        '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
        '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Filter standalone ─────────────────────────────────────────────────────────
filter_standalone_srcs = files([
    '../effects/filter/filter_processor.cpp',
    '../plugin/filter_plugin.cpp',
    '../plugin/filter_editor.cpp',
    '../plugin/look_and_feel.cpp',
    '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
])

filter_standalone_inc = include_directories([
    '../plugin',
    '../effects/filter',
])

executable('kaos-engine-filter',
    sources: filter_standalone_srcs,
    include_directories: [filter_standalone_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + [
        '-DJucePlugin_Build_VST3=0',
        '-DJucePlugin_Build_VST=0',
        '-DJucePlugin_Build_AU=0',
        '-DJucePlugin_Build_AUv3=0',
        '-DJucePlugin_Build_AAX=0',
        '-DJucePlugin_Build_Standalone=1',
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
        '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
        '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Noise standalone ──────────────────────────────────────────────────────────
executable('kaos-engine-noise',
    sources: files([
        '../effects/noise/noise_processor.cpp',
        '../plugin/noise_plugin.cpp',
        '../plugin/noise_editor.cpp',
        '../plugin/look_and_feel.cpp',
        '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
    ]),
    include_directories: [
        include_directories(['../plugin', '../effects/noise']),
        juce_include_dirs,
    ],
    cpp_args: build_flags_cpp + build_flags + [
        '-DJucePlugin_Build_VST3=0',
        '-DJucePlugin_Build_VST=0',
        '-DJucePlugin_Build_AU=0',
        '-DJucePlugin_Build_AUv3=0',
        '-DJucePlugin_Build_AAX=0',
        '-DJucePlugin_Build_Standalone=1',
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
        '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
        '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Spectrogram standalone ────────────────────────────────────────────────────
executable('kaos-engine-spectrogram',
    sources: files([
        '../effects/spectrogram/spectrogram_processor.cpp',
        '../plugin/spectrogram_plugin.cpp',
        '../plugin/spectrogram_editor.cpp',
        '../plugin/look_and_feel.cpp',
        '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
    ]),
    include_directories: [
        include_directories(['../plugin', '../effects/spectrogram']),
        juce_include_dirs,
    ],
    cpp_args: build_flags_cpp + build_flags + [
        '-DJucePlugin_Build_VST3=0',
        '-DJucePlugin_Build_VST=0',
        '-DJucePlugin_Build_AU=0',
        '-DJucePlugin_Build_AUv3=0',
        '-DJucePlugin_Build_AAX=0',
        '-DJucePlugin_Build_Standalone=1',
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
        '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
        '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── Looper standalone ────────────────────────────────────────────────────────
executable('kaos-engine-looper',
    sources: files([
        '../effects/looper/looper_processor.cpp',
        '../plugin/looper_plugin.cpp',
        '../plugin/looper_editor.cpp',
        '../plugin/look_and_feel.cpp',
        '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
    ]),
    include_directories: [
        include_directories(['../plugin', '../effects/looper']),
        juce_include_dirs,
    ],
    cpp_args: build_flags_cpp + build_flags + [
        '-DJucePlugin_Build_VST3=0',
        '-DJucePlugin_Build_VST=0',
        '-DJucePlugin_Build_AU=0',
        '-DJucePlugin_Build_AUv3=0',
        '-DJucePlugin_Build_AAX=0',
        '-DJucePlugin_Build_Standalone=1',
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
        '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
        '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)

# ── LFO standalone ─────────────────────────────────────────────────────────────
lfo_standalone_srcs = files([
    '../framework/lfo/lfo_processor.cpp',
    '../plugin/lfo_plugin.cpp',
    '../plugin/lfo_editor.cpp',
    '../plugin/look_and_feel.cpp',
    '../../third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp',
])

lfo_standalone_inc = include_directories([
    '../plugin',
    '../framework/lfo',
    '../effects',
])

lfo_standalone_defines = [
    '-DJucePlugin_Build_VST3=0',
    '-DJucePlugin_Build_VST=0',
    '-DJucePlugin_Build_AU=0',
    '-DJucePlugin_Build_AUv3=0',
    '-DJucePlugin_Build_AAX=0',
    '-DJucePlugin_Build_Standalone=1',
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
    '-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1',
    '-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1',
]

executable('kaos-engine-lfo',
    sources: lfo_standalone_srcs,
    include_directories: [lfo_standalone_inc, juce_include_dirs],
    cpp_args: build_flags_cpp + build_flags + lfo_standalone_defines + [
        '-Wno-deprecated-declarations', '-Wno-non-virtual-dtor', '-Wno-sign-compare',
    ],
    link_args: standalone_link_flags,
    link_with: lib_juce,
    dependencies: system_deps,
    win_subsystem: 'windows',
    install: false,
)
