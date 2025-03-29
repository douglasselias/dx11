#define WIN32_LEAN_AND_MEAN

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

#undef near
#undef far

#include <stdio.h>
#include <math.h>
#include "types.cpp"
#include "xube.h"
#include "renderer.cpp"

#define PI 3.14159265358979323846f
#define DEG2RAD (PI/180.0f)

struct Camera
{
  float3 position;
  float3 target;
  float3 up;
  float fovy;
};

float3 Vector3Scale(float3 v, float scalar)
{
  float3 result = { v.x*scalar, v.y*scalar, v.z*scalar };
  return result;
}

float3 Vector3Add(float3 v1, float3 v2)
{
  float3 result = { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
  return result;
}

float3 GetCameraUp(Camera* camera)
{
  return camera->up;
}

void CameraMoveUp(Camera *camera, float distance)
{
  float3 up = GetCameraUp(camera);
  up = Vector3Scale(up, distance);

  camera->position = Vector3Add(camera->position, up);
  camera->target   = Vector3Add(camera->target,   up);
}

float3 Vector3Subtract(float3 v1, float3 v2)
{
  float3 result = { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
  return result;
}

float3 Vector3RotateByAxisAngle(float3 v, float3 axis, float angle)
{
  // Using Euler-Rodrigues Formula
  // Ref.: https://en.wikipedia.org/w/index.php?title=Euler%E2%80%93Rodrigues_formula

  float3 result = v;

  // Vector3Normalize(axis);
  float length = sqrtf(axis.x*axis.x + axis.y*axis.y + axis.z*axis.z);
  if (length == 0.0f) length = 1.0f;
  float ilength = 1.0f/length;
  axis.x *= ilength;
  axis.y *= ilength;
  axis.z *= ilength;

  angle /= 2.0f;
  float a = sinf(angle);
  float b = axis.x*a;
  float c = axis.y*a;
  float d = axis.z*a;
  a = cosf(angle);
  float3 w = { b, c, d };

  // Vector3CrossProduct(w, v)
  float3 wv = { w.y*v.z - w.z*v.y, w.z*v.x - w.x*v.z, w.x*v.y - w.y*v.x };

  // Vector3CrossProduct(w, wv)
  float3 wwv = { w.y*wv.z - w.z*wv.y, w.z*wv.x - w.x*wv.z, w.x*wv.y - w.y*wv.x };

  // Vector3Scale(wv, 2*a)
  a *= 2;
  wv.x *= a;
  wv.y *= a;
  wv.z *= a;

  // Vector3Scale(wwv, 2)
  wwv.x *= 2;
  wwv.y *= 2;
  wwv.z *= 2;

  result.x += wv.x;
  result.y += wv.y;
  result.z += wv.z;

  result.x += wwv.x;
  result.y += wwv.y;
  result.z += wwv.z;

  return result;
}

void CameraYaw(Camera *camera, float angle)
{
  // Rotation axis
  float3 up = GetCameraUp(camera);

  // View vector
  float3 targetPosition = Vector3Subtract(camera->target, camera->position);

  // Rotate view vector around up axis
  targetPosition = Vector3RotateByAxisAngle(targetPosition, up, angle);

  camera->target = Vector3Add(camera->position, targetPosition);
}

float camera_yaw = 0;

void UpdateCamera(Camera* camera, float deltaTime)
{
  // Calculate forward vector (normalized direction from position to target)
  float3 forward = normalize(Vector3Subtract(camera->target, camera->position));

  // Calculate right vector (perpendicular to forward and up)
  float3 right = normalize(cross(forward, camera->up));

  float speed = 4.75f * deltaTime;

  if (GetAsyncKeyState('W') & 0x8000) // Move forward
  {
    float3 movement = Vector3Scale(forward, -speed);
    camera->position = Vector3Add(camera->position, movement);
    camera->target = Vector3Add(camera->target, movement);
  }
  if (GetAsyncKeyState('S') & 0x8000) // Move backward
  {
    float3 movement = Vector3Scale(forward, speed);
    camera->position = Vector3Add(camera->position, movement);
    camera->target = Vector3Add(camera->target, movement);
  }
  if (GetAsyncKeyState('A') & 0x8000) // Strafe left
  {
    float3 movement = Vector3Scale(right, -speed);
    camera->position = Vector3Add(camera->position, movement);
    camera->target = Vector3Add(camera->target, movement);
  }
  if (GetAsyncKeyState('D') & 0x8000) // Strafe right
  {
    float3 movement = Vector3Scale(right, speed);
    camera->position = Vector3Add(camera->position, movement);
    camera->target = Vector3Add(camera->target, movement);
  }
  if (GetAsyncKeyState('Z') & 0x8000) // Move up
  {
    float3 movement = Vector3Scale(camera->up, speed);
    camera->position = Vector3Add(camera->position, movement);
    camera->target = Vector3Add(camera->target, movement);
  }
  if (GetAsyncKeyState('X') & 0x8000) // Move down
  {
    float3 movement = Vector3Scale(camera->up, -speed);
    camera->position = Vector3Add(camera->position, movement);
    camera->target = Vector3Add(camera->target, movement);
  }

  float yaw_speed = 1;
  if (GetAsyncKeyState('J') & 0x8000) // Rotate left
  {
    float rotationAmount = -yaw_speed * deltaTime;
    CameraYaw(camera, rotationAmount);
  }
  if (GetAsyncKeyState('L') & 0x8000) // Rotate right
  {
    float rotationAmount = yaw_speed * deltaTime;
    CameraYaw(camera, rotationAmount);
  }
}

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

  FLOAT clearcolor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };

  UINT stride = 11 * sizeof(float); // vertex size (11 floats: float3 position, float3 normal, float2 texcoord, float3 color)
  UINT offset = 0;

  float near = 1.0f;
  float far  = 100.0f;
  matrix projection_matrix = create_projection_matrix(renderer.viewport.Width, renderer.viewport.Height, near, far);

  float3 modelrotation    = { 0.0f, 0.0f, 0.0f };
  float3 modelscale       = { 1.0f, 1.0f, 1.0f };
  float3 modeltranslation = { 0.0f, 0.0f, 4.0f };
  float3 light_direction  = { 1.0f, -1.0f, 1.0f };

  Camera camera = {};
  camera.position = { 0.0f, 0.0f, 0.5f };
  camera.target   = { 0.0f, 0.0f, 0.0f };
  camera.up       = { 0.0f, 1.0f, 0.0f };

  while (true)
  {
    MSG msg;

    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_KEYDOWN)
      {
        float dt = 0.016f;
        float speed = 4.75f;
        switch (msg.wParam)
        {
          case VK_ESCAPE:
          case VK_OEM_3: return 0;
        }

      }
      DispatchMessageA(&msg);
    }

    UpdateCamera(&camera, 0.016f);

    matrix rotatex   = create_rotation_x_matrix(modelrotation.x);
    matrix rotatey   = create_rotation_y_matrix(modelrotation.y);
    matrix rotatez   = create_rotation_z_matrix(modelrotation.z);
    matrix scale     = create_scale_matrix(modelscale);
    matrix translate = create_translation_matrix(modeltranslation);
    matrix view      = create_view_matrix(camera.position, camera.target, camera.up);
    matrix transform = rotatex * rotatey * rotatez * scale * translate * view;

    // modelrotation.x += 0.005f;
    // modelrotation.y += 0.009f;
    // modelrotation.z += 0.001f;

    Constants constants = {transform, projection_matrix, light_direction};
    update_constant_buffer(renderer, constantbuffer, constants);

    ///////////////////////////////////////////////////////////////////////////////////////////

    clear_screen(renderer, clearcolor);

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