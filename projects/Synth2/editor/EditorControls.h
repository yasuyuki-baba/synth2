#pragma once
#include "IControls.h"
#include "editor/EditorLayout.h"
#include "host/PatchCatalog.h"
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace synth2::editor::native
{
using namespace iplug;
using namespace iplug::igraphics;

// Compose the SDK momentary-button lifecycle with application callbacks.
class ActionButtonControl final : public IVButtonControl
{
public:
    ActionButtonControl(const IRECT& bounds, IActionFunction action, const char* label,
                        const IVStyle& style);
};

class HeaderControl final : public IControl
{
public:
    HeaderControl();
    void Draw(IGraphics& graphics) override;
};

class SectionControl final : public IControl
{
public:
    explicit SectionControl(const EditorPanel& section);
    void Draw(IGraphics& graphics) override;
private:
    const EditorPanel& section;
};

class PanelKnobControl final : public IVKnobControl
{
public:
    PanelKnobControl(const IRECT& bounds,int parameter,const char* label,const IVStyle& style);
    void OnResize() override;
    void DrawValue(IGraphics& graphics,bool mouseOver) override;
    void DrawHandle(IGraphics& graphics,const IRECT& bounds) override;
    void DrawPointer(IGraphics& graphics,float angle,float cx,float cy,float radius) override;
    void DrawIndicatorTrack(IGraphics& graphics,float angle,float cx,float cy,float radius) override;
};

// Selectors retain original source words and their host automation ranges.
class SourceChoiceControl final : public IControl
{
public:
    SourceChoiceControl(const IRECT& bounds,int parameter,const char* label);
    void Draw(IGraphics& graphics) override;
    void OnMouseDown(float x,float y,const IMouseMod& modifiers) override;
    void OnPopupMenuSelection(IPopupMenu* selection,int value) override;
private:
    const char* label;
    bool toggle;
    std::vector<EditorChoice> choices;
    IPopupMenu menu;
};

// Borrow the catalog/selection; only the visible result page is drawn.
class CatalogListControl final : public IControl
{
public:
    CatalogListControl(const IRECT& bounds,const host::PatchCatalog& catalog,
        const std::string& selected,std::function<void(std::size_t)> load,
        std::function<void(const std::string&,bool)> favorite,
        std::function<void(int)> scroll,std::function<void(const char*)> import);
    void setResults(std::vector<std::size_t> results,std::size_t start);
    void Draw(IGraphics& graphics) override;
    void OnMouseDown(float x,float y,const IMouseMod& modifiers) override;
    void OnMouseWheel(float x,float y,const IMouseMod& modifiers,float delta) override;
    void OnDrop(const char* path) override;
    void OnDropMultiple(const std::vector<const char*>& paths) override;
private:
    const host::PatchCatalog& catalog;
    const std::string& selected;
    std::vector<std::size_t> matches;
    std::size_t offset=0;
    std::function<void(std::size_t)> load;
    std::function<void(const std::string&,bool)> favorite;
    std::function<void(int)> scroll;
    std::function<void(const char*)> import;
};

class SearchControl final : public IEditableTextControl
{
public:
    SearchControl(const IRECT& bounds,const char* text,const IText& style,
                  std::function<void(const char*)> change);
    void OnTextEntryCompletion(const char* text,int value) override;
private:
    std::function<void(const char*)> onChange;
};
}
