#pragma once
#include <Synth1PatchSemantics.h>
#include <string>
#include <string_view>

namespace synth2
{
    bool hasPatchTitle(std::string_view text);
    Synth1PatchDocument parsePatchDocument(std::string_view text);
    std::string trimPatchLine(std::string_view text);
}
