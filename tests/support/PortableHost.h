#pragma once
// Test host helpers adapt callback buffers to the actual SDK-independent bridge.
#include "host/ProcessorBridge.h"
#include "audio/AudioTypes.h"
#include "audio/Midi.h"
#include <gtest/gtest.h>
#include <memory>
inline synth2::host::ProcessorBridge* createPortableProcessor(){return new synth2::host::ProcessorBridge;}
inline bool loadPortablePatch(synth2::host::ProcessorBridge& p,const std::string& text,std::string& error){return p.loadPatch(text,error);}
inline void processPortableBlock(synth2::host::ProcessorBridge& p,synth2::audio::AudioBuffer<float>& audio,const synth2::audio::MidiBuffer& midi,double tempo=120.) {
    for(const auto& e:midi){const auto* b=e.getMessage().getRawData();ASSERT_TRUE(p.enqueueMidi(e.samplePosition,b[0],b[1],b[2]));}
    ASSERT_TRUE(p.process(audio.getWritePointer(0),audio.getWritePointer(1),audio.getNumSamples(),tempo));
}
