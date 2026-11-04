#pragma once
#include <cstddef>
namespace testheap
{
struct Counts { std::size_t allocations{}, deallocations{}, cAllocations{}, cDeallocations{}; };
// Only the measured host thread is tracked. GTest/setup/inspection stay outside.
extern thread_local Counts* active;
struct Scope
{
    explicit Scope(Counts& counts) { active=&counts; }
    ~Scope() { active=nullptr; }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
}
