// SPDX-License-Identifier: GPL-3.0-or-later
#include "eden_uwp/update_checker.h"
#include <chrono>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Filters.h>
namespace EdenXbox::Updates {
Result CheckGithubReleases() {
    Result result;
    try {
        const auto current=winrt::Windows::ApplicationModel::Package::Current().Id().Version();
        result.current={current.Major,current.Minor,current.Build,current.Revision};
        using namespace winrt::Windows::Web::Http;
        Filters::HttpBaseProtocolFilter filter;
        filter.CacheControl().ReadBehavior(Filters::HttpCacheReadBehavior::NoCache);
        HttpClient client{filter};
        client.Timeout(std::chrono::seconds{8});
        client.DefaultRequestHeaders().UserAgent().TryParseAdd(L"EdenXboxOneUpdater/1.0");
        const auto response=client.GetAsync(winrt::Windows::Foundation::Uri{
            L"https://api.github.com/repos/kuaan08/one-eden/releases?per_page=8"}).get();
        if(response.StatusCode()!=HttpStatusCode::Ok) return result;
        const auto length=response.Content().Headers().ContentLength();
        if(length && length.Value()>256*1024) return result;
        const auto body=response.Content().ReadAsStringAsync().get();
        if(body.size()>256*1024) return result;
        const auto releases=winrt::Windows::Data::Json::JsonArray::Parse(body);
        for(const auto& item:releases) {
            if(item.ValueType()!=winrt::Windows::Data::Json::JsonValueType::Object) continue;
            const auto release=item.GetObject();
            if(release.GetNamedBoolean(L"draft",false)) continue;
            Version latest{};
            if(!ParseTag(winrt::to_string(release.GetNamedString(L"tag_name",L"")),latest)) continue;
            bool normal=false,diagnostic=false;
            for(const auto& asset:release.GetNamedArray(L"assets")) {
                if(asset.ValueType()!=winrt::Windows::Data::Json::JsonValueType::Object) continue;
                const auto name=asset.GetObject().GetNamedString(L"name",L"");
                normal|=name==L"eden-xbox-normal.appx";
                diagnostic|=name==L"eden-xbox-diagnostic.appx";
            }
            if(!normal || !diagnostic) continue;
            result.latest=latest;
            result.status=latest>result.current ? Status::Available : Status::UpToDate;
            return result;
        }
        result.status=Status::NoRelease;
    } catch(...) { result.status=Status::NetworkError; }
    return result;
}
} // namespace EdenXbox::Updates
