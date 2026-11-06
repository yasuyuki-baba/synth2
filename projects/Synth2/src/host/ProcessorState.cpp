#include "ProcessorState.h"
#include "dsp/OversamplingSettings.h"
#include "patch/PatchLoader.h"
#include "patch/PatchFormat.h"
#include "patch/Synth1SourceRange.h"
#include "serialization/FiniteNumber.h"
#include <Synth1NativeLoading.h>
#include <Synth1PatchNativeControls.h>
#include <tinyxml2.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>

namespace synth2::host
{
namespace
{
constexpr int parameterCount=processorParameterCount;
std::string escapeXml(const std::string& value)
{
    std::string out;
    for(char c:value)switch(c)
    {
        case '&':out+="&amp;";break;case '<':out+="&lt;";break;case '>':out+="&gt;";break;
        case '\"':out+="&quot;";break;case '\r':out+="&#13;";break;case '\n':out+="&#10;";break;
        case '\t':out+="&#9;";break;default:out+=c;break;
    }
    return out;
}
uint32_t read32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
void append32(std::vector<uint8_t>& bytes,uint32_t word){for(int i=0;i<4;++i)bytes.push_back(static_cast<uint8_t>(word>>(8*i)));}
}
std::vector<uint8_t> encodeProcessorState(const ProcessorState& state,
    const std::array<ParameterInfo,processorParameterCount>& info)
{
    const bool native=state.nativeSource;
    std::ostringstream xml;xml.imbue(std::locale::classic());xml<<std::setprecision(17);
    xml<<"<?xml version=\"1.0\" encoding=\"UTF-8\"?> <PARAMS stateVersion=\""<<state.version<<'"';
    if(state.oversamplingFactor!=1)xml<<" oversamplingFactor=\""<<state.oversamplingFactor<<'"';
    if(state.version==3)xml<<" nativeSource=\""<<(native ? 1 : 0)<<'"';
    if(state.directControls)xml<<" patchFormat=\"200\"";
    if(native)
    {
        for(const auto& field:{std::make_pair("sourcePresence",state.sourcePresence),std::make_pair("nativeRuntimeEdits",state.runtimeEdits)})
        {xml<<' '<<field.first<<"=\"";for(int i=0;i<126;++i)xml<<((field.second[i/64]>>(i%64))&1);xml<<'"';}
    }
    xml<<" missingEnvelopeFields=\""<<state.missingEnvelopeFields<<"\" program=\""<<state.program<<"\" presetText=\""<<escapeXml(state.presetText)<<"\" patchName=\""<<escapeXml(state.patchName)<<"\">";
    for(int i=0;i<parameterCount;++i)xml<<"<PARAM id=\""<<info[i].id<<"\" value=\""<<state.values[i]<<"\"/>";
    xml<<"</PARAMS>";const auto text=xml.str();std::vector<uint8_t> bytes;bytes.reserve(text.size()+9);
    append32(bytes,0x21324356);append32(bytes,static_cast<uint32_t>(text.size()));bytes.insert(bytes.end(),text.begin(),text.end());bytes.push_back(0);return bytes;
}
std::optional<ProcessorState> decodeProcessorState(const void* data,int size,
    const std::array<ParameterInfo,processorParameterCount>& info)
{
    ProcessorState state;
    if(!data || size<9 || size>4*1024*1024)return std::nullopt;
    const auto* bytes=static_cast<const uint8_t*>(data);const auto length=read32(bytes+4);
    if(read32(bytes)!=0x21324356 || length>static_cast<uint32_t>(size-8))return std::nullopt;
    tinyxml2::XMLDocument xml(true,tinyxml2::PRESERVE_WHITESPACE);
    if(xml.Parse(reinterpret_cast<const char*>(bytes+8),length)!=tinyxml2::XML_SUCCESS)return std::nullopt;
    auto* root=xml.RootElement();if(!root || std::strcmp(root->Name(),"PARAMS"))return std::nullopt;
    auto property=[&](const char* key)->std::string{const auto* s=root->Attribute(key);return s ? s : "";};
    const auto oversampling=property("oversamplingFactor");
    if(!oversampling.empty())
    {
        const auto parsed=serialization::parseFiniteNumber(oversampling);
        if(!parsed || *parsed!=std::floor(*parsed) || *parsed<1 || *parsed>16
            || !dsp::OversamplingSettings::valid(static_cast<int>(*parsed)))return std::nullopt;
        state.oversamplingFactor=static_cast<int>(*parsed);
    }
    const auto stateVersion=property("stateVersion");if(stateVersion!="1" && stateVersion!="2" && stateVersion!="3")return std::nullopt;
    const auto patchFormat=property("patchFormat");
    if(!patchFormat.empty() && (patchFormat!="200" || stateVersion!="3" || property("nativeSource")!="0"))return std::nullopt;
    const bool direct=patchFormat=="200";
    if(stateVersion=="3" && property("nativeSource")!="0" && property("nativeSource")!="1")return std::nullopt;
    const bool native=stateVersion=="2" || (stateVersion=="3" && property("nativeSource")=="1");
    auto& presence=state.sourcePresence;auto& edits=state.runtimeEdits;auto& initial=state.initialControls;
    if(native)
    {
        for(const auto& field:{std::make_pair("sourcePresence",&presence),std::make_pair("nativeRuntimeEdits",&edits)})
        {const auto bits=property(field.first);if(bits.size()!=126 || bits.find_first_not_of("01")!=std::string::npos)return std::nullopt;
         for(int i=0;i<126;++i)if(bits[i]=='1')(*field.second)[i/64]|=uint64_t{1}<<(i%64);}
    }
    auto& next=state.values;std::array<bool,parameterCount> seen{};int count=0;
    for(auto* child=root->FirstChildElement();child;child=child->NextSiblingElement())
    {
        const auto* id=child->Attribute("id");const auto* valueText=child->Attribute("value");
        if(std::strcmp(child->Name(),"PARAM") || !id || !valueText)return std::nullopt;
        int index=-1;for(int i=0;i<parameterCount;++i)if(info[i].id==id){index=i;break;}
        if(index<0 || seen[index])return std::nullopt;
        const auto parsed=serialization::parseFiniteNumber(valueText);
        if(!parsed || *parsed<info[index].minimum || *parsed>info[index].maximum)return std::nullopt;
        const double value=*parsed;
        next[index]=static_cast<float>(value);seen[index]=true;++count;
    }
    if(count!=parameterCount)return std::nullopt;
    unsigned missing=0;
    if(root->Attribute("missingEnvelopeFields"))
    {const auto text=property("missingEnvelopeFields");if(text.empty() || text.size()>2 || text.find_first_not_of("0123456789")!=std::string::npos)return std::nullopt;missing=static_cast<unsigned>(std::stoul(text));if(missing>63)return std::nullopt;}
    const int version=static_cast<int>(std::lround(next[126]));const auto preset=property("presetText");
    if(!direct && isContinuousPatch(preset))return std::nullopt;
    if(direct)
    {
        if(missing!=0)return std::nullopt;
        try {parseContinuousPatch(preset);}catch(const std::exception&){return std::nullopt;}
    }
    if(native)
    {
        if(!synth1::patch::isNativeSourceVersion(version))return std::nullopt;
        unsigned expected=63;
        for(int i:synth1::patch::nativeEnvelopeIndices)if((presence[i/64]|edits[i/64])&(uint64_t{1}<<(i%64)))expected&=~static_cast<unsigned>(synth1::patch::nativeEnvelopePresenceBit(i));
        if(!root->Attribute("missingEnvelopeFields") || missing!=expected)return std::nullopt;
        try
        {
            const auto original=parsePatchDocument(preset);
            if(original.version().value_or(hasPatchTitle(preset) ? 100 : 105)!=version || (!original.version() && !hasPatchTitle(preset)))return std::nullopt;
            std::array<uint64_t,2> originalPresence{};std::array<int,126> source{};
            for(int i=0;i<99;++i)source[i]=std::clamp(synth1::patch::nativeDefaultSourceDial(i,version),sourceMinimum(i),sourceMaximum(i));
            for(const auto& e:original.entries)
            {if(e.index<0 || e.index>=126)return std::nullopt;originalPresence[e.index/64]|=uint64_t{1}<<(e.index%64);source[e.index]=std::clamp(e.rawValue,sourceMinimum(e.index),sourceMaximum(e.index));}
            if(originalPresence!=presence)return std::nullopt;
            initial=synth1::patch::resolveNativePatchControls(original).values;
            for(int i=0;i<126;++i)if(!(edits[i/64]&(uint64_t{1}<<(i%64))) && std::abs(next[i]-source[i])>.001)return std::nullopt;
        }
        catch(const std::exception&){return std::nullopt;}
    }
    auto restoredName=property("patchName");
    if(restoredName.empty())
    {
        restoredName=preset.empty() ? "Init" : "Untitled";
        try { if(direct)restoredName=parseContinuousPatch(preset).name;
              else if(hasPatchTitle(preset))restoredName=parsePatchDocument(preset).name; }
        catch(const std::exception&) {}
    }

    state.version=std::stoi(stateVersion);
    state.nativeSource=native;state.directControls=direct;
    state.program=std::max(0,root->IntAttribute("program",0));
    state.presetText=preset;state.patchName=std::move(restoredName);
    state.missingEnvelopeFields=missing;
    return state;
}
}
