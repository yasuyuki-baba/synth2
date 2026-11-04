#include "host/EditorLayout.h"
#include "host/EditorPages.h"
#include "host/EditorAppearance.h"
#include "host/EditorValueDisplay.h"
#include "host/ParameterPresentation.h"
#include "host/ProcessorBridge.h"
#include "patch/Synth1SourceRange.h"
#include <iomanip>
#include <iostream>
#include <cmath>

// SDK-independent manifest for the interactive layout preview. The native
// editor and preview consume the same panel geometry and source-choice values.
int main()
{
    synth2::host::ProcessorBridge processor;
    const auto& a=synth2::host::editorAppearance();
    std::cout << "{\"width\":1280,\"height\":920,\"appearance\":{";
    bool firstColor=true;
    for (const auto& color:std::initializer_list<std::pair<const char*,const char*>>{
        {"background",a.background},{"header",a.header},{"headerEdge",a.headerEdge},{"panel",a.panel},
        {"filterPanel",a.filterPanel},{"border",a.border},{"text",a.text},{"muted",a.muted},{"accent",a.accent},
        {"display",a.display},{"displayText",a.displayText},{"control",a.control},{"controlEdge",a.controlEdge},
        {"track",a.track},{"pointer",a.pointer}})
    {
        if (!firstColor) std::cout << ','; firstColor=false;
        std::cout << std::quoted(color.first) << ':' << std::quoted(color.second);
    }
    std::cout << "},\"panels\":[";
    bool firstPanel=true;
    for (const auto& panel:synth2::host::editorPanels())
    {
        if (!firstPanel) std::cout << ','; firstPanel=false;
        std::cout << "{\"title\":" << std::quoted(panel.title) << ",\"description\":" << std::quoted(panel.description)
                  << ",\"accent\":" << std::quoted(panel.accent) << ",\"bounds\":[" << panel.bounds.x << ','
                  << panel.bounds.y << ',' << panel.bounds.width << ',' << panel.bounds.height << "],\"controls\":[";
        bool first=true;
        for (std::size_t cell=0;cell<panel.controls.size();++cell)
        {
            if (!first) std::cout << ','; first=false;
            const auto& control=panel.controls[cell];
            const auto b=synth2::host::editorControlBounds(panel,cell);
            std::cout << "{\"parameter\":" << control.parameter << ",\"label\":" << std::quoted(control.label)
                      << ",\"bounds\":[" << b.x << ',' << b.y << ',' << b.width << ',' << b.height << "]"
                      << ",\"diameter\":" << synth2::host::editorKnobDiameter(control.parameter,b.height)
                      << ",\"tone\":" << std::quoted(control.parameter==19 ? a.accent : a.pointer) << '}';
        }
        std::cout << "]}";
    }
    std::cout << "],\"parameters\":[";
    for (int index=0;index<128;++index)
    {
        if (index) std::cout << ',';
        const auto& info=processor.parameter(index);
        const auto initial=std::lround(info.minimum+processor.normalizedValue(index)*(info.maximum-info.minimum));
        std::cout << "{\"minimum\":" << synth2::sourceMinimum(index) << ",\"maximum\":" << synth2::sourceMaximum(index)
                  << ",\"initial\":" << initial
                  << ",\"integerSteps\":" << (synth2::host::parameterUsesIntegerSteps(index) ? "true" : "false")
                  << ",\"toggle\":" << (synth2::host::editorToggle(index) ? "true" : "false") << ",\"choices\":[";
        bool first=true;
        for (const auto& choice:synth2::host::editorChoices(index))
        {
            if (!first) std::cout << ','; first=false;
            std::cout << '[' << choice.value << ',' << std::quoted(choice.label) << ']';
        }
        std::cout << "],\"unitDisplay\":[";
        if (synth2::host::editorHasUnit(index))
            for (int raw=0;raw<128;++raw)
            {
                if (raw) std::cout << ',';
                std::cout << std::quoted(synth2::host::editorValueDisplay(index,raw));
            }
        std::cout << "]}";
    }
    std::cout << "],\"supplements\":[";
    for (std::size_t i=5;i<synth2::host::editorPages().size();++i)
    {
        if (i!=5) std::cout << ',';
        const auto& page=synth2::host::editorPages()[i];
        std::cout << "{\"name\":" << std::quoted(page.name) << ",\"parameters\":[";
        bool first=true;
        for (int parameter:page.parameters) { if (!first) std::cout << ','; first=false; std::cout << parameter; }
        std::cout << "]}";
    }
    std::cout << "]}\n";
}
