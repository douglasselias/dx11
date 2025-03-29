HWND create_window(const char* title)
{
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

Renderer create_renderer(HWND window)
{
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

void create_render_target_view_and_depth_stencil_view(Renderer* renderer)
{
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

VertexShader create_vertex_shader(Renderer renderer, wchar_t* filename)
{
  ID3DBlob* vertexshaderCSO;
  D3DCompileFromFile(filename, nullptr, nullptr, "VsMain", "vs_5_0", shader_compilation_flags, 0, &vertexshaderCSO, nullptr);

  ID3D11VertexShader* vertexshader;
  renderer.device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &vertexshader);

  VertexShader vertex_shader = {};
  vertex_shader.vertexshaderCSO = vertexshaderCSO;
  vertex_shader.vertexshader = vertexshader;

  return vertex_shader;
}

ID3D11PixelShader* create_pixel_shader(Renderer renderer, wchar_t* filename)
{
  ID3DBlob* pixelshaderCSO;
  D3DCompileFromFile(filename, nullptr, nullptr, "PsMain", "ps_5_0", shader_compilation_flags, 0, &pixelshaderCSO, nullptr);

  ID3D11PixelShader* pixelshader;
  renderer.device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &pixelshader);
  return pixelshader;
}

ID3D11RasterizerState* create_rasterizer(Renderer renderer)
{
  D3D11_RASTERIZER_DESC rasterizerdesc = {};
  rasterizerdesc.FillMode = D3D11_FILL_SOLID;
  rasterizerdesc.CullMode = D3D11_CULL_BACK;

  ID3D11RasterizerState* rasterizerstate;
  renderer.device->CreateRasterizerState(&rasterizerdesc, &rasterizerstate);

  return rasterizerstate;
}

ID3D11SamplerState* create_sampler_state(Renderer renderer)
{
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

ID3D11DepthStencilState* create_depth_stencil_state(Renderer renderer)
{
  D3D11_DEPTH_STENCIL_DESC depthstencildesc = {};
  depthstencildesc.DepthEnable    = TRUE;
  depthstencildesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depthstencildesc.DepthFunc      = D3D11_COMPARISON_LESS;

  ID3D11DepthStencilState* depthstencilstate;
  renderer.device->CreateDepthStencilState(&depthstencildesc, &depthstencilstate);

  return depthstencilstate;
}

struct Constants { matrix transform, projection; float3 lightvector; };

ID3D11Buffer* create_constant_buffer(Renderer renderer)
{
  D3D11_BUFFER_DESC constantbufferdesc = {};
  /// @todo: Might be worth to have ByteWidth as a parameter, and the alignment be a macro.
  constantbufferdesc.ByteWidth      = sizeof(Constants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
  constantbufferdesc.Usage          = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
  constantbufferdesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
  constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  ID3D11Buffer* constantbuffer;
  renderer.device->CreateBuffer(&constantbufferdesc, nullptr, &constantbuffer);

  return constantbuffer;
}

ID3D11ShaderResourceView* create_texture_shader_resource_view(Renderer renderer)
{
  D3D11_TEXTURE2D_DESC texturedesc = {};
  /// @todo: Might be worth to have the width and height as parameters.
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

  return textureSRV;
}

ID3D11Buffer* create_vertex_buffer(Renderer renderer)
{
  D3D11_BUFFER_DESC vertexbufferdesc = {};
  vertexbufferdesc.ByteWidth = sizeof(vertexdata);
  vertexbufferdesc.Usage     = D3D11_USAGE_IMMUTABLE; // will never be updated 
  vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

  D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertexdata }; // in xube.h

  ID3D11Buffer* vertexbuffer;
  renderer.device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexbuffer);

  return vertexbuffer;
}

ID3D11Buffer* create_index_buffer(Renderer renderer)
{
  D3D11_BUFFER_DESC indexbufferdesc = {};
  indexbufferdesc.ByteWidth = sizeof(indexdata);
  indexbufferdesc.Usage     = D3D11_USAGE_IMMUTABLE; // will never be updated
  indexbufferdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

  D3D11_SUBRESOURCE_DATA indexbufferSRD = { indexdata }; // in xube.h

  ID3D11Buffer* indexbuffer;
  renderer.device->CreateBuffer(&indexbufferdesc, &indexbufferSRD, &indexbuffer);

  return indexbuffer;
}

matrix create_projection_matrix(float width, float height, float near, float far)
{
  float aspect_ratio = width / height;
  float h = 1.0f;

  return
  {
    2 * near / aspect_ratio, 0,            0,                         0,
    0,                       2 * near / h, 0,                         0,
    0,                       0,            far / (far - near),        1,
    0,                       0,            near * far / (near - far), 0,
  };
}

matrix create_rotation_x_matrix(float angle)
{
  float sine   = sin(angle);
  float cosine = cos(angle);

  return
  {
    1, 0,       0,      0,
    0, cosine, -sine,   0,
    0, sine,    cosine, 0,
    0, 0,       0,      1,
  };
}

matrix create_rotation_y_matrix(float angle)
{
  float sine   = sin(angle);
  float cosine = cos(angle);

  return
  {
    cosine, 0, sine,   0,
    0,      1, 0,      0,
    -sine,  0, cosine, 0,
    0,      0, 0,      1,
  };
}

matrix create_rotation_z_matrix(float angle)
{
  float sine   = sin(angle);
  float cosine = cos(angle);

  return
  {
    cosine, -sine,   0, 0,
    sine,    cosine, 0, 0,
    0,       0,      1, 0,
    0,       0,      0, 1,
  };
}

matrix create_scale_matrix(float3 scale_vector)
{
  return
  {
    scale_vector.x, 0,              0,              0,
    0,              scale_vector.y, 0,              0,
    0,              0,              scale_vector.z, 0,
    0,              0,              0,              1,
  };
}

matrix create_translation_matrix(float3 translation_vector)
{
  return
  {
    1,                    0,                    0,                    0,
    0,                    1,                    0,                    0,
    0,                    0,                    1,                    0,
    translation_vector.x, translation_vector.y, translation_vector.z, 1,
  };
}

void update_constant_buffer(Renderer renderer, ID3D11Buffer* constantbuffer, Constants constants)
{
  D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
  renderer.device_context->Map(constantbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);

  Constants* constants_mapped   = (Constants*)constantbufferMSR.pData;
  constants_mapped->transform   = constants.transform;
  constants_mapped->projection  = constants.projection;
  constants_mapped->lightvector = constants.lightvector;

  renderer.device_context->Unmap(constantbuffer, 0);
}

void clear_screen(Renderer renderer, float clearcolor[4])
{
  renderer.device_context->ClearRenderTargetView(renderer.framebufferRTV, clearcolor);
  renderer.device_context->ClearDepthStencilView(renderer.depthbufferDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
}