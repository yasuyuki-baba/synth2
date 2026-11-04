#pragma once

#include "Synth1PatchSemantics.h"

#include <string_view>

Synth1PatchDocument parseSynth1PatchText(std::string_view text, bool allowNativeDefaults = false);