#pragma once
#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

namespace synth2::editor
{
struct EditorChoice { int value; const char* label; };
struct EditorBounds { int x,y,width,height; };
struct EditorControl { int parameter; const char* label; };
struct EditorPanel
{
    const char* title;
    const char* description;
    const char* accent;
    EditorBounds bounds;
    int columns;
    std::vector<EditorControl> controls;
};
inline bool editorToggle(int index)
{
    switch(index)
    {
        case 4: case 6: case 7: case 10: case 57: case 58: case 59: case 65: case 66:
        case 67: case 68: case 69: case 70: case 73: case 74: case 77: return true;
        default: return false;
    }
}
// Source integers from Synth1PatchIoCodec.cpp, not indices into a new host
// enumeration. The host's source ranges and saved values stay intact.
inline std::vector<EditorChoice> editorChoices(int index)
{
    switch(index)
    {
        case 0: return {{0,"Sine"},{1,"Saw"},{2,"Pulse"},{3,"Triangle"}};
        case 1: return {{1,"Saw"},{2,"Pulse"},{3,"Triangle"},{4,"Noise"}};
        case 14: return {{0,"LP12"},{1,"LP24"},{2,"HP12"},{3,"BP12"},{4,"Diode"}};
        case 31: return {{1,"Up / down"},{2,"Up"},{3,"Down"},{4,"Random"}};
        case 32: return {{0,"1 oct"},{1,"2 oct"},{2,"3 oct"},{3,"4 oct"}};
        case 33: return {{0,"1/1"},{1,"1/2+1/4+1/8"},{2,"1/2+1/4"},{3,"1/2"},{4,"1/4+1/8+1/16"},
                         {5,"1/4+1/8"},{6,"1/1 triplet"},{7,"1/4"},{8,"1/8+1/16+1/32"},{9,"1/8+1/16"},
                         {10,"1/2 triplet"},{11,"1/8"},{12,"1/16+1/32"},{13,"1/4 triplet"},{14,"1/16"},
                         {15,"1/8 triplet"},{16,"1/32"},{17,"1/16 triplet"},{18,"1/32 triplet"}};
        case 38: return {{0,"Poly"},{1,"Mono"},{2,"Legato"}};
        case 41: case 46: return {{1,"OSC2"},{2,"OSC1 + 2"},{3,"Filter"},{4,"Amp"},{5,"Width"},{6,"FM"},{7,"Pan"}};
        case 42: case 47: return {{0,"Saw"},{1,"Triangle"},{2,"Square"},{3,"Random S&H"},{4,"Random smooth"},{5,"Sine"}};
        case 64: return {{1,"1x"},{2,"2x"},{4,"4x"}};
        case 71: return {{0,"OSC2"},{1,"FM"},{2,"Width"}};
        case 78: return {{0,"Overdrive 1"},{1,"Overdrive 2"},{2,"Decimator drive"},{3,"Decimator"},{4,"Ring mod"},
                         {5,"Compressor"},{6,"Phaser 1"},{7,"Phaser 2"},{8,"Phaser 3"},{9,"Phaser 4"}};
        case 82: return {{0,"Stereo"},{1,"Cross"},{2,"Ping-pong"}};
        case 96: return {{0,"Sine"},{1,"Triangle"},{2,"Saw"},{3,"Pulse"}};
        case 97: return {{0,"Same"},{1,"-1 oct"}};
        default: return {};
    }
}

inline const std::vector<EditorPanel>& editorPanels()
{
    static const std::vector<EditorPanel> panels {
        {"OSCILLATORS","OSC1 + OSC2  /  FM, Ring & Sub","#ded7cc",{20,124,500,260},6,
            {{0,"Wave 1"},{76,"Detune 1"},{45,"FM"},{1,"Wave 2"},{2,"Pitch 2"},{3,"Fine 2"},
             {4,"Track"},{6,"Sync"},{7,"Ring"},{5,"Mix"},{8,"Width"},{9,"Key shift"},
             {10,"Mod env"},{71,"Env to"},{11,"Amount"},{12,"Attack"},{13,"Decay"},{72,"Fine"},
             {95,"Sub level"},{96,"Sub wave"},{97,"Sub octave"},{91,"Phase"}}},
        {"FILTER","Tone & filter envelope","#ed8b88",{532,124,356,260},4,
            {{14,"Type"},{19,"Cutoff"},{20,"Resonance"},{23,"Drive"},{15,"Attack"},{16,"Decay"},
             {17,"Sustain"},{18,"Release"},{21,"Env depth"},{22,"Key track"},{24,"Velocity"}}},
        {"AMPLIFIER","Level & amplitude envelope","#ded7cc",{900,124,360,260},4,
            {{25,"Attack"},{26,"Decay"},{27,"Sustain"},{28,"Release"},{29,"Level"},{30,"Velocity"}}},
        {"LFO 1","Independent modulation","#ded7cc",{20,396,244,204},3,
            {{57,"Enabled"},{42,"Wave"},{41,"To"},{43,"Rate"},{44,"Amount"},{67,"Tempo"},{68,"Key sync"}}},
        {"LFO 2","Independent modulation","#ded7cc",{276,396,244,204},3,
            {{58,"Enabled"},{47,"Wave"},{46,"To"},{48,"Rate"},{49,"Amount"},{69,"Tempo"},{70,"Key sync"}}},
        {"VOICE","Play mode, glide & Unison","#ded7cc",{532,396,356,204},4,
            {{38,"Mode"},{39,"Glide"},{74,"Auto glide"},{40,"Bend range"},{73,"Unison"},{93,"Voices"},
             {75,"Detune"},{84,"Spread"},{85,"Pitch"},{92,"Phase"},{94,"Polyphony"}}},
        {"ARPEGGIATOR","Tempo-based note motion","#ded7cc",{900,396,360,204},3,
            {{59,"Enabled"},{31,"Pattern"},{32,"Range"},{33,"Rate"},{34,"Gate"}}},
        {"EFFECT","Drive & character","#ded7cc",{20,612,244,178},3,
            {{77,"Enabled"},{78,"Type"},{81,"Output"},{79,"Control 1"},{80,"Control 2"}}},
        {"CHORUS","Width & movement","#ded7cc",{276,612,244,178},4,
            {{66,"Enabled"},{64,"Voices"},{52,"Time"},{53,"Depth"},{54,"Rate"},{55,"Feedback"},{56,"Level"}}},
        {"DELAY","Space & repeats","#ded7cc",{532,612,356,178},4,
            {{65,"Enabled"},{82,"Type"},{35,"Time"},{36,"Feedback"},{37,"Mix"},{83,"Spread"},{98,"Tone"}}},
        {"EQ / PAN","Final tone & stereo placement","#ded7cc",{900,612,360,178},3,
            {{60,"Tone"},{61,"Frequency"},{62,"Level"},{63,"Q"},{90,"Pan"}}}
    };
    return panels;
}

inline EditorBounds editorControlBounds(const EditorPanel& panel,std::size_t cell)
{
    if (std::string(panel.title)=="FILTER")
    {
        const int x=panel.bounds.x,y=panel.bounds.y;
        const int parameter=panel.controls[cell].parameter;
        if (parameter==19) return {x+10,y+44,140,100};
        if (parameter==20) return {x+154,y+44,94,100};
        if (parameter==14) return {x+252,y+44,94,44};
        if (parameter==23) return {x+252,y+88,94,56};
        if (parameter>=15 && parameter<=18) return {x+10+(parameter-15)*84,y+144,84,50};
        const int column=parameter==21 ? 0 : parameter==22 ? 1 : 2;
        return {x+10+column*112,y+194,112,54};
    }
    if (std::string(panel.title)=="EFFECT")
    {
        const int x=panel.bounds.x+10,y=panel.bounds.y+44;
        const int width=(panel.bounds.width-20)/3,height=(panel.bounds.height-52)/2;
        if (cell==0) return {x,y,width,height};
        if (cell==1) return {x+width,y,width*2,height};
        return {x+(cell==2 ? 2 : static_cast<int>(cell)-3)*width,y+height,width,height};
    }
    if (std::string(panel.title)=="LFO 1" || std::string(panel.title)=="LFO 2")
    {
        const int x=panel.bounds.x+10,y=panel.bounds.y+44;
        const int inner=panel.bounds.width-20;
        if (cell==0 || cell==5 || cell==6)
        {
            const int column=cell==0 ? 0 : static_cast<int>(cell)-4;
            return {x+column*(inner/3),y,inner/3,44};
        }
        const bool knob=cell==3 || cell==4;
        const int column=knob ? static_cast<int>(cell)-3 : static_cast<int>(cell)-1;
        return {x+column*(inner/2),y+(knob ? 88 : 44),inner/2,knob ? 64 : 44};
    }
    const int rows=(static_cast<int>(panel.controls.size())+panel.columns-1)/panel.columns;
    const int width=(panel.bounds.width-20)/panel.columns;
    const int height=(panel.bounds.height-52)/rows;
    return {panel.bounds.x+10+static_cast<int>(cell%panel.columns)*width,
            panel.bounds.y+44+static_cast<int>(cell/panel.columns)*height,width,height};
}
}
