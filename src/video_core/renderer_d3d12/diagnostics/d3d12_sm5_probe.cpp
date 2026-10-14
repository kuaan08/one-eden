// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

#include "common/logging.h"
#include "video_core/renderer_d3d12/d3d12_device.h"
#include "video_core/renderer_d3d12/diagnostics/d3d12_sm5_probe.h"

namespace D3D12 {
namespace {

// DXBC is compiled offline by the Windows SDK's fxc.exe at APPX packaging time.
// Do not try to load d3dcompiler_47.dll or compile shader sources on the Xbox.
std::vector<u8> ReadPackagedShader(const wchar_t* filename) {
    std::array<wchar_t, 1024> executable{};
    const DWORD len = GetModuleFileNameW(nullptr, executable.data(),
                                         static_cast<DWORD>(executable.size()));
    if (len == 0 || len >= executable.size()) {
        LOG_WARNING(Render, "D3D12 SM5 probe: GetModuleFileNameW failed (error {})",
                    GetLastError());
        return {};
    }
    const auto path = std::filesystem::path{executable.data()}.parent_path() / filename;
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        LOG_WARNING(Render, "D3D12 SM5 probe: shader bytecode not packaged");
        return {};
    }
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC MakeMinimalPso(ID3D12RootSignature* root,
                                                  const std::vector<u8>& vs,
                                                  const std::vector<u8>& ps) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = root;
    desc.VS = {vs.data(), vs.size()};
    desc.PS = {ps.data(), ps.size()};
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.FrontCounterClockwise = FALSE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.DepthStencilState.StencilEnable = FALSE;
    desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.DepthStencilState.BackFace = desc.DepthStencilState.FrontFace;

    desc.BlendState.IndependentBlendEnable = FALSE;
    auto& blend = desc.BlendState.RenderTarget[0];
    blend.BlendEnable = FALSE;
    blend.LogicOpEnable = FALSE;
    blend.SrcBlend = D3D12_BLEND_ONE;
    blend.DestBlend = D3D12_BLEND_ZERO;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    return desc;
}

} // namespace

void DiagnoseShaderModel5Pipeline(ID3D12Device* device) {
    const auto vs = ReadPackagedShader(L"eden_probe_vs.cso");
    const auto ps = ReadPackagedShader(L"eden_probe_ps.cso");
    if (vs.empty() || ps.empty()) {
        LOG_WARNING(Render, "D3D12 SM5 probe: missing offline DXBC shader(s); "
                            "use an APPX packaged with -ShaderModelProbe");
        return;
    }
    LOG_INFO(Render, "D3D12 SM5 probe: testing independent DXBC vs_5_0 ({} bytes) and "
                     "ps_5_0 ({} bytes) PSO", vs.size(), ps.size());

    const D3D12_ROOT_SIGNATURE_DESC root_desc{
        .NumParameters = 0,
        .pParameters = nullptr,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE,
    };
    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    const HRESULT serialized_hr = D3D12SerializeRootSignature(
        &root_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &serialized, &errors);
    if (FAILED(serialized_hr)) {
        LOG_ERROR(Render, "D3D12 SM5 probe: root signature serialization FAILED 0x{:08X}",
                  static_cast<u32>(serialized_hr));
        return;
    }

    ComPtr<ID3D12RootSignature> root;
    const HRESULT root_hr = device->CreateRootSignature(
        0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
        IID_PPV_ARGS(&root));
    if (FAILED(root_hr)) {
        LOG_ERROR(Render, "D3D12 SM5 probe: simple root signature FAILED 0x{:08X}",
                  static_cast<u32>(root_hr));
        return;
    }

    auto pso_desc = MakeMinimalPso(root.Get(), vs, ps);
    ComPtr<ID3D12PipelineState> pso;
    const HRESULT graphics_hr =
        device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso));
    LOG_INFO(Render, "D3D12 SM5 probe: minimal DXBC VS+PS graphics PSO -> {} (0x{:08X})",
             SUCCEEDED(graphics_hr) ? "SUCCESS" : "FAILED", static_cast<u32>(graphics_hr));
    pso.Reset();

    // A vertex-only PSO may still be accepted on devices that reject this RTV
    // format or a pixel shader. This helps isolate those failure classes.
    pso_desc.PS = {};
    pso_desc.NumRenderTargets = 0;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    const HRESULT vertex_hr =
        device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso));
    LOG_INFO(Render, "D3D12 SM5 probe: vertex-only DXBC graphics PSO -> {} (0x{:08X})",
             SUCCEEDED(vertex_hr) ? "SUCCESS" : "FAILED", static_cast<u32>(vertex_hr));

    // Creating a PSO is not a full rendered-frame test. The existing DXIL SM6
    // shader path remains unchanged; these results must not be called playable.
}

} // namespace D3D12
