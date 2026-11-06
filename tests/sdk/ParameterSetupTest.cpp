// Optional SDK check: exercises the actual iPlug2 parameter class without a GUI,
// plugin format or audio device. See tests/README.md for the build command.
#include "ParameterSetup.h"
#include <stdexcept>
#include <string>

using namespace synth2::host;
void require(bool condition) { if(!condition) throw std::runtime_error("Host parameter contract failed"); }
int main()
{
    iplug::IParam cutoff;
    initializeHostParameter(cutoff,19,{"p19","Cutoff",0,127,0.5});
    require(!cutoff.GetStepped() && cutoff.Type()==iplug::IParam::kTypeDouble);
    cutoff.Set(64.25); require(cutoff.Value()==64.25);

    iplug::IParam chorus;
    initializeHostParameter(chorus,64,{"p64","Voices",0,127,0});
    require(chorus.GetStepped() && chorus.GetStep()==1 && chorus.Type()==iplug::IParam::kTypeInt);
    require(chorus.GetMin()==0 && chorus.GetMax()==127);
    chorus.Set(3.7); require(chorus.Value()==4);
    require(std::string(chorus.GetDisplayText(4))=="4x");
    require(chorus.StringToValue("4x")==4);
    require(chorus.NDisplayTexts()==128);
    for(int i=0;i<128;++i) require(chorus.GetDisplayText(i)!=nullptr);
    chorus.Set(127); require(chorus.Value()==127); // Do not clamp to known choices.

    iplug::IParam source;
    initializeHostParameter(source,86,{"p86","MIDI source",-1,65535,0});
    source.Set(-1); require(source.Value()==-1);
    source.Set(45057); require(source.Value()==45057);
    source.Set(53247.7); require(source.Value()==53248);

    iplug::IParam toggle;
    initializeHostParameter(toggle,4,{"p4","Track",0,127,0});
    require(toggle.GetStepped() && toggle.GetMax()==127);
    require(std::string(toggle.GetDisplayText(0))=="OFF");
    require(std::string(toggle.GetDisplayText(1))=="ON");
    require(toggle.NDisplayTexts()==128);

    iplug::IParam shift;
    initializeHostParameter(shift,9,{"p9","Key shift",-24,24,0.5});
    shift.Set(-11.6); require(shift.Value()==-12);
    shift.Set(-11.5); require(shift.Value()==-12);

    iplug::IParam fine,sensitivity,destination;
    initializeHostParameter(fine,3,{"p3","Fine",0,127,.5});
    initializeHostParameter(sensitivity,50,{"p50","Sensitivity",0,127,.5});
    initializeHostParameter(destination,87,{"p87","Destination",0,127,0});
    fine.Set(67.125);require(!fine.GetStepped() && fine.Value()==67.125);
    sensitivity.Set(64.125);require(!sensitivity.GetStepped() && sensitivity.Value()==64.125);
    destination.Set(19.25);require(destination.GetStepped() && destination.Value()==19);

    iplug::IParam master;
    initializeHostParameter(master,128,{"masterVolume","Master Volume",0,1,1});
    master.Set(0.12345); require(!master.GetStepped() && master.Value()==0.12345);
}
