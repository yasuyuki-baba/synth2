#include "editor/EditorLibraryColumns.h"
#include "editor/EditorLibraryScroll.h"
#include "editor/EditorResize.h"
#include "Synth2.h"
#include "editor/EditorControls.h"
#include "editor/EditorControlTags.h"
#include "editor/EditorStyle.h"
#include "editor/EditorPages.h"
#include "editor/EditorFonts.h"
#include "editor/EditorKeyboardControl.h"
#include "editor/EditorKeyboardFocus.h"
#include "editor/EditorWindowBounds.h"
#include <algorithm>
#include <cstdio>

using namespace iplug;
using namespace iplug::igraphics;
using namespace synth2::editor::native;

void Synth2::OnParentWindowResize(int width,int height)
{
    auto* graphics=GetUI();
    if(!graphics || resizingParentWindow)return;
    reconcileEditorScale=false;
    const auto scale=synth2::editor::editorResizeScale(width,height,
        graphics->GetPlatformWindowScale(),keyboardVisible,graphics->GetDrawScale());
    if(!scale)return;
    observedParentWidth=width;observedParentHeight=height;
    resizingParentWindow=true;
    // The parent already changed size. Suppress the SDK's relative parent
    // adjustment, which would otherwise apply the size delta a second time.
    graphics->Resize(PLUG_WIDTH,keyboardVisible ? 936 : PLUG_HEIGHT,*scale,false);
    const auto windowWidth=static_cast<int>(graphics->WindowWidth()*graphics->GetPlatformWindowScale());
    const auto windowHeight=static_cast<int>(graphics->WindowHeight()*graphics->GetPlatformWindowScale());
    SetEditorSize(windowWidth,windowHeight);
    resizingParentWindow=false;
    refreshEditorScale();
}

void Synth2::requestEditorScale(float scale)
{
    auto* graphics=GetUI();
    if(!graphics || !std::isfinite(scale))return;
    scale=std::clamp(scale,synth2::editor::editorMinimumScale,synth2::editor::editorMaximumScale);
#if defined(APP_API)
    if(const auto bounds=editorWindowBounds(graphics->GetWindow(),true))
        if(const auto fit=synth2::editor::editorResizeScale(bounds->maximumWidth,bounds->maximumHeight,
            graphics->GetPlatformWindowScale(),keyboardVisible))scale=std::min(scale,*fit);
#endif
    releaseComputerNotes();
#if defined(__APPLE__) && defined(AU_API)
    // Remember the pre-request parent even before the first UI timer tick.
    // Its unchanged size must not undo the explicitly resized editor view.
    if(const auto bounds=editorWindowBounds(graphics->GetWindow(),false)) {
        observedParentWidth=bounds->width;observedParentHeight=bounds->height;
    }
#endif
    reconcileEditorScale=true;
#if defined(APP_API)
    // Request an exact client size, including when the old parent contains
    // unused space. The SDK's relative parent delta would retain that space.
    graphics->Resize(PLUG_WIDTH,keyboardVisible ? 936 : PLUG_HEIGHT,scale,false);
    requestStandaloneWindowSize(graphics->GetWindow(),
        static_cast<int>(graphics->WindowWidth()*graphics->GetPlatformWindowScale()),
        static_cast<int>(graphics->WindowHeight()*graphics->GetPlatformWindowScale()));
#else
    graphics->Resize(PLUG_WIDTH,keyboardVisible ? 936 : PLUG_HEIGHT,scale);
#endif
    // Hosts/WM may reject or adjust the request, or deliver their size later.
    // Re-read the parent on the next UI tick rather than displaying a request
    // as though it were the accepted scale.
    refreshEditorScale();
}

void Synth2::refreshEditorScale()
{
    if(auto* graphics=GetUI()) {
        editorScale=graphics->GetDrawScale();
        if(auto* control=graphics->GetControlWithTag(displayScaleTag))
            control->As<DisplayScaleControl>()->setScale(editorScale);
    }
}

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
    graphics->SetScaleConstraints(synth2::editor::editorMinimumScale,synth2::editor::editorMaximumScale);
    graphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    graphics->AttachTextEntryControl();
    const IText text {Typography::input,ink,"Synth2UI"};
    graphics->AttachPopupMenuControl(text);
    const auto style = DEFAULT_STYLE.WithColor(kBG,panel).WithColor(kFG,color(appearance.control))
        .WithColor(kPR,accent).WithColor(kFR,panel).WithColor(kHL,accent)
        .WithDrawShadows(false).WithLabelText(text.WithSize(Typography::button))
        .WithValueText(text.WithSize(Typography::value).WithVAlign(EVAlign::Bottom)).WithRoundness(0.1f);
    graphics->AttachControl(new ITextControl(IRECT(20,16,214,65),"SYNTH2",text.WithFont("Synth2UIBold").WithSize(29.f).WithAlign(EAlign::Near)));
    graphics->AttachControl(new ITextControl(IRECT(20,62,214,78),"VIRTUAL ANALOG SYNTHESIZER",text.WithSize(11.f).WithAlign(EAlign::Near)));
    graphics->AttachControl(new IVSliderControl(IRECT(1020,16,1260,72),128,"Master volume",style,true,EDirection::Horizontal));
    graphics->AttachControl(new DisplayScaleControl(rect(synth2::editor::editorDisplayScaleBounds),
        [this](float scale){requestEditorScale(scale);}),displayScaleTag);
    refreshEditorScale();
    graphics->AttachControl(new ITextControl(rect(synth2::editor::editorLibrarySummaryBounds),"",text.WithSize(Typography::label).WithAlign(EAlign::Near)),bankTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(756,82,820,112),[this](IControl* caller) { showImportMenu(caller); },"Add...",style));
    graphics->AttachControl(new ActionButtonControl(IRECT(826,82,878,112),[this](IControl* caller){showSaveMenu(caller);},"Save",style));
    graphics->AttachControl(new PresetDisplayControl(IRECT(240,16,878,78),[this](IControl* caller) {
        showPresetMenu(caller);
    },"",style.WithColor(kBG,color(appearance.display)).WithColor(kFG,color(appearance.display))
        .WithLabelText(text.WithFont("Synth2Mono").WithSize(Typography::patchTitle).WithFGColor(color(appearance.displayText)))),presetTag);
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
                control=new SourceChoiceControl(bounds,item.parameter,item.label,[this,index=item.parameter] {
                    const auto& range=processor.parameter(index);
                    return range.minimum+processor.normalizedValue(index)*(range.maximum-range.minimum);
                });
            else control=new PanelKnobControl(bounds,item.parameter,item.label,style.WithColor(kPR,tint));
            const auto tooltip=processor.parameter(item.parameter).name;
            control->SetTooltip(tooltip.c_str());
            graphics->AttachControl(control,parameterTag+item.parameter);
        }
    }
    // Group tempo and actual stereo tap times directly below Delay Time.
    const auto timingBounds=synth2::editor::editorDelayTimingBounds;
    for(int line=0;line<3;++line)
    {
        auto* timing=new ITextControl(IRECT(timingBounds.x,timingBounds.y+line*17,
            timingBounds.x+timingBounds.width,timingBounds.y+(line+1)*17),"",
            text.WithFGColor(muted).WithSize(Typography::auxiliary).WithAlign(EAlign::Near));
        timing->SetTooltip("Playback tempo and current left/right delay times, including Spread. Invalid host tempo uses 120 BPM.");
        graphics->AttachControl(timing,delayTimingTag+line);
    }
    refreshDelayTiming();
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
    graphics->AttachControl(new ITextControl(IRECT(20,802,530,826),"",text.WithFGColor(muted).WithSize(Typography::auxiliary).WithAlign(EAlign::Near)),statusTag);
    graphics->AttachControl(new ITextControl(rect(synth2::editor::editorVersionBounds),"v" PLUG_VERSION_STR,
        text.WithFGColor(muted).WithSize(Typography::auxiliary).WithAlign(EAlign::Far)));
    auto* quality=new FooterChoiceControl(rect(synth2::editor::editorOversamplingBounds),"Oversampling",90,
        (std::to_string(processor.oversamplingFactor())+"x").c_str(),
        [this](IControl* caller){showOversamplingMenu(caller);});
    quality->SetTooltip("Click to choose oversampling. Higher factors reduce aliasing and use more CPU. Saved with the project.");
    graphics->AttachControl(quality,oversamplingTag);
    auto* keyboard=new EditorKeyboardControl(IRECT(20,844,1260,924),processor);
    keyboard->SetText(text);
    graphics->AttachControl(keyboard,keyboardTag)->Hide(!keyboardVisible);
    graphics->AttachControl(new IVToggleControl(rect(synth2::editor::editorKeyboardToggleBounds),[this](IControl* control) {
        setKeyboardVisible(control->GetValue()>.5);
    },"",style,"Keyboard","Keyboard"),keyboardToggleTag);
    graphics->SetKeyHandlerFunc([this,graphics](const IKeyPress& key,bool up) {
        if(graphics->GetControlInTextEntry() || !editorKeyboardHasFocus(graphics->GetWindow())) {
            releaseComputerNotes();return false;
        }
        if(handleLibraryKey(key,up))return true;
        if(!keyboardVisible){releaseComputerNotes();return false;}
        const bool handled=computerKeyboard.key(key.VK,up,key.C || key.A,[this](int note,int velocity) {
            processor.setEditorNote(note,velocity,synth2::host::EditorNoteSource::computer);
        });
        if(auto* control=graphics->GetControlWithTag(keyboardTag))control->As<EditorKeyboardControl>()->refresh();
        return handled;
    });
    // A supplementary browser keeps sound-design controls on the main surface.
    graphics->AttachControl(new SearchControl(IRECT(20,136,320,172),libraryFilter.query.c_str(),text.WithAlign(EAlign::Near),[this](const char* query) {
        changeFilter(query);
    },"Search sounds..."),librarySearchTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(604,136,636,172),[this](IControl*) { stepBank(-1); },"<",style),previousBankTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(640,136,672,172),[this](IControl*) { stepBank(1); },">",style),nextBankTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(332,136,600,172),[this](IControl* caller) { showBankMenu(caller); },"All banks",style),bankFilterTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(684,136,874,172),[this](IControl* caller) { showTagMenu(caller); },"All tags",style),tagFilterTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(886,136,1034,172),[this](IControl*) {
        libraryFilter.favoritesOnly=!libraryFilter.favoritesOnly;resultOffset=0;browserStatus.clear();refreshBrowser();
    },"Favorites: off",style),favoritesTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(1046,136,1138,172),[this](IControl*) { reloadCatalog(true); },"Refresh",style),4008);
    graphics->AttachControl(new ActionButtonControl(IRECT(1150,136,1260,172),[this](IControl*) {
        libraryFilter={};resultOffset=0;browserStatus.clear();refreshBrowser();
    },"Clear filters",style),4009);
    graphics->AttachControl(new ITextControl(IRECT(244,180,1260,200),"Ctrl/Cmd+F: Search   /   Up/Down: Browse   /   Enter: Load",text.WithSize(Typography::auxiliary).WithFGColor(muted).WithAlign(EAlign::Far)),4010);
    graphics->AttachControl(new ITextControl(IRECT(20,180,226,200),"",text.WithSize(Typography::auxiliary).WithFGColor(muted).WithAlign(EAlign::Near)),libraryMatchesTag);
    using Columns=synth2::editor::EditorLibraryColumns;
    graphics->AttachControl(new ITextControl(IRECT(20+Columns::nameLeft,204,20+Columns::nameRight,226),"SOUND",text.WithSize(Typography::auxiliary).WithFGColor(muted).WithAlign(EAlign::Near)),4011);
    graphics->AttachControl(new ITextControl(IRECT(20+Columns::bankLeft,204,20+Columns::bankRight,226),"BANK",text.WithSize(Typography::auxiliary).WithFGColor(muted).WithAlign(EAlign::Near)),4012);
    graphics->AttachControl(new ITextControl(IRECT(20+Columns::tagsLeft,204,1252,226),"TAGS",text.WithSize(Typography::auxiliary).WithFGColor(muted).WithAlign(EAlign::Near)),4013);
    graphics->AttachControl(new CatalogListControl(IRECT(20,228,1260,738),catalog,selectedPatchId,keyboardPatchId,
        [this](std::size_t index) { loadLibraryPatch(index); },
        [this](const std::string& id,bool favorite) {
            std::string error;if(!catalog.setFavorite(id,favorite,error))browserStatus=error;
            else browserStatus.clear();refreshBrowser();
        },
        [this](double offset) {
            resultOffset=offset;refreshBrowser();
        },[this](const char* path) { importBank(path); }),libraryListTag);
    graphics->AttachControl(new ITextControl(IRECT(20,754,112,782),"Manual tags",text.WithSize(Typography::auxiliary).WithFGColor(muted).WithAlign(EAlign::Near)),selectedNameTag);
    graphics->AttachControl(new SearchControl(IRECT(116,754,1128,782),"",text.WithSize(Typography::button).WithAlign(EAlign::Near),[this](const char* tags) {
        std::string error;
        if(!selectedPatchId.empty() && !catalog.setTags(selectedPatchId,tags,error))browserStatus=error;
        else browserStatus="Tags saved. Separate tags with commas.";
        refreshBrowser();
    }),tagsEditTag);
    graphics->AttachControl(new ActionButtonControl(IRECT(1140,754,1260,782),[this](IControl* caller) {
        showAutomaticTagMenu(caller);
    },"Auto tags...",style.WithColor(kFG,background).WithColor(kFR,background)
        .WithLabelText(text.WithSize(Typography::auxiliary).WithFGColor(muted))),4016);
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
        for(int line=0;line<3;++line)
            if(auto* control=graphics->GetControlWithTag(delayTimingTag+line))control->Hide(page!=0);
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

void Synth2::refreshDelayTiming()
{
    auto* graphics=GetUI();if(!graphics)return;
    const auto timing=processor.delayTiming();
    std::array<std::string,3> labels {"-- BPM","L -- ms","R -- ms"};
    if(timing.bpm>0)
    {
        char text[64];
        std::snprintf(text,sizeof(text),"%.2f BPM",static_cast<double>(timing.bpm));labels[0]=text;
        std::snprintf(text,sizeof(text),"L %.2f ms",static_cast<double>(timing.leftMilliseconds));labels[1]=text;
        std::snprintf(text,sizeof(text),"R %.2f ms",static_cast<double>(timing.rightMilliseconds));labels[2]=text;
    }
    for(int line=0;line<3;++line)
        if(auto* control=graphics->GetControlWithTag(delayTimingTag+line))
            control->As<ITextControl>()->SetStr(labels[line].c_str());
}

void Synth2::refreshOversampling()
{
    syncQualityLatency();
    if(auto* graphics=GetUI())
        if(auto* control=graphics->GetControlWithTag(oversamplingTag))
        {
            control->As<FooterChoiceControl>()->setText((std::to_string(processor.oversamplingFactor())+"x").c_str());
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
    const bool requested=reconcileEditorScale;
    bool checkParentSize=requested;
#if defined(__APPLE__) && defined(AU_API)
    // AU hosts can resize the Cocoa parent without an SDK resize callback.
    // Read its geometry on the existing UI timer; unchanged bounds do no work.
    checkParentSize=true;
#endif
    if(checkParentSize && GetUI()) {
        reconcileEditorScale=false;
#if defined(APP_API)
        constexpr bool standalone=true;
#else
        constexpr bool standalone=false;
#endif
        if(const auto bounds=editorWindowBounds(GetUI()->GetWindow(),standalone)) {
#if defined(__APPLE__) && defined(AU_API)
            // Some AU hosts resize the returned editor view directly while its
            // container stays fixed. Do not mistake our own fitted view for a
            // new parent resize on the following tick.
            const auto* graphics=GetUI();
            const auto dpi=graphics->GetPlatformWindowScale();
            const auto target=synth2::editor::editorObservedResize(
                {bounds->width,bounds->height},{observedParentWidth,observedParentHeight},
                {bounds->viewWidth,bounds->viewHeight},
                {static_cast<int>(graphics->WindowWidth()*dpi),static_cast<int>(graphics->WindowHeight()*dpi)});
            if(target)OnParentWindowResize(target->width,target->height);
            observedParentWidth=bounds->width;observedParentHeight=bounds->height;
#else
            if(requested || bounds->width!=observedParentWidth || bounds->height!=observedParentHeight)
                OnParentWindowResize(bounds->width,bounds->height);
#endif
        }
    }
    refreshEditorScale();
    refreshDelayTiming();
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
                const auto matches=catalog.search(libraryFilter);for(std::size_t n=0;n<matches.size();++n)if(catalog.patches()[matches[n]].id==selectedPatchId)resultOffset=synth2::editor::editorLibraryReveal(matches.size(),visibleRows,resultOffset,n);
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
    observedParentWidth=observedParentHeight=0;
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
    refreshEditorScale();
    reconcileEditorScale=false;
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
        requestEditorScale(graphics->GetDrawScale());
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
