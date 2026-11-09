#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <tuple>
#include <utility>
#include <vector>

namespace synth2::audio
{
using uint32=uint32_t;
template<class T> struct MathConstants
{
    static constexpr T pi=static_cast<T>(3.141592653589793238462643383279502884L);
    static constexpr T twoPi=static_cast<T>(2)*pi;
};
template<class T> T jlimit(T low,T high,T value) { return std::clamp(value,low,high); }
template<class T> class ScopedValueSetter
{
public:
    ScopedValueSetter(T& value,T next):ref(value),previous(value){ref=next;}
    ~ScopedValueSetter(){ref=previous;}
private:T& ref;T previous;
};
struct Decibels
{
    static double decibelsToGain(double db,double floor=-100.0) {return db>floor ? std::pow(10.0,db*.05) : 0.0;}
    static double gainToDecibels(double gain,double floor=-100.0) {return gain>0.0 ? std::max(floor,20.0*std::log10(gain)) : floor;}
};
struct ProcessSpec
{
    double sampleRate{};uint32_t maximumBlockSize{},numChannels{};
    ProcessSpec()=default;
    ProcessSpec(double rate,uint32_t frames,uint32_t channels):sampleRate(rate),maximumBlockSize(frames),numChannels(channels){}
    template<class Spec> ProcessSpec(const Spec& s):ProcessSpec(s.sampleRate,s.maximumBlockSize,s.numChannels){}
};
// Storage is resized only during preparation. A pointer-backed view never owns
// samples and can be constructed on the audio stack without allocating.
template<class T> class AudioBuffer
{
public:
    AudioBuffer()=default;
    AudioBuffer(int c,int n){setSize(c,n);}
    AudioBuffer(const AudioBuffer& other){makeCopyOf(other);}
    AudioBuffer& operator=(const AudioBuffer& other){if(this!=&other)makeCopyOf(other);return *this;}
    AudioBuffer(AudioBuffer&&)=default;
    AudioBuffer& operator=(AudioBuffer&&)=default;
    void makeCopyOf(const AudioBuffer& other){setSize(other.channels,other.frames);for(int c=0;c<channels;++c)copyFrom(c,0,other.getReadPointer(c),frames);}
    T getMagnitude(int start,int count)const{T result{};for(int c=0;c<channels;++c)result=std::max(result,getMagnitude(c,start,count));return result;}
    T getMagnitude(int c,int start,int count)const{T result{};for(int i=start;i<start+count;++i)result=std::max(result,std::abs(pointers[c][i]));return result;}
    AudioBuffer(T*const* data,int c,int n):channels(c),frames(n){std::copy_n(data,c,pointers.begin());}
    void setSize(int c,int n,bool keep=false,bool=false,bool=false)
    {
        assert(c>=0 && c<=64 && n>=0);
        if(!keep || c!=channels || n!=frames)storage.resize(static_cast<size_t>(c)*n);
        channels=c;frames=n;
        for(int i=0;i<c;++i)pointers[i]=storage.data()+static_cast<size_t>(i)*n;
    }
    int getNumChannels()const{return channels;}
    int getNumSamples()const{return frames;}
    T* getWritePointer(int c,int offset=0){return pointers[c]+offset;}
    const T* getReadPointer(int c,int offset=0)const{return pointers[c]+offset;}
    T*const* getArrayOfWritePointers(){return pointers.data();}
    void clear(){clear(0,frames);}
    void clear(int start,int count){for(int c=0;c<channels;++c)clear(c,start,count);}
    void clear(int c,int start,int count){std::fill_n(pointers[c]+start,count,T{});}
    void addSample(int c,int i,T sample){pointers[c][i]+=sample;}
    void setSample(int c,int i,T sample){pointers[c][i]=sample;}
    T getSample(int c,int i)const{return pointers[c][i];}
    void applyGain(int start,int count,T gain){for(int c=0;c<channels;++c)applyGain(c,start,count,gain);}
    void applyGain(int c,int start,int count,T gain){for(int i=start;i<start+count;++i)pointers[c][i]*=gain;}
    void copyFrom(int c,int start,const T* source,int count){std::copy_n(source,count,pointers[c]+start);}
    void addFrom(int c,int start,const T* source,int count,T gain=T{1}){for(int i=0;i<count;++i)pointers[c][start+i]+=source[i]*gain;}
private:
    std::vector<T> storage;
    std::array<T*,64> pointers{};
    int channels{},frames{};
};
template<class T> class AudioBlock
{
public:
    explicit AudioBlock(AudioBuffer<T>& b):channels(b.getNumChannels()),frames(b.getNumSamples())
    {for(size_t c=0;c<channels;++c)pointers[c]=b.getWritePointer(static_cast<int>(c));}
    size_t getNumChannels()const{return channels;}
    size_t getNumSamples()const{return frames;}
    T* getChannelPointer(size_t c)const{return pointers[c];}
    template<class Block> void copyFrom(const Block& other)const
    {for(size_t c=0;c<channels;++c)if(pointers[c]!=other.getChannelPointer(c))std::copy_n(other.getChannelPointer(c),frames,pointers[c]);}
    AudioBlock getSubBlock(size_t offset,size_t count)const
    {auto result=*this;for(size_t c=0;c<channels;++c)result.pointers[c]+=offset;result.frames=count;return result;}
private:std::array<T*,64> pointers{};size_t channels{},frames{};
};
template<class T> struct ProcessContextReplacing
{
    explicit ProcessContextReplacing(AudioBlock<T>& b):block(b){}
    const AudioBlock<T>& getInputBlock()const{return block;}
    const AudioBlock<T>& getOutputBlock()const{return block;}
    bool isBypassed=false;
private:AudioBlock<T> block;
};
template<class... Processors> class ProcessorChain
{
public:
    template<size_t I> auto& get(){return std::get<I>(processors);}
    template<size_t I> void setBypassed(bool value){bypassed[I]=value;}
    void prepare(const ProcessSpec& spec){std::apply([&](auto&... p){(p.prepare(spec),...);},processors);}
    template<class Context> void process(const Context& context){processAt(context,std::index_sequence_for<Processors...>{});}
private:
    template<class Context,size_t... I> void processAt(const Context& context,std::index_sequence<I...>)
    {([&]{auto next=context;next.isBypassed=context.isBypassed || bypassed[I];std::get<I>(processors).process(next);}(),...);}
    std::tuple<Processors...> processors;
    std::array<bool,sizeof...(Processors)> bypassed{};
};

}
