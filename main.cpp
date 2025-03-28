#include "renderer.cpp"

#include <math.h>

#include "xube.h"

struct float3 { float x, y, z; };
struct matrix { float m[4][4]; };

matrix operator*(const matrix& m1, const matrix& m2);

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  HWND window = create_window("DX11");

  Renderer renderer = create_renderer(window);

  create_render_target_view_and_depth_stencil_view(&renderer);

  ID3DBlob* vertexshaderCSO;
  D3DCompileFromFile(L"gpu.hlsl", nullptr, nullptr, "VsMain", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);

  ID3D11VertexShader* vertexshader;
  renderer.device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &vertexshader);

  D3D11_INPUT_ELEMENT_DESC inputelementdesc[] = // maps to vertexdesc struct in gpu.hlsl via semantic names ("POS", "NOR", "TEX", "COL")
  {
    { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float3 position
    { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float3 normal
    { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float2 texcoord
    { "COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float3 color
  };

  ID3D11InputLayout* inputlayout;
  renderer.device->CreateInputLayout(inputelementdesc, ARRAYSIZE(inputelementdesc), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &inputlayout);

  ///////////////////////////////////////////////////////////////////////////////////////////////

  ID3DBlob* pixelshaderCSO;
  D3DCompileFromFile(L"gpu.hlsl", nullptr, nullptr, "PsMain", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);

  ID3D11PixelShader* pixelshader;
  renderer.device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &pixelshader);

  ///////////////////////////////////////////////////////////////////////////////////////////////

  D3D11_RASTERIZER_DESC rasterizerdesc = {};
  rasterizerdesc.FillMode = D3D11_FILL_SOLID;
  rasterizerdesc.CullMode = D3D11_CULL_BACK;

  ID3D11RasterizerState* rasterizerstate;
  renderer.device->CreateRasterizerState(&rasterizerdesc, &rasterizerstate);

  ///////////////////////////////////////////////////////////////////////////////////////////////

  D3D11_SAMPLER_DESC samplerdesc = {};
  samplerdesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
  samplerdesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
  samplerdesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
  samplerdesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
  samplerdesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

  ID3D11SamplerState* samplerstate;
  renderer.device->CreateSamplerState(&samplerdesc, &samplerstate);

  ///////////////////////////////////////////////////////////////////////////////////////////////

  D3D11_DEPTH_STENCIL_DESC depthstencildesc = {};
  depthstencildesc.DepthEnable    = TRUE;
  depthstencildesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depthstencildesc.DepthFunc      = D3D11_COMPARISON_LESS;

  ID3D11DepthStencilState* depthstencilstate;
  renderer.device->CreateDepthStencilState(&depthstencildesc, &depthstencilstate);

  ///////////////////////////////////////////////////////////////////////////////////////////////

  struct Constants { matrix transform, projection; float3 lightvector; };

  D3D11_BUFFER_DESC constantbufferdesc = {};
  constantbufferdesc.ByteWidth      = sizeof(Constants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
  constantbufferdesc.Usage          = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
  constantbufferdesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
  constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  ID3D11Buffer* constantbuffer;
  renderer.device->CreateBuffer(&constantbufferdesc, nullptr, &constantbuffer);

  ///////////////////////////////////////////////////////////////////////////////////////////////

  D3D11_TEXTURE2D_DESC texturedesc = {};
  texturedesc.Width              = TEXTURE_WIDTH;  // in xube.h
  texturedesc.Height             = TEXTURE_HEIGHT; // in xube.h
  texturedesc.MipLevels          = 1;
  texturedesc.ArraySize          = 1;
  texturedesc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // same as framebuffer(view)
  texturedesc.SampleDesc.Count   = 1;
  texturedesc.Usage              = D3D11_USAGE_IMMUTABLE; // will never be updated
  texturedesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

  D3D11_SUBRESOURCE_DATA textureSRD = {};
  textureSRD.pSysMem     = texturedata; // in xube.h
  textureSRD.SysMemPitch = TEXTURE_WIDTH * sizeof(UINT); // 1 UINT = 4 bytes per pixel, 0xAARRGGBB

  ID3D11Texture2D* texture;
  renderer.device->CreateTexture2D(&texturedesc, &textureSRD, &texture);

  ID3D11ShaderResourceView* textureSRV;
  renderer.device->CreateShaderResourceView(texture, nullptr, &textureSRV);

  ///////////////////////////////////////////////////////////////////////////////////////////////

  D3D11_BUFFER_DESC vertexbufferdesc = {};
  vertexbufferdesc.ByteWidth = sizeof(vertexdata);
  vertexbufferdesc.Usage     = D3D11_USAGE_IMMUTABLE; // will never be updated 
  vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

  D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertexdata }; // in xube.h

  ID3D11Buffer* vertexbuffer;
  renderer.device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexbuffer);

  ///////////////////////////////////////////////////////////////////////////////////////////////

  D3D11_BUFFER_DESC indexbufferdesc = {};
  indexbufferdesc.ByteWidth = sizeof(indexdata);
  indexbufferdesc.Usage     = D3D11_USAGE_IMMUTABLE; // will never be updated
  indexbufferdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

  D3D11_SUBRESOURCE_DATA indexbufferSRD = { indexdata }; // in xube.h

  ID3D11Buffer* indexbuffer;
  renderer.device->CreateBuffer(&indexbufferdesc, &indexbufferSRD, &indexbuffer);

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

    renderer.device_context->VSSetShader(vertexshader, nullptr, 0);
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

matrix operator*(const matrix& m1, const matrix& m2)
{
  return
  {
    m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0] + m1.m[0][3] * m2.m[3][0],
    m1.m[0][0] * m2.m[0][1] + m1.m[0][1] * m2.m[1][1] + m1.m[0][2] * m2.m[2][1] + m1.m[0][3] * m2.m[3][1],
    m1.m[0][0] * m2.m[0][2] + m1.m[0][1] * m2.m[1][2] + m1.m[0][2] * m2.m[2][2] + m1.m[0][3] * m2.m[3][2],
    m1.m[0][0] * m2.m[0][3] + m1.m[0][1] * m2.m[1][3] + m1.m[0][2] * m2.m[2][3] + m1.m[0][3] * m2.m[3][3],
    m1.m[1][0] * m2.m[0][0] + m1.m[1][1] * m2.m[1][0] + m1.m[1][2] * m2.m[2][0] + m1.m[1][3] * m2.m[3][0],
    m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1] + m1.m[1][3] * m2.m[3][1],
    m1.m[1][0] * m2.m[0][2] + m1.m[1][1] * m2.m[1][2] + m1.m[1][2] * m2.m[2][2] + m1.m[1][3] * m2.m[3][2],
    m1.m[1][0] * m2.m[0][3] + m1.m[1][1] * m2.m[1][3] + m1.m[1][2] * m2.m[2][3] + m1.m[1][3] * m2.m[3][3],
    m1.m[2][0] * m2.m[0][0] + m1.m[2][1] * m2.m[1][0] + m1.m[2][2] * m2.m[2][0] + m1.m[2][3] * m2.m[3][0],
    m1.m[2][0] * m2.m[0][1] + m1.m[2][1] * m2.m[1][1] + m1.m[2][2] * m2.m[2][1] + m1.m[2][3] * m2.m[3][1],
    m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2] + m1.m[2][3] * m2.m[3][2],
    m1.m[2][0] * m2.m[0][3] + m1.m[2][1] * m2.m[1][3] + m1.m[2][2] * m2.m[2][3] + m1.m[2][3] * m2.m[3][3],
    m1.m[3][0] * m2.m[0][0] + m1.m[3][1] * m2.m[1][0] + m1.m[3][2] * m2.m[2][0] + m1.m[3][3] * m2.m[3][0],
    m1.m[3][0] * m2.m[0][1] + m1.m[3][1] * m2.m[1][1] + m1.m[3][2] * m2.m[2][1] + m1.m[3][3] * m2.m[3][1],
    m1.m[3][0] * m2.m[0][2] + m1.m[3][1] * m2.m[1][2] + m1.m[3][2] * m2.m[2][2] + m1.m[3][3] * m2.m[3][2],
    m1.m[3][0] * m2.m[0][3] + m1.m[3][1] * m2.m[1][3] + m1.m[3][2] * m2.m[2][3] + m1.m[3][3] * m2.m[3][3],
  };
}