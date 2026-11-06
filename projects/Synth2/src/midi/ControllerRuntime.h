#pragma once
#include <array>
#include <algorithm>

namespace synth2::midi
{
    struct ControllerRoute
    {
        int source {-1},destination {-1},base {},minimum {},maximum {127};
        float sensitivity {};
    };

    // The two native MIDI routes sum offsets before one truncation/clamp.
    // Preset values remain untouched; inputs are running performance history.
    struct ControllerRuntime
    {
        std::array<float,2> inputs {};
        void reset(const std::array<bool,2>& sources={{true,true}}) noexcept
        {for(size_t i=0;i<inputs.size();++i)if(sources[i])inputs[i]=0;}
        int dial(size_t slot,const std::array<ControllerRoute,2>& routes) const noexcept
        {
            const auto& route=routes[slot];float value=static_cast<float>(route.base);
            for(size_t i=0;i<routes.size();++i)if(routes[i].destination==route.destination)
            {
                float offset=float(route.maximum-route.minimum+1)*inputs[i];
                offset*=routes[i].sensitivity;value+=offset;
            }
            return std::clamp(static_cast<int>(value),route.minimum,route.maximum);
        }
        template<class Sink>
        void applyActive(const std::array<ControllerRoute,2>& routes,Sink&& apply) const
        {
            for(size_t slot=0;slot<routes.size();++slot)
                if(routes[slot].destination>=0 && inputs[slot]!=0)apply(routes[slot].destination,dial(slot,routes));
        }
        template<class Sink>
        void message(int status,int first,int second,const std::array<ControllerRoute,2>& routes,Sink&& apply)
        {
            const int source=(status&0xf0)*256+((status&0xf0)==176 ? first : 0);
            for(size_t slot=0;slot<routes.size();++slot)
            {
                const auto& route=routes[slot];
                if(route.destination<0 || route.source!=source)continue;
                inputs[slot]=(status&0xf0)==224 ? float((second-64)*128+first)*(1.0f/8192.0f)
                    : float(second)*(1.0f/127.0f);
                apply(route.destination,dial(slot,routes));
            }
        }
    };
}
