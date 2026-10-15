// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <initializer_list>
#include "common/settings_enums.h"
namespace EdenXbox {
enum class XboxFastmem : std::uint8_t { Off, Hybrid };
enum class XboxFrameSkip : std::uint8_t { Off, Half, Third };
enum class XboxPreset : std::uint8_t { Custom, Balanced, TwoD, Experimental3D };
inline Settings::ResolutionSetup SafeXboxResolution(int v) {
    using R = Settings::ResolutionSetup;
    for (R option : {R::Res1_2X, R::Res3_4X, R::Res1X}) if (static_cast<int>(option) == v) return option;
    return R::Res1X;
}
inline Settings::ResolutionSetup StepXboxResolution(Settings::ResolutionSetup current, bool backward) {
    using R = Settings::ResolutionSetup;
    constexpr R options[]{R::Res1_2X, R::Res3_4X, R::Res1X};
    int index = 2;
    for (int i = 0; i < 3; ++i) if (options[i] == current) index = i;
    return options[(index + (backward ? 2 : 1)) % 3];
}
inline XboxFastmem SafeXboxFastmem(int v) { return v == 1 ? XboxFastmem::Hybrid : XboxFastmem::Off; }
inline XboxFrameSkip SafeXboxFrameSkip(int v) { return v == 1 ? XboxFrameSkip::Half : v == 2 ? XboxFrameSkip::Third : XboxFrameSkip::Off; }
inline XboxPreset SafeXboxPreset(int v) { return v >= 0 && v <= 3 ? static_cast<XboxPreset>(v) : XboxPreset::Custom; }
inline XboxPreset StepXboxPreset(XboxPreset p, bool backward) {
    if (p == XboxPreset::Custom) return backward ? XboxPreset::Experimental3D : XboxPreset::Balanced;
    const int i = static_cast<int>(p)-1;
    return static_cast<XboxPreset>((i+(backward?2:1))%3+1);
}
inline XboxFrameSkip StepXboxFrameSkip(XboxFrameSkip p, bool backward) {
    return static_cast<XboxFrameSkip>((static_cast<int>(p)+(backward?2:1))%3);
}
inline unsigned XboxPresentInterval(XboxFrameSkip s) {
    return s == XboxFrameSkip::Half ? 2u : s == XboxFrameSkip::Third ? 3u : 1u;
}
} // namespace EdenXbox
