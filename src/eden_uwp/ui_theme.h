// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
namespace EdenXbox {
enum class UiTheme : std::uint8_t { Eden, XboxGreen, Amoled };
inline std::atomic<UiTheme> current_ui_theme{UiTheme::Eden};
inline void SetUiTheme(UiTheme theme) noexcept {
    current_ui_theme.store(theme, std::memory_order_release);
}
inline UiTheme GetUiTheme() noexcept {
    return current_ui_theme.load(std::memory_order_acquire);
}
inline UiTheme UiThemeFromValue(std::int32_t value) noexcept {
    if (value == 1) return UiTheme::XboxGreen;
    if (value == 2) return UiTheme::Amoled;
    return UiTheme::Eden;
}
inline UiTheme StepUiTheme(UiTheme theme, bool backward) noexcept {
    const int index = static_cast<int>(theme);
    return static_cast<UiTheme>((index + (backward ? 2 : 1)) % 3);
}
inline unsigned ThemeColor(unsigned color) noexcept {
    switch (GetUiTheme()) {
    case UiTheme::XboxGreen:
        switch (color) {
        case 0x080d15: case 0x0b1018: return 0x08120d;
        case 0x202b39: return 0x15271c;
        case 0x151f2c: case 0x121c29: return 0x102017;
        case 0x304b49: return 0x205d38;
        case 0x77e3bd: return 0x55e28d;
        case 0x102921: return 0x092313;
        case 0x2a3848: return 0x294f38;
        default: return color;
        }
    case UiTheme::Amoled:
        switch (color) {
        case 0x080d15: case 0x0b1018: return 0x000000;
        case 0x202b39: return 0x090909;
        case 0x151f2c: case 0x121c29: return 0x141414;
        case 0x304b49: return 0x203d32;
        case 0x77e3bd: return 0x62efac;
        case 0x102921: return 0x101b15;
        case 0x2a3848: return 0x282828;
        default: return color;
        }
    default: return color;
    }
}
} // namespace EdenXbox
