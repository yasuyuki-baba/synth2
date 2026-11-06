#pragma once
#include "AudioTypes.h"
#include "Midi.h"
#include <memory>

namespace synth2::audio
{
class Sound
{
public:virtual ~Sound()=default;virtual bool appliesToNote(int)=0;virtual bool appliesToChannel(int)=0;
};
class VoiceBank;
class Voice
{
public:
    virtual ~Voice()=default;
    virtual bool canPlaySound(Sound*)=0;
    virtual void startNote(int,float,Sound*,int)=0;
    virtual void stopNote(float,bool)=0;
    virtual void pitchWheelMoved(int)=0;
    virtual void controllerMoved(int,int)=0;
    virtual void renderNextBlock(AudioBuffer<float>&,int,int)=0;
    virtual void aftertouchChanged(int){}
    virtual void channelPressureChanged(int){}
    bool isVoiceActive()const{return note>=0;}
    bool isPlayingChannel(int c)const{return channel==c;}
    int getCurrentlyPlayingNote()const{return note;}
    Sound* getCurrentlyPlayingSound()const{return sound;}
    bool wasStartedBefore(const Voice& other)const{return order<other.order;}
    double getSampleRate()const{return sampleRate;}
    void clearCurrentNote(){note=-1;sound=nullptr;channel=0;}
private:
    friend class VoiceBank;
    Sound* sound=nullptr;int note=-1,channel=0;uint64_t order{};
    bool keyDown=false,sustain=false,sostenuto=false;
    double sampleRate=44100;
};
// Audio-thread-owned logical voices. Physical DSP allocation remains in
// VoiceCorePool; this class only dispatches channel messages and note lifetime.
class VoiceBank
{
public:
    VoiceBank(){pitch.fill(8192);}
    virtual ~VoiceBank()=default;
    void setNoteStealingEnabled(bool value){stealNotes=value;}
    void addVoice(Voice* voice){voice->sampleRate=sampleRate;voices.emplace_back(voice);}
    void addSound(Sound* sound){sounds.emplace_back(sound);}
    int getNumVoices()const{return static_cast<int>(voices.size());}
    Voice* getVoice(int i)const{return voices.at(static_cast<size_t>(i)).get();}
    Sound* getSound(int i)const{return sounds.at(static_cast<size_t>(i)).get();}
    void setCurrentPlaybackSampleRate(double rate)
    {
        if(rate==sampleRate)return;
        allNotesOff(0,false);sampleRate=rate;
        for(auto& voice:voices)voice->sampleRate=rate;
    }
    virtual void noteOn(int c,int note,float velocity)
    {
        for(auto& sound:sounds)
        {
            if(!sound->appliesToNote(note) || !sound->appliesToChannel(c))continue;
            for(auto& voice:voices)
                if(voice->note==note && voice->channel==c)stopVoice(voice.get(),1.f,true);
            startVoice(findFreeVoice(sound.get(),c,note,stealNotes),sound.get(),c,note,velocity);
        }
    }
    virtual void noteOff(int c,int note,float velocity,bool tail)
    {
        for(auto& v:voices)
        {
            if(v->note!=note || v->channel!=c || !v->sound)continue;
            if(!v->sound->appliesToNote(note) || !v->sound->appliesToChannel(c))continue;
            v->keyDown=false;
            if(!v->sustain && !v->sostenuto)stopVoice(v.get(),velocity,tail);
        }
    }
    double getSampleRate()const{return sampleRate;}
    virtual void allNotesOff(int c,bool tail)
    {
        for(auto& v:voices)if(c<=0 || v->channel==c)v->stopNote(1.f,tail);
        sustain.fill(false);
    }
protected:
    virtual Voice* findFreeVoice(Sound* sound,int,int,bool)const
    {for(auto& v:voices)if(!v->isVoiceActive() && v->canPlaySound(sound))return v.get();return nullptr;}
    void startVoice(Voice* v,Sound* sound,int c,int note,float velocity)
    {
        if(!v || !sound || c<1 || c>16)return;
        if(v->sound)v->stopNote(0.f,false);
        v->note=note;v->channel=c;v->order=++counter;v->sound=sound;
        v->keyDown=true;v->sostenuto=false;v->sustain=sustain[c-1];
        v->startNote(note,velocity,sound,pitch[c-1]);
    }
    void stopVoice(Voice* voice,float velocity,bool tail){voice->stopNote(velocity,tail);}
    void renderVoices(AudioBuffer<float>& output,int start,int frames)
    {for(auto& voice:voices)voice->renderNextBlock(output,start,frames);}
    virtual void handlePitchWheel(int c,int value) {
        pitch[c-1]=value;for(auto& v:voices)if(v->channel==c)v->pitchWheelMoved(value);
    }
    virtual void handleSustainPedal(int c,bool down) {
        if(down) {
            sustain[c-1]=true;for(auto& v:voices)if(v->channel==c && v->keyDown)v->sustain=true;
        } else {
            for(auto& v:voices)if(v->channel==c){v->sustain=false;if(!v->keyDown && !v->sostenuto)stopVoice(v.get(),1.f,true);}
            sustain[c-1]=false;
        }
    }
    virtual void handleController(int c,int id,int value) {
        if(id==64)handleSustainPedal(c,value>=64);
        else if(id==66)for(auto& v:voices)if(v->channel==c){if(value>=64)v->sostenuto=true;else if(v->sostenuto)stopVoice(v.get(),1.f,true);}
        for(auto& v:voices)if(v->channel==c)v->controllerMoved(id,value);
    }
    void handleMidiEvent(const MidiMessage& message) {
        const int c=message.getChannel(),status=message.getRawData()[0]&0xf0;
        if(message.isNoteOn()){noteOn(c,message.getNoteNumber(),message.getFloatVelocity());return;}
        if(message.isNoteOff()){noteOff(c,message.getNoteNumber(),message.getFloatVelocity(),true);return;}
        if(message.isAllNotesOff() || message.isAllSoundOff()){allNotesOff(c,true);return;}
        if(message.isPitchWheel()){handlePitchWheel(c,message.getPitchWheelValue());return;}
        if(status==0xa0){for(auto& v:voices)if(v->channel==c && v->note==message.getNoteNumber())v->aftertouchChanged(message.getRawData()[2]);return;}
        if(message.isChannelPressure()){for(auto& v:voices)if(v->channel==c)v->channelPressureChanged(message.getRawData()[1]);return;}
        if(message.isController())handleController(c,message.getControllerNumber(),message.getControllerValue());
    }

private:
    std::vector<std::unique_ptr<Voice>> voices;
    std::vector<std::unique_ptr<Sound>> sounds;
    std::array<bool,16> sustain{};
    std::array<int,16> pitch{};
    uint64_t counter{};double sampleRate=0;bool stealNotes=true;
};
}
