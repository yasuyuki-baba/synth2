#pragma once
#include <cstddef>

namespace synth2::dsp
{
    enum class NoteTransitionKind { Retrigger,Legato,RetainedRetrigger };
    struct NoteTransition
    {
        NoteTransitionKind kind {NoteTransitionKind::Retrigger};
        bool glide {};
        bool legato() const noexcept { return kind==NoteTransitionKind::Legato; }
        bool preserveOnBookkeepingStop() const noexcept { return kind!=NoteTransitionKind::Retrigger; }
    };

    // Table-driven State: logical note lifetime is independent of physical
    // envelope phases and retained core history. No allocation or virtual calls.
    class VoiceLifecycle
    {
    public:
        enum class Phase { Idle,Playing,Releasing };
        enum class StopAction { Release,Preserve,Reset };
        void schedule(NoteTransition next) noexcept { pending=next; }
        const NoteTransition& next() const noexcept { return pending; }
        void consumeTransition() noexcept { pending={}; }
        void noteStarted() noexcept { advance(Event::Start); }
        void retired() noexcept { advance(Event::Retire); }
        Phase phase() const noexcept { return current; }
        StopAction stop(bool tail) noexcept
        {
            if(tail) { advance(Event::Release);return StopAction::Release; }
            if(pending.preserveOnBookkeepingStop())return StopAction::Preserve;
            // Reset is completed by the caller, which then retires the note.
            return StopAction::Reset;
        }
    private:
        enum class Event { Start,Release,Retire };
        static constexpr Phase transitions[3][3] {
            {Phase::Playing,Phase::Idle,Phase::Idle},
            {Phase::Playing,Phase::Releasing,Phase::Idle},
            {Phase::Playing,Phase::Releasing,Phase::Idle}
        };
        void advance(Event event) noexcept
        { current=transitions[static_cast<std::size_t>(current)][static_cast<std::size_t>(event)]; }
        Phase current {Phase::Idle};
        NoteTransition pending;
    };
}
