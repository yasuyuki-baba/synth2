#include "PatchTagInference.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace synth2::host
{
std::vector<std::string> inferPatchTags(std::string_view name,const std::array<float,129>& p)
{
    std::vector<std::string> words;std::string word;
    const auto flush=[&] {if(!word.empty()){words.push_back(word);word.clear();}};
    char previous=0;
    for(unsigned char c:name) {
        if(c>='A' && c<='Z') {
            if(previous>='a' && previous<='z')flush();
            word+=static_cast<char>(c-'A'+'a');
        } else if(c>='a' && c<='z')word+=static_cast<char>(c);
        else flush();
        previous=static_cast<char>(c);
    }
    flush();
    const std::pair<const char*,const char*> aliases[]={{"bass","Bass"},
        {"basses","Bass"},
        {"subbass","Bass"},
        {"lead","Lead"},
        {"leads","Lead"},
        {"pad","Pad"},
        {"pads","Pad"},
        {"atmosphere","Pad"},
        {"ambient","Pad"},
        {"pluck","Pluck"},
        {"plucks","Pluck"},
        {"plucked","Pluck"},
        {"piano","Keys"},
        {"epiano","Keys"},
        {"keys","Keys"},
        {"keyboard","Keys"},
        {"clav","Keys"},
        {"clavinet","Keys"},
        {"harpsichord","Keys"},
        {"organ","Organ"},
        {"organs","Organ"},
        {"hammond","Organ"},
        {"string","Strings"},
        {"strings","Strings"},
        {"violin","Strings"},
        {"cello","Strings"},
        {"brass","Brass"},
        {"trumpet","Brass"},
        {"trombone","Brass"},
        {"horn","Brass"},
        {"bell","Bell"},
        {"bells","Bell"},
        {"chime","Bell"},
        {"chimes","Bell"},
        {"glockenspiel","Bell"},
        {"drum","Percussion"},
        {"drums","Percussion"},
        {"kick","Percussion"},
        {"snare","Percussion"},
        {"hihat","Percussion"},
        {"hat","Percussion"},
        {"clap","Percussion"},
        {"tom","Percussion"},
        {"perc","Percussion"},
        {"percussion","Percussion"},
        {"cymbal","Percussion"},
        {"fx","FX"},
        {"sfx","FX"},
        {"effect","FX"},
        {"effects","FX"},
        {"laser","FX"},
        {"explosion","FX"},
        {"arp","Arp"},
        {"arpeggio","Arp"},
        {"arpeggiator","Arp"},
        {"arpeggiated","Arp"}};
    const std::pair<const char*,const char*> prefixes[]={{"bs","Bass"},
        {"bas","Bass"},
        {"ld","Lead"},
        {"pd","Pad"},
        {"plk","Pluck"},
        {"kys","Keys"},
        {"ky","Keys"},
        {"str","Strings"},
        {"brs","Brass"},
        {"bl","Bell"},
        {"prc","Percussion"}};
    std::vector<std::string> tags;
    const auto add=[&](std::string tag) {if(std::find(tags.begin(),tags.end(),tag)==tags.end())tags.push_back(std::move(tag));};
    for(const auto& token:words)for(const auto& [match,tag]:aliases)if(token==match)add(tag);
    if(!words.empty())for(const auto& [match,tag]:prefixes)if(words.front()==match)add(tag);
    const auto attack=[](float dial) {return .0005*std::pow(56000.,std::clamp(dial,0.f,127.f)/127.);};
    const auto decay=[](float dial) {return .01*std::pow(6000.,std::clamp(dial,0.f,127.f)/127.);};
    const double a=attack(p[25]),d=decay(p[26]),release=decay(p[28]);
    const double raw=std::clamp(p[27],0.f,127.f);
    const double sustain=raw==0 ? 0 : std::floor(std::pow(2048.,std::pow(raw/127.,.3)-1)*2048)/2048;
    // Defer obvious name/settings disagreements instead of asserting a use.
    std::erase_if(tags,[&](const auto& tag) {
        return (tag=="Pad" && a<=.01 && sustain<=.03 && d<=.3)
            || ((tag=="Pluck" || tag=="Percussion") && a>=.15)
            || (tag=="Arp" && p[59]==0);
    });
    const bool filterPluck=(p[14]==0 || p[14]==1 || p[14]==4) && attack(p[15])<=.03
        && decay(p[16])<=2 && p[17]<=.3*127 && p[21]>=80 && p[19]<=96;
    if(filterPluck)add("Filter pluck");
    if(a>=.15)add("Slow attack");
    if(release>=1)add("Long release");
    if(a<=.03 && sustain<=.12 && d<=2)add("Short decay");
    if(p[1]==4 && p[5]>=.15*127)add("Noise");
    if(p[7]==0 && p[45]>=8)add("FM");
    if(p[6]!=0)add("Sync");
    if(p[7]!=0)add("Ring mod");
    if(p[59]!=0)add("Arp");
    if(p[73]!=0 && p[93]>=2)add("Unison");
    if(p[38]==1)add("Mono");
    if(p[38]==2)add("Legato");
    if(p[77]!=0 && p[78]>=0 && p[78]<=3)add("Distortion");
    return tags;
}
}
