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
        case 35: return {{1,"1/64 triplet"},{2,"1/32 triplet"},{3,"1/32"},{4,"1/16 triplet"},
                         {5,"1/16"},{6,"1/8 triplet"},{7,"1/16 dotted"},{8,"1/8"},
                         {9,"1/4 triplet"},{10,"1/8 dotted"},{11,"1/8+1/16+1/32"},{12,"1/4"},
                         {13,"1/2 triplet"},{14,"1/4 dotted"},{15,"1/4+1/8+1/16"},{16,"1/2"},
                         {17,"1/2 dotted"},{18,"1/2+1/4+1/8"},{19,"1/1"}};
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
        {"OSCILLATORS","OSC1 + OSC2  /  FM, Ring & Sub","#d9aaa2",{20,124,500,264},6,
            {{0,"Wave 1"},{76,"Detune 1"},{45,"FM"},{95,"Sub level"},{96,"Sub wave"},{97,"Sub octave"},
             {1,"Wave 2"},{2,"Pitch 2"},{3,"Fine 2"},{4,"Track"},{6,"Sync"},{7,"Ring"},
             {10,"Mod env"},{71,"Env to"},{12,"Attack"},{13,"Decay"},{11,"Amount"},
             {5,"Mix"},{8,"Width"},{9,"Key shift"},{72,"Fine"},{91,"Phase"}}},
        {"FILTER","Tone & filter envelope","#ed8b88",{532,124,356,264},4,
            {{14,"Type"},{19,"Cutoff"},{20,"Resonance"},{23,"Drive"},{15,"Attack"},{16,"Decay"},
             {17,"Sustain"},{18,"Release"},{21,"Env depth"},{22,"Key track"},{24,"Velocity"}}},
        {"AMPLIFIER","Level & amplitude envelope","#d9aaa2",{900,124,360,264},4,
            {{25,"Attack"},{26,"Decay"},{27,"Sustain"},{28,"Release"},{29,"Level"},{30,"Velocity"}}},
        {"LFO 1","Independent modulation","#d9aaa2",{20,396,244,208},3,
            {{57,"Enabled"},{42,"Wave"},{41,"To"},{43,"Rate"},{44,"Amount"},{67,"Tempo"},{68,"Key sync"}}},
        {"LFO 2","Independent modulation","#d9aaa2",{276,396,244,208},3,
            {{58,"Enabled"},{47,"Wave"},{46,"To"},{48,"Rate"},{49,"Amount"},{69,"Tempo"},{70,"Key sync"}}},
        {"VOICE","Play mode, glide & Unison","#d9aaa2",{532,396,356,208},4,
            {{38,"Mode"},{39,"Glide"},{74,"Auto glide"},{40,"Bend range"},{94,"Polyphony"},
             {73,"Unison"},{93,"Voices"},{75,"Detune"},{84,"Spread"},{85,"Pitch"},{92,"Phase"}}},
        {"ARPEGGIATOR","Tempo-based note motion","#d9aaa2",{900,396,360,208},3,
            {{59,"Enabled"},{31,"Pattern"},{32,"Range"},{33,"Rate"},{34,"Gate"}}},
        {"EFFECT","Drive & character","#d9aaa2",{20,612,244,178},3,
            {{77,"Enabled"},{78,"Type"},{81,"Output"},{79,"Control 1"},{80,"Control 2"}}},
        {"CHORUS","Width & movement","#d9aaa2",{276,612,244,178},4,
            {{66,"Enabled"},{64,"Voices"},{52,"Time"},{53,"Depth"},{54,"Rate"},{55,"Feedback"},{56,"Level"}}},
        {"DELAY","Space & repeats","#d9aaa2",{532,612,356,178},4,
            {{65,"Enabled"},{82,"Type"},{35,"Time"},{36,"Feedback"},{37,"Mix"},{83,"Spread"},{98,"Tone"}}},
        {"EQ / PAN","Final tone & stereo placement","#d9aaa2",{900,612,360,178},3,
            {{61,"Frequency"},{62,"Level"},{63,"Q"},{60,"Tone"},{90,"Pan"}}}
    };
    return panels;
}

inline EditorBounds editorControlBounds(const EditorPanel& panel,std::size_t cell)
{
    if (std::string(panel.title)=="OSCILLATORS")
    {
        // OSC1/Sub, OSC2, Mod Env, then shared mix/tuning controls.
        const int row=cell<12 ? static_cast<int>(cell)/6 : cell<17 ? 2 : 3;
        const int columns=row<2 ? 6 : 5;
        const int column=static_cast<int>(cell)-(row<2 ? row*6 : row==2 ? 12 : 17);
        const int width=(panel.bounds.width-16)/columns,height=(panel.bounds.height-44)/4;
        return {panel.bounds.x+8+column*width,panel.bounds.y+38+row*height,width,height};
    }
    if (std::string(panel.title)=="AMPLIFIER")
    {
        const int width=(panel.bounds.width-16)/(cell<4 ? 4 : 2);
        return {panel.bounds.x+8+static_cast<int>(cell<4 ? cell : cell-4)*width,
                panel.bounds.y+(cell<4 ? 38 : 154),width,100};
    }
    if (std::string(panel.title)=="VOICE")
    {
        // Play mode/glide on the left; all Unison controls on the right.
        const int x=panel.bounds.x+8,y=panel.bounds.y+38;
        const int width=(panel.bounds.width-16)/4,height=(panel.bounds.height-44)/3;
        if(cell==0)return {x,y,width*2,height};
        const int column=cell<5 ? static_cast<int>(cell-1)%2 : 2+static_cast<int>(cell-5)%2;
        const int row=cell<5 ? 1+static_cast<int>(cell-1)/2 : static_cast<int>(cell-5)/2;
        return {x+column*width,y+row*height,width,height};
    }
    if (std::string(panel.title)=="ARPEGGIATOR")
    {
        const int x=panel.bounds.x+8,y=panel.bounds.y+38;
        const int width=(panel.bounds.width-16)/3,height=(panel.bounds.height-44)/2;
        if(cell==0)return {x,y,width,height};
        if(cell==1)return {x+width,y,width*2,height};
        return {x+static_cast<int>(cell-2)*width,y+height,width,height};
    }
    if (std::string(panel.title)=="EQ / PAN" && cell>=3)
    {
        const int width=(panel.bounds.width-16)/2,height=(panel.bounds.height-44)/2;
        return {panel.bounds.x+8+static_cast<int>(cell-3)*width,panel.bounds.y+38+height,width,height};
    }
    if (std::string(panel.title)=="FILTER")
    {
        const int x=panel.bounds.x,y=panel.bounds.y;
        const int parameter=panel.controls[cell].parameter;
        if (parameter==19) return {x+8,y+38,144,98};
        if (parameter==20) return {x+156,y+38,96,98};
        if (parameter==14) return {x+256,y+38,92,44};
        if (parameter==23) return {x+256,y+82,92,54};
        if (parameter>=15 && parameter<=18) return {x+8+(parameter-15)*85,y+136,85,58};
        const int column=parameter==21 ? 0 : parameter==22 ? 1 : 2;
        return {x+8+column*113,y+194,113,60};
    }
    if (std::string(panel.title)=="EFFECT")
    {
        const int x=panel.bounds.x+8,y=panel.bounds.y+38;
        const int width=(panel.bounds.width-16)/3,height=(panel.bounds.height-44)/2;
        if (cell==0) return {x,y,width,height};
        if (cell==1) return {x+width,y,width*2,height};
        return {x+(cell==2 ? 2 : static_cast<int>(cell)-3)*width,y+height,width,height};
    }
    if (std::string(panel.title)=="LFO 1" || std::string(panel.title)=="LFO 2")
    {
        const int x=panel.bounds.x+8,y=panel.bounds.y+38;
        const int inner=panel.bounds.width-16;
        if (cell==0 || cell==5 || cell==6)
        {
            const int column=cell==0 ? 0 : static_cast<int>(cell)-4;
            return {x+column*(inner/3),y,inner/3,44};
        }
        const bool knob=cell==3 || cell==4;
        const int column=knob ? static_cast<int>(cell)-3 : static_cast<int>(cell)-1;
        return {x+column*(inner/2),y+(knob ? 88 : 44),inner/2,knob ? 74 : 44};
    }
    if(std::string(panel.title)=="CHORUS" && cell>=4)
    {
        const int width=(panel.bounds.width-16)/3,height=(panel.bounds.height-44)/2;
        return {panel.bounds.x+8+static_cast<int>(cell-4)*width,panel.bounds.y+38+height,width,height};
    }
    const int rows=(static_cast<int>(panel.controls.size())+panel.columns-1)/panel.columns;
    const int width=(panel.bounds.width-16)/panel.columns;
    const int height=(panel.bounds.height-44)/rows;
    // Reserve the cell below Time for tempo and actual stereo delay times.
    if(std::string(panel.title)=="DELAY" && panel.controls[cell].parameter==98)
        return {panel.bounds.x+8+3*width,panel.bounds.y+38+height,width,height};
    return {panel.bounds.x+8+static_cast<int>(cell%panel.columns)*width,
            panel.bounds.y+38+static_cast<int>(cell/panel.columns)*height,width,height};
}
inline std::vector<EditorBounds> editorPanelDividers(const EditorPanel& panel)
{
    const int x=panel.bounds.x,y=panel.bounds.y;
    if(std::string(panel.title)=="OSCILLATORS")
    {
        const int width=(panel.bounds.width-16)/6,height=(panel.bounds.height-44)/4;
        return {{x+8+3*width,y+40,1,height-4},{x+8+3*width,y+40+height,1,height-4}};
    }
    if(std::string(panel.title)=="VOICE")
        return {{x+panel.bounds.width/2,y+38,1,panel.bounds.height-46}};
    if(std::string(panel.title)=="AMPLIFIER")
        return {{x+12,y+144,panel.bounds.width-24,1}};
    return {};
}
inline constexpr EditorBounds editorDelayTimingBounds {718,722,77,51};
}
