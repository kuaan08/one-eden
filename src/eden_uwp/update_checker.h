// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
namespace EdenXbox::Updates {
using Version = std::array<unsigned, 4>;
constexpr std::string_view TagPrefix = "xbox-one-dev-";
inline bool ParseVersion(std::string_view text, Version& out) noexcept {
    Version v{};
    for (unsigned i=0;i<4;++i) {
        const auto pos=text.find('.');
        const auto field=text.substr(0,pos);
        if(field.empty()) return false;
        unsigned num=0;
        auto [ptr,ec]=std::from_chars(field.data(), field.data()+field.size(), num);
        if(ec!=std::errc{} || ptr!=field.data()+field.size() || num>65535) return false;
        v[i]=num;
        if(i==3) { if(pos!=std::string_view::npos) return false; }
        else { if(pos==std::string_view::npos) return false; text.remove_prefix(pos+1); }
    }
    out=v; return true;
}
inline bool ParseTag(std::string_view tag, Version& out) noexcept {
    return tag.starts_with(TagPrefix) && ParseVersion(tag.substr(TagPrefix.size()),out);
}
enum class Status:std::uint8_t { Available, UpToDate, NoRelease, NetworkError };
struct Result { Status status{Status::NetworkError}; Version current{},latest{}; };
Result CheckGithubReleases(); // read only: no APPX installation
} // namespace EdenXbox::Updates
