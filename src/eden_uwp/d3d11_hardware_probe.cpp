// SPDX-License-Identifier: GPL-3.0-or-later
#include "eden_uwp/d3d11_hardware_probe.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace EdenXbox {
static D3D11HardwareReport ProbeD3D11(bool use_warp) {
    using Microsoft::WRL::ComPtr;
    D3D11HardwareReport out;
    auto fail = [&](const char* stage, HRESULT h) {
        out.stage = stage;
        out.error = static_cast<std::uint32_t>(h);
        return out;
    };
    const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0,
                                      D3D_FEATURE_LEVEL_10_1,
                                      D3D_FEATURE_LEVEL_10_0};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    const auto type = use_warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
    HRESULT hr = D3D11CreateDevice(nullptr, type, nullptr, 0,
                                  levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                  &device, &level, &context);
    if (FAILED(hr)) return fail(use_warp ? "D3D11CreateDevice WARP" :
                                   "D3D11CreateDevice HARDWARE", hr);
    out.feature_level = static_cast<unsigned>(level);

    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC desc{};
    if (FAILED(device.As(&dxgi)) || FAILED(dxgi->GetAdapter(&adapter)) ||
        FAILED(adapter->GetDesc(&desc))) return fail("DXGI hardware adapter", E_FAIL);
    out.vendor_id = desc.VendorId;
    out.device_id = desc.DeviceId;
    if (!use_warp && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        return fail("Software adapter detected", E_FAIL);
    if (level < D3D_FEATURE_LEVEL_10_1) return fail("SM4.1 unavailable", E_NOTIMPL);

    // Compile real SM4.1 vertex/pixel shaders for the FL10.1 hardware path.
    constexpr char shader[] = R"(
struct Output { float4 position : SV_POSITION; };
Output VS(uint i : SV_VertexID) {
    Output o;
    o.position = float4(i == 1 ? 3.0 : -1.0, i == 2 ? 3.0 : -1.0, 0.5, 1.0);
    return o;
}
float4 PS(Output i) : SV_TARGET { return float4(0.2, 0.5, 0.8, 1.0); }
)";
    ComPtr<ID3DBlob> vsb, psb, errors;
    hr = D3DCompile(shader, sizeof(shader) - 1, nullptr, nullptr, nullptr,
                    "VS", "vs_4_1", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vsb, &errors);
    if (FAILED(hr)) return fail("FXC VS SM4.1", hr);
    errors.Reset();
    hr = D3DCompile(shader, sizeof(shader) - 1, nullptr, nullptr, nullptr,
                    "PS", "ps_4_1", D3DCOMPILE_ENABLE_STRICTNESS, 0, &psb, &errors);
    if (FAILED(hr)) return fail("FXC PS SM4.1", hr);
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    hr = device->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(),
                                    nullptr, &vs);
    if (FAILED(hr)) return fail("CreateVertexShader", hr);
    hr = device->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(),
                                   nullptr, &ps);
    if (FAILED(hr)) return fail("CreatePixelShader", hr);

    D3D11_TEXTURE2D_DESC tex{};
    tex.Width = 32; tex.Height = 32; tex.MipLevels = 1; tex.ArraySize = 1;
    tex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex.SampleDesc.Count = 1;
    tex.Usage = D3D11_USAGE_DEFAULT;
    tex.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> color;
    ComPtr<ID3D11RenderTargetView> rtv;
    hr = device->CreateTexture2D(&tex, nullptr, &color);
    if (FAILED(hr)) return fail("CreateTexture2D color", hr);
    hr = device->CreateRenderTargetView(color.Get(), nullptr, &rtv);
    if (FAILED(hr)) return fail("CreateRenderTargetView", hr);
    tex.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    tex.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth;
    ComPtr<ID3D11DepthStencilView> dsv;
    hr = device->CreateTexture2D(&tex, nullptr, &depth);
    if (FAILED(hr)) return fail("CreateTexture2D depth", hr);
    hr = device->CreateDepthStencilView(depth.Get(), nullptr, &dsv);
    if (FAILED(hr)) return fail("CreateDepthStencilView", hr);
    D3D11_VIEWPORT viewport{};
    viewport.Width = 32; viewport.Height = 32; viewport.MaxDepth = 1;
    context->RSSetViewports(1, &viewport);
    const float clear[]{0, 0, 0, 1};
    context->ClearRenderTargetView(rtv.Get(), clear);
    context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1, 0);
    ID3D11RenderTargetView* target = rtv.Get();
    context->OMSetRenderTargets(1, &target, dsv.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs.Get(), nullptr, 0);
    context->PSSetShader(ps.Get(), nullptr, 0);
    context->Draw(3, 0);
    context->Flush();

    // An accepted Draw call does not prove the GPU rendered pixels. Read back
    // the center texel before reporting DX11 hardware success.
    D3D11_TEXTURE2D_DESC staging = tex;
    staging.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    staging.Usage = D3D11_USAGE_STAGING;
    staging.BindFlags = 0;
    staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> readback;
    hr = device->CreateTexture2D(&staging, nullptr, &readback);
    if (FAILED(hr)) return fail("CreateTexture2D staging", hr);
    context->CopyResource(readback.Get(), color.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return fail("Map offscreen draw", hr);
    const auto* pixels = static_cast<const unsigned char*>(mapped.pData);
    const auto* pixel = pixels + 16u * mapped.RowPitch + 16u * 4u;
    const bool drawn = pixel[0] > 20 && pixel[1] > 90 &&
                       pixel[2] > 145 && pixel[3] > 220;
    context->Unmap(readback.Get(), 0);
    context->ClearState();
    if (!drawn) return fail("SM4.1 offscreen pixel mismatch", E_FAIL);
    out.passed = true;
    return out;
}
D3D11HardwareReport ProbeD3D11Hardware() { return ProbeD3D11(false); }
D3D11HardwareReport ProbeD3D11Warp() { return ProbeD3D11(true); }
} // namespace EdenXbox
