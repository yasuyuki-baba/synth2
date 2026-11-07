#include "editor/EditorLibraryScroll.h"
#include "../Synth2.h"
#include "EditorControls.h"
#include "EditorControlTags.h"
#include "EditorStyle.h"
#include "editor/EditorInteraction.h"
#include "editor/EditorValueDisplay.h"
#include "editor/EditorTextFit.h"
#include "editor/EditorLibraryColumns.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace synth2::editor::native
{
ActionButtonControl::ActionButtonControl(const IRECT& bounds, IActionFunction action,
                                         const char* label, const IVStyle& style)
    : IVButtonControl(bounds, [action=std::move(action)](IControl* caller) {
        // The base button sets its value to 1 on mouse-down and clears it only
        // at animation completion. A custom action must retain that lifecycle,
        // including when it opens a menu/dialog and no mouse-up reaches us.
        DefaultClickActionFunc(caller);
        if(action)action(caller);
    },label,style)
{
}

void PresetDisplayControl::setDisplay(const synth2::editor::EditorPresetDisplay& value)
{
    display=value;
    SetTooltip(display.tooltip().c_str());
    SetDirty(false);
}

void PresetDisplayControl::Draw(IGraphics& g)
{
    ActionButtonControl::Draw(g);
    const auto title=IText(16.f,color(appearance.displayText),"Synth2Mono",EAlign::Near);
    const auto fitted=[&](const IText& style,const std::string& value,const IRECT& bounds,bool keepEnd=false) {
        const auto measure=[&](std::string_view text) {
            IRECT measured;const std::string copy(text);g.MeasureText(style,copy.c_str(),measured);return measured.W();
        };
        const auto label=synth2::editor::editorFitText(value,bounds.W(),measure,keepEnd);
        g.DrawText(style,label.c_str(),bounds);
    };
    if(display.detail.empty() && display.tags.empty()) {
        fitted(title,display.title,IRECT(mRECT.L+10,mRECT.T,mRECT.R-10,mRECT.B));
        return;
    }
    fitted(title,display.title,IRECT(mRECT.L+10,mRECT.T+2,mRECT.R-10,mRECT.T+26));
    const auto detailText=IText(11.f,muted,"Synth2UI",EAlign::Near);
    if(!display.detail.empty())
        fitted(detailText,display.detail,IRECT(mRECT.L+10,mRECT.T+26,mRECT.R-10,
            display.tags.empty() ? mRECT.B-2 : mRECT.T+42),true);
    float x=mRECT.L+10;
    const float y=display.detail.empty() ? mRECT.T+26 : mRECT.T+42;
    for(const auto& tag:display.tags) {
        const auto style=detailText.WithFGColor(tag.automatic ? muted : ink);
        IRECT measured;g.MeasureText(style,tag.text.c_str(),measured);
        const float width=measured.W()+10;
        if(x+width>mRECT.R-10) {
            fitted(style,"...",IRECT(x,y,mRECT.R-10,mRECT.B-2));break;
        }
        g.DrawText(style,tag.text.c_str(),IRECT(x,y,x+width,mRECT.B-2));x+=width;
    }
}

BackgroundControl::BackgroundControl(const IRECT& bounds) : IPanelControl(bounds,background) {}

void BackgroundControl::OnResize() {
    if(auto* graphics=GetUI())mRECT=mTargetRECT=graphics->GetBounds();
    IPanelControl::OnResize();
}

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
                   const std::string& selected,const std::string& keyboardSelection,
                   std::function<void(std::size_t)> load,
                   std::function<void(const std::string&,bool)> favorite,
                   std::function<void(double)> scroll,
                   std::function<void(const char*)> import)
    : IControl(bounds),catalog(catalog),selected(selected),keyboardSelection(keyboardSelection),load(std::move(load)),
      favorite(std::move(favorite)),scroll(std::move(scroll)),import(std::move(import)) {
    SetTooltip("Click a sound to audition. Click + / - to favorite. Scroll to browse sounds. Drag the scrollbar to jump.");
}

void CatalogListControl::setResults(std::vector<std::size_t> results,double start) {
    const bool changed=matches!=results;
    matches=std::move(results);
    start=synth2::editor::editorLibraryScrollOffset(matches.size(),visibleRows,start);
    if(changed || draggingScroll) {
        OnEndAnimation();offset=drawOffset=start;
    } else if(start!=offset) {
        animationStart=drawOffset;offset=start;
        SetAnimation([this](IControl*) {
            const auto progress=std::min(GetAnimationProgress(),1.);
            const auto eased=1.-std::pow(1.-progress,3.);
            drawOffset=animationStart+(offset-animationStart)*eased;
            SetDirty(false);
            if(progress>=1.) { drawOffset=offset;OnEndAnimation(); }
        },100);
    }
    SetDirty(false);
}

IRECT CatalogListControl::scrollTrack() const {
    return IRECT(mRECT.R-14,mRECT.T+3,mRECT.R-3,mRECT.B-3);
}

IRECT CatalogListControl::scrollThumb() const {
    const auto track=scrollTrack();
    const auto height=std::min(track.H(),std::max(24.f,track.H()*visibleRows/std::max<std::size_t>(matches.size(),1)));
    const auto limit=synth2::editor::editorLibraryScrollLimit(matches.size(),visibleRows);
    const auto top=track.T+(limit>0 ? drawOffset/limit : 0.)*(track.H()-height);
    return IRECT(track.L,top,track.R,top+height);
}

void CatalogListControl::dragScroll(float y) {
    const auto track=scrollTrack(),thumb=scrollThumb();
    const auto travel=track.H()-thumb.H();
    if(travel<=0)return;
    scroll(std::clamp(static_cast<double>((y-track.T-scrollGrab)/travel),0.,1.)*
           synth2::editor::editorLibraryScrollLimit(matches.size(),visibleRows));
}

void CatalogListControl::Draw(IGraphics& g) {
    g.FillRect(panel,mRECT);
    if(matches.empty()) {
        g.DrawText(IText(15.f,muted,"Synth2UI"),"No matching sounds. Clear a filter or add a sound bank.",mRECT);
        return;
    }
    const auto first=static_cast<std::size_t>(drawOffset);
    g.PathClipRegion(IRECT(mRECT.L,mRECT.T,mRECT.R-18,mRECT.B));
    for(std::size_t row=first;row<matches.size();++row) {
        const auto y=mRECT.T+static_cast<float>((row-drawOffset)*libraryRowHeight);
        if(y>=mRECT.B)break;
        const auto& item=catalog.patches()[matches[row]];
        const IRECT bounds(mRECT.L,y,mRECT.R-18,y+libraryRowHeight);
        const bool isSelected=item.id==selected;
        if(isSelected) {
            g.FillRect(color(appearance.selectedRow),bounds);
            g.FillRect(accent,IRECT(mRECT.L,y,mRECT.L+4,y+libraryRowHeight));
        }
        else if(row%2==1)g.FillRect(background,bounds);
        if(item.id==keyboardSelection && !isSelected)
            g.DrawRect(accent,IRECT(bounds.L+1,bounds.T+1,bounds.R-1,bounds.B-1),nullptr,1.f);
        const IText text(13.f,ink,"Synth2UI",EAlign::Near);
        g.DrawText(IText(16.f,item.favorite ? accent : muted,"Synth2UI"),item.favorite ? "+" : "-",IRECT(mRECT.L,y,mRECT.L+32,y+libraryRowHeight));
        if(item.patch.color) {
            const auto rgb=*item.patch.color;
            g.FillRect(IColor(255,(rgb>>16)&255,(rgb>>8)&255,rgb&255),
                       IRECT(mRECT.L+34,y+libraryRowHeight/2-5,mRECT.L+42,y+libraryRowHeight/2+5));
        }
        const auto label=item.patch.name+(item.error.empty() ? "" : " [Cannot load]");
        const auto titleText=isSelected ? text.WithFont("Synth2UIBold").WithFGColor(color(appearance.displayText)) : text;
        using Columns=synth2::editor::EditorLibraryColumns;
        const auto drawFitted=[&](const IText& style,const std::string& value,const IRECT& cell,bool keepEnd=false) {
            const auto measure=[&](std::string_view text) {
                IRECT measured;const std::string copy(text);g.MeasureText(style,copy.c_str(),measured);return measured.W();
            };
            const auto fitted=synth2::editor::editorFitText(value,cell.W(),measure,keepEnd);
            g.DrawText(style,fitted.c_str(),cell);
        };
        drawFitted(titleText,label,IRECT(mRECT.L+Columns::nameLeft,y,mRECT.L+Columns::nameRight,y+libraryRowHeight));
        drawFitted(text.WithSize(11.f).WithFGColor(muted),item.bankName,
                   IRECT(mRECT.L+Columns::bankLeft,y,mRECT.L+Columns::bankRight,y+libraryRowHeight),true);
        float tagX=mRECT.L+Columns::tagsLeft;
        const float tagRight=mRECT.R-26;
        const auto drawTag=[&](const std::string& tag,bool automatic) {
            if(tagX>=tagRight)return;
            const auto tagText=text.WithSize(11.f).WithFGColor(automatic ? muted : ink);
            IRECT measured;g.MeasureText(tagText,tag.c_str(),measured);
            const float width=measured.W()+10;
            if(tagX+width>tagRight) {
                drawFitted(tagText,"...",IRECT(tagX,y,tagRight,y+libraryRowHeight));tagX=tagRight;return;
            }
            g.DrawText(tagText,tag.c_str(),IRECT(tagX,y,tagX+width,y+libraryRowHeight));tagX+=width;
        };
        for(const auto& tag:item.tags)drawTag(tag,false);
        for(const auto& tag:item.visibleAutomaticTags())drawTag(tag,true);
    }
    g.PathClipRegion(IRECT());
    g.FillRoundRect(background,scrollTrack(),4.f);
    if(matches.size()>visibleRows)g.FillRoundRect(draggingScroll ? accent : muted,scrollThumb(),4.f);
}

void CatalogListControl::OnMouseDown(float x,float y,const IMouseMod&) {
    if(x>=mRECT.R-18) {
        if(matches.size()<=visibleRows)return;
        OnEndAnimation();drawOffset=offset;
        const auto thumb=scrollThumb();
        draggingScroll=true;
        scrollGrab=thumb.Contains(x,y) ? y-thumb.T : thumb.H()/2;
        dragScroll(y);return;
    }
    const auto row=static_cast<std::size_t>((y-mRECT.T)/libraryRowHeight+drawOffset);
    if(row>=matches.size())return;
    const auto index=matches[row];const auto item=catalog.patches()[index];
    if(x<mRECT.L+32)favorite(item.id,!item.favorite);
    else load(index);
}

void CatalogListControl::OnMouseDrag(float,float y,float,float,const IMouseMod&) {
    if(draggingScroll)dragScroll(y);
}

void CatalogListControl::OnMouseUp(float,float,const IMouseMod&) {
    draggingScroll=false;SetDirty(false);
}

void CatalogListControl::OnMouseOver(float x,float y,const IMouseMod& mod) {
    const auto row=static_cast<std::size_t>((y-mRECT.T)/libraryRowHeight+drawOffset);
    if(x>=mRECT.R-18)SetTooltip("Drag to scroll through matching sounds.");
    else if(row<matches.size()) {
        const auto& item=catalog.patches()[matches[row]];
        const auto tooltip=item.patch.name+"\n"+item.bankName+"\n"+host::PatchCatalog::tagText(item.allTags());
        SetTooltip(tooltip.c_str());
    }
    IControl::OnMouseOver(x,y,mod);
}

void CatalogListControl::OnMouseWheel(float,float,const IMouseMod&,float delta) {
    if(delta!=0)scroll(synth2::editor::editorLibraryScrollOffset(matches.size(),visibleRows,offset-delta*3.));
}

void CatalogListControl::OnDrop(const char* path) {import(path);}

void CatalogListControl::OnDropMultiple(const std::vector<const char*>& paths) {for(const auto* path:paths)import(path);}

SearchControl::SearchControl(const IRECT& bounds, const char* text, const IText& style,
              std::function<void(const char*)> change,const char* placeholder)
    : IEditableTextControl(bounds,text,style,panel), onChange(std::move(change)),placeholder(placeholder) {}

void SearchControl::Draw(IGraphics& g) {
    IEditableTextControl::Draw(g);
    if(!*GetStr() && !placeholder.empty())
        g.DrawText(GetText().WithFGColor(muted),placeholder.c_str(),mRECT.GetPadded(-10.f));
}

void SearchControl::OnTextEntryCompletion(const char* text, int value) {
    IEditableTextControl::OnTextEntryCompletion(text,value);
    onChange(text);
}
}
