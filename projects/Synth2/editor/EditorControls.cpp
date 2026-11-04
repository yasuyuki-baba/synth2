#include "../Synth2.h"
#include "EditorControls.h"
#include "EditorControlTags.h"
#include "EditorStyle.h"
#include "editor/EditorInteraction.h"
#include "editor/EditorValueDisplay.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace synth2::editor::native
{
HeaderControl::HeaderControl() : IControl(IRECT(0,0,1280,118)) { mIgnoreMouse=true; }

void HeaderControl::Draw(IGraphics& g) {
    g.FillRect(color(appearance.header),mRECT);
    g.FillRect(color(appearance.headerEdge),IRECT(0,0,1280,4));
    g.DrawLine(color(appearance.headerEdge),0,117,1280,117);
}

SectionControl::SectionControl(const synth2::editor::EditorPanel& section)
    : IControl(rect(section.bounds)), section(section) { mIgnoreMouse=true; }

void SectionControl::Draw(IGraphics& g) {
    const auto surface=std::string(section.title)=="FILTER" ? color(appearance.filterPanel) : panel;
    g.FillRoundRect(surface,mRECT,3.f);
    g.DrawRoundRect(color(appearance.border),mRECT,3.f,nullptr,0.5f);
    const auto heading=IRECT(mRECT.L+14,mRECT.T+9,mRECT.R-12,mRECT.T+25);
    g.DrawText(IText(13.f,color(section.accent),"Synth2UIBold",EAlign::Near),section.title,heading);
    g.DrawLine(color(appearance.border),heading.L,mRECT.T+32,heading.R,mRECT.T+32,nullptr,0.5f);
}

PanelKnobControl::PanelKnobControl(const IRECT& bounds,int parameter,const char* label,const IVStyle& style)
    : IVKnobControl(bounds,parameter,label,style,true,false) {
    mTrackToHandleDistance=2.5f;
    SetTooltip("Drag to adjust. Click value to edit the original source number.");
}

void PanelKnobControl::OnResize() {
    IVKnobControl::OnResize();
    const float diameter=static_cast<float>(synth2::editor::editorKnobDiameter(GetParamIdx(),static_cast<int>(mRECT.H())));
    mLabelBounds=IRECT(mRECT.L,mRECT.T,mRECT.R,mRECT.T+13);
    mWidgetBounds=IRECT(mRECT.MW()-diameter/2,mRECT.T+15,mRECT.MW()+diameter/2,mRECT.T+15+diameter);
    mValueBounds=IRECT(mRECT.L,mRECT.T+16+diameter,mRECT.R,mRECT.T+28+diameter);
}

void PanelKnobControl::DrawValue(IGraphics& g,bool mouseOver) {
    if (mouseOver) g.FillRect(COLOR_TRANSLUCENT,mValueBounds);
    if (!mStyle.showValue) return;
    const auto value=synth2::editor::editorValueDisplay(GetParamIdx(),GetParam()->FromNormalized(GetValue()));
    const auto blend=GetBlend();
    g.DrawText(mStyle.valueText,value.c_str(),mValueBounds,&blend);
}

void PanelKnobControl::DrawHandle(IGraphics& g,const IRECT& bounds) {
    g.FillCircle(color(appearance.control),bounds.MW(),bounds.MH(),bounds.W()/2);
    g.DrawCircle(color(appearance.controlEdge),bounds.MW(),bounds.MH(),bounds.W()/2,nullptr,1.f);
    g.DrawCircle(IColor(255,40,39,41),bounds.MW(),bounds.MH(),std::max(1.f,bounds.W()/2-2),nullptr,0.5f);
}

void PanelKnobControl::DrawPointer(IGraphics& g,float angle,float cx,float cy,float radius) {
    g.DrawRadialLine(color(appearance.pointer),cx,cy,angle,radius*0.56f,radius*0.96f,nullptr,1.8f);
}

void PanelKnobControl::DrawIndicatorTrack(IGraphics& g,float angle,float cx,float cy,float radius) {
    g.DrawArc(color(appearance.track),cx,cy,radius,-135.f,135.f,nullptr,1.f);
    g.DrawArc(GetParamIdx()==19 ? accent : color(appearance.pointer),cx,cy,radius,-135.f,angle,nullptr,1.25f);
}

SourceChoiceControl::SourceChoiceControl(const IRECT& bounds,int parameter,const char* label)
    : IControl(bounds,parameter), label(label), toggle(synth2::editor::editorToggle(parameter)),
      choices(synth2::editor::editorChoices(parameter)) {}

void SourceChoiceControl::Draw(IGraphics& g) {
    const auto raw=GetParam()->FromNormalized(GetValue());
    const auto value=synth2::editor::editorChoiceDisplay(toggle,raw,choices);
    g.DrawText(IText(11.f,muted,"Synth2UI"),label,
               IRECT(mRECT.L,mRECT.T,mRECT.R,mRECT.T+16));
    const auto box=IRECT(mRECT.L+4,mRECT.T+19,mRECT.R-4,std::min(mRECT.B-6,mRECT.T+46));
    g.FillRoundRect(color(appearance.control),box,3.f);
    g.DrawRoundRect(color(appearance.controlEdge),box,3.f,nullptr,0.5f);
    const auto textColor=ink;
    if (toggle)
        g.FillCircle(raw!=0 ? accent : color(appearance.track),box.L+8,box.MH(),2.f);
    const float size=std::clamp(box.W()/(std::max<std::size_t>(value.size(),1)*0.6f),9.5f,12.f);
    g.DrawText(IText(size,textColor,"Synth2UI"),value.c_str(),box.GetPadded(-3.f));
}

void SourceChoiceControl::OnMouseDown(float,float,const IMouseMod&) {
    if (toggle)
        SetValueFromUserInput(GetParam()->ToNormalized(synth2::editor::editorToggledSourceValue(GetParam()->FromNormalized(GetValue()))));
    else
    {
        menu.Clear();
        for (const auto& choice:choices) menu.AddItem(choice.label);
        GetUI()->CreatePopupMenu(*this,menu,mRECT);
    }
}

void SourceChoiceControl::OnPopupMenuSelection(IPopupMenu* selection,int) {
    if (!selection) return;
    const int index=selection->GetChosenItemIdx();
    if (const auto source=synth2::editor::editorSelectedSourceValue(choices,index))
        SetValueFromUserInput(GetParam()->ToNormalized(*source));
}

CatalogListControl::CatalogListControl(const IRECT& bounds,const synth2::host::PatchCatalog& catalog,
                   const std::string& selected,
                   std::function<void(std::size_t)> load,
                   std::function<void(const std::string&,bool)> favorite,
                   std::function<void(const std::string&)> remove,
                   std::function<void(int)> scroll,
                   std::function<void(const char*)> import)
    : IControl(bounds),catalog(catalog),selected(selected),load(std::move(load)),
      favorite(std::move(favorite)),remove(std::move(remove)),scroll(std::move(scroll)),import(std::move(import)) {
    SetTooltip("Click a sound to audition. Click + / - to favorite. Scroll to change pages.");
}

void CatalogListControl::setResults(std::vector<std::size_t> results,std::size_t start) {
    matches=std::move(results);offset=start;SetDirty(false);
}

void CatalogListControl::Draw(IGraphics& g) {
    g.FillRect(panel,mRECT);
    if(matches.empty()) {
        g.DrawText(IText(15.f,muted,"Synth2UI"),"No matching sounds. Clear a filter or add a sound bank.",mRECT);
        return;
    }
    for(std::size_t row=0;row<visibleRows && offset+row<matches.size();++row) {
        const auto& item=catalog.patches()[matches[offset+row]];
        const auto y=mRECT.T+row*40.f;
        const IRECT bounds(mRECT.L,y,mRECT.R,y+40.f);
        if(item.id==selected)g.FillRect(color(appearance.display),bounds);
        else if(row%2==1)g.FillRect(background,bounds);
        const IText text(13.f,ink,"Synth2UI",EAlign::Near);
        g.DrawText(IText(16.f,item.favorite ? accent : muted,"Synth2UI"),item.favorite ? "+" : "-",IRECT(mRECT.L,y,mRECT.L+32,y+40));
        g.DrawText(text,item.patch.name.c_str(),IRECT(mRECT.L+38,y,mRECT.L+565,y+40));
        g.DrawText(text.WithSize(11.f).WithFGColor(muted),item.bankName.c_str(),IRECT(mRECT.L+580,y,mRECT.L+760,y+40));
        const auto tags=synth2::host::PatchCatalog::tagText(item.tags);
        g.DrawText(text.WithSize(11.f).WithFGColor(muted),tags.c_str(),IRECT(mRECT.L+780,y,mRECT.R-86,y+40));
        g.DrawText(text.WithSize(11.f).WithFGColor(muted),"Remove",IRECT(mRECT.R-70,y,mRECT.R-8,y+40));
    }
}

void CatalogListControl::OnMouseDown(float x,float y,const IMouseMod&) {
    const auto row=static_cast<std::size_t>((y-mRECT.T)/40.f);
    if(row>=visibleRows || offset+row>=matches.size())return;
    const auto index=matches[offset+row];const auto item=catalog.patches()[index];
    if(x<mRECT.L+32)favorite(item.id,!item.favorite);
    else if(x>mRECT.R-80)remove(item.id);
    else load(index);
}

void CatalogListControl::OnMouseWheel(float,float,const IMouseMod&,float delta) {if(delta!=0)scroll(delta>0 ? -1 : 1);}

void CatalogListControl::OnDrop(const char* path) {import(path);}

void CatalogListControl::OnDropMultiple(const std::vector<const char*>& paths) {for(const auto* path:paths)import(path);}

SearchControl::SearchControl(const IRECT& bounds, const char* text, const IText& style,
              std::function<void(const char*)> change)
    : IEditableTextControl(bounds,text,style,panel), onChange(std::move(change)) {}

void SearchControl::OnTextEntryCompletion(const char* text, int value) {
    IEditableTextControl::OnTextEntryCompletion(text,value);
    onChange(text);
}
}
