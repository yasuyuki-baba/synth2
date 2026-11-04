#pragma once
// Offline test inspection/mutation of the retained binary XML envelope.
// Production serialization is exercised through ProcessorBridge, never this writer.
#include "host/ProcessorBridge.h"
#include <tinyxml2.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>
namespace stateinspection {
inline std::unique_ptr<tinyxml2::XMLDocument> document(const std::vector<uint8_t>& bytes) {
    if(bytes.size()<9)throw std::runtime_error("Missing state envelope");
    auto xml=std::make_unique<tinyxml2::XMLDocument>();
    if(xml->Parse(reinterpret_cast<const char*>(bytes.data()+8),bytes.size()-9)!=tinyxml2::XML_SUCCESS || !xml->RootElement())throw std::runtime_error("Invalid state XML");
    return xml;
}
inline std::unique_ptr<tinyxml2::XMLDocument> document(const synth2::host::ProcessorBridge& p) {return document(p.saveState());}
inline std::string attribute(const tinyxml2::XMLDocument& xml,const char* key) {
    const auto* value=xml.RootElement()->Attribute(key);return value ? value : "";
}
inline std::vector<uint8_t> bytes(const tinyxml2::XMLDocument& xml) {
    tinyxml2::XMLPrinter printer;xml.Print(&printer);
    const uint32_t length=printer.CStrSize()-1;std::vector<uint8_t> out(8+length+1);
    const uint32_t fields[]={0x21324356,length};
    for(int i=0;i<2;++i)for(int j=0;j<4;++j)out[i*4+j]=static_cast<uint8_t>(fields[i]>>(j*8));
    std::copy_n(printer.CStr(),length+1,out.begin()+8);return out;
}
inline float raw(const synth2::host::ProcessorBridge& p,int i) {
    const auto& info=p.parameter(i);return static_cast<float>(info.minimum)+(static_cast<float>(info.maximum)-static_cast<float>(info.minimum))*static_cast<float>(p.normalizedValue(i));
}
inline float normalized(const synth2::host::ProcessorBridge& p,int i,float raw) {
    const auto& info=p.parameter(i);return (raw-static_cast<float>(info.minimum))/(static_cast<float>(info.maximum)-static_cast<float>(info.minimum));
}
}
