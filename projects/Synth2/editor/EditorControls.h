#pragma once
#include "IControls.h"
#include "editor/EditorLayout.h"
#include "editor/EditorPresetDisplay.h"
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
class ActionButtonControl : public IVButtonControl
{
public:
    ActionButtonControl(const IRECT& bounds, IActionFunction action, const char* label,
                        const IVStyle& style);
};

// A single clickable display with independent title and bank lines.
class PresetDisplayControl final : public ActionButtonControl
{
public:
    using ActionButtonControl::ActionButtonControl;
    void setDisplay(const synth2::editor::EditorPresetDisplay& display);
    void Draw(IGraphics& graphics) override;
private:
    synth2::editor::EditorPresetDisplay display;
};

// Covers the complete editor after the optional keyboard changes its height.
class BackgroundControl final : public IPanelControl
{
public:
    explicit BackgroundControl(const IRECT& bounds);
    void OnResize() override;
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

// Borrow the catalog/selection; draw only rows intersecting the scroll viewport.
class CatalogListControl final : public IControl
{
public:
    CatalogListControl(const IRECT& bounds,const host::PatchCatalog& catalog,
        const std::string& selected,const std::string& keyboardSelection,std::function<void(std::size_t)> load,
        std::function<void(const std::string&,bool)> favorite,
        std::function<void(double)> scroll,std::function<void(const char*)> import);
    void setResults(std::vector<std::size_t> results,double start);
    void Draw(IGraphics& graphics) override;
    void OnMouseDown(float x,float y,const IMouseMod& modifiers) override;
    void OnMouseDrag(float x,float y,float dX,float dY,const IMouseMod& modifiers) override;
    void OnMouseUp(float x,float y,const IMouseMod& modifiers) override;
    void OnMouseWheel(float x,float y,const IMouseMod& modifiers,float delta) override;
    void OnMouseOver(float x,float y,const IMouseMod& modifiers) override;
    void OnDrop(const char* path) override;
    void OnDropMultiple(const std::vector<const char*>& paths) override;
private:
    const host::PatchCatalog& catalog;
    const std::string& selected;
    const std::string& keyboardSelection;
    std::vector<std::size_t> matches;
    double offset=0,drawOffset=0,animationStart=0;
    bool draggingScroll=false;
    float scrollGrab=0;
    IRECT scrollTrack() const;
    IRECT scrollThumb() const;
    void dragScroll(float y);
    std::function<void(std::size_t)> load;
    std::function<void(const std::string&,bool)> favorite;
    std::function<void(double)> scroll;
    std::function<void(const char*)> import;
};

class SearchControl final : public IEditableTextControl
{
public:
    SearchControl(const IRECT& bounds,const char* text,const IText& style,
                  std::function<void(const char*)> change,const char* placeholder="");
    void Draw(IGraphics& graphics) override;
    void OnTextEntryCompletion(const char* text,int value) override;
private:
    std::function<void(const char*)> onChange;
    std::string placeholder;
};

// Click the value to type a percentage; the arrow opens the preset menu.
class DisplayScaleControl final : public IControl
{
public:
    DisplayScaleControl(const IRECT& bounds,std::function<void(float)> change);
    void Draw(IGraphics& graphics) override;
    void OnMouseDown(float x,float y,const IMouseMod& mod) override;
    void OnMouseDblClick(float x,float y,const IMouseMod& mod) override;
    void OnTextEntryCompletion(const char* text,int value) override;
    void OnPopupMenuSelection(IPopupMenu* selection,int value) override;
    void setScale(float scale);
private:
    IRECT valueBounds() const;
    std::function<void(float)> onChange;
    IPopupMenu menu;
    std::string text="100%";
    float scale=1.f;
};
}
