// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

struct ID3D12Device;

namespace D3D12 {

// Diagnostic only: test basic DXBC Shader Model 5.0 PSO creation on a device
// reporting < 6.0, to separate driver support from Eden's current DXIL SM6.4
// path. Does not substitute for a real SPIR-V -> DXBC renderer.
void DiagnoseShaderModel5Pipeline(ID3D12Device* device);

} // namespace D3D12
