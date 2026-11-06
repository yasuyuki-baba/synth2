#pragma once
#include "IVKeyboardControl.h"
#include "EditorStyle.h"
#include "host/ProcessorBridge.h"

namespace synth2::editor::native {
// Own mouse notes independently of computer keys and host MIDI.
class EditorKeyboardControl final : public iplug::igraphics::IVKeyboardControl {
public:
    EditorKeyboardControl(const iplug::igraphics::IRECT& bounds,host::ProcessorBridge& processor)
        : IVKeyboardControl(bounds,36,84,true,iplug::igraphics::IColor(255,218,215,205),color(appearance.control),accent,color(appearance.border),ink),processor(processor) { SetWantsMidi(false); }
    void OnMouseDown(float x,float y,const iplug::igraphics::IMouseMod&) override { touch(x,y); }
    void OnMouseDrag(float x,float y,float,float,const iplug::igraphics::IMouseMod&) override { touch(x,y); }
    void OnMouseUp(float,float,const iplug::igraphics::IMouseMod&) override { releaseMouse(); }
    void OnMouseOut() override { releaseMouse(); }
    void releaseMouse() {
        if(mLastTouchedKey>=0)processor.setEditorNote(GetMidiNoteNumberForKey(mLastTouchedKey),0,host::EditorNoteSource::mouse);
        mLastTouchedKey=mMouseOverKey=-1;mLastVelocity=0;refresh();
    }
    void refresh() {
        for(int note=mMinNote;note<=mMaxNote;++note) {
            const bool pressed=processor.editorNotePressed(note);
            if(mPressedKeys.Get()[note-mMinNote]!=pressed)SetNoteFromMidi(note,pressed);
        }
    }
private:
    int hitKey(float x,float y) const {
        for(bool black:{true,false})
            for(int key=0;key<=mMaxNote-mMinNote;++key) {
                if(mIsBlackKeyList.Get()[key]!=black)continue;
                const float left=mKeyXPos.Get()[key];
                const float width=mWKWidth*(black ? mBKWidthRatio : 1.f);
                const float bottom=mRECT.T+mRECT.H()*(black ? mBKHeightRatio : 1.f);
                if(x>=left && x<left+width && y<=bottom)return key;
            }
        return -1;
    }
    void touch(float x,float y) {
        const int key=mRECT.Contains(x,y) ? hitKey(x,y) : -1;
        if(key==mLastTouchedKey)return;
        releaseMouse();
        if(key<0 || key>mMaxNote-mMinNote)return;
        mLastTouchedKey=mMouseOverKey=key;
        const float height=mRECT.H()*(mIsBlackKeyList.Get()[key] ? mBKHeightRatio : 1.f);
        mLastVelocity=std::clamp((y-mRECT.T)/height,1.f/127.f,1.f);
        processor.setEditorNote(GetMidiNoteNumberForKey(key),std::max(1,static_cast<int>(127*mLastVelocity)),host::EditorNoteSource::mouse);
        refresh();
    }
    host::ProcessorBridge& processor;
};
}
