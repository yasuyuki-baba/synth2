#pragma once
// Included only by the separate, opt-in profiling target. No timers in the plugin.
#include <chrono>
namespace synth2::hostprofile
{
struct Record { float parameters{}, synthesis{}, effects{}; };
inline thread_local Record current;
class Scope
{
    float& output;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
public:
    explicit Scope(float& value) noexcept : output(value) {}
    ~Scope() { output+=std::chrono::duration<float,std::micro>(std::chrono::steady_clock::now()-start).count(); }
};
}
