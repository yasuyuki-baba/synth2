#pragma once
#include <cstddef>

namespace synth2::editor::native
{
inline constexpr int oversamplingTag=1013;
inline constexpr int displayScaleTag=1014;
inline constexpr int bankTag = 1000, presetTag = 1001, statusTag = 1002, parameterTag = 2000;
inline constexpr int panelTag = 3000;
inline constexpr int keyboardTag=1010, keyboardToggleTag=1011;
inline constexpr int libraryListTag=4000, librarySearchTag=4001, bankFilterTag=4002, tagFilterTag=4003;
inline constexpr int libraryMatchesTag=4017, previousBankTag=4018, nextBankTag=4019;
inline constexpr int favoritesTag=4004, tagsEditTag=4005, selectedNameTag=4006;
inline constexpr int libraryFirstTag=4000, libraryLastTag=4020, tabsTag=1004;
inline constexpr std::size_t visibleRows=17;
inline constexpr float libraryRowHeight=30.f;
}
