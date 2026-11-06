#pragma once
// Small offline RIFF/WAVE reader/writer. No plugin framework or device APIs.
#include "audio/AudioTypes.h"
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace synth2::offline {
inline uint32_t little(const unsigned char* p,int bytes) { uint32_t v=0;for(int i=0;i<bytes;++i)v|=uint32_t(p[i])<<(8*i);return v; }
inline void put(std::ostream& out,uint32_t v,int bytes) { for(int i=0;i<bytes;++i)out.put(static_cast<char>(v>>(8*i))); }
struct WaveFile {
    double sampleRate=0;
    int64_t lengthInSamples=0;
    unsigned numChannels=0;
    audio::AudioBuffer<float> samples;
    explicit WaveFile(const std::filesystem::path& path) {
        std::ifstream in(path,std::ios::binary);
        auto read=[&](unsigned char* p,size_t n){if(!in.read(reinterpret_cast<char*>(p),n))throw std::runtime_error("Truncated WAV");};
        unsigned char head[12];read(head,12);
        if(std::string(reinterpret_cast<char*>(head),4)!="RIFF" || std::string(reinterpret_cast<char*>(head+8),4)!="WAVE")throw std::runtime_error("Expected RIFF/WAVE");
        unsigned format=0,bits=0,align=0;std::vector<unsigned char> data;
        while(in.peek()!=std::char_traits<char>::eof()) {
            unsigned char h[8];read(h,8);const auto size=little(h+4,4);const std::string id(reinterpret_cast<char*>(h),4);
            if(size>512u*1024u*1024u)throw std::runtime_error("WAV chunk exceeds limit");
            if(id=="fmt ") {
                if(size<16 || size>65536)throw std::runtime_error("Invalid WAV format");
                std::vector<unsigned char> f(size);read(f.data(),size);
                format=little(f.data(),2);numChannels=little(f.data()+2,2);sampleRate=little(f.data()+4,4);align=little(f.data()+12,2);bits=little(f.data()+14,2);
                if(format==0xfffe && size>=40)format=little(f.data()+24,2);
            } else if(id=="data") {data.resize(size);read(data.data(),size);}
            else {in.seekg(size,std::ios::cur);if(!in)throw std::runtime_error("Invalid WAV chunk");}
            if(size&1)in.get();
        }
        if(numChannels<1 || numChannels>64 || sampleRate<=0 || (format!=1 && format!=3)
           || (format==3 ? bits!=32 : (bits!=8 && bits!=16 && bits!=24 && bits!=32))
           || align!=numChannels*(bits/8) || data.size()%align)throw std::runtime_error("Unsupported WAV format");
        lengthInSamples=data.size()/align;
        if(lengthInSamples>std::numeric_limits<int>::max())throw std::runtime_error("WAV too long");
        samples.setSize(numChannels,static_cast<int>(lengthInSamples));
        for(int64_t i=0;i<lengthInSamples;++i)for(unsigned c=0;c<numChannels;++c) {
            const auto word=little(data.data()+i*align+c*(bits/8),bits/8);float value;
            if(format==3)value=std::bit_cast<float>(word);
            else if(bits==8)value=(int(word)-128)/128.0f;
            else {const int32_t signedValue=static_cast<int32_t>(word<<(32-bits))>>(32-bits);value=static_cast<float>(signedValue/std::ldexp(1.0,bits-1));}
            samples.setSample(c,static_cast<int>(i),value);
        }
    }

};
inline void writeWave(const std::filesystem::path& path,const audio::AudioBuffer<float>& audio,double rate,bool floating) {
    const auto channels=audio.getNumChannels(),frames=audio.getNumSamples(),bytes=floating?4:3;
    const uint64_t size=uint64_t(channels)*frames*bytes;
    if(size>UINT32_MAX-36)throw std::runtime_error("WAV too large");
    std::ofstream out(path,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("Cannot open WAV output");
    out.write("RIFF",4);put(out,36+size+(size&1),4);out.write("WAVEfmt ",8);put(out,16,4);put(out,floating?3:1,2);put(out,channels,2);
    put(out,static_cast<uint32_t>(std::lround(rate)),4);put(out,static_cast<uint32_t>(std::lround(rate))*channels*bytes,4);put(out,channels*bytes,2);put(out,bytes*8,2);out.write("data",4);put(out,size,4);
    for(int i=0;i<frames;++i)for(int c=0;c<channels;++c) {
        const float v=audio.getSample(c,i);if(!std::isfinite(v))throw std::runtime_error("Non-finite WAV sample");
        if(floating)put(out,std::bit_cast<uint32_t>(v),4);
        else { // Preserve the preceding renderer's 32-bit conversion then 24-bit truncation.
            const double scaled=std::clamp(static_cast<double>(v)*2147483647.0,-2147483648.0,2147483647.0);
            const auto word=static_cast<int32_t>(std::lrint(scaled));put(out,static_cast<uint32_t>(word)>>8,3);
        }
    }
    if(size&1)out.put(0);if(!out)throw std::runtime_error("Cannot write WAV completely");
}
}
