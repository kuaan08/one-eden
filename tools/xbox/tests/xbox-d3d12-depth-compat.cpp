// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include "../../../src/video_core/renderer_d3d12/d3d12_xbox3d_compat.h"
int main() {
    using namespace D3D12;
    static_assert(!SkipPostDepthDeviceQuery(0));
    static_assert(!UseSingleLayerDepthView(0));
    static_assert(SkipPostDepthDeviceQuery(1));
    static_assert(!UseSingleLayerDepthView(1));
    static_assert(!SkipPostDepthDeviceQuery(2));
    static_assert(UseSingleLayerDepthView(2));
    static_assert(SkipPostDepthDeviceQuery(3));
    static_assert(UseSingleLayerDepthView(3));
    SetXboxDepthCompatMode(2);
    assert(XboxDepthCompatMode()==2);
    SetXboxDepthCompatMode(7);
    assert(XboxDepthCompatMode()==3);
}
