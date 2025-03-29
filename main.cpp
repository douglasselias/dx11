#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#pragma warning(push)
#pragma warning(disable: 4061)
#pragma warning(disable: 4820)
#pragma warning(disable: 4365)
#pragma warning(disable: 4668)
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#pragma warning(pop)

#include "types.cpp"
#include "xube.h"
#include "renderer.cpp"
#include <math.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  HWND window = create_window("DX11");

  Renderer renderer = create_renderer(window);

  create_render_target_view_and_depth_stencil_view(&renderer);

  VertexShader vs_struct = create_vertex_shader(renderer, L"gpu.hlsl");

  D3D11_INPUT_ELEMENT_DESC inputelementdesc[] = // maps to vertexdesc struct in gpu.hlsl via semantic names ("POS", "NOR", "TEX", "COL")
  {
    { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float3 position
    { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float3 normal
    { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float2 texcoord
    { "COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float3 color
  };

  ID3D11InputLayout* inputlayout;
  renderer.device->CreateInputLayout(inputelementdesc, ARRAYSIZE(inputelementdesc), vs_struct.vertexshaderCSO->GetBufferPointer(), vs_struct.vertexshaderCSO->GetBufferSize(), &inputlayout);

  ID3D11PixelShader* pixelshader = create_pixel_shader(renderer, L"gpu.hlsl");

  ID3D11RasterizerState* rasterizerstate = create_rasterizer(renderer);

  ID3D11SamplerState* samplerstate = create_sampler_state(renderer);

  ID3D11DepthStencilState* depthstencilstate = create_depth_stencil_state(renderer);

  ID3D11Buffer* constantbuffer = create_constant_buffer(renderer);

  ID3D11ShaderResourceView* textureSRV = create_texture_shader_resource_view(renderer);

  ID3D11Buffer* vertexbuffer = create_vertex_buffer(renderer);

  ID3D11Buffer* indexbuffer = create_index_buffer(renderer);

  ///////////////////////////////////////////////////////////////////////////////////////////////

  FLOAT clearcolor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };

  UINT stride = 11 * sizeof(float); // vertex size (11 floats: float3 position, float3 normal, float2 texcoord, float3 color)
  UINT offset = 0;

  ///////////////////////////////////////////////////////////////////////////////////////////////

  float w = renderer.viewport.Width / renderer.viewport.Height; // width (aspect ratio)
  float h = 1.0f;                             // height
  float n = 1.0f;                             // near
  float f = 9.0f;                             // far

  float3 modelrotation    = { 0.0f, 0.0f, 0.0f };
  float3 modelscale       = { 1.0f, 1.0f, 1.0f };
  float3 modeltranslation = { 0.0f, 0.0f, 4.0f };

  while (true)
  {
    MSG msg;

    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_KEYDOWN) return 0; // PRESS ANY KEY TO EXIT
      DispatchMessageA(&msg);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////

    matrix rotatex   = { 1, 0, 0, 0, 0, (float)cos(modelrotation.x), -(float)sin(modelrotation.x), 0, 0, (float)sin(modelrotation.x), (float)cos(modelrotation.x), 0, 0, 0, 0, 1 };
    matrix rotatey   = { (float)cos(modelrotation.y), 0, (float)sin(modelrotation.y), 0, 0, 1, 0, 0, -(float)sin(modelrotation.y), 0, (float)cos(modelrotation.y), 0, 0, 0, 0, 1 };
    matrix rotatez   = { (float)cos(modelrotation.z), -(float)sin(modelrotation.z), 0, 0, (float)sin(modelrotation.z), (float)cos(modelrotation.z), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    matrix scale     = { modelscale.x, 0, 0, 0, 0, modelscale.y, 0, 0, 0, 0, modelscale.z, 0, 0, 0, 0, 1 };
    matrix translate = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, modeltranslation.x, modeltranslation.y, modeltranslation.z, 1 };

    modelrotation.x += 0.005f;
    modelrotation.y += 0.009f;
    modelrotation.z += 0.001f;

    ///////////////////////////////////////////////////////////////////////////////////////////

    D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

    renderer.device_context->Map(constantbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
    {
      Constants* constants = (Constants*)constantbufferMSR.pData;

      constants->transform   = rotatex * rotatey * rotatez * scale * translate;
      constants->projection  = { 2 * n / w, 0, 0, 0, 0, 2 * n / h, 0, 0, 0, 0, f / (f - n), 1, 0, 0, n * f / (n - f), 0 };
      constants->lightvector = { 1.0f, -1.0f, 1.0f };
    }
    renderer.device_context->Unmap(constantbuffer, 0);

    ///////////////////////////////////////////////////////////////////////////////////////////

    renderer.device_context->ClearRenderTargetView(renderer.framebufferRTV, clearcolor);
    renderer.device_context->ClearDepthStencilView(renderer.depthbufferDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    renderer.device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    renderer.device_context->IASetInputLayout(inputlayout);
    renderer.device_context->IASetVertexBuffers(0, 1, &vertexbuffer, &stride, &offset);
    renderer.device_context->IASetIndexBuffer(indexbuffer, DXGI_FORMAT_R32_UINT, 0);

    renderer.device_context->VSSetShader(vs_struct.vertexshader, nullptr, 0);
    renderer.device_context->VSSetConstantBuffers(0, 1, &constantbuffer);

    renderer.device_context->RSSetViewports(1, &renderer.viewport);
    renderer.device_context->RSSetState(rasterizerstate);

    renderer.device_context->PSSetShader(pixelshader, nullptr, 0);
    renderer.device_context->PSSetShaderResources(0, 1, &textureSRV);
    renderer.device_context->PSSetSamplers(0, 1, &samplerstate);

    renderer.device_context->OMSetRenderTargets(1, &renderer.framebufferRTV, renderer.depthbufferDSV);
    renderer.device_context->OMSetDepthStencilState(depthstencilstate, 0);
    renderer.device_context->OMSetBlendState(nullptr, nullptr, 0xffffffff); // use default blend mode (i.e. no blending)

    ///////////////////////////////////////////////////////////////////////////////////////////

    renderer.device_context->DrawIndexed(ARRAYSIZE(indexdata), 0, 0);

    ///////////////////////////////////////////////////////////////////////////////////////////

    renderer.swapchain->Present(1, 0);
  }

  return 0;
}