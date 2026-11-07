#include "editor/EditorLibraryColumns.h"
#include "Synth2.h"
#include "editor/EditorControls.h"
#include "editor/EditorControlTags.h"
#include "editor/EditorStyle.h"
#include "editor/EditorPages.h"
#include "editor/EditorFonts.h"
#include "editor/EditorKeyboardControl.h"
#include "editor/EditorKeyboardFocus.h"
#include <algorithm>

using namespace iplug;
using namespace iplug::igraphics;
using namespace synth2::editor::native;

void Synth2::layoutEditor(IGraphics* graphics)
{
    if (graphics->NControls()) return;
    const auto regular=synth2::editor::editorRegularFont();
    const auto bold=synth2::editor::editorBoldFont();
    const auto mono=synth2::editor::editorMonoFont();
    graphics->LoadFont("Synth2UI",regular.data,regular.size);
    graphics->LoadFont("Synth2UIBold",bold.data,bold.size);
    graphics->LoadFont("Synth2Mono",mono.data,mono.size);
    graphics->EnableMouseOver(true);
    graphics->EnableTooltips(true);
    graphics->AttachControl(new BackgroundControl(graphics->GetBounds()));
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
    graphics->AttachControl(new ITextControl(IRECT(20,82,592,112),"",text.WithSize(12.f).WithAlign(EAlign::Near)),bankTag);
    auto* quality=new ActionButtonControl(IRECT(600,82,744,112),[this](IControl* caller){showOversamplingMenu(caller);},
        ("Oversampling "+std::to_string(processor.oversamplingFactor())+"x").c_str(),style);
    quality->SetTooltip("Higher factors reduce aliasing and use more CPU. Saved with the project.");
    graphics->AttachControl(quality,oversamplingTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(756,82,820,112),[this](IControl* caller) { showImportMenu(caller); },"Add...",style));
    graphics->AttachControl(new ActionButtonControl(IRECT(826,82,878,112),[this](IControl* caller){showSaveMenu(caller);},"Save",style));
    graphics->AttachControl(new PresetDisplayControl(IRECT(240,16,878,78),[this](IControl* caller) {
        showPresetMenu(caller);
    },"",style.WithColor(kBG,color(appearance.display)).WithColor(kFG,color(appearance.display))
        .WithLabelText(text.WithFont("Synth2Mono").WithSize(16.f).WithFGColor(color(appearance.displayText)))),presetTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(890,16,934,78),[this](IControl*) { stepPatch(-1); },"<",style));
    graphics->AttachControl(new ActionButtonControl(IRECT(944,16,988,78),[this](IControl*) { stepPatch(1); },">",style));

    std::vector<const char*> labels(synth2::editor::editorViews.begin(),synth2::editor::editorViews.end());
    auto* tabs = new IVTabSwitchControl(IRECT(900,82,1260,112),[this](IControl* control) {
        showPage(control->As<IVTabSwitchControl>()->GetSelectedIdx());
    },labels,"",style.WithShowValue(false));
    tabs->SetValue(static_cast<double>(page)/(labels.size()-1));
    graphics->AttachControl(tabs,tabsTag);

    for (std::size_t i=0;i<synth2::editor::editorPanels().size();++i)
    {
        const auto& section=synth2::editor::editorPanels()[i];
        graphics->AttachControl(new SectionControl(section),panelTag+static_cast<int>(i));
        for (std::size_t cell=0;cell<section.controls.size();++cell)
        {
            const auto& item=section.controls[cell];
            const auto bounds=rect(synth2::editor::editorControlBounds(section,cell));
            const auto tint=color(section.accent);
            IControl* control=nullptr;
            if (synth2::editor::editorToggle(item.parameter) || !synth2::editor::editorChoices(item.parameter).empty())
                control=new SourceChoiceControl(bounds,item.parameter,item.label);
            else control=new PanelKnobControl(bounds,item.parameter,item.label,style.WithColor(kPR,tint));
            const auto tooltip=processor.parameter(item.parameter).name;
            control->SetTooltip(tooltip.c_str());
            graphics->AttachControl(control,parameterTag+item.parameter);
        }
    }
    // Controller assignments remain available without
    // displacing the ordinary sound-design controls from the main surface.
    for (std::size_t group=5;group<synth2::editor::editorPages().size();++group)
    {
        const auto& supplement=synth2::editor::editorPages()[group];
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
    graphics->AttachControl(new ITextControl(IRECT(720,802,1090,826),"Click a value to type  /  Drag corner to scale",
        text.WithFGColor(muted).WithSize(11.f).WithAlign(EAlign::Far)));
    auto* keyboard=new EditorKeyboardControl(IRECT(20,844,1260,924),processor);
    keyboard->SetText(text);
    graphics->AttachControl(keyboard,keyboardTag)->Hide(!keyboardVisible);
    graphics->AttachControl(new IVToggleControl(IRECT(1100,802,1260,830),[this](IControl* control) {
        setKeyboardVisible(control->GetValue()>.5);
    },"",style,"Keyboard","Keyboard"),keyboardToggleTag);
    graphics->SetKeyHandlerFunc([this,graphics](const IKeyPress& key,bool up) {
        if(!keyboardVisible || graphics->GetControlInTextEntry() || !editorKeyboardHasFocus(graphics->GetWindow())) {
            releaseComputerNotes();return false;
        }
        const bool handled=computerKeyboard.key(key.VK,up,key.C || key.A,[this](int note,int velocity) {
            processor.setEditorNote(note,velocity,synth2::host::EditorNoteSource::computer);
        });
        if(auto* control=graphics->GetControlWithTag(keyboardTag))control->As<EditorKeyboardControl>()->refresh();
        return handled;
    });
    // A supplementary browser keeps sound-design controls on the main surface.
    graphics->AttachControl(new SearchControl(IRECT(20,136,430,172),libraryFilter.query.c_str(),text.WithAlign(EAlign::Near),[this](const char* query) {
        changeFilter(query);
    }),librarySearchTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(442,136,650,172),[this](IControl* caller) { showBankMenu(caller); },"All banks",style),bankFilterTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(662,136,868,172),[this](IControl* caller) { showTagMenu(caller); },"All tags",style),tagFilterTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(880,136,1048,172),[this](IControl*) {
        libraryFilter.favoritesOnly=!libraryFilter.favoritesOnly;resultOffset=0;browserStatus.clear();refreshBrowser();
    },"Favorites: off",style),favoritesTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(1060,136,1152,172),[this](IControl*) { reloadCatalog(true); },"Refresh",style),4008);
    graphics->AttachControl(new ActionButtonControl(IRECT(1164,136,1260,172),[this](IControl*) {
        libraryFilter={};resultOffset=0;browserStatus.clear();refreshBrowser();
    },"Clear filters",style),4009);
    graphics->AttachControl(new ITextControl(IRECT(244,180,1260,200),"Search names, banks and tags. Use several words to narrow results. Drop .zip or .sy1 files onto the list.",text.WithSize(11.f).WithFGColor(muted).WithAlign(EAlign::Near)),4010);
    graphics->AttachControl(new ITextControl(IRECT(20,180,226,200),"",text.WithSize(11.f).WithFGColor(muted).WithAlign(EAlign::Near)),libraryMatchesTag);
    using Columns=synth2::editor::EditorLibraryColumns;
    graphics->AttachControl(new ITextControl(IRECT(20+Columns::nameLeft,204,20+Columns::nameRight,226),"SOUND",text.WithSize(11.f).WithFGColor(muted).WithAlign(EAlign::Near)),4011);
    graphics->AttachControl(new ITextControl(IRECT(20+Columns::bankLeft,204,20+Columns::bankRight,226),"BANK",text.WithSize(11.f).WithFGColor(muted).WithAlign(EAlign::Near)),4012);
    graphics->AttachControl(new ITextControl(IRECT(20+Columns::tagsLeft,204,1252,226),"TAGS",text.WithSize(11.f).WithFGColor(muted).WithAlign(EAlign::Near)),4013);
    graphics->AttachControl(new CatalogListControl(IRECT(20,228,1260,708),catalog,selectedPatchId,
        [this](std::size_t index) { loadLibraryPatch(index); },
        [this](const std::string& id,bool favorite) {
            std::string error;if(!catalog.setFavorite(id,favorite,error))browserStatus=error;
            else browserStatus.clear();refreshBrowser();
        },
        [this](int direction) {
            const auto count=catalog.search(libraryFilter).size();
            if(direction>0 && resultOffset+visibleRows<count)resultOffset+=visibleRows;
            else if(direction<0)resultOffset=resultOffset>=visibleRows ? resultOffset-visibleRows : 0;
            refreshBrowser();
        },[this](const char* path) { importBank(path); }),libraryListTag);
    graphics->AttachControl(new ITextControl(IRECT(20,712,1020,736),"Select a sound to edit its tags",text.WithSize(12.f).WithAlign(EAlign::Near)),selectedNameTag);
    graphics->AttachControl(new SearchControl(IRECT(20,742,916,778),"",text.WithAlign(EAlign::Near),[this](const char* tags) {
        std::string error;
        if(!selectedPatchId.empty() && !catalog.setTags(selectedPatchId,tags,error))browserStatus=error;
        else browserStatus="Tags saved. Separate tags with commas.";
        refreshBrowser();
    }),tagsEditTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(928,742,1048,778),[this](IControl* caller) {
        showAutomaticTagMenu(caller);
    },"Auto tags...",style),4016);
    graphics->AttachControl(new ActionButtonControl(IRECT(1060,742,1152,778),[this](IControl*) {
        resultOffset=resultOffset>=visibleRows ? resultOffset-visibleRows : 0;refreshBrowser();
    },"Page <",style),4014);
    graphics->AttachControl(new ActionButtonControl(IRECT(1164,742,1260,778),[this](IControl*) {
        if(resultOffset+visibleRows<catalog.search(libraryFilter).size())resultOffset+=visibleRows;refreshBrowser();
    },"Page >",style),4015);
    graphics->AttachControl(new ITextControl(IRECT(20,780,1260,798),"",text.WithSize(11.f).WithFGColor(muted).WithAlign(EAlign::Near)),resultsTag);
    if(!catalogLoaded)reloadCatalog();
    showPage(page);
    refreshBrowser();
}

void Synth2::showPage(int next)
{
    const auto& pages=synth2::editor::editorPages();
    if (next<0 || next>=static_cast<int>(synth2::editor::editorViews.size())) return;
    page=next;
    if (auto* graphics=GetUI())
    {
        if(auto* tabs=graphics->GetControlWithTag(tabsTag)){tabs->SetValue(static_cast<double>(page)/(synth2::editor::editorViews.size()-1));tabs->SetDirty(false);}
        for(int tag=libraryFirstTag;tag<=libraryLastTag;++tag)
            if(auto* control=graphics->GetControlWithTag(tag))control->Hide(page!=synth2::editor::editorLibraryView);
        for (std::size_t i=0;i<synth2::editor::editorPanels().size();++i)
        {
            if (auto* control=graphics->GetControlWithTag(panelTag+static_cast<int>(i))) control->Hide(page!=0);
            for (const auto& item:synth2::editor::editorPanels()[i].controls)
                if (auto* control=graphics->GetControlWithTag(parameterTag+item.parameter)) control->Hide(page!=0);
        }
        for (std::size_t group=5;group<pages.size();++group)
            for (int index:pages[group].parameters)
                if (auto* control=graphics->GetControlWithTag(parameterTag+index)) control->Hide(page!=static_cast<int>(group)-4);
        // Invalidate the background as well as the newly visible controls so a
        // page transition clears the entire previous surface in one redraw.
        // Do not push parameter values or invoke control actions while repainting.
        graphics->SetAllControlsDirty();
    }
}

void Synth2::refreshOversampling()
{
    syncQualityLatency();
    if(auto* graphics=GetUI())
        if(auto* control=graphics->GetControlWithTag(oversamplingTag))
        {
            control->As<IVButtonControl>()->SetLabelStr(("Oversampling "+std::to_string(processor.oversamplingFactor())+"x").c_str());
            control->SetDirty(false);
        }
    displayedOversampling=processor.oversamplingFactor();
}
void Synth2::showOversamplingMenu(IControl* caller)
{
    qualityMenu=std::make_unique<IPopupMenu>("Oversampling");
    for(int factor:{1,2,4,8,16})
        qualityMenu->AddItem((std::to_string(factor)+"x").c_str())->SetChecked(factor==processor.oversamplingFactor());
    qualityMenu->SetFunction([this](IPopupMenu* menu) {
        const int index=menu->GetChosenItemIdx();if(index<0 || index>4)return;
        BeginInformHostOfParamChange(qualityParameter);
        SendParameterValueFromUI(qualityParameter,GetParam(qualityParameter)->ToNormalized(index));
        EndInformHostOfParamChange(qualityParameter);
        refreshOversampling();
    });
    caller->GetUI()->CreatePopupMenu(*caller,*qualityMenu,caller->GetRECT());
}
void Synth2::OnIdle()
{
    if(displayedOversampling!=processor.oversamplingFactor())refreshOversampling();
    updateKeyboardInput();
    if (importPrompt && !conflictShown && GetUI())
    {
        std::unique_lock lock(importPrompt->mutex);
        if (importPrompt->alive && importPrompt->pending && !importPrompt->answered)
        {
            conflictShown = true;
            auto prompt = importPrompt;
            auto filename = prompt->path.filename().u8string();
            lock.unlock();
            browserStatus = "File already exists: " + std::string(filename.begin(), filename.end());
            conflictMenu = std::make_unique<IPopupMenu>("File already exists");
            for (auto text : {"Replace", "Keep both", "Skip", "Replace all", "Keep both for all", "Skip all"})
                conflictMenu->AddItem(text);
            conflictMenu->SetFunction(
                [this, prompt](IPopupMenu *menu)
                {
                    int i = menu->GetChosenItemIdx();
                    {
                        std::lock_guard lock(prompt->mutex);
                        prompt->choice = {i < 0 ? synth2::host::FileConflict::skip
                                                : static_cast<synth2::host::FileConflict>(i % 3),
                                          i < 0 || i >= 3};
                        prompt->answered = true;
                        prompt->pending = false;
                    }
                    prompt->changed.notify_all();
                    conflictShown = false;
                });
            if (auto *caller = GetUI()->GetControlWithTag(bankTag))
                GetUI()->CreatePopupMenu(*caller, *conflictMenu, caller->GetRECT());
            refreshBrowser();
        }
    }
    if (libraryJob.valid() && libraryJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try
        {
            auto result=libraryJob.get();
            if(result.error.empty() && (!result.importing || result.imported.catalogReady)){catalog=std::move(result.catalog);catalogLoaded=true;}
            if(result.generation==browserGeneration.load() && result.saving && result.error.empty()) {
                selectedPatchId=result.savedId;currentPatch=result.savedName;browser.baseline=result.savedBaseline;
                libraryFilter={};resultOffset=0;browserStatus="Preset saved.";
                const auto matches=catalog.search(libraryFilter);for(std::size_t n=0;n<matches.size();++n)if(catalog.patches()[matches[n]].id==selectedPatchId)resultOffset=(n/visibleRows)*visibleRows;
                showPage(synth2::editor::editorLibraryView);
            } else if(result.generation==browserGeneration.load() && result.importing) {
                libraryFilter={};libraryFilter.registeredOnly=true;libraryFilter.registeredIds=std::move(result.imported.ids);resultOffset=0;
                browserStatus=std::to_string(result.imported.imported)+" files registered, "+std::to_string(result.imported.skipped)+" skipped, "+std::to_string(result.imported.errors.size())+" errors";
                if(!result.imported.errors.empty())browserStatus+=" / "+result.imported.errors.front();
            }else browserStatus=result.error;
            refreshBrowser();
        }
        catch (const std::exception &e)
        {
            browserStatus = e.what();
            refreshBrowser();
        }
    }
    if(catalogReloadPending && !libraryJob.valid())
    {
        catalogReloadPending=false;
        reloadCatalog();
    }
    const bool modified = browser.modified(processor);
    if (stateRestored.exchange(false))
    {
        showPage(page);
        reloadCatalog();
        refreshBrowser();
    }
    if (modified != lastModified)
    {
        lastModified = modified;
        refreshBrowser();
    }
}
void Synth2::OnUIOpen()
{
    releaseKeyboardNotes();
    dialogLifetime=std::make_shared<int>(0);
    iplug::Plugin::OnUIOpen();
    setKeyboardVisible(keyboardVisible);
    if(!libraryJob.valid()) {
        auto root=synth2::host::PatchCatalog::userDirectory();
        if(!root.empty() && root!=catalog.root()){catalog=synth2::host::PatchCatalog(root);catalogLoaded=false;libraryFilter={};selectedPatchId.clear();resultOffset=0;}
    }
    reloadCatalog();
}
void Synth2::OnUIClose()
{
    releaseKeyboardNotes();
    keyboardVisible=false;
    dialogLifetime.reset();
    if(importPrompt){{std::lock_guard lock(importPrompt->mutex);importPrompt->alive=false;}importPrompt->changed.notify_all();}
    conflictShown=false;
    iplug::Plugin::OnUIClose();
}


void Synth2::setKeyboardVisible(bool visible)
{
    if(!visible)releaseKeyboardNotes();
    keyboardVisible=visible;
    if(auto* graphics=GetUI()) {
        if(auto* control=graphics->GetControlWithTag(keyboardTag))control->Hide(!visible);
        if(auto* toggle=graphics->GetControlWithTag(keyboardToggleTag)) {
            toggle->SetValue(visible ? 1. : 0.);toggle->SetDirty(false);
        }
        graphics->Resize(PLUG_WIDTH,visible ? 936 : PLUG_HEIGHT,graphics->GetDrawScale());
        graphics->SetAllControlsDirty();
    }
}
void Synth2::releaseComputerNotes()
{
    computerKeyboard.release([this](int note,int velocity) {
        processor.setEditorNote(note,velocity,synth2::host::EditorNoteSource::computer);
    });
}
void Synth2::releaseKeyboardNotes()
{
    releaseComputerNotes();
    processor.releaseEditorNotes();
    if(auto* graphics=GetUI())
        if(auto* control=graphics->GetControlWithTag(keyboardTag))control->As<EditorKeyboardControl>()->releaseMouse();
}
void Synth2::updateKeyboardInput()
{
    auto* graphics=GetUI();
    if(!graphics || !keyboardVisible)return;
    if(!editorKeyboardHasFocus(graphics->GetWindow()))releaseKeyboardNotes();
    else if(graphics->GetControlInTextEntry())releaseComputerNotes();
    if(auto* control=graphics->GetControlWithTag(keyboardTag))control->As<EditorKeyboardControl>()->refresh();
}
