// Fixed-function-free fullscreen triangle for the Xbox One D3D12 DXBC probe.
// Compiled offline with fxc.exe /T vs_5_0.
float4 main(uint vertex_id : SV_VertexID) : SV_Position {
    float2 pos = float2(vertex_id == 2 ? 3.0f : -1.0f,
                        vertex_id == 1 ? 3.0f : -1.0f);
    return float4(pos, 0.0f, 1.0f);
}
