#pragma once
#include "host/ProcessorBridge.h"
#include "audio/AudioTypes.h"
#include "audio/Midi.h"
#include "SynthDiagnostics.h"
#include "patch/PatchLoader.h"
#include <cmath>
#include <memory>
#include <stdexcept>



namespace hostvalidation
{
inline auto processor() { return std::make_unique<synth2::host::ProcessorBridge>(); }
inline void load(synth2::host::ProcessorBridge& p,const std::string& text)
{
    std::string error;if(!p.loadPatch(text,error))throw std::runtime_error(error);
}
// Explicit dry polyphonic saw/triangle control, short release, fixed phase.
// Source controls, not production parameter overrides or a factory-number branch.
inline std::string patch(int unison=1,int mode=0)
{
    return "Host validation\nver=113\n0,0\n1,2\n2,64\n3,64\n4,1\n5,64\n6,0\n7,0\n8,64\n"
        "9,0\n10,0\n11,64\n12,0\n13,0\n14,1\n15,0\n16,0\n17,127\n18,0\n19,127\n20,0\n21,63\n"
        "22,0\n23,0\n24,0\n25,0\n26,0\n27,127\n28,0\n29,80\n30,0\n31,1\n32,0\n33,11\n34,64\n"
        "35,0\n36,0\n37,0\n38,"+std::to_string(mode)+"\n39,0\n41,1\n42,1\n43,64\n44,0\n45,0\n46,1\n"
        "47,1\n48,64\n49,0\n57,0\n58,0\n59,0\n60,64\n61,64\n62,64\n63,64\n64,0\n65,0\n66,0\n"
        "67,0\n68,0\n69,0\n70,0\n72,64\n73,"+std::to_string(unison>1 ? 1 : 0)+"\n74,0\n75,0\n76,0\n77,0\n78,0\n"
        "79,0\n80,0\n81,0\n82,0\n83,0\n84,64\n86,0\n87,0\n88,0\n89,0\n90,64\n91,0\n92,64\n93,"
        +std::to_string(unison)+"\n94,32\n95,0\n96,0\n97,0\n98,0\n";
}
inline void notes(synth2::audio::MidiBuffer& midi,int count,bool on=true,int offset=0)
{
    for(int n=0;n<count;++n)midi.addEvent(on ? synth2::audio::MidiMessage::noteOn(1,36+n,static_cast<uint8_t>(80))
                                                         : synth2::audio::MidiMessage::noteOff(1,36+n),offset);
}
inline int activeCores(synth2::host::ProcessorBridge& p)
{
    int result=0;
    for(size_t voice=0;voice<32;++voice)for(size_t core=0;core<8;++core)
    {
        std::array<uint32_t,16> words{};
        if(synth2::readRenderingCoreWords(p,voice,core,words) && words[0])++result;
    }
    return result;
}
inline bool finite(const synth2::audio::AudioBuffer<float>& audio)
{
    for(int ch=0;ch<audio.getNumChannels();++ch)for(int i=0;i<audio.getNumSamples();++i)
        if(!std::isfinite(audio.getSample(ch,i)))return false;
    return true;
}
inline void process(synth2::host::ProcessorBridge& p,synth2::audio::AudioBuffer<float>& a,const synth2::audio::MidiBuffer& midi) {
    for(const auto& e:midi){const auto* b=e.getMessage().getRawData();if(!p.enqueueMidi(e.samplePosition,b[0],b[1],b[2]))throw std::runtime_error("MIDI rejected");}
    if(!p.process(a.getWritePointer(0),a.getWritePointer(1),a.getNumSamples(),120.))throw std::runtime_error("Block rejected");
}
inline auto save(synth2::host::ProcessorBridge& p){return p.saveState();}
inline void restore(synth2::host::ProcessorBridge& p,const std::vector<uint8_t>& state)
{p.restoreState(state.data(),static_cast<int>(state.size()));}
}
