// SPDX-FileCopyrightText: Copyright 2026 JulianDr14
// SPDX-License-Identifier: GPL-3.0-or-later
#include "eden_uwp/uwp_system_settings.h"

#include <algorithm>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.UserProfile.h>
#include "common/logging.h"
#include "common/settings.h"
#include "video_core/performance_overlay.h"
#include "video_core/renderer_d3d12/d3d12_resource_allocator.h"
#include "video_core/renderer_d3d12/d3d12_present_filter.h"
#include "video_core/renderer_d3d12/d3d12_present_skip.h"
#include "video_core/renderer_d3d12/d3d12_cache_policy.h"
#include "core/hle/service/acc/profile_manager.h"

namespace EdenXbox {
namespace {
constexpr auto UiLanguageKey = L"system_ui_language";
constexpr auto GameLanguageKey = L"system_game_language";
constexpr auto RegionKey = L"system_region";
constexpr auto UserNameKey = L"system_user_name";
constexpr auto PerformanceStatsKey = L"system_show_performance_stats";
constexpr auto AdultModsKey = L"system_show_adult_mods";
constexpr auto PulseSessionsKey = L"system_pulse_sessions";
constexpr auto UpscalingKey = L"system_upscaling";
constexpr auto ThemeKey = L"system_ui_theme";
constexpr auto DedicatedDepthKey = L"system_xbox_dedicated_depth";
constexpr auto ResolutionKey = L"system_xbox_resolution";
constexpr auto FastmemKey = L"system_xbox_fastmem";
constexpr auto SkipKey = L"system_xbox_present_skip";
constexpr auto PresetKey = L"system_xbox_preset";
constexpr auto CacheKey = L"system_xbox_disk_shader_cache";
constexpr auto SpeedKey = L"system_xbox_speed_cap";
constexpr auto OverlayFieldsKey = L"system_xbox_overlay_fields";
constexpr auto OverlayPositionKey = L"system_xbox_overlay_position";
constexpr auto OverlayScaleKey = L"system_xbox_overlay_scale";
constexpr auto EarlyTextureGcKey = L"system_xbox_early_texture_gc";
constexpr auto JitCacheKey = L"system_xbox_jit_cache_mib";
constexpr auto PipelineEvictionKey = L"system_xbox_pipeline_eviction";

/// Saved as the enum's value, or -1 for "follow the system". A value that is not a selectable
/// option (an older or newer build's) also falls back to the system.
template <typename T>
std::optional<T> LoadChoice(const winrt::Windows::Foundation::Collections::IPropertySet& values,
                            const wchar_t* key, std::span<const T> options) {
    if (!values.HasKey(key)) return std::nullopt;
    const auto raw = winrt::unbox_value<int32_t>(values.Lookup(key));
    for (const T option : options)
        if (static_cast<int32_t>(option) == raw) return option;
    return std::nullopt;
}
template <typename T>
int32_t ChoiceValue(std::optional<T> choice) {
    return choice ? static_cast<int32_t>(*choice) : -1;
}
} // namespace

SystemSettings LoadSystemSettings() {
    SystemSettings settings;
    try {
        const auto values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        if (values.HasKey(UiLanguageKey)) {
            settings.ui_language = winrt::unbox_value<winrt::hstring>(values.Lookup(UiLanguageKey)).c_str();
            if (!I18n::FindUiLanguage(settings.ui_language)) settings.ui_language.clear();
        }
        settings.game_language = LoadChoice(values, GameLanguageKey, SelectableGameLanguages());
        settings.region = LoadChoice(values, RegionKey, SelectableRegions());
        if (values.HasKey(UserNameKey))
            settings.user_name = SanitizeUserName(
                winrt::unbox_value<winrt::hstring>(values.Lookup(UserNameKey)).c_str());
        if (values.HasKey(PerformanceStatsKey)) settings.show_performance_stats = winrt::unbox_value<bool>(values.Lookup(PerformanceStatsKey));
        if (values.HasKey(OverlayFieldsKey))
            settings.xbox_overlay_fields = static_cast<unsigned>(
                winrt::unbox_value<int32_t>(values.Lookup(OverlayFieldsKey))) & 31u;
        if (values.HasKey(OverlayPositionKey))
            settings.xbox_overlay_position = winrt::unbox_value<int32_t>(values.Lookup(OverlayPositionKey)) == 1 ? 1u : 0u;
        if (values.HasKey(OverlayScaleKey))
            settings.xbox_overlay_scale = std::min(2, std::max(0, winrt::unbox_value<int32_t>(values.Lookup(OverlayScaleKey))));
        if (values.HasKey(AdultModsKey)) settings.show_adult_mods = winrt::unbox_value<bool>(values.Lookup(AdultModsKey));
        if (values.HasKey(PulseSessionsKey)) settings.pulse_sessions = winrt::unbox_value<bool>(values.Lookup(PulseSessionsKey));
        if (values.HasKey(UpscalingKey))
            settings.upscaling = UpscalingFromValue(winrt::unbox_value<int32_t>(values.Lookup(UpscalingKey)))
                                     .value_or(Upscaling::Bilinear);
        if (values.HasKey(ThemeKey))
            settings.ui_theme = UiThemeFromValue(winrt::unbox_value<int32_t>(values.Lookup(ThemeKey)));
        if (values.HasKey(DedicatedDepthKey))
            settings.xbox_dedicated_depth = winrt::unbox_value<bool>(values.Lookup(DedicatedDepthKey));
        if (values.HasKey(ResolutionKey))
            settings.xbox_resolution = SafeXboxResolution(winrt::unbox_value<int32_t>(values.Lookup(ResolutionKey)));
        if (values.HasKey(FastmemKey))
            settings.xbox_fastmem = SafeXboxFastmem(winrt::unbox_value<int32_t>(values.Lookup(FastmemKey)));
        if (values.HasKey(SkipKey))
            settings.xbox_frame_skip = SafeXboxFrameSkip(winrt::unbox_value<int32_t>(values.Lookup(SkipKey)));
        if (values.HasKey(PresetKey))
            settings.xbox_preset = SafeXboxPreset(winrt::unbox_value<int32_t>(values.Lookup(PresetKey)));
        if (values.HasKey(CacheKey))
            settings.xbox_disk_shader_cache = winrt::unbox_value<bool>(values.Lookup(CacheKey));
        if (values.HasKey(SpeedKey))
            settings.xbox_speed_cap = winrt::unbox_value<bool>(values.Lookup(SpeedKey));
        if (values.HasKey(EarlyTextureGcKey))
            settings.xbox_early_texture_gc = winrt::unbox_value<bool>(values.Lookup(EarlyTextureGcKey));
        if (values.HasKey(JitCacheKey))
            settings.xbox_jit_cache_mib = SafeXboxJitCache(winrt::unbox_value<int32_t>(values.Lookup(JitCacheKey)));
        if (values.HasKey(PipelineEvictionKey))
            settings.xbox_pipeline_eviction = winrt::unbox_value<bool>(values.Lookup(PipelineEvictionKey));
    } catch (...) { /* Missing or invalid values keep following the system. */ }
    return settings;
}

bool SaveSystemSettings(const SystemSettings& settings) {
    try {
        const auto values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        values.Insert(UiLanguageKey, winrt::box_value(winrt::hstring{settings.ui_language}));
        values.Insert(GameLanguageKey, winrt::box_value(ChoiceValue(settings.game_language)));
        values.Insert(RegionKey, winrt::box_value(ChoiceValue(settings.region)));
        values.Insert(UserNameKey, winrt::box_value(winrt::hstring{SanitizeUserName(settings.user_name)}));
        values.Insert(PerformanceStatsKey, winrt::box_value(settings.show_performance_stats));
        values.Insert(OverlayFieldsKey, winrt::box_value(static_cast<int32_t>(settings.xbox_overlay_fields)));
        values.Insert(OverlayPositionKey, winrt::box_value(static_cast<int32_t>(settings.xbox_overlay_position)));
        values.Insert(OverlayScaleKey, winrt::box_value(static_cast<int32_t>(settings.xbox_overlay_scale)));
        values.Insert(AdultModsKey, winrt::box_value(settings.show_adult_mods));
        values.Insert(PulseSessionsKey, winrt::box_value(settings.pulse_sessions));
        values.Insert(UpscalingKey, winrt::box_value(static_cast<int32_t>(settings.upscaling)));
        values.Insert(ThemeKey, winrt::box_value(static_cast<int32_t>(settings.ui_theme)));
        values.Insert(DedicatedDepthKey, winrt::box_value(settings.xbox_dedicated_depth));
        values.Insert(ResolutionKey, winrt::box_value(static_cast<int32_t>(settings.xbox_resolution)));
        values.Insert(FastmemKey, winrt::box_value(static_cast<int32_t>(settings.xbox_fastmem)));
        values.Insert(SkipKey, winrt::box_value(static_cast<int32_t>(settings.xbox_frame_skip)));
        values.Insert(PresetKey, winrt::box_value(static_cast<int32_t>(settings.xbox_preset)));
        values.Insert(CacheKey, winrt::box_value(settings.xbox_disk_shader_cache));
        values.Insert(SpeedKey, winrt::box_value(settings.xbox_speed_cap));
        values.Insert(EarlyTextureGcKey, winrt::box_value(settings.xbox_early_texture_gc));
        values.Insert(JitCacheKey, winrt::box_value(static_cast<int32_t>(settings.xbox_jit_cache_mib)));
        values.Insert(PipelineEvictionKey, winrt::box_value(settings.xbox_pipeline_eviction));
        return true;
    } catch (...) { return false; }
}

SystemLocale DetectSystemLocale() {
    SystemLocale locale;
    using winrt::Windows::System::UserProfile::GlobalizationPreferences;
    try {
        for (const auto& language : GlobalizationPreferences::Languages())
            locale.languages.emplace_back(language.c_str());
    } catch (const winrt::hresult_error&) {}
    try {
        locale.region = GlobalizationPreferences::HomeGeographicRegion().c_str();
    } catch (const winrt::hresult_error&) {}
    return locale;
}

ResolvedSystem ApplySystemSettings() {
    const auto locale = DetectSystemLocale();
    const auto settings = LoadSystemSettings();
    VideoCore::performance_overlay_enabled.store(settings.show_performance_stats, std::memory_order_relaxed);
    VideoCore::performance_overlay_fields.store(settings.xbox_overlay_fields, std::memory_order_relaxed);
    VideoCore::performance_overlay_position.store(settings.xbox_overlay_position, std::memory_order_relaxed);
    VideoCore::performance_overlay_scale.store(settings.xbox_overlay_scale, std::memory_order_relaxed);
    D3D12::SetXboxEarlyTextureGcEnabled(settings.xbox_early_texture_gc);
    SetUiTheme(settings.ui_theme);
    D3D12::SetXboxDedicatedDepthEnabled(settings.xbox_dedicated_depth);
    D3D12::SetXboxPresentInterval(XboxPresentInterval(settings.xbox_frame_skip));
    ApplyUpscaling(settings.upscaling);
    const auto resolved = ResolveSystem(settings, locale);
    I18n::SetUiLanguage(*resolved.ui_language);
    Settings::values.language_index = resolved.game_language;
    Settings::values.region_index = resolved.region;
    LOG_INFO(Frontend, "System: Windows language {}, region {}; interface {}, game language {}, region {}",
             locale.languages.empty() ? std::string{"(none)"} : I18n::ToUtf8(locale.languages.front()),
             locale.region.empty() ? std::string{"(none)"} : I18n::ToUtf8(locale.region),
             I18n::ToUtf8(resolved.ui_language->tag), Settings::CanonicalizeEnum(resolved.game_language),
             Settings::CanonicalizeEnum(resolved.region));
    return resolved;
}

void ApplyUpscaling(Upscaling upscaling) {
    D3D12::SetNearestPresentFilterEnabled(upscaling == Upscaling::Nearest);
    const UpscalingSettings values = ToSettings(upscaling);
    Settings::values.scaling_filter.SetValue(values.filter);
    Settings::values.fsr_sharpening_slider.SetValue(values.fsr_sharpening_slider);
}

bool ApplyUserName(Service::Account::ProfileManager& profiles, std::wstring_view name) {
    // No saved name is the default one: a profile renamed before goes back to it.
    auto clean = SanitizeUserName(name);
    if (clean.empty()) clean = DefaultUserName;
    const auto index = static_cast<std::size_t>(std::clamp<s32>(
        Settings::values.current_user.GetValue(), 0, static_cast<s32>(Service::Account::MAX_USERS) - 1));
    const auto uuid = profiles.GetUser(index);
    Service::Account::ProfileBase base{};
    if (!uuid || !profiles.GetProfileBase(*uuid, base)) return false;
    // Ten UTF-16 units are at most 30 UTF-8 bytes, inside the 32-byte field with room to spare.
    const auto utf8 = I18n::ToUtf8(clean);
    Service::Account::ProfileUsername username{};
    std::copy_n(utf8.begin(), std::min(utf8.size(), username.size()), username.begin());
    if (base.username == username) return true;
    base.username = username;
    const bool saved = profiles.SetProfileBase(*uuid, base);
    LOG_INFO(Frontend, "System: profile {} renamed to '{}'{}", index, utf8, saved ? "" : " (failed)");
    return saved;
}
} // namespace EdenXbox
