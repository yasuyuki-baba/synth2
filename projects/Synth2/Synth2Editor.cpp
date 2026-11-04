#include "src/host/EditorInteraction.h"
#include "Synth2.h"
#include "EditorPages.h"
#include "EditorLayout.h"
#include "EditorAppearance.h"
#include "EditorFonts.h"
#include "EditorValueDisplay.h"
#include "IControls.h"
#include "IVKeyboardControl.h"
#include <algorithm>
#include <functional>
#include <cmath>
#include <set>
#include <stdexcept>

using namespace iplug;
using namespace iplug::igraphics;

namespace
{
constexpr int bankTag = 1000, presetTag = 1001, statusTag = 1002, searchTag = 1003, parameterTag = 2000;
constexpr int panelTag = 3000;
constexpr int libraryListTag=4000, librarySearchTag=4001, bankFilterTag=4002, tagFilterTag=4003;
constexpr int favoritesTag=4004, tagsEditTag=4005, selectedNameTag=4006, resultsTag=4007;
constexpr int libraryFirstTag=4000, libraryLastTag=4020, tabsTag=1004;
constexpr std::size_t visibleRows=12;
IRECT rect(const synth2::host::EditorBounds& b) { return IRECT(b.x,b.y,b.x+b.width,b.y+b.height); }
IColor color(const char* hex)
{
    const auto value=std::stoul(hex+1,nullptr,16);
    return IColor(255,(value>>16)&255,(value>>8)&255,value&255);
}
const auto& appearance=synth2::host::editorAppearance();
const IColor background=color(appearance.background),panel=color(appearance.panel),ink=color(appearance.text);
const IColor muted=color(appearance.muted),accent=color(appearance.accent);
class HeaderControl final : public IControl
{
public:
    HeaderControl() : IControl(IRECT(0,0,1280,118)) { mIgnoreMouse=true; }
    void Draw(IGraphics& g) override
    {
        g.FillRect(color(appearance.header),mRECT);
        g.FillRect(color(appearance.headerEdge),IRECT(0,0,1280,4));
        g.DrawLine(color(appearance.headerEdge),0,117,1280,117);
    }
};
class SectionControl final : public IControl
{
public:
    explicit SectionControl(const synth2::host::EditorPanel& section)
        : IControl(rect(section.bounds)), section(section) { mIgnoreMouse=true; }
    void Draw(IGraphics& g) override
    {
        const auto surface=std::string(section.title)=="FILTER" ? color(appearance.filterPanel) : panel;
        g.FillRoundRect(surface,mRECT,3.f);
        g.DrawRoundRect(color(appearance.border),mRECT,3.f,nullptr,0.5f);
        const auto heading=IRECT(mRECT.L+14,mRECT.T+9,mRECT.R-12,mRECT.T+25);
        g.DrawText(IText(13.f,color(section.accent),"Synth2UIBold",EAlign::Near),section.title,heading);
        g.DrawLine(color(appearance.border),heading.L,mRECT.T+32,heading.R,mRECT.T+32,nullptr,0.5f);
    }
private:
    const synth2::host::EditorPanel& section;
};
class PanelKnobControl final : public IVKnobControl
{
public:
    PanelKnobControl(const IRECT& bounds,int parameter,const char* label,const IVStyle& style)
        : IVKnobControl(bounds,parameter,label,style,true,false) {
        mTrackToHandleDistance=2.5f;
        SetTooltip("Drag to adjust. Click value to edit the original source number.");
    }
    void OnResize() override
    {
        IVKnobControl::OnResize();
        const float diameter=static_cast<float>(synth2::host::editorKnobDiameter(GetParamIdx(),static_cast<int>(mRECT.H())));
        mLabelBounds=IRECT(mRECT.L,mRECT.T,mRECT.R,mRECT.T+13);
        mWidgetBounds=IRECT(mRECT.MW()-diameter/2,mRECT.T+15,mRECT.MW()+diameter/2,mRECT.T+15+diameter);
        mValueBounds=IRECT(mRECT.L,mRECT.T+16+diameter,mRECT.R,mRECT.T+28+diameter);
    }
    void DrawValue(IGraphics& g,bool mouseOver) override
    {
        if (mouseOver) g.FillRect(COLOR_TRANSLUCENT,mValueBounds);
        if (!mStyle.showValue) return;
        const auto value=synth2::host::editorValueDisplay(GetParamIdx(),GetParam()->FromNormalized(GetValue()));
        const auto blend=GetBlend();
        g.DrawText(mStyle.valueText,value.c_str(),mValueBounds,&blend);
    }
    void DrawHandle(IGraphics& g,const IRECT& bounds) override
    {
        g.FillCircle(color(appearance.control),bounds.MW(),bounds.MH(),bounds.W()/2);
        g.DrawCircle(color(appearance.controlEdge),bounds.MW(),bounds.MH(),bounds.W()/2,nullptr,1.f);
        g.DrawCircle(IColor(255,40,39,41),bounds.MW(),bounds.MH(),std::max(1.f,bounds.W()/2-2),nullptr,0.5f);
    }
    void DrawPointer(IGraphics& g,float angle,float cx,float cy,float radius) override
    {
        g.DrawRadialLine(color(appearance.pointer),cx,cy,angle,radius*0.56f,radius*0.96f,nullptr,1.8f);
    }
    void DrawIndicatorTrack(IGraphics& g,float angle,float cx,float cy,float radius) override
    {
        g.DrawArc(color(appearance.track),cx,cy,radius,-135.f,135.f,nullptr,1.f);
        g.DrawArc(GetParamIdx()==19 ? accent : color(appearance.pointer),cx,cy,radius,-135.f,angle,nullptr,1.25f);
    }
};
// A selector changes the original source integer. Binding a standard enum widget
// to an artificial 0..N range would reinterpret existing host automation.
class SourceChoiceControl final : public IControl
{
public:
    SourceChoiceControl(const IRECT& bounds,int parameter,const char* label)
        : IControl(bounds,parameter), label(label), toggle(synth2::host::editorToggle(parameter)),
          choices(synth2::host::editorChoices(parameter)) {}
    void Draw(IGraphics& g) override
    {
        const auto raw=GetParam()->FromNormalized(GetValue());
        const auto value=synth2::host::editorChoiceDisplay(toggle,raw,choices);
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
    void OnMouseDown(float,float,const IMouseMod&) override
    {
        if (toggle)
            SetValueFromUserInput(GetParam()->ToNormalized(synth2::host::editorToggledSourceValue(GetParam()->FromNormalized(GetValue()))));
        else
        {
            menu.Clear();
            for (const auto& choice:choices) menu.AddItem(choice.label);
            GetUI()->CreatePopupMenu(*this,menu,mRECT);
        }
    }
    void OnPopupMenuSelection(IPopupMenu* selection,int) override
    {
        if (!selection) return;
        const int index=selection->GetChosenItemIdx();
        if (const auto source=synth2::host::editorSelectedSourceValue(choices,index))
            SetValueFromUserInput(GetParam()->ToNormalized(*source));
    }
private:
    const char* label;
    bool toggle;
    std::vector<synth2::host::EditorChoice> choices;
    IPopupMenu menu;
};

// Only the visible result page is drawn, regardless of catalog size.
class CatalogListControl final : public IControl
{
public:
    CatalogListControl(const IRECT& bounds,const synth2::host::PatchCatalog& catalog,
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
    void setResults(std::vector<std::size_t> results,std::size_t start) {
        matches=std::move(results);offset=start;SetDirty(false);
    }
    void Draw(IGraphics& g) override {
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
    void OnMouseDown(float x,float y,const IMouseMod&) override {
        const auto row=static_cast<std::size_t>((y-mRECT.T)/40.f);
        if(row>=visibleRows || offset+row>=matches.size())return;
        const auto index=matches[offset+row];const auto item=catalog.patches()[index];
        if(x<mRECT.L+32)favorite(item.id,!item.favorite);
        else if(x>mRECT.R-80)remove(item.id);
        else load(index);
    }
    void OnMouseWheel(float,float,const IMouseMod&,float delta) override {if(delta!=0)scroll(delta>0 ? -1 : 1);}
    void OnDrop(const char* path) override {import(path);}
    void OnDropMultiple(const std::vector<const char*>& paths) override {for(const auto* path:paths)import(path);}
private:
    const synth2::host::PatchCatalog& catalog;
    const std::string& selected;
    std::vector<std::size_t> matches;
    std::size_t offset=0;
    std::function<void(std::size_t)> load;
    std::function<void(const std::string&,bool)> favorite;
    std::function<void(const std::string&)> remove;
    std::function<void(int)> scroll;
    std::function<void(const char*)> import;
};

class SearchControl final : public IEditableTextControl
{
public:
    SearchControl(const IRECT& bounds, const char* text, const IText& style,
                  std::function<void(const char*)> change)
        : IEditableTextControl(bounds,text,style,panel), onChange(std::move(change)) {}
    void OnTextEntryCompletion(const char* text, int value) override
    {
        IEditableTextControl::OnTextEntryCompletion(text,value);
        onChange(text);
    }
private:
    std::function<void(const char*)> onChange;
};
}

void Synth2::layoutEditor(IGraphics* graphics)
{
    if (graphics->NControls()) return;
    const auto regular=synth2::host::editorRegularFont();
    const auto bold=synth2::host::editorBoldFont();
    const auto mono=synth2::host::editorMonoFont();
    graphics->LoadFont("Synth2UI",regular.data,regular.size);
    graphics->LoadFont("Synth2UIBold",bold.data,bold.size);
    graphics->LoadFont("Synth2Mono",mono.data,mono.size);
    graphics->EnableMouseOver(true);
    graphics->EnableTooltips(true);
    graphics->AttachPanelBackground(background);
    graphics->AttachControl(new HeaderControl());
    graphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    graphics->AttachTextEntryControl();
    const IText text {14.f,ink,"Synth2UI"};
    graphics->AttachPopupMenuControl(text);
    const auto style = DEFAULT_STYLE.WithColor(kBG,panel).WithColor(kFG,color(appearance.control))
        .WithColor(kPR,accent).WithColor(kFR,panel).WithColor(kHL,accent)
        .WithDrawShadows(false).WithLabelText(text.WithSize(12.f))
        .WithValueText(text.WithSize(10.f).WithVAlign(EVAlign::Bottom)).WithRoundness(0.1f);
    graphics->AttachControl(new ITextControl(IRECT(20,16,214,65),"SYNTH2",text.WithFont("Synth2UIBold").WithSize(29.f).WithAlign(EAlign::Near)));
    graphics->AttachControl(new ITextControl(IRECT(20,62,214,78),"VIRTUAL ANALOG SYNTHESIZER",text.WithSize(9.f).WithAlign(EAlign::Near)));
    graphics->AttachControl(new IVSliderControl(IRECT(1020,16,1260,72),128,"Master volume",style,true,EDirection::Horizontal));
    graphics->AttachControl(new ITextControl(IRECT(266,82,644,112),"",text.WithSize(12.f).WithAlign(EAlign::Near)),bankTag);
    graphics->AttachControl(new IVButtonControl(IRECT(654,82,744,112),[this](IControl*) { showPage(synth2::host::editorLibraryView); },"Library",style));
    graphics->AttachControl(new IVButtonControl(IRECT(756,82,878,112),[this](IControl* caller) { showImportMenu(caller); },"Add sounds...",style));
    graphics->AttachControl(new ITextControl(IRECT(20,82,66,112),"Find",text.WithSize(12.f).WithAlign(EAlign::Near)));
    graphics->AttachControl(new SearchControl(IRECT(68,82,248,112),libraryFilter.query.c_str(),text.WithAlign(EAlign::Near),[this](const char* query) {
        changeFilter(query);
    }),searchTag);
    graphics->AttachControl(new IVButtonControl(IRECT(240,20,878,66),[this](IControl* caller) {
        showPresetMenu(caller);
    },"",style.WithColor(kBG,color(appearance.display)).WithColor(kFG,color(appearance.display))
        .WithLabelText(text.WithFont("Synth2Mono").WithSize(16.f).WithFGColor(color(appearance.displayText)))),presetTag);
    graphics->AttachControl(new IVButtonControl(IRECT(890,20,934,66),[this](IControl*) { stepPatch(-1); },"<",style));
    graphics->AttachControl(new IVButtonControl(IRECT(944,20,988,66),[this](IControl*) { stepPatch(1); },">",style));

    std::vector<const char*> labels(synth2::host::editorViews.begin(),synth2::host::editorViews.end());
    auto* tabs = new IVTabSwitchControl(IRECT(900,82,1260,112),[this](IControl* control) {
        showPage(control->As<IVTabSwitchControl>()->GetSelectedIdx());
    },labels,"",style.WithShowValue(false));
    tabs->SetValue(static_cast<double>(page)/(labels.size()-1));
    graphics->AttachControl(tabs,tabsTag);

    for (std::size_t i=0;i<synth2::host::editorPanels().size();++i)
    {
        const auto& section=synth2::host::editorPanels()[i];
        graphics->AttachControl(new SectionControl(section),panelTag+static_cast<int>(i));
        for (std::size_t cell=0;cell<section.controls.size();++cell)
        {
            const auto& item=section.controls[cell];
            const auto bounds=rect(synth2::host::editorControlBounds(section,cell));
            const auto tint=color(section.accent);
            IControl* control=nullptr;
            if (synth2::host::editorToggle(item.parameter) || !synth2::host::editorChoices(item.parameter).empty())
                control=new SourceChoiceControl(bounds,item.parameter,item.label);
            else control=new PanelKnobControl(bounds,item.parameter,item.label,style.WithColor(kPR,tint));
            const auto tooltip=processor.parameter(item.parameter).name;
            control->SetTooltip(tooltip.c_str());
            graphics->AttachControl(control,parameterTag+item.parameter);
        }
    }
    // Controller assignments remain available without
    // displacing the ordinary sound-design controls from the main surface.
    for (std::size_t group=5;group<synth2::host::editorPages().size();++group)
    {
        const auto& supplement=synth2::host::editorPages()[group];
        for (std::size_t cell=0; cell<supplement.parameters.size(); ++cell)
        {
            const int index=supplement.parameters[cell];
            const float x=24.f+static_cast<float>(cell%6)*205.f;
            const float y=148.f+static_cast<float>(cell/6)*120.f;
            auto* knob=new IVKnobControl(IRECT(x,y,x+194,y+112),index,processor.parameter(index).name.c_str(),style,true);
            knob->SetTooltip(processor.parameter(index).id.c_str());
            graphics->AttachControl(knob,parameterTag+index);
        }
    }
    graphics->AttachControl(new ITextControl(IRECT(20,802,700,826),"",text.WithFGColor(muted).WithSize(12.f).WithAlign(EAlign::Near)),statusTag);
    graphics->AttachControl(new ITextControl(IRECT(720,802,1260,826),"Click a value to enter it  /  Drag the corner to scale",
        text.WithFGColor(muted).WithSize(11.f).WithAlign(EAlign::Far)));
    auto* keyboard=new IVKeyboardControl(IRECT(20,840,1260,900),36,84,false,
        IColor(255,218,215,205),color(appearance.control),accent,color(appearance.border),ink);
    keyboard->SetText(text);
    graphics->AttachControl(keyboard);
    // A supplementary browser keeps sound-design controls on the main surface.
    graphics->AttachControl(new SearchControl(IRECT(20,136,430,172),libraryFilter.query.c_str(),text.WithAlign(EAlign::Near),[this](const char* query) {
        changeFilter(query);
    }),librarySearchTag);
    graphics->AttachControl(new IVButtonControl(IRECT(442,136,650,172),[this](IControl* caller) { showBankMenu(caller); },"All banks",style),bankFilterTag);
    graphics->AttachControl(new IVButtonControl(IRECT(662,136,868,172),[this](IControl* caller) { showTagMenu(caller); },"All tags",style),tagFilterTag);
    graphics->AttachControl(new IVButtonControl(IRECT(880,136,1048,172),[this](IControl*) {
        libraryFilter.favoritesOnly=!libraryFilter.favoritesOnly;resultOffset=0;browserStatus.clear();refreshBrowser();
    },"Favorites: off",style),favoritesTag);
    graphics->AttachControl(new IVButtonControl(IRECT(1060,136,1152,172),[this](IControl*) { reloadCatalog(); },"Refresh",style),4008);
    graphics->AttachControl(new IVButtonControl(IRECT(1164,136,1260,172),[this](IControl*) {
        libraryFilter={};resultOffset=0;browserStatus.clear();refreshBrowser();
    },"Clear filters",style),4009);
    graphics->AttachControl(new ITextControl(IRECT(20,180,1260,200),"Search names, banks and tags. Use several words to narrow results. Drop .zip or .sy1 files onto the list.",text.WithSize(11.f).WithFGColor(muted).WithAlign(EAlign::Near)),4010);
    graphics->AttachControl(new ITextControl(IRECT(58,204,580,226),"SOUND",text.WithSize(11.f).WithFGColor(muted).WithAlign(EAlign::Near)),4011);
    graphics->AttachControl(new ITextControl(IRECT(600,204,780,226),"BANK",text.WithSize(11.f).WithFGColor(muted).WithAlign(EAlign::Near)),4012);
    graphics->AttachControl(new ITextControl(IRECT(800,204,1170,226),"TAGS",text.WithSize(11.f).WithFGColor(muted).WithAlign(EAlign::Near)),4013);
    graphics->AttachControl(new CatalogListControl(IRECT(20,228,1260,708),catalog,selectedPatchId,
        [this](std::size_t index) { loadLibraryPatch(index); },
        [this](const std::string& id,bool favorite) {
            std::string error;if(!catalog.setFavorite(id,favorite,error))browserStatus=error;
            else browserStatus.clear();refreshBrowser();
        },[this](const std::string& id) { removeLibraryPatch(id); },
        [this](int direction) {
            const auto count=catalog.search(libraryFilter).size();
            if(direction>0 && resultOffset+visibleRows<count)resultOffset+=visibleRows;
            else if(direction<0)resultOffset=resultOffset>=visibleRows ? resultOffset-visibleRows : 0;
            refreshBrowser();
        },[this](const char* path) { importBank(path); }),libraryListTag);
    graphics->AttachControl(new ITextControl(IRECT(20,712,1020,736),"Select a sound to edit its tags",text.WithSize(12.f).WithAlign(EAlign::Near)),selectedNameTag);
    graphics->AttachControl(new SearchControl(IRECT(20,742,940,778),"",text.WithAlign(EAlign::Near),[this](const char* tags) {
        std::string error;
        if(!selectedPatchId.empty() && !catalog.setTags(selectedPatchId,tags,error))browserStatus=error;
        else browserStatus="Tags saved. Separate tags with commas.";
        refreshBrowser();
    }),tagsEditTag);
    graphics->AttachControl(new IVButtonControl(IRECT(950,742,1048,778),[this](IControl*) {
        resultOffset=resultOffset>=visibleRows ? resultOffset-visibleRows : 0;refreshBrowser();
    },"Page <",style),4014);
    graphics->AttachControl(new IVButtonControl(IRECT(1060,742,1152,778),[this](IControl*) {
        if(resultOffset+visibleRows<catalog.search(libraryFilter).size())resultOffset+=visibleRows;refreshBrowser();
    },"Page >",style),4015);
    graphics->AttachControl(new IVButtonControl(IRECT(1164,742,1260,778),[this](IControl*) { removeLibraryBank(); },"Remove bank",style),4016);
    graphics->AttachControl(new ITextControl(IRECT(20,780,1260,798),"",text.WithSize(11.f).WithFGColor(muted).WithAlign(EAlign::Near)),resultsTag);
    if(!catalogLoaded)reloadCatalog();
    showPage(page);
    refreshBrowser();
}

void Synth2::showPage(int next)
{
    const auto& pages=synth2::host::editorPages();
    if (next<0 || next>=static_cast<int>(synth2::host::editorViews.size())) return;
    page=next;
    if (auto* graphics=GetUI())
    {
        if(auto* tabs=graphics->GetControlWithTag(tabsTag)){tabs->SetValue(static_cast<double>(page)/(synth2::host::editorViews.size()-1));tabs->SetDirty(false);}
        for(int tag=libraryFirstTag;tag<=libraryLastTag;++tag)
            if(auto* control=graphics->GetControlWithTag(tag))control->Hide(page!=synth2::host::editorLibraryView);
        for (std::size_t i=0;i<synth2::host::editorPanels().size();++i)
        {
            if (auto* control=graphics->GetControlWithTag(panelTag+static_cast<int>(i))) control->Hide(page!=0);
            for (const auto& item:synth2::host::editorPanels()[i].controls)
                if (auto* control=graphics->GetControlWithTag(parameterTag+item.parameter)) control->Hide(page!=0);
        }
        for (std::size_t group=5;group<pages.size();++group)
            for (int index:pages[group].parameters)
                if (auto* control=graphics->GetControlWithTag(parameterTag+index)) control->Hide(page!=static_cast<int>(group)-4);
    }
}

void Synth2::refreshBrowser()
{
    auto* graphics=GetUI(); if (!graphics) return;
    if(!libraryFilter.bankId.empty() && std::none_of(catalog.patches().begin(),catalog.patches().end(),[&](const auto& p){return p.bankId==libraryFilter.bankId;}))
        libraryFilter.bankId.clear();
    const auto matches=catalog.search(libraryFilter);
    if(resultOffset>=matches.size())resultOffset=matches.empty() ? 0 : ((matches.size()-1)/visibleRows)*visibleRows;
    const auto setText=[&](int tag,const std::string& value) {
        if(auto* c=graphics->GetControlWithTag(tag)){c->As<ITextControl>()->SetStr(value.c_str());c->SetDirty(false);}
    };
    const auto setButton=[&](int tag,const std::string& value) {
        if(auto* c=graphics->GetControlWithTag(tag)){c->As<IVButtonControl>()->SetLabelStr(value.c_str());c->SetDirty(false);}
    };
    setText(bankTag,std::to_string(catalog.patches().size())+" sounds in library");
    setButton(presetTag,currentPatch);
    setText(statusTag,browserStatus.empty() ? (catalog.patches().empty() ? "No sounds yet. Use Add sounds... to import a bank or patch." : std::to_string(matches.size())+" matching sounds") : browserStatus);
    setText(searchTag,libraryFilter.query);setText(librarySearchTag,libraryFilter.query);
    std::string bankLabel="All banks";
    for(const auto& patch:catalog.patches())if(patch.bankId==libraryFilter.bankId){bankLabel=patch.bankName;break;}
    setButton(bankFilterTag,bankLabel);setButton(tagFilterTag,libraryFilter.tag.empty() ? "All tags" : libraryFilter.tag);
    setButton(favoritesTag,libraryFilter.favoritesOnly ? "Favorites: on" : "Favorites: off");
    if(auto* c=graphics->GetControlWithTag(libraryListTag))c->As<CatalogListControl>()->setResults(matches,resultOffset);
    const auto selected=std::find_if(catalog.patches().begin(),catalog.patches().end(),[&](const auto& p){return p.id==selectedPatchId;});
    const bool canEdit=selected!=catalog.patches().end();
    setText(selectedNameTag,canEdit ? "Tags for "+selected->patch.name+" (comma-separated; press Enter to save)" : "Select a sound to edit its tags");
    setText(tagsEditTag,canEdit ? synth2::host::PatchCatalog::tagText(selected->tags) : "");
    if(auto* c=graphics->GetControlWithTag(tagsEditTag))c->SetDisabled(!canEdit);
    if(auto* c=graphics->GetControlWithTag(4016))c->SetDisabled(libraryFilter.bankId.empty());
    const auto first=matches.empty() ? 0 : resultOffset+1;
    setText(resultsTag,std::to_string(first)+" - "+std::to_string(std::min(resultOffset+visibleRows,matches.size()))+" of "+std::to_string(matches.size())+" sounds  /  + marks a favorite");
}

void Synth2::changeFilter(const char* query)
{
    libraryFilter.query=query;resultOffset=0;browserStatus.clear();showPage(synth2::host::editorLibraryView);refreshBrowser();
}

void Synth2::showPresetMenu(IControl*) { showPage(synth2::host::editorLibraryView); }

void Synth2::showBankMenu(IControl* caller)
{
    std::vector<std::pair<std::string,std::string>> banks;
    std::set<std::string> seen;
    for(const auto& p:catalog.patches())if(seen.insert(p.bankId).second)banks.push_back({p.bankId,p.bankName});
    std::stable_sort(banks.begin(),banks.end(),[](const auto& a,const auto& b){return a.second<b.second;});
    presetMenu=std::make_unique<IPopupMenu>("Banks");presetMenu->AddItem("All banks");
    for(const auto& bank:banks)presetMenu->AddItem(bank.second.c_str());
    presetMenu->SetFunction([this,banks](IPopupMenu* menu) {
        const int index=menu->GetChosenItemIdx();if(index<0 || index>static_cast<int>(banks.size()))return;
        libraryFilter.bankId=index==0 ? "" : banks[index-1].first;resultOffset=0;browserStatus.clear();refreshBrowser();
    });
    caller->GetUI()->CreatePopupMenu(*caller,*presetMenu,caller->GetRECT());
}
void Synth2::showTagMenu(IControl* caller)
{
    std::set<std::string> unique;
    auto scoped=libraryFilter;scoped.tag.clear();scoped.query.clear();
    for(auto i:catalog.search(scoped))for(const auto& tag:catalog.patches()[i].tags)unique.insert(tag);
    const std::vector<std::string> tags(unique.begin(),unique.end());
    presetMenu=std::make_unique<IPopupMenu>("Tags");presetMenu->AddItem("All tags");
    for(const auto& tag:tags)presetMenu->AddItem(tag.c_str());
    presetMenu->SetFunction([this,tags](IPopupMenu* menu) {
        const int index=menu->GetChosenItemIdx();if(index<0 || index>static_cast<int>(tags.size()))return;
        libraryFilter.tag=index==0 ? "" : tags[index-1];resultOffset=0;browserStatus.clear();refreshBrowser();
    });
    caller->GetUI()->CreatePopupMenu(*caller,*presetMenu,caller->GetRECT());
}
void Synth2::stepPatch(int direction)
{
    const auto matches=catalog.search(libraryFilter);if(matches.empty())return;
    const auto found=std::find_if(matches.begin(),matches.end(),[&](auto i){return catalog.patches()[i].id==selectedPatchId;});
    const auto next=found==matches.end() ? (direction>0 ? 0 : static_cast<int>(matches.size())-1)
        : (static_cast<int>(found-matches.begin())+direction+static_cast<int>(matches.size()))%static_cast<int>(matches.size());
    resultOffset=(next/visibleRows)*visibleRows;loadLibraryPatch(matches[next]);
}

void Synth2::notifyHostOfPatch()
{
    syncParametersFromProcessor();
    // Reflect the transaction to the host and editor without invoking
    // SetParameterValue/OnParamChange for every already-loaded source word.
    for (int index=0; index<NParams(); ++index)
    {
        BeginInformHostOfParamChange(index);
        InformHostOfParamChange(index,GetParam(index)->GetNormalized());
        EndInformHostOfParamChange(index);
    }
    SendCurrentParamValuesFromDelegate();
}

void Synth2::loadLibraryPatch(std::size_t index)
{
    if(index>=catalog.patches().size())return;
    const auto& item=catalog.patches()[index];std::string error;
    if(!processor.loadProgram(item.patch.text,item.program,error,item.patch.name)){browserStatus=error;refreshBrowser();return;}
    selectedPatchId=item.id;currentPatch=item.patch.name;browserStatus.clear();stateRestored.store(false);
    notifyHostOfPatch();refreshBrowser();
}
void Synth2::reloadCatalog()
{
    std::string error;
    if(!catalog.reload(error))browserStatus=error;else {catalogLoaded=true;browserStatus.clear();}
    refreshBrowser();
}
void Synth2::importBank(const std::string& path)
{
    synth2::host::ImportResult result;std::string error;
    std::set<std::string> previousBanks;
    for(const auto& p:catalog.patches())previousBanks.insert(p.bankId);
    if(!catalog.importFile(path,result,error))browserStatus=error;
    else {
        catalogLoaded=true;
        if(result.added) {
            libraryFilter={};resultOffset=0;
            for(const auto& p:catalog.patches())if(!previousBanks.count(p.bankId))libraryFilter.bankId=p.bankId;
        }
        browserStatus=std::to_string(result.added)+" sounds added, "+std::to_string(result.duplicates)+" duplicates skipped.";
    }
    showPage(synth2::host::editorLibraryView);refreshBrowser();
}
void Synth2::showImportMenu(IControl* caller)
{
    presetMenu=std::make_unique<IPopupMenu>("Add sounds");
    presetMenu->AddItem("Add .zip bank or .sy1 file...");
    presetMenu->AddItem("Add folder of .sy1 sounds...");
    presetMenu->AddItem("Open library folder");
    presetMenu->SetFunction([this](IPopupMenu* menu) {
        const auto index=menu->GetChosenItemIdx();
        if(index==0)promptForBank();
        else if(index==1)promptForFolder();
        else if(index==2) {
            try {
                if(catalog.root().empty())throw std::runtime_error("The user library folder is unavailable.");
                std::filesystem::create_directories(catalog.root());
                const auto utf8=catalog.root().u8string();const std::string path(utf8.begin(),utf8.end());
                WDL_String folder(path.c_str());
                if(auto* ui=GetUI())if(!ui->RevealPathInExplorerOrFinder(folder))browserStatus="The library folder could not be opened.";
            } catch(const std::exception& e){browserStatus=e.what();}
            refreshBrowser();
        }
    });
    caller->GetUI()->CreatePopupMenu(*caller,*presetMenu,caller->GetRECT());
}
void Synth2::promptForFolder()
{
    auto* graphics=GetUI();if(!graphics)return;
    const std::weak_ptr<int> lifetime=dialogLifetime;WDL_String path;
    graphics->PromptForDirectory(path,[this,lifetime](const WDL_String&,const WDL_String& chosen) {
        if(lifetime.expired() || !chosen.GetLength())return;
        importBank(chosen.Get());
    });
}
void Synth2::promptForBank()
{
    auto* graphics=GetUI();if(!graphics)return;
    const std::weak_ptr<int> lifetime=dialogLifetime;WDL_String file,path;
    graphics->PromptForFile(file,path,EFileAction::Open,"sy1 zip",[this,lifetime](const WDL_String& chosen,const WDL_String&) {
        if(lifetime.expired() || !chosen.GetLength())return;
        importBank(chosen.Get());
    });
}
void Synth2::removeLibraryPatch(const std::string& id)
{
    auto* graphics=GetUI();if(!graphics)return;
    const auto found=std::find_if(catalog.patches().begin(),catalog.patches().end(),[&](const auto& p){return p.id==id;});
    if(found==catalog.patches().end())return;
    const auto message="Remove "+found->patch.name+" from the library? The current sound and saved DAW projects are preserved.";
    const std::weak_ptr<int> lifetime=dialogLifetime;
    graphics->ShowMessageBox(message.c_str(),"Remove sound",kMB_YESNO,[this,lifetime,id](EMsgBoxResult result) {
        if(lifetime.expired() || result!=kYES)return;
        std::string error;if(!catalog.removePatch(id,error))browserStatus=error;else browserStatus="Sound removed from library.";
        refreshBrowser();
    });
}
void Synth2::removeLibraryBank()
{
    auto* graphics=GetUI();if(!graphics || libraryFilter.bankId.empty())return;
    const auto id=libraryFilter.bankId;const std::weak_ptr<int> lifetime=dialogLifetime;
    graphics->ShowMessageBox("Remove the selected bank from the library? The original file, current sound and saved DAW projects are preserved.","Remove bank",kMB_YESNO,[this,lifetime,id](EMsgBoxResult result) {
        if(lifetime.expired() || result!=kYES)return;
        std::string error;if(!catalog.removeBank(id,error))browserStatus=error;
        else {libraryFilter.bankId.clear();browserStatus="Bank removed from library.";}
        refreshBrowser();
    });
}

void Synth2::OnIdle()
{
    if (stateRestored.exchange(false))
    {
        selectedPatchId.clear(); currentPatch=processor.currentPatchName(); browserStatus.clear(); refreshBrowser();
    }
}
void Synth2::OnUIOpen()
{
    dialogLifetime=std::make_shared<int>(0);
    iplug::Plugin::OnUIOpen();
    reloadCatalog();
}
void Synth2::OnUIClose()
{
    dialogLifetime.reset();
    iplug::Plugin::OnUIClose();
}
