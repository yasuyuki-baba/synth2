#pragma once
#include <algorithm>
#include <array>

namespace synth2::dsp
{
    // Native full-load MIDI source -> sensitivity -> destination dispatches,
    // Win64 RVA37a10/379b0/378c0. Controllers are at their initial zero value.
    // Getter dials and retained destinations are deliberately distinct.
    class MidiRoutingHistory
    {
    public:
        std::array<int,2> load(const std::array<int,4>& controls) noexcept
        {
            return load(controls,[](int){});
        }

        template <typename Dispatch>
        std::array<int,2> load(const std::array<int,4>& controls,Dispatch&& observe) noexcept
        {
            std::array<int,2> rateCalls {};
            auto dispatch = [&](int destination) {
                observe(destination);
                if (destination == 43) ++rateCalls[0];
                if (destination == 48) ++rateCalls[1];
            };
            for (size_t route = 0; route < destinations.size(); ++route)
            {
                auto& destination = destinations[route];
                // Invalid source/destination choices return before touching
                // runtime state; sensitivity always re-dispatches an old route.
                if (validSource(controls[route*2])) dispatch(destination);
                dispatch(destination);
                if (validDestination(controls[route*2+1]))
                {
                    dispatch(destination);
                    destination = controls[route*2+1];
                    dispatch(destination);
                }
            }
            return rateCalls;
        }

        static bool validSource(int value) noexcept
        { return (value >= 45057 && value <= 45067) || value == 53248 || value == 57344; }

        static bool validDestination(int value) noexcept
        {
            // Read from the initialized DLL choice list, not the legal source
            // knob range; notably zero is absent and -1 is a valid DSP choice.
            constexpr std::array<int,53> choices {
                -1,76,45,95,2,12,13,11,5,8,91,15,16,17,18,19,20,21,23,22,
                25,26,27,28,29,79,80,81,61,62,63,60,90,35,83,36,98,37,52,
                53,54,55,56,39,75,92,84,85,34,43,44,48,49};
            return std::find(choices.begin(),choices.end(),value) != choices.end();
        }

        std::array<int,2> destinations {{-1,-1}};
    };
}
