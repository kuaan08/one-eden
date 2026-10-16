// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
#include "eden_uwp/d3d11_hardware_probe.h"

#include <array>
#include <cstring>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>
#include "common/logging.h"

namespace EdenXbox::D3D11Probe {
using Microsoft::WRL::ComPtr;
namespace {
constexpr char VertexSource[] = R"(
float4 main(uint vertex : SV_VertexID) : SV_POSITION {
    float2 p = vertex == 0 ? float2(-1.0,-1.0) :
               vertex == 1 ? float2(-1.0, 3.0) : float2(3.0,-1.0);
    return float4(p, 0.0, 1.0);
})";
constexpr char PixelSource[] = R"(
float4 main() : SV_Target { return float4(1.0, 0.0, 0.0, 1.0); }
)";
void Report(const char* stage, HRESULT hr, const Result& r) {
    LOG_WARNING(Frontend,
        "Xbox D3D11 hardware smoke: {} hr=0x{:08x} feature=0x{:x} vendor=0x{:04x} device=0x{:04x} hardware={}",
        stage, static_cast<std::uint32_t>(hr), r.feature_level,
        r.vendor_id, r.device_id, r.hardware_driver);
}
} // namespace

Result RunHardwareSmokeTest() {
    Result result{};
    const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0,
                                      D3D_FEATURE_LEVEL_10_1,
                                      D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL selected{};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    // Explicit HARDWARE only. Falling back to WARP would saturate the slow
    // Jaguar CPU and invalidate any claimed 2D/3D performance improvement.
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
        &device, &selected, &context);
    result.feature_level = static_cast<std::uint32_t>(selected);
    result.hresult = static_cast<std::uint32_t>(hr);
    if (FAILED(hr) || !device || !context) {
        result.status = Status::NoHardware;
        Report("create-hardware-device", hr, result);
        return result;
    }
    result.hardware_driver = true;
    ComPtr<IDXGIDevice> dxgi_device;
    if (SUCCEEDED(device.As(&dxgi_device))) {
        ComPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(dxgi_device->GetAdapter(&adapter))) {
            DXGI_ADAPTER_DESC desc{};
            if (SUCCEEDED(adapter->GetDesc(&desc))) {
                result.vendor_id = desc.VendorId;
                result.device_id = desc.DeviceId;
            }
        }
    }
    Report("hardware-device-created", S_OK, result);

    const char* const profile = selected >= D3D_FEATURE_LEVEL_10_1 ? "vs_4_1" : "vs_4_0";
    const char* const pixel_profile = selected >= D3D_FEATURE_LEVEL_10_1 ? "ps_4_1" : "ps_4_0";
    ComPtr<ID3DBlob> vertex_blob, pixel_blob, errors;
    hr = D3DCompile(VertexSource, std::strlen(VertexSource), nullptr, nullptr, nullptr,
                    "main", profile, D3DCOMPILE_ENABLE_STRICTNESS, 0,
                    &vertex_blob, &errors);
    if (FAILED(hr) || !vertex_blob) {
        result.status = Status::ShaderFailed; result.hresult = static_cast<std::uint32_t>(hr);
        Report("compile-vertex-SM4", hr, result); return result;
    }
    errors.Reset();
    hr = D3DCompile(PixelSource, std::strlen(PixelSource), nullptr, nullptr, nullptr,
                    "main", pixel_profile, D3DCOMPILE_ENABLE_STRICTNESS, 0,
                    &pixel_blob, &errors);
    if (FAILED(hr) || !pixel_blob) {
        result.status = Status::ShaderFailed; result.hresult = static_cast<std::uint32_t>(hr);
        Report("compile-pixel-SM4", hr, result); return result;
    }
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    hr = device->CreateVertexShader(vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(),
                                    nullptr, &vs);
    if (SUCCEEDED(hr)) hr = device->CreatePixelShader(pixel_blob->GetBufferPointer(),
                                                      pixel_blob->GetBufferSize(), nullptr, &ps);
    if (FAILED(hr)) {
        result.status = Status::ShaderFailed; result.hresult = static_cast<std::uint32_t>(hr);
        Report("create-shaders", hr, result); return result;
    }
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width = 4; texture.Height = 4;
    texture.MipLevels = 1; texture.ArraySize = 1;
    texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture.SampleDesc.Count = 1;
    texture.Usage = D3D11_USAGE_DEFAULT;
    texture.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> color, staging;
    ComPtr<ID3D11RenderTargetView> rtv;
    hr = device->CreateTexture2D(&texture, nullptr, &color);
    if (SUCCEEDED(hr)) hr = device->CreateRenderTargetView(color.Get(), nullptr, &rtv);
    texture.Usage = D3D11_USAGE_STAGING;
    texture.BindFlags = 0;
    texture.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (SUCCEEDED(hr)) hr = device->CreateTexture2D(&texture, nullptr, &staging);
    if (FAILED(hr)) {
        result.status = Status::SetupFailed; result.hresult = static_cast<std::uint32_t>(hr);
        Report("create-offscreen-textures", hr, result); return result;
    }

    ID3D11RenderTargetView* targets[]{rtv.Get()};
    const D3D11_VIEWPORT viewport{0.f, 0.f, 4.f, 4.f, 0.f, 1.f};
    const float clear_color[]{0.f, 0.f, 0.f, 1.f};
    context->OMSetRenderTargets(1, targets, nullptr);
    context->RSSetViewports(1, &viewport);
    context->ClearRenderTargetView(rtv.Get(), clear_color);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs.Get(), nullptr, 0);
    context->PSSetShader(ps.Get(), nullptr, 0);
    context->Draw(3, 0);
    context->CopyResource(staging.Get(), color.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        result.status = Status::SetupFailed; result.hresult = static_cast<std::uint32_t>(hr);
        Report("readback-map", hr, result); return result;
    }
    const auto* pixel = static_cast<const std::uint8_t*>(mapped.pData) +
                        2 * mapped.RowPitch + 2 * 4;
    const bool red = pixel[0] > 200 && pixel[1] < 32 &&
                     pixel[2] < 32 && pixel[3] > 200;
    context->Unmap(staging.Get(), 0);
    context->ClearState();
    result.status = red ? Status::Passed : Status::PixelMismatch;
    result.hresult = static_cast<std::uint32_t>(S_OK);
    Report(red ? "draw-and-readback-PASS" : "draw-and-readback-MISMATCH", S_OK, result);
    return result;
}
} // namespace EdenXbox::D3D11Probe
