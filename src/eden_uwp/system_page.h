// SPDX-FileCopyrightText: Copyright 2026 JulianDr14
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Settings > System: the page's rows, what each shows and how input changes them. The library
// loop routes input here and acts on the returned effect (save, clear the cache, ...); the canvas
// draws from the same state and the layout below, which pointer hit tests share.

#include <optional>
#include <string>

#include "eden_uwp/localization.h"
#include "eden_uwp/system_settings.h"
#include "eden_uwp/text_entry.h"
#include "eden_uwp/text_entry_panel.h"

namespace EdenXbox {

struct SystemPage {
    enum Row : std::size_t { UserName, InterfaceLanguage, GameLanguage, Region,
    UiThemeChoice, CheckUpdates, AutoUpdateCheck, D3D11HardwareTest, XboxDepthFix, XboxDepthProbe, PerfPreset, InternalResolution, FastmemMode,
    DisplayFrameSkip, DiskShaderCache, SpeedCap, JitCacheSize, PipelineEviction, HudFps, HudFrame, HudCpu, HudGpu,
    HudMemory, HudPosition, HudScale, EarlyTextureGc, AdultMods, PerformanceStats,
    PulseSessions, ClearCache, RowCount };

    bool open{};
    std::size_t selected{};
    SystemSettings settings; ///< As saved.
    SystemLocale locale;     ///< As Windows reports it, for the "Automatic (...)" values.
    std::wstring notice;
    std::optional<TextEntry> name_editor;
    std::wstring editor_notice;
};

enum class SystemPageEffect : std::uint8_t {
    None,
    Changed,    ///< Settings changed: save and apply them.
    EditName,   ///< The name editor opened.
    NameDone,   ///< The editor closed with a new name in settings: save and apply.
    ClearCache, ///< Clear the library cache.
    CheckUpdates, ///< Start a read-only GitHub Releases query.
    D3D11HardwareTest, ///< Run hardware-only SM4 offscreen triangle; not a game backend.
};

enum class SystemInput : std::uint8_t { Up, Down, Left, Right, Confirm, Back };

namespace SystemPageDetail {
inline std::optional<std::wstring> StepUiLanguage(const std::wstring& current, bool back) {
    const auto languages = I18n::UiLanguages();
    const std::size_t count = languages.size() + 1;
    std::size_t slot = 0;
    for (std::size_t i = 0; i < languages.size(); ++i)
        if (languages[i].tag == current) slot = i + 1;
    slot = (slot + (back ? count - 1 : 1)) % count;
    return slot == 0 ? std::wstring{} : std::wstring{languages[slot - 1].tag};
}
} // namespace SystemPageDetail

/// Applies one input to the page (not to the name editor, which the library drives directly).
inline SystemPageEffect HandleSystemInput(SystemPage& page, SystemInput input) {
    using Row = SystemPage::Row;
    switch (input) {
    case SystemInput::Up:
        page.selected = (page.selected + Row::RowCount - 1) % Row::RowCount;
        return SystemPageEffect::None;
    case SystemInput::Down:
        page.selected = (page.selected + 1) % Row::RowCount;
        return SystemPageEffect::None;
    case SystemInput::Back:
        page.open = false;
        return SystemPageEffect::None;
    case SystemInput::Left:
    case SystemInput::Right:
    case SystemInput::Confirm:
        break;
    }
    const bool back = input == SystemInput::Left;
    auto& settings = page.settings;
    switch (static_cast<Row>(page.selected)) {
    case Row::UserName:
        if (input != SystemInput::Confirm) return SystemPageEffect::None;
        page.name_editor.emplace(settings.user_name, MaxUserNameLength);
        page.editor_notice.clear();
        return SystemPageEffect::EditName;
    case Row::InterfaceLanguage:
        settings.ui_language = *SystemPageDetail::StepUiLanguage(settings.ui_language, back);
        return SystemPageEffect::Changed;
    case Row::GameLanguage:
        settings.game_language = StepChoice(settings.game_language, SelectableGameLanguages(), back);
        return SystemPageEffect::Changed;
    case Row::Region:
        settings.region = StepChoice(settings.region, SelectableRegions(), back);
        return SystemPageEffect::Changed;
    case Row::UiThemeChoice:
        settings.ui_theme = StepUiTheme(settings.ui_theme, back);
        return SystemPageEffect::Changed;
    case Row::CheckUpdates:
        return input == SystemInput::Confirm ? SystemPageEffect::CheckUpdates : SystemPageEffect::None;
    case Row::AutoUpdateCheck:
        settings.auto_check_updates = !settings.auto_check_updates;
        return SystemPageEffect::Changed;
    case Row::D3D11HardwareTest:
        return input == SystemInput::Confirm ? SystemPageEffect::D3D11HardwareTest : SystemPageEffect::None;
    case Row::XboxDepthFix:
        settings.xbox_dedicated_depth = !settings.xbox_dedicated_depth;
        return SystemPageEffect::Changed;
    case Row::XboxDepthProbe:
        settings.xbox_depth_probe_mode = (settings.xbox_depth_probe_mode + (back ? 3u : 1u)) % 4u;
        settings.xbox_preset = XboxPreset::Custom;
        return SystemPageEffect::Changed;
    case Row::PerfPreset:
        ApplyXboxPreset(settings, StepXboxPreset(settings.xbox_preset, back));
        return SystemPageEffect::Changed;
    case Row::InternalResolution:
        settings.xbox_resolution = StepXboxResolution(settings.xbox_resolution, back);
        settings.xbox_preset = XboxPreset::Custom;
        return SystemPageEffect::Changed;
    case Row::FastmemMode:
        settings.xbox_fastmem = settings.xbox_fastmem == XboxFastmem::Off ? XboxFastmem::Hybrid : XboxFastmem::Off;
        settings.xbox_preset = XboxPreset::Custom;
        return SystemPageEffect::Changed;
    case Row::DisplayFrameSkip:
        settings.xbox_frame_skip = StepXboxFrameSkip(settings.xbox_frame_skip, back);
        settings.xbox_preset = XboxPreset::Custom;
        return SystemPageEffect::Changed;
    case Row::DiskShaderCache:
        settings.xbox_disk_shader_cache = !settings.xbox_disk_shader_cache;
        settings.xbox_preset = XboxPreset::Custom;
        return SystemPageEffect::Changed;
    case Row::SpeedCap:
        settings.xbox_speed_cap = !settings.xbox_speed_cap;
        settings.xbox_preset = XboxPreset::Custom;
        return SystemPageEffect::Changed;
    case Row::JitCacheSize:
        settings.xbox_jit_cache_mib = StepXboxJitCache(settings.xbox_jit_cache_mib, back);
        settings.xbox_preset = XboxPreset::Custom;
        return SystemPageEffect::Changed;
    case Row::PipelineEviction:
        settings.xbox_pipeline_eviction = !settings.xbox_pipeline_eviction;
        settings.xbox_preset = XboxPreset::Custom;
        return SystemPageEffect::Changed;
    case Row::HudFps:
        settings.xbox_overlay_fields ^= VideoCore::OverlayFps;
        return SystemPageEffect::Changed;
    case Row::HudFrame:
        settings.xbox_overlay_fields ^= VideoCore::OverlayFrameTime;
        return SystemPageEffect::Changed;
    case Row::HudCpu:
        settings.xbox_overlay_fields ^= VideoCore::OverlayCpu;
        return SystemPageEffect::Changed;
    case Row::HudGpu:
        settings.xbox_overlay_fields ^= VideoCore::OverlayGpu;
        return SystemPageEffect::Changed;
    case Row::HudMemory:
        settings.xbox_overlay_fields ^= VideoCore::OverlayMemory;
        return SystemPageEffect::Changed;
    case Row::HudPosition:
        settings.xbox_overlay_position = settings.xbox_overlay_position == 0 ? 1 : 0;
        return SystemPageEffect::Changed;
    case Row::HudScale:
        settings.xbox_overlay_scale = (settings.xbox_overlay_scale + (back ? 2u : 1u)) % 3u;
        return SystemPageEffect::Changed;
    case Row::EarlyTextureGc:
        settings.xbox_early_texture_gc = !settings.xbox_early_texture_gc;
        settings.xbox_preset = XboxPreset::Custom;
        return SystemPageEffect::Changed;
    case Row::AdultMods:
        settings.show_adult_mods = !settings.show_adult_mods;
        return SystemPageEffect::Changed;
    case Row::PerformanceStats:
        settings.show_performance_stats = !settings.show_performance_stats;
        return SystemPageEffect::Changed;
    case Row::PulseSessions:
        settings.pulse_sessions = !settings.pulse_sessions;
        return SystemPageEffect::Changed;
    case Row::ClearCache:
        return input == SystemInput::Confirm ? SystemPageEffect::ClearCache : SystemPageEffect::None;
    case Row::RowCount:
        break;
    }
    return SystemPageEffect::None;
}

/// Closes the name editor; `accept` keeps its text. An empty name is refused with a notice.
inline SystemPageEffect FinishNameEditor(SystemPage& page, bool accept) {
    if (!page.name_editor) return SystemPageEffect::None;
    if (!accept) {
        page.name_editor.reset();
        return SystemPageEffect::None;
    }
    auto name = SanitizeUserName(page.name_editor->Value());
    if (name.empty()) {
        page.editor_notice = I18n::Tr(I18n::Text::NameEmpty);
        return SystemPageEffect::None;
    }
    page.name_editor.reset();
    // The default name is stored as "no name": it then follows any future default.
    page.settings.user_name = name == DefaultUserName ? std::wstring{} : std::move(name);
    return SystemPageEffect::NameDone;
}

inline I18n::Text SystemRowTitle(SystemPage::Row row) {
    using I18n::Text;
    switch (row) {
    case SystemPage::UserName:          return Text::UserName;
    case SystemPage::InterfaceLanguage: return Text::InterfaceLanguage;
    case SystemPage::GameLanguage:      return Text::GameLanguage;
    case SystemPage::Region:            return Text::ConsoleRegion;
    case SystemPage::UiThemeChoice:     return Text::ThemeTitle;
    case SystemPage::CheckUpdates:      return Text::UpdateCheckTitle;
    case SystemPage::AutoUpdateCheck:   return Text::UpdateAutoTitle;
    case SystemPage::D3D11HardwareTest: return Text::D3D11TestTitle;
    case SystemPage::XboxDepthFix:      return Text::XboxDepthFixTitle;
    case SystemPage::XboxDepthProbe:    return Text::XboxDepthProbeTitle;
    case SystemPage::PerfPreset:        return Text::PerfPresetTitle;
    case SystemPage::InternalResolution:return Text::InternalResolutionTitle;
    case SystemPage::FastmemMode:       return Text::FastmemTitle;
    case SystemPage::DisplayFrameSkip:  return Text::DisplaySkipTitle;
    case SystemPage::DiskShaderCache:   return Text::DiskCacheTitle;
    case SystemPage::SpeedCap:          return Text::SpeedCapTitle;
    case SystemPage::JitCacheSize:      return Text::JitCacheTitle;
    case SystemPage::PipelineEviction:  return Text::PipelineEvictTitle;
    case SystemPage::HudFps:            return Text::HudFpsTitle;
    case SystemPage::HudFrame:          return Text::HudFrameTitle;
    case SystemPage::HudCpu:            return Text::HudCpuTitle;
    case SystemPage::HudGpu:            return Text::HudGpuTitle;
    case SystemPage::HudMemory:         return Text::HudMemoryTitle;
    case SystemPage::HudPosition:       return Text::HudPositionTitle;
    case SystemPage::HudScale:          return Text::HudScaleTitle;
    case SystemPage::EarlyTextureGc:    return Text::EarlyTextureGcTitle;
    case SystemPage::PerformanceStats:  return Text::PerformanceStats;
    case SystemPage::AdultMods:         return Text::ModsAdult;
    case SystemPage::PulseSessions:     return Text::PulseSessions;
    default:                            return Text::ClearLibraryCache;
    }
}

inline I18n::Text SystemRowHint(SystemPage::Row row) {
    using I18n::Text;
    switch (row) {
    case SystemPage::UserName:          return Text::UserNameHint;
    case SystemPage::InterfaceLanguage: return Text::InterfaceLanguageHint;
    case SystemPage::GameLanguage:      return Text::GameLanguageHint;
    case SystemPage::Region:            return Text::ConsoleRegionHint;
    case SystemPage::UiThemeChoice:     return Text::ThemeHint;
    case SystemPage::CheckUpdates:      return Text::UpdateCheckHint;
    case SystemPage::AutoUpdateCheck:   return Text::UpdateAutoHint;
    case SystemPage::D3D11HardwareTest: return Text::D3D11TestHint;
    case SystemPage::XboxDepthFix:      return Text::XboxDepthFixHint;
    case SystemPage::XboxDepthProbe:    return Text::XboxDepthProbeHint;
    case SystemPage::PerfPreset:        return Text::PerfPresetHint;
    case SystemPage::InternalResolution:return Text::InternalResolutionHint;
    case SystemPage::FastmemMode:       return Text::FastmemHint;
    case SystemPage::DisplayFrameSkip:  return Text::DisplaySkipHint;
    case SystemPage::DiskShaderCache:   return Text::DiskCacheHint;
    case SystemPage::SpeedCap:          return Text::SpeedCapHint;
    case SystemPage::JitCacheSize:      return Text::JitCacheHint;
    case SystemPage::PipelineEviction:  return Text::PipelineEvictHint;
    case SystemPage::HudFps:
    case SystemPage::HudFrame:
    case SystemPage::HudCpu:
    case SystemPage::HudGpu:
    case SystemPage::HudMemory:         return Text::HudFieldsHint;
    case SystemPage::HudPosition:       return Text::HudPositionHint;
    case SystemPage::HudScale:          return Text::HudScaleHint;
    case SystemPage::EarlyTextureGc:    return Text::EarlyTextureGcHint;
    case SystemPage::PerformanceStats:  return Text::PerformanceStatsHint;
    case SystemPage::AdultMods:         return Text::ModsAdultHint;
    case SystemPage::PulseSessions:     return Text::PulseSessionsHint;
    default:                            return Text::ClearLibraryCacheHint;
    }
}

/// The row's current value; choices that follow the system read "Automatic · <what it is>".
inline std::wstring SystemRowValue(const SystemPage& page, SystemPage::Row row) {
    using I18n::Text;
    const auto& settings = page.settings;
    const auto automatic = [](std::wstring_view value) {
        return I18n::Format(Text::AutomaticValue, {value});
    };
    switch (row) {
    case SystemPage::UserName:
        return settings.user_name.empty() ? std::wstring{DefaultUserName} : settings.user_name;
    case SystemPage::InterfaceLanguage:
        if (const auto* language = I18n::FindUiLanguage(settings.ui_language))
            return std::wstring{language->native_name};
        return automatic(I18n::NegotiateUiLanguage(I18n::UiLanguages(), page.locale.languages).native_name);
    case SystemPage::GameLanguage:
        if (settings.game_language) return std::wstring{GameLanguageName(*settings.game_language)};
        return automatic(GameLanguageName(GameLanguageForLocale(page.locale)));
    case SystemPage::Region:
        if (settings.region) return std::wstring{I18n::Tr(RegionText(*settings.region))};
        return automatic(I18n::Tr(RegionText(RegionForLocale(page.locale))));
    case SystemPage::UiThemeChoice:
        return std::wstring{I18n::Tr(settings.ui_theme == UiTheme::XboxGreen ? Text::ThemeXboxGreen :
            settings.ui_theme == UiTheme::Amoled ? Text::ThemeAmoled : Text::ThemeEden)};
    case SystemPage::CheckUpdates:
    case SystemPage::D3D11HardwareTest: return {};
    case SystemPage::AutoUpdateCheck:
        return std::wstring{I18n::Tr(settings.auto_check_updates ? Text::Yes : Text::No)};
    case SystemPage::XboxDepthFix:
        return std::wstring{I18n::Tr(settings.xbox_dedicated_depth ? Text::Yes : Text::No)};
    case SystemPage::XboxDepthProbe:
        return std::wstring{I18n::Tr(settings.xbox_depth_probe_mode == 1 ? Text::XboxDepthSkipQuery :
            settings.xbox_depth_probe_mode == 2 ? Text::XboxDepthSingleDsv :
            settings.xbox_depth_probe_mode == 3 ? Text::XboxDepthBoth :
            Text::XboxDepthBaseline)};
    case SystemPage::PerfPreset:
        return std::wstring{I18n::Tr(settings.xbox_preset == XboxPreset::Balanced ? Text::PerfPresetBalanced :
            settings.xbox_preset == XboxPreset::TwoD ? Text::PerfPreset2D :
            settings.xbox_preset == XboxPreset::Experimental3D ? Text::PerfPreset3D : Text::PerfPresetCustom)};
    case SystemPage::InternalResolution:
        return std::wstring{I18n::Tr(settings.xbox_resolution == Settings::ResolutionSetup::Res1_2X ?
            Text::InternalResolution50 : settings.xbox_resolution == Settings::ResolutionSetup::Res3_4X ?
            Text::InternalResolution75 : Text::InternalResolution100)};
    case SystemPage::FastmemMode:
        return std::wstring{I18n::Tr(settings.xbox_fastmem == XboxFastmem::Hybrid ? Text::FastmemHybrid : Text::FastmemOff)};
    case SystemPage::DisplayFrameSkip:
        return std::wstring{I18n::Tr(settings.xbox_frame_skip == XboxFrameSkip::Half ?
            Text::DisplaySkipHalf : settings.xbox_frame_skip == XboxFrameSkip::Third ?
            Text::DisplaySkipThird : Text::DisplaySkipOff)};
    case SystemPage::DiskShaderCache:
        return std::wstring{I18n::Tr(settings.xbox_disk_shader_cache ? Text::Yes : Text::No)};
    case SystemPage::SpeedCap:
        return std::wstring{I18n::Tr(settings.xbox_speed_cap ? Text::Yes : Text::No)};
    case SystemPage::JitCacheSize:
        return std::to_wstring(settings.xbox_jit_cache_mib) + L" MiB";
    case SystemPage::PipelineEviction:
        return std::wstring{I18n::Tr(settings.xbox_pipeline_eviction ? Text::Yes : Text::No)};
    case SystemPage::HudFps:
    case SystemPage::HudFrame:
    case SystemPage::HudCpu:
    case SystemPage::HudGpu:
    case SystemPage::HudMemory: {
        const unsigned flag = row == SystemPage::HudFps ? VideoCore::OverlayFps :
                              row == SystemPage::HudFrame ? VideoCore::OverlayFrameTime :
                              row == SystemPage::HudCpu ? VideoCore::OverlayCpu :
                              row == SystemPage::HudGpu ? VideoCore::OverlayGpu : VideoCore::OverlayMemory;
        return std::wstring{I18n::Tr((settings.xbox_overlay_fields & flag) ? Text::Yes : Text::No)};
    }
    case SystemPage::HudPosition:
        return std::wstring{I18n::Tr(settings.xbox_overlay_position == 1 ? Text::HudTopLeft : Text::HudTopRight)};
    case SystemPage::HudScale:
        return std::wstring{I18n::Tr(settings.xbox_overlay_scale == 0 ? Text::HudSmall :
                                       settings.xbox_overlay_scale == 2 ? Text::HudLarge : Text::HudNormal)};
    case SystemPage::EarlyTextureGc:
        return std::wstring{I18n::Tr(settings.xbox_early_texture_gc ? Text::EarlyTextureGcOn : Text::EarlyTextureGcOff)};
    case SystemPage::PerformanceStats:
        return std::wstring{I18n::Tr(settings.show_performance_stats ? Text::Yes : Text::No)};
    case SystemPage::AdultMods:
        return std::wstring{I18n::Tr(settings.show_adult_mods ? Text::ModsAdultShown : Text::ModsAdultHidden)};
    case SystemPage::PulseSessions:
        return std::wstring{I18n::Tr(settings.pulse_sessions ? Text::Yes : Text::No)};
    default:
        return {};
    }
}

/// Geometry shared by the canvas and pointer hit tests (1280x720 layout units).
struct SystemPageLayout {
    static constexpr std::size_t VisibleRows = 8;
    static constexpr std::size_t FirstRow(std::size_t selected) {
        return selected < VisibleRows ? 0 : selected - VisibleRows + 1;
    }
    static constexpr float PanelX = 140, PanelY = 72, PanelWidth = 1000, PanelHeight = 612;
    static constexpr float RowX = 180, RowWidth = 920, RowTop = 172, RowStride = 52, RowHeight = 50;
    static constexpr float Notice = 594, Footer = 630;
    /// The name editor's keyboard panel (text_entry_panel.h), centered on the screen.
    static constexpr float EditorX = (1280 - TextEntryPanelLayout::Width) / 2;
    static constexpr float EditorY = (720 - TextEntryPanelLayout::Height) / 2;
};

} // namespace EdenXbox
