#pragma once
#define PLUG_NAME "Synth2"
#define PLUG_MFR "babab"
#define PLUG_VERSION_HEX 0x00000100
#define PLUG_VERSION_STR "0.1.0"
// Product identity is independent of internal DSP and loading class names.
#define PLUG_UNIQUE_ID 'Syn2'
#define PLUG_MFR_ID 'BABA'
#define PLUG_URL_STR ""
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Copyright 2026 Synth2 contributors"
#define PLUG_CLASS_NAME Synth2
#define BUNDLE_NAME "Synth2"
#define BUNDLE_MFR "yasuyuki-baba"
#define BUNDLE_DOMAIN "io.github"
#define SHARED_RESOURCES_SUBPATH "Synth2"
#define PLUG_CHANNEL_IO "0-2"
#define PLUG_LATENCY 0
#define PLUG_TYPE 1
#define PLUG_DOES_MIDI_IN 1
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 1
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 1280
#define PLUG_HEIGHT 920
#define PLUG_FPS 30
#define PLUG_HOST_RESIZE 1
#define PLUG_SHARED_RESOURCES 0
#define AUV2_ENTRY Synth2_Entry
#define AUV2_ENTRY_STR "Synth2_Entry"
#define AUV2_FACTORY Synth2_Factory
#define AUV2_VIEW_CLASS Synth2_View
#define AUV2_VIEW_CLASS_STR "Synth2_View"
#define VST3_SUBCATEGORY "Instrument|Synth"
#define CLAP_MANUAL_URL ""
#define CLAP_SUPPORT_URL ""
#define CLAP_DESCRIPTION "Synth2 synthesizer"
#define CLAP_FEATURES "instrument", "synthesizer", "stereo"

// SDK standalone host: stereo output, unity gain and 64-frame DSP vectors.
#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64
