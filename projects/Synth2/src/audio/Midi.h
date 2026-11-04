#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace synth2::audio
{
class MidiMessage
{
public:
    MidiMessage()=default;
    MidiMessage(int status,int first,int second=0):bytes{static_cast<uint8_t>(status),static_cast<uint8_t>(first),static_cast<uint8_t>(second)}{}
    MidiMessage(const void* data,int size,double=0){std::copy_n(static_cast<const uint8_t*>(data),std::min(3,size),bytes.begin());}
    void setChannel(int c){bytes[0]=static_cast<uint8_t>((bytes[0]&0xf0)|((c-1)&15));}
    static MidiMessage channelPressureChange(int c,int value){return {0xd0+c-1,value,0};}
    static MidiMessage noteOn(int c,int note,float velocity){return {0x90+c-1,note,std::clamp(static_cast<int>(std::lround(velocity*127.f)),0,127)};}
    static MidiMessage noteOn(int c,int note,uint8_t velocity){return {0x90+c-1,note,velocity};}
    static MidiMessage noteOff(int c,int note,float velocity=0.f){return {0x80+c-1,note,std::clamp(static_cast<int>(std::lround(velocity*127.f)),0,127)};}
    static MidiMessage controllerEvent(int c,int id,int value){return {0xb0+c-1,id,value};}
    static MidiMessage allSoundOff(int c){return controllerEvent(c,120,0);}
    static MidiMessage allNotesOff(int c){return controllerEvent(c,123,0);}
    static MidiMessage pitchWheel(int c,int value){return {0xe0+c-1,value&127,(value>>7)&127};}
    static double getMidiNoteInHertz(int note,double reference=440.){return reference*std::pow(2.,(note-69)*(1./12.));}
    int getChannel()const{return (bytes[0]&15)+1;}
    int getNoteNumber()const{return bytes[1];}
    uint8_t getVelocity()const{return bytes[2];}
    float getFloatVelocity()const{return bytes[2]*(1.f/127.f);}
    int getControllerNumber()const{return bytes[1];}
    int getControllerValue()const{return bytes[2];}
    int getPitchWheelValue()const{return bytes[1]+(bytes[2]<<7);}
    bool isNoteOn(bool zeroVelocity=false)const{return (bytes[0]&0xf0)==0x90 && (zeroVelocity || bytes[2]!=0);}
    bool isNoteOff(bool zeroVelocity=true)const{return (bytes[0]&0xf0)==0x80 || (zeroVelocity && (bytes[0]&0xf0)==0x90 && bytes[2]==0);}
    bool isNoteOnOrOff()const{return (bytes[0]&0xe0)==0x80;}
    bool isController()const{return (bytes[0]&0xf0)==0xb0;}
    bool isAllNotesOff()const{return isController() && bytes[1]==123;}
    bool isAllSoundOff()const{return isController() && bytes[1]==120;}
    bool isSustainPedalOn()const{return isController() && bytes[1]==64 && bytes[2]>=64;}
    bool isSustainPedalOff()const{return isController() && bytes[1]==64 && bytes[2]<64;}
    bool isPitchWheel()const{return (bytes[0]&0xf0)==0xe0;}
    bool isChannelPressure()const{return (bytes[0]&0xf0)==0xd0;}
    const uint8_t* getRawData()const{return bytes.data();}
    int getRawDataSize()const{return (bytes[0]&0xe0)==0xc0 ? 2 : 3;}
private:std::array<uint8_t,3> bytes{};
};
struct MidiEvent
{
    MidiMessage message;
    int samplePosition{};
    const MidiMessage& getMessage()const{return message;}
};
class MidiBuffer
{
public:
    static constexpr size_t capacity=4096;
    void clear(){count=0;}
    void ensureSize(int){}
    bool addEvent(const MidiMessage& m,int position)
    {
        if(count==capacity)return false;
        size_t at=count;
        while(at>0 && events[at-1].samplePosition>position){events[at]=events[at-1];--at;}
        events[at]={m,position};++count;return true;
    }
    bool addEvent(const void* bytes,int size,int position){return addEvent(MidiMessage(bytes,size),position);}
    const MidiEvent* begin()const{return events.data();}
    const MidiEvent* end()const{return events.data()+count;}
    const MidiEvent* cend()const{return end();}
    const MidiEvent* findNextSamplePosition(int position)const
    {return std::lower_bound(begin(),end(),position,[](const MidiEvent& e,int p){return e.samplePosition<p;});}
    void addEvents(const MidiBuffer& other,int start,int frames,int delta) {
        for(const auto& e:other)if(e.samplePosition>=start && (frames<0 || e.samplePosition<start+frames))addEvent(e.message,e.samplePosition+delta);
    }
    int getNumEvents()const{return static_cast<int>(count);}
    bool isEmpty()const{return count==0;}
private:std::array<MidiEvent,capacity> events{};size_t count{};
};
}
