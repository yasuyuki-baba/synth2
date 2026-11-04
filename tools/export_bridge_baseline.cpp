#include "host/ProcessorBridge.h"
#include "host/PatchLibrary.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

using synth2::host::ProcessorBridge;
namespace fs=std::filesystem;
int main(int argc,char** argv)
{
    if(argc!=3) { std::cerr<<"Usage: ExportBridgeBaseline OUTPUT_DIRECTORY BANK_FILE\n";return 2; }
    synth2::host::PatchLibrary bank;std::string error;
    if(!bank.openFile(argv[2],error)){std::cerr<<error;return 1;}
    const fs::path directory=argv[1];
    if(fs::exists(directory)){std::cerr<<"Refusing to replace baseline directory\n";return 2;}
    fs::create_directories(directory);
    ProcessorBridge init;
    std::ofstream metadata(directory/"parameters.tsv");metadata<<std::setprecision(17);
    for(int i=0;i<ProcessorBridge::parameterCount;++i)
    {
        const auto& p=init.parameter(i);
        metadata<<i<<'\t'<<p.id<<'\t'<<p.name<<'\t'<<p.minimum<<'\t'<<p.maximum<<'\t'<<p.defaultNormalized<<'\t'<<init.normalizedValue(i)<<'\n';
    }
    for(std::size_t index=0;index<bank.patches().size();++index)
    {
        const auto stem=std::to_string(index);ProcessorBridge processor;
        if(!processor.loadPatch(bank.patches()[index].text,error)){std::cerr<<error;return 1;}
        {std::ofstream f(directory/(stem+".sy1"),std::ios::binary);f<<bank.patches()[index].text;}
        {const auto state=processor.saveState();std::ofstream f(directory/(stem+".state"),std::ios::binary);f.write(reinterpret_cast<const char*>(state.data()),state.size());}
        std::ofstream controls(directory/(stem+".values"));controls<<std::setprecision(17);
        for(int p=0;p<ProcessorBridge::parameterCount;++p)controls<<processor.normalizedValue(p)<<'\n';
        processor.prepare(44100,64);
        std::ofstream audio(directory/(stem+".f32"),std::ios::binary);
        std::array<float,64> left{},right{};std::array<float,128> stereo{};
        for(int block=0;block<64;++block)
        {
            if(block==1 || block==40)
                for(int note:{60,64,67})processor.enqueueMidi(block==1 ? 13 : 29,block==1 ? 0x90 : 0x80,note,block==1 ? 95 : 0);
            if(!processor.process(left.data(),right.data(),64,120)){std::cerr<<"Render rejected";return 1;}
            for(int i=0;i<64;++i){stereo[i*2]=left[i];stereo[i*2+1]=right[i];}
            audio.write(reinterpret_cast<const char*>(stereo.data()),sizeof(stereo));
        }
        processor.release();
    }
    std::cout<<"Exported "<<bank.patches().size()<<" bank states/controls/stereo renders\n";
}
