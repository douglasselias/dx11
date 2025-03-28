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

HWND create_window(const char* title) {
  /// @todo: Add a proper window proc.
  WNDCLASSA window_class = {0, DefWindowProcA, 0, 0, 0, 0, 0, 0, 0, title };
  RegisterClassA(&window_class);

  HWND window = CreateWindowExA(0, title, title, WS_POPUP | WS_MAXIMIZE | WS_VISIBLE, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);

  return window;
}

struct Renderer {
  DXGI_SWAP_CHAIN_DESC swap_chain_desc;
  IDXGISwapChain* swapchain;
  ID3D11Device* device;
  ID3D11DeviceContext* device_context;
  D3D11_VIEWPORT viewport;

  ID3D11RenderTargetView* framebufferRTV;
  ID3D11DepthStencilView* depthbufferDSV;
};

Renderer create_renderer(HWND window) {
  Renderer renderer = {};

  DXGI_MODE_DESC BufferDesc = {};
  /// @todo: Can't specify SRGB framebuffer directly when using FLIP model swap effect.
  BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  /// @note: You can also specify the refresh rate.

  DXGI_SAMPLE_DESC SampleDesc = {};
  SampleDesc.Count = 1;

  renderer.swap_chain_desc.BufferDesc = BufferDesc;
  renderer.swap_chain_desc.SampleDesc = SampleDesc;

  renderer.swap_chain_desc.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  renderer.swap_chain_desc.BufferCount  = 2;
  renderer.swap_chain_desc.OutputWindow = window;
  renderer.swap_chain_desc.Windowed     = true;
  renderer.swap_chain_desc.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD;

  D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };

  /// @todo: Look into this flag for shader debugging: D3D11_CREATE_DEVICE_DEBUGGABLE
  D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG, feature_levels, ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &renderer.swap_chain_desc, &renderer.swapchain, &renderer.device, nullptr, &renderer.device_context);

  /// @note: Update swap_chain_desc with actual window size.
  renderer.swapchain->GetDesc(&renderer.swap_chain_desc);

  D3D11_VIEWPORT viewport = {};
  viewport.Width    = (float)renderer.swap_chain_desc.BufferDesc.Width;
  viewport.Height   = (float)renderer.swap_chain_desc.BufferDesc.Height;
  viewport.MaxDepth = 1;

  renderer.viewport = viewport;

  return renderer;
}

void create_render_target_view_and_depth_stencil_view(Renderer* renderer) {
  ID3D11Texture2D* framebuffer;
  renderer->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&framebuffer); // get buffer from swapchain..

  D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {}; // (needed for SRGB framebuffer when using FLIP model swap effect)
  framebufferRTVdesc.Format        = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

  ID3D11RenderTargetView* framebufferRTV;
  renderer->device->CreateRenderTargetView(framebuffer, &framebufferRTVdesc, &framebufferRTV); // ..and put a render target view on it

  renderer->framebufferRTV = framebufferRTV;

  D3D11_TEXTURE2D_DESC depth_buffer_desc;
  framebuffer->GetDesc(&depth_buffer_desc); // copy framebuffer properties; they're mostly the same

  depth_buffer_desc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
  depth_buffer_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

  ID3D11Texture2D* depthbuffer;
  renderer->device->CreateTexture2D(&depth_buffer_desc, nullptr, &depthbuffer);

  ID3D11DepthStencilView* depthbufferDSV;
  renderer->device->CreateDepthStencilView(depthbuffer, nullptr, &depthbufferDSV);

  renderer->depthbufferDSV = depthbufferDSV;
}

/// @todo: This flag D3DCOMPILE_WARNINGS_ARE_ERRORS does not compile, but there are no errors on the console. Maybe I need to check for errors.
// UINT shader_compilation_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS;

UINT shader_compilation_flags = D3DCOMPILE_DEBUG;
// UINT shader_compilation_flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;

struct VertexShader {
  ID3DBlob* vertexshaderCSO;
  ID3D11VertexShader* vertexshader;
};

VertexShader create_vertex_shader(Renderer renderer, wchar_t* filename) {
  ID3DBlob* vertexshaderCSO;
  D3DCompileFromFile(filename, nullptr, nullptr, "VsMain", "vs_5_0", shader_compilation_flags, 0, &vertexshaderCSO, nullptr);

  ID3D11VertexShader* vertexshader;
  renderer.device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &vertexshader);

  VertexShader vertex_shader = {};
  vertex_shader.vertexshaderCSO = vertexshaderCSO;
  vertex_shader.vertexshader = vertexshader;

  return vertex_shader;
}

ID3D11PixelShader* create_pixel_shader(Renderer renderer, wchar_t* filename) {
  ID3DBlob* pixelshaderCSO;
  D3DCompileFromFile(filename, nullptr, nullptr, "PsMain", "ps_5_0", shader_compilation_flags, 0, &pixelshaderCSO, nullptr);

  ID3D11PixelShader* pixelshader;
  renderer.device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &pixelshader);
  return pixelshader;
}

ID3D11RasterizerState* create_rasterizer(Renderer renderer) {
  D3D11_RASTERIZER_DESC rasterizerdesc = {};
  rasterizerdesc.FillMode = D3D11_FILL_SOLID;
  rasterizerdesc.CullMode = D3D11_CULL_BACK;

  ID3D11RasterizerState* rasterizerstate;
  renderer.device->CreateRasterizerState(&rasterizerdesc, &rasterizerstate);

  return rasterizerstate;
}

ID3D11SamplerState* create_sampler_state(Renderer renderer) {
  D3D11_SAMPLER_DESC samplerdesc = {};
  samplerdesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
  samplerdesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
  samplerdesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
  samplerdesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
  samplerdesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

  ID3D11SamplerState* samplerstate;
  renderer.device->CreateSamplerState(&samplerdesc, &samplerstate);

  return samplerstate;
}

ID3D11DepthStencilState* create_depth_stencil_state(Renderer renderer) {
  D3D11_DEPTH_STENCIL_DESC depthstencildesc = {};
  depthstencildesc.DepthEnable    = TRUE;
  depthstencildesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depthstencildesc.DepthFunc      = D3D11_COMPARISON_LESS;

  ID3D11DepthStencilState* depthstencilstate;
  renderer.device->CreateDepthStencilState(&depthstencildesc, &depthstencilstate);

  return depthstencilstate;
}