#pragma once
#include <algorithm>

namespace synth2::editor
{
// Shared by native iGraphics and both review artifacts. Red is concentrated in
// the header and filter; ordinary controls retain a quiet, warm neutral palette.
struct EditorAppearance
{
    const char* background="#19191c";
    const char* header="#762c39";
    const char* headerEdge="#aa4a59";
    const char* panel="#232226";
    const char* filterPanel="#3a272e";
    const char* border="#494044";
    const char* text="#f0ebe2";
    const char* muted="#b5aaa6";
    const char* accent="#ed8b88";
    const char* display="#15191a";
    const char* displayText="#e8d1a0";
    const char* control="#18191c";
    const char* controlEdge="#52494a";
    const char* track="#615456";
    const char* pointer="#e9e5dc";
};
inline const EditorAppearance& editorAppearance()
{
    static const EditorAppearance appearance;
    return appearance;
}
inline int editorKnobDiameter(int parameter,int height)
{
    const int limit=parameter==19 ? 66 : parameter==20 ? 48 : 34;
    return std::max(18,std::min(limit,height-28));
}
}
