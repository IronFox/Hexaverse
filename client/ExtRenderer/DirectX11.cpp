#ifdef HAS_WINDOW

#include <global_root.h>
#define INITGUID
#include "DirectX11.h"
#include <io/file_system.h>
//#include <DxErr.h>
#include <io/cryptographic_hash.h>
#include <engine/timing.h>
#include <comdef.h>

namespace Engine
{
	static inline size_t Align32(size_t s)
	{
		if (s%4)
			s =(s/4+1)*32;
		return s;
	}

	static UINT SizeOf(DXGI_FORMAT format)
	{
		switch (format)
		{
			default:
				FATAL__("Unexpected format");
			return 0;

			case DXGI_FORMAT_R32G32B32A32_TYPELESS:
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
			case DXGI_FORMAT_R32G32B32A32_UINT:
			case DXGI_FORMAT_R32G32B32A32_SINT:
				return 4 * 4;
			case DXGI_FORMAT_R32G32B32_TYPELESS:
			case DXGI_FORMAT_R32G32B32_FLOAT:
			case DXGI_FORMAT_R32G32B32_UINT:
			case DXGI_FORMAT_R32G32B32_SINT:
				return 3 * 4;
			case DXGI_FORMAT_R16G16B16A16_TYPELESS:
			case DXGI_FORMAT_R16G16B16A16_FLOAT:
			case DXGI_FORMAT_R16G16B16A16_UNORM:
			case DXGI_FORMAT_R16G16B16A16_UINT:
			case DXGI_FORMAT_R16G16B16A16_SNORM:
			case DXGI_FORMAT_R16G16B16A16_SINT:
				return 4 * 2;
			case DXGI_FORMAT_R32G32_TYPELESS:
			case DXGI_FORMAT_R32G32_FLOAT:
			case DXGI_FORMAT_R32G32_UINT:
			case DXGI_FORMAT_R32G32_SINT:
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
				return 2 * 4;
			case DXGI_FORMAT_R10G10B10A2_TYPELESS:
			case DXGI_FORMAT_R10G10B10A2_UNORM:
			case DXGI_FORMAT_R10G10B10A2_UINT:
			case DXGI_FORMAT_R11G11B10_FLOAT:
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_R8G8B8A8_UINT:
			case DXGI_FORMAT_R8G8B8A8_SNORM:
			case DXGI_FORMAT_R8G8B8A8_SINT:
			case DXGI_FORMAT_R16G16_TYPELESS:
			case DXGI_FORMAT_R16G16_FLOAT:
			case DXGI_FORMAT_R16G16_UNORM:
			case DXGI_FORMAT_R16G16_UINT:
			case DXGI_FORMAT_R16G16_SNORM:
			case DXGI_FORMAT_R16G16_SINT:
			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32_UINT:
			case DXGI_FORMAT_R32_SINT:
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
				return 4;

			case DXGI_FORMAT_R8G8_TYPELESS:
			case DXGI_FORMAT_R8G8_UNORM:
			case DXGI_FORMAT_R8G8_UINT:
			case DXGI_FORMAT_R8G8_SNORM:
			case DXGI_FORMAT_R8G8_SINT:
			case DXGI_FORMAT_R16_TYPELESS:
			case DXGI_FORMAT_R16_FLOAT:
			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_UNORM:
			case DXGI_FORMAT_R16_UINT:
			case DXGI_FORMAT_R16_SNORM:
			case DXGI_FORMAT_R16_SINT:
				return 2;
			case DXGI_FORMAT_R8_TYPELESS:
			case DXGI_FORMAT_R8_UNORM:
			case DXGI_FORMAT_R8_UINT:
			case DXGI_FORMAT_R8_SNORM:
			case DXGI_FORMAT_R8_SINT:
			case DXGI_FORMAT_A8_UNORM:
			case DXGI_FORMAT_R1_UNORM:
				return 1;
			case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
			case DXGI_FORMAT_R8G8_B8G8_UNORM:
			case DXGI_FORMAT_G8R8_G8B8_UNORM:
				return 4;
			//case DXGI_FORMAT_BC1_TYPELESS:
			//case DXGI_FORMAT_BC1_UNORM:
			//case DXGI_FORMAT_BC1_UNORM_SRGB:
			//case DXGI_FORMAT_BC2_TYPELESS:
			//case DXGI_FORMAT_BC2_UNORM:
			//case DXGI_FORMAT_BC2_UNORM_SRGB:
			//case DXGI_FORMAT_BC3_TYPELESS:
			//case DXGI_FORMAT_BC3_UNORM:
			//case DXGI_FORMAT_BC3_UNORM_SRGB:
			//case DXGI_FORMAT_BC4_TYPELESS:
			//case DXGI_FORMAT_BC4_UNORM:
			//case DXGI_FORMAT_BC4_SNORM:
			//case DXGI_FORMAT_BC5_TYPELESS:
			//case DXGI_FORMAT_BC5_UNORM:
			//case DXGI_FORMAT_BC5_SNORM:
			case DXGI_FORMAT_B5G6R5_UNORM:
			case DXGI_FORMAT_B5G5R5A1_UNORM:
				return 2;
			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
			case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8X8_TYPELESS:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
				return 4;
			//case DXGI_FORMAT_BC6H_TYPELESS:
			//case DXGI_FORMAT_BC6H_UF16:
			//case DXGI_FORMAT_BC6H_SF16:
			//case DXGI_FORMAT_BC7_TYPELESS:
			//case DXGI_FORMAT_BC7_UNORM:
			//case DXGI_FORMAT_BC7_UNORM_SRGB:
		}
	}


	/*static*/ DirectX11			*DirectX11::globalInstance = NULL;


	void			DirectX11::ReportError(HRESULT rs, const TCodeLocation&location)
	{
		errorCode = (int)rs;
		_com_error err(rs);
		errorString = err.ErrorMessage();
			//String(DXGetErrorString(rs)) + " ("+ DXGetErrorDescription(rs)+")";
		 
		//ErrMessage(errorString);
		//this is actually a fatal exception. It just should not happen
		Except::fatal(location,errorString);
		//FATAL__(errorString);
	}


	template <typename T>
		inline static void Release(T*&object)
		{
			if (object)
			{
				object->Release();
				object = NULL;
			}
		}

	void			DirectX11::_CreateBlendState(BlendMode mode,bool enable,D3D11_BLEND srcColor, D3D11_BLEND dstColor, D3D11_BLEND_OP colorOp,  D3D11_BLEND srcAlpha, D3D11_BLEND dstAlpha, D3D11_BLEND_OP alphaOp)
	{
		D3D11_BLEND_DESC blendDesc;
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;

		D3D11_RENDER_TARGET_BLEND_DESC rtbd;
		ZeroMemory( &rtbd, sizeof(rtbd) );

		rtbd.BlendEnable			 = enable;
		rtbd.SrcBlend				 = srcColor;
		rtbd.DestBlend				 = dstColor;
		rtbd.BlendOp				 = colorOp;
		rtbd.SrcBlendAlpha			 = srcAlpha;
		rtbd.DestBlendAlpha			 = dstAlpha;
		rtbd.BlendOpAlpha			 = alphaOp;
		rtbd.RenderTargetWriteMask	 = D3D11_COLOR_WRITE_ENABLE_ALL;
		blendDesc.RenderTarget[0] = rtbd;

		HRESULT rs = dev->CreateBlendState(&blendDesc, blendState[mode].Init());
		if (rs != S_OK)
		{
			ReportError(rs, CLOCATION);
			DestroyContext();
		}
	}


	const char*		DirectX11::GetVertexShaderProfile()	const
	{
		switch (featureLevel)
		{
			case D3D_FEATURE_LEVEL_11_0:
				return "vs_5_0";
			case D3D_FEATURE_LEVEL_10_1:
		    case D3D_FEATURE_LEVEL_10_0:
				return "vs_4_0";
			default:
				return "vs_2_0";
		}
	}
	const char*		DirectX11::GetPixelShaderProfile()	const
	{
		switch (featureLevel)
		{
			case D3D_FEATURE_LEVEL_11_0:
				return "ps_5_0";
			case D3D_FEATURE_LEVEL_10_1:
		    case D3D_FEATURE_LEVEL_10_0:
				return "ps_4_0";
			default:
				return "ps_2_0";
		}
	}
	const char*		DirectX11::GetGeometryShaderProfile()	const
	{
		switch (featureLevel)
		{
			case D3D_FEATURE_LEVEL_11_0:
				return "gs_5_0";
			case D3D_FEATURE_LEVEL_10_1:
		    case D3D_FEATURE_LEVEL_10_0:
				return "gs_4_0";
			default:
				return "gs_2_0";
		}
	}


	bool			DirectX11::CreateContext(HWND hWnd, TVisualConfig&config, const Resolution&res)
	{
		DXGI_SWAP_CHAIN_DESC scd;
		// clear out the struct for use
		ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

		// fill the swap chain description struct
		scd.BufferCount = 1;                                    // one back buffer
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // use 32-bit color
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
		scd.OutputWindow = hWnd;                                // the window to be used
		vsync = config.vertical_sync;

		scd.SampleDesc.Quality = 0;
		if (config.fsaa_samples > 1 && knownQualityLevels)
		{
			scd.SampleDesc.Quality = knownQualityLevels - 1;
		}
		//config.fsaa_samples > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
		scd.SampleDesc.Count = std::max<BYTE>(1, (BYTE)(knownQualityLevels ? config.fsaa_samples : 1));             // how many multisamples
		scd.Windowed = TRUE;                                    // windowed/full-screen mode
		
		this->fsaa = scd.SampleDesc;


		UINT flags = 0;
		#ifdef _DEBUG
			flags |= /*D3D11_CREATE_DEVICE_DEBUGGABLE |*/ D3D11_CREATE_DEVICE_DEBUG;
		#endif
		// create a device, device context and swap chain using the information in the scd struct
		

		D3D_FEATURE_LEVEL inputFeatureLevel = D3D_FEATURE_LEVEL_10_0;

		//D3D_FEATURE_LEVEL_11_0;

		HRESULT rs = D3D11CreateDeviceAndSwapChain(NULL,
                                  D3D_DRIVER_TYPE_HARDWARE,
                                  NULL,
                                  flags,
                                  &inputFeatureLevel,
                                  1,
                                  D3D11_SDK_VERSION,
                                  &scd,
                                  &swapchain,
                                  &dev,
                                  &featureLevel,
                                  &devcon);
		if (rs != S_OK)
		{
			ReportError(rs, CLOCATION);
			DestroyContext();
			return false;
		}

		{
			ID3D11Debug *d3dDebug = nullptr;
			if( SUCCEEDED( dev->QueryInterface( __uuidof(ID3D11Debug), (void**)&d3dDebug ) ) )
			{
				ID3D11InfoQueue *d3dInfoQueue = nullptr;
				if( SUCCEEDED( dev->QueryInterface( __uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue ) ) )
				{
					#ifdef _DEBUG
						d3dInfoQueue->SetBreakOnSeverity( D3D11_MESSAGE_SEVERITY_CORRUPTION, true );
						d3dInfoQueue->SetBreakOnSeverity( D3D11_MESSAGE_SEVERITY_ERROR, true );
					#endif
 
					D3D11_MESSAGE_ID hide [] =
					{
						D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
						//D3D11_MESSAGE_ID_DEVICE_UNORDEREDACCESSVIEW_NOT_SET,
						//(D3D11_MESSAGE_ID)3146081,
						//D3D11_MESSAGE_ID_DEVICE_PSSETSHADERRESOURCES_HAZARD,	//the engine doesn't unset these because this would require additional complexities
						//D3D11_MESSAGE_ID_DEVICE_OMSETRENDERTARGETS_HAZARD,
						// Add more message IDs here as needed
					};
 
					D3D11_INFO_QUEUE_FILTER filter;
					memset( &filter, 0, sizeof(filter) );
					filter.DenyList.NumIDs = _countof(hide);
					filter.DenyList.pIDList = hide;
					d3dInfoQueue->AddStorageFilterEntries( &filter );
					d3dInfoQueue->Release();
				}
				d3dDebug->Release();
			}
		}
		{
			IDXGIDevice * pDXGIDevice;
			rs =  dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
			if (rs == S_OK)
			{
				IDXGIAdapter * pDXGIAdapter;
				rs = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);
				if (rs == S_OK)
				{
					IDXGIFactory * pIDXGIFactory;
					pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&pIDXGIFactory);
					pIDXGIFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
				}
			}
		}


		if (!knownQualityLevels && config.fsaa_samples > 1)
		{
			//UINT maxQuality;
			rs = dev->CheckMultisampleQualityLevels(scd.BufferDesc.Format, config.fsaa_samples, &knownQualityLevels);
			if (rs != S_OK)
			{
				ReportError(rs, CLOCATION);
				DestroyContext();
				return false;
			}

			if (knownQualityLevels > 0)
			{
				errorCode = ERR_RETRY;
				errorString = "Determined sample quality levels at " + String(knownQualityLevels);
				DestroyContext();
				return false;
			}	//else: no point retrying
		}


	    //DX11::Handle<ID3D11Texture2D> pBackBuffer;
	    rs = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)backbufferTexture.Init());
		if (rs != S_OK)
		{
			ReportError(rs, CLOCATION);
			DestroyContext();
			return false;
		}
	    // use the back buffer address to create the render target
		rs = dev->CreateRenderTargetView(backbufferTexture, NULL, backbufferView.Init());
		if (rs != S_OK)
		{
			ReportError(rs, CLOCATION);
			DestroyContext();
			return false;
		}
		//pBackBuffer.Release();


		if (!_CreateDepthStencilTarget(res))
			return false;





		{	//create camera buffer
			CameraData data;
			Mat::eye(data.projectionMatrix);
			Mat::eye(data.viewMatrix);
			Mat::eye(data.viewProjectionMatrix);
			Mat::eye(data.worldCamera);
			cameraBuffer.Create(data,DX11::BufferAccess::Dynamic,D3D11_BIND_CONSTANT_BUFFER);
		}
		{	//create object buffer
			objectBuffer.Create(Matrix<>::eye4,DX11::BufferAccess::Dynamic,D3D11_BIND_CONSTANT_BUFFER);
		}
		{	//create object position buffer
			objectPositionBuffer.Create(float4(),DX11::BufferAccess::Dynamic,D3D11_BIND_CONSTANT_BUFFER);
		}
		{	//create object position buffer
			timeBuffer.Create(0,DX11::BufferAccess::Dynamic,D3D11_BIND_CONSTANT_BUFFER);
		}
		baseBindings << DX11::Shader::Binding(cameraBuffer,"Camera");
		baseBindings << DX11::Shader::Binding(timeBuffer,"Time");
		baseBindings << DX11::Shader::Binding(objectBuffer,"Object");
		baseBindings << DX11::Shader::Binding(objectPositionBuffer,"ObjectPosition");

		// set the render target as the back buffer
		TargetBackbuffer();

		for (int i = 0; i < 2; i++)
			for (int j = 0; j < 2; j++)
			{
				depthStencilState[i][j] = CreateDepthStencilState(!!i,!!j);
				if (!depthStencilState[i][j])
				{
					DestroyContext();
					return false;
				}


				D3D11_RASTERIZER_DESC	rasterizer;
				ZeroMemory(&rasterizer,sizeof(rasterizer));
				rasterizer.FillMode = i ? D3D11_FILL_SOLID : D3D11_FILL_WIREFRAME;
				rasterizer.CullMode = j ? D3D11_CULL_BACK : D3D11_CULL_NONE;
				rasterizer.FrontCounterClockwise = true;
				rasterizer.AntialiasedLineEnable = true;	//let's see what this does

				dev->CreateRasterizerState(&rasterizer,rasterizerState[i][j].Init());
			}


			{
				_CreateBlendState(BlendMode::None,false,D3D11_BLEND_ONE,D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,D3D11_BLEND_ONE,D3D11_BLEND_ZERO,D3D11_BLEND_OP_ADD);
				_CreateBlendState(BlendMode::AlphaBlend,true,D3D11_BLEND_SRC_ALPHA,D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,D3D11_BLEND_ONE,D3D11_BLEND_ONE,D3D11_BLEND_OP_ADD);
				_CreateBlendState(BlendMode::FlareBlend,true,D3D11_BLEND_SRC_ALPHA,D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,D3D11_BLEND_ONE,D3D11_BLEND_ONE,D3D11_BLEND_OP_ADD);
				_CreateBlendState(BlendMode::Add,true,D3D11_BLEND_ONE,D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,D3D11_BLEND_ONE,D3D11_BLEND_ONE,D3D11_BLEND_OP_ADD);
														
			}

		SetDepthTest0(true,true);
		SetRasterizer0(true,true);
		SetBlendMode0(BlendMode::AlphaBlend);




		_currentResolution = res;
		return true;
	}


	void				DirectX11::SetDepthTest(bool depthTest, bool depthWrite)
	{
		if (current.depthTest == depthTest && current.depthWrite == depthWrite)
			return;
		SetDepthTest0(depthTest,depthWrite);
	}

	void				DirectX11::SetDepthTest0(bool depthTest, bool depthWrite)
	{
		current.depthTest = depthTest;
		current.depthWrite = depthWrite;
		devcon->OMSetDepthStencilState(depthStencilState[depthTest][depthWrite], 0);
	}


	void				DirectX11::SetRasterizer(bool fill, bool cull)
	{
		if (current.fill == fill && current.cull == cull)
			return;
		SetRasterizer0(fill,cull);
	}


	void				DirectX11::SetRasterizer0(bool fill, bool cull)
	{
		current.fill = fill;
		current.cull = cull;

		devcon->RSSetState(rasterizerState[fill][cull]);
	}

	void				DirectX11::SetBlendMode(BlendMode mode)
	{
		if (current.blendMode == mode)
			return;
		SetBlendMode0(mode);
	}

	void				DirectX11::SetBlendMode0(BlendMode mode)
	{
		current.blendMode = mode;
		devcon->OMSetBlendState(blendState[mode],NULL,0xFFFFFFFF);
	}

	void				DirectX11::RestoreRendererState()
	{
		ASSERT__(stack.IsNotEmpty());
		TRendererState state = stack.Pop();
		SetBlendMode(state.blendMode);
		SetRasterizer(state.fill,state.cull);
		SetDepthTest(state.depthTest,state.depthWrite);
	}



	DX11::Handle<ID3D11DepthStencilState>	DirectX11::CreateDepthStencilState(bool depthTest, bool depthWrite)
	{
		DX11::Handle<ID3D11DepthStencilState> rs;
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
		// Initialize the description of the stencil state.
		ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

		// Set up the description of the stencil state.
		depthStencilDesc.DepthEnable = depthTest;
		depthStencilDesc.DepthWriteMask = depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = depthTest ? D3D11_COMPARISON_LESS_EQUAL : D3D11_COMPARISON_ALWAYS;

		depthStencilDesc.StencilEnable = false;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;

		// Stencil operations if pixel is front-facing.
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		// Stencil operations if pixel is back-facing.
		depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;



		// Create the depth stencil state.
		HRESULT hr = dev->CreateDepthStencilState(&depthStencilDesc, rs.Init());
		if(FAILED(hr))
		{
			ReportError(hr, CLOCATION);
			rs.Clear();
		}
		return rs;
	}

	void	DirectX11::TargetBackbuffer()
	{
		UnbindShaderResources();
		devcon->OMSetRenderTargets(1, backbufferView.Reference(),backbufferDepthView);
	//	devcon->OMSetRenderTargets(1, backbufferView.Reference(),NULL);
	}

	bool	DirectX11::TargetFBO(const FBO&fbo)
	{
		if (fbo.IsEmpty())
			return false;
		ID3D11RenderTargetView*views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		for (index_t i = 0; i < fbo.numTargets; i++)
			views[i] = fbo.colorTarget[i].rtView;

		UnbindShaderResources();
		devcon->OMSetRenderTargets((UINT)fbo.numTargets, views, fbo.depthStencilTarget.dsView);
		return true;
	}




	void		DirectX11::DestroyContext()
	{
		for (int i = 0; i < 2; i++)
			for (int j = 0; j < 2; j++)
			{
				depthStencilState[i][j].Clear();
				rasterizerState[i][j].Clear();
			}
		for (index_t i = 0; i < BlendMode::NumberOfPossibilities; i++)
			blendState[i].Clear();
		baseBindings.clear();
		cameraBuffer.Clear();
		objectBuffer.Clear();
		objectPositionBuffer.Clear();
		backbufferView.Clear();
		backbufferTexture.Clear();
		backbufferDepthView.Clear();
		Release(swapchain);
		Release(dev);
		Release(devcon);

		_currentResolution = Resolution();

	}


	DirectX11::DirectX11() :swapchain(NULL), dev(NULL), devcon(NULL), errorCode(0), knownQualityLevels(0)
	{}


	void		DirectX11::UnbindShaderResources()
	{
		if (!devcon)
			return;

		static ID3D11Buffer* nullBuffer[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]={nullptr};
		devcon->VSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,nullBuffer);
		devcon->PSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,nullBuffer);
		devcon->GSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,nullBuffer);

		static ID3D11ShaderResourceView* nullView[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]={nullptr};
		devcon->VSSetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,nullView);
		devcon->PSSetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,nullView);
		devcon->GSSetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,nullView);

		static ID3D11SamplerState* nullState[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT]={nullptr};
		devcon->VSSetSamplers(0,D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,nullState);
		devcon->PSSetSamplers(0,D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,nullState);
		devcon->GSSetSamplers(0,D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,nullState);

		lazyPixelShaderHandle = nullptr;
	}

	void		DirectX11::SignalWindowResize(const Resolution&res)
	{
		if (!swapchain)
			return;
		if (_currentResolution == res)
			return;
		_currentResolution = res;


		UnbindShaderResources();
		devcon->OMSetRenderTargets(0, NULL, NULL);

		backbufferView.Clear();
		backbufferDepthView.Clear();
		backbufferTexture.Clear();
		HRESULT rs = swapchain->ResizeBuffers(1,res.width,res.height,DXGI_FORMAT_R8G8B8A8_UNORM,0);
		if (rs != S_OK)
		{
			ReportError(rs, CLOCATION);
			DestroyContext();
			application_shutting_down = true;
			return;
		}
	    //DX11::Handle<ID3D11Texture2D> pBackBuffer;
	    rs = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)backbufferTexture.Init());
		if (rs != S_OK)
		{
			ReportError(rs, CLOCATION);
			DestroyContext();
			application_shutting_down = true;
			return;
		}
	    // use the back buffer address to create the render target
		rs = dev->CreateRenderTargetView(backbufferTexture, NULL, backbufferView.Init());
		if (rs != S_OK)
		{
			ReportError(rs, CLOCATION);
			DestroyContext();
			application_shutting_down = true;
			return;
		}
		//backbufferTexture.Release();

		if (!_CreateDepthStencilTarget(res))
		{
			DestroyContext();
			application_shutting_down = true;
			return;
		}

		TargetBackbuffer();
	}

	void		DirectX11::SetViewport(const RECT&region, const Resolution&windowRes)
	{
		D3D11_VIEWPORT viewport;
		ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

		viewport.TopLeftX = region.left;
		viewport.TopLeftY = windowRes.height - region.top;
		viewport.Width = region.right - region.left ;
		viewport.Height = region.top - region.bottom ;
		viewport.MaxDepth = 1;
		viewport.MinDepth = 0;
		if (devcon)
			devcon->RSSetViewports(1, &viewport);
	}



	void		DirectX11::NextFrameNoClr()
	{
		lazyPixelShaderHandle = nullptr;
		swapchain->Present(vsync,0);
		timeBuffer.Update(Engine::timing.now);
	}

	void		DirectX11::NextFrame()
	{
		lazyPixelShaderHandle = nullptr;
		swapchain->Present(vsync,0);
		 
		devcon->ClearRenderTargetView(backbufferView, clearColor.v); // Next frame
			//D3DXCOLOR(clearColor.r,clearColor.g,clearColor.b,clearColor.a)); // Next frame
		devcon->ClearDepthStencilView(backbufferDepthView,  D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
		timeBuffer.Update(Engine::timing.now);
	}

	void		DirectX11::ClearDepth()
	{
		devcon->ClearDepthStencilView(backbufferDepthView,  D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
	
	}

	
	void		DirectX11::ClearTarget(const FBO&fbo, const TVec4<>&color /*= Vector4<>::zero*/)
	{
		ClearTargetColor(fbo,color);
		ClearTargetDepth(fbo);
	}
	void		DirectX11::ClearTargetColor(const FBO&fbo, const TVec4<>&color /*= Vector4<>::zero*/)
	{
		for (index_t i = 0; i < fbo.numTargets; i++)
			devcon->ClearRenderTargetView(fbo.colorTarget[i].rtView, color.v);
				//D3DXCOLOR(color.r,color.g,color.b,color.a));
	}

	void		DirectX11::ClearTargetDepth(const FBO&fbo)
	{
		if (fbo.depthStencilTarget.IsNotEmpty())
			devcon->ClearDepthStencilView(fbo.depthStencilTarget.dsView,D3D11_CLEAR_STENCIL | D3D11_CLEAR_DEPTH,1,0);
	}



	void DX11::Shader::Clear()
	{
		vertexStage.Clear();
		geometryStage.Clear();
		pixelStage.Clear();
	}

	void DX11::Shader::StageReflection::AppendBindings(const Binding*bindings, count_t numBindings)
	{
		if (!reflection)
			return;
		D3D11_SHADER_INPUT_BIND_DESC d;
		for (index_t i = 0; i < numBindings; i++)
		{
			HRESULT hr = reflection->GetResourceBindingDescByName(bindings[i].bufferName.c_str(),&d);
			if (hr == S_OK)
			{
				if (bindings[i].bufferHandle)
				{
					ResolvedBufferBinding&b = bindPoints.append();
					b.bufferHandle = bindings[i].bufferHandle;
					b.bindPoint = d.BindPoint;
				}
				elif (bindings[i].samplerHandle)
				{
					ResolvedSamplerBinding&b = samplerBindPoints.append();
					b.samplerHandle = bindings[i].samplerHandle;
					b.bindPoint = d.BindPoint;
				}
				elif (bindings[i].resourceView)
				{
					ResolvedTextureBinding&b = texBindPoints.append();
					b.viewHandle = bindings[i].resourceView;
					b.bindPoint = d.BindPoint;
					b.quickSlot = InvalidIndex;
				}
				elif (bindings[i].textureQuickSlot != InvalidIndex)
				{
					ResolvedTextureBinding&b = texBindPoints.append();
					b.bindPoint = d.BindPoint;
					b.quickSlot = bindings[i].textureQuickSlot;
				}

			}
		}
	}

	void					DX11::Shader::StageReflection::AppendBindingsAndCompact(const Binding*bindings, count_t numBindings)
	{
		AppendBindings(bindings,numBindings);
		bindPoints.compact();
		texBindPoints.compact();
		samplerBindPoints.compact();
	}


	void			DX11::Shader::LoadFromFile(const PathString&filename, const String&vertexShaderName, const String&pixelShaderName, const Binding*b/*=NULL*/, count_t numBindings/*=0*/)
	{
		LoadFromFile(filename,vertexShaderName,"",pixelShaderName,b,numBindings);
	}

	void			DX11::Shader::LoadFromFile(const PathString&filename, const String&vertexShaderName, const String&geometryShaderName, const String&pixelShaderName, const Binding*b/*=NULL*/, count_t numBindings/*=0*/)
	{
		Clear();

		Array<D3D_SHADER_MACRO>macros;
		if (macroDefinitions.IsNotEmpty())
		{
			macros.SetSize(macroDefinitions.count()+1);
			for (index_t i = 0; i < macroDefinitions.count(); i++)
			{
				macros[i].Definition = macroDefinitions[i].definition.c_str();
				macros[i].Name = macroDefinitions[i].name.c_str();
			}
			macros[macroDefinitions.count()].Definition = macros[macroDefinitions.count()].Name = NULL;
		}


		const BasicBuffer<Binding,Swap>&base = DirectX11::globalInstance->GetBaseBindings();
		if (vertexShaderName.IsNotEmpty())
			Load<ID3D11VertexShader>(vertexStage,filename,vertexShaderName,DirectX11::globalInstance->GetVertexShaderProfile(),[this](const void*data, SIZE_T size, ID3D11VertexShader**shader)->HRESULT
			{
				return DirectX11::globalInstance->GetDevice()->CreateVertexShader(data,size,NULL,shader);
			},base.pointer(),base.count(),b,numBindings,macros.pointer());
		if (geometryShaderName.IsNotEmpty())
			Load<ID3D11GeometryShader>(geometryStage,filename,geometryShaderName,DirectX11::globalInstance->GetGeometryShaderProfile(),[this](const void*data, SIZE_T size, ID3D11GeometryShader**shader)->HRESULT
			{
				return DirectX11::globalInstance->GetDevice()->CreateGeometryShader(data,size,NULL,shader);
			},base.pointer(),base.count(),b,numBindings,macros.pointer());
		if (pixelShaderName.IsNotEmpty())
			Load<ID3D11PixelShader>(pixelStage,filename,pixelShaderName,DirectX11::globalInstance->GetPixelShaderProfile(),[this](const void*data, SIZE_T size, ID3D11PixelShader**shader)->HRESULT
			{
				return DirectX11::globalInstance->GetDevice()->CreatePixelShader(data,size,NULL,shader);
			},base.pointer(),base.count(),b,numBindings,macros.pointer());
	}

	void			DX11::Shader::LoadGeometryStreamFromFile(const PathString&filename, const String&vertexShaderName, const String&geometryShaderName, const Array<StreamBinding>&outDefinition, const Binding*b/*=NULL*/, count_t numBindings/*=0*/)
	{
		Clear();

		Array<D3D_SHADER_MACRO>macros;
		if (macroDefinitions.IsNotEmpty())
		{
			macros.SetSize(macroDefinitions.count()+1);
			for (index_t i = 0; i < macroDefinitions.count(); i++)
			{
				macros[i].Definition = macroDefinitions[i].definition.c_str();
				macros[i].Name = macroDefinitions[i].name.c_str();
			}
			macros[macroDefinitions.count()].Definition = macros[macroDefinitions.count()].Name = NULL;
		}


		const BasicBuffer<Binding,Swap>&base = DirectX11::globalInstance->GetBaseBindings();
		Load<ID3D11VertexShader>(vertexStage,filename,vertexShaderName,"vs_5_0",[this](const void*data, SIZE_T size, ID3D11VertexShader**shader)->HRESULT
		{
			return DirectX11::globalInstance->GetDevice()->CreateVertexShader(data,size,NULL,shader);
		},base.pointer(),base.count(),b,numBindings,macros.pointer());
		if (geometryShaderName.IsNotEmpty())
			Load<ID3D11GeometryShader>(geometryStage,filename,geometryShaderName,"gs_5_0",[this,outDefinition](const void*data, SIZE_T size, ID3D11GeometryShader**shader)->HRESULT
			{
				D3D11_SO_DECLARATION_ENTRY entries[64];
				UINT stride = 0,
					at = 0;
				foreach(outDefinition,element)
				{
					D3D11_SO_DECLARATION_ENTRY&desc = entries[at++];
					desc.SemanticName = element->semanticName.c_str();
					desc.SemanticIndex = 0;
					desc.OutputSlot = 0;
					desc.ComponentCount = element->componentCount;
					desc.StartComponent = 0;
					desc.Stream = 0;
					stride += element->GetByteSize();
				}
				//return DirectX11::globalInstance->GetDevice()->CreateGeometryShaderWithStreamOutput(data,size,entries,at,/*&stride*/NULL,/*1*/0,/*D3D11_SO_NO_RASTERIZED_STREAM*/0,NULL,shader);
				return DirectX11::globalInstance->GetDevice()->CreateGeometryShaderWithStreamOutput(data,size,entries,at,&stride,1,D3D11_SO_NO_RASTERIZED_STREAM,NULL,shader);
			},base.pointer(),base.count(),b,numBindings,macros.pointer());

	}


	void			DX11::Shader::LoadFromFiles(const PathString&baseFilename, const String&vertexShaderName, const String&pixelShaderName, const Binding*b/*=NULL*/, count_t numBindings/*=0*/)
	{
		LoadFromFiles(baseFilename,vertexShaderName,"",pixelShaderName,b,numBindings);
	}

	void			DX11::Shader::LoadFromFiles(const PathString&baseFilename, const String&vertexShaderName, const String&geometryShaderName, const String&pixelShaderName, const Binding*b/*=NULL*/, count_t numBindings/*=0*/)
	{
		Clear();

		Array<D3D_SHADER_MACRO>macros;
		if (macroDefinitions.IsNotEmpty())
		{
			macros.SetSize(macroDefinitions.count()+1);
			for (index_t i = 0; i < macroDefinitions.count(); i++)
			{
				macros[i].Definition = macroDefinitions[i].definition.c_str();
				macros[i].Name = macroDefinitions[i].name.c_str();
			}
			macros[macroDefinitions.count()].Definition = macros[macroDefinitions.count()].Name = NULL;
		}

		const BasicBuffer<Binding,Swap>&base = DirectX11::globalInstance->GetBaseBindings();
		Load<ID3D11VertexShader>(vertexStage,baseFilename+".vs.hlsl",vertexShaderName,"vs_5_0",[this](const void*data, SIZE_T size, ID3D11VertexShader**shader)->HRESULT
		{
			return DirectX11::globalInstance->GetDevice()->CreateVertexShader(data,size,NULL,shader);
		},base.pointer(),base.count(),b,numBindings,macros.pointer());
		if (geometryShaderName.IsNotEmpty())
			Load<ID3D11GeometryShader>(geometryStage,baseFilename+".gs.hlsl",geometryShaderName,"gs_5_0",[this](const void*data, SIZE_T size, ID3D11GeometryShader**shader)->HRESULT
			{
				return DirectX11::globalInstance->GetDevice()->CreateGeometryShader(data,size,NULL,shader);
			},base.pointer(),base.count(),b,numBindings,macros.pointer());
		Load<ID3D11PixelShader>(pixelStage,baseFilename+".ps.hlsl",pixelShaderName,"ps_5_0",[this](const void*data, SIZE_T size, ID3D11PixelShader**shader)->HRESULT
		{
			return DirectX11::globalInstance->GetDevice()->CreatePixelShader(data,size,NULL,shader);
		},base.pointer(),base.count(),b,numBindings,macros.pointer());
	}

	/*static*/ Buffer<DX11::Shader::Macro,0,Strategy::Swap>	DX11::Shader::macroDefinitions;
	/*static*/ bool			DX11::Shader::RedefineMacro(const Macro&m, bool forceExists /*= false*/)
	{
		foreach (macroDefinitions,def)
			if (def->name == m.name)
			{
				def->definition = m.definition;
				return true;
			}
		if (forceExists)
		{
			FATAL__("Unable to find definition '"+m.name+"'");
		}
		else
			macroDefinitions << m;
		return false;
	}


	
	void	DirectX11::SetShader(const DX11::Shader&shader, const Array<DX11::Texture>&quickSlots)
	{
		devcon->VSSetShader(shader.vertexStage.shaderHandle, NULL, 0);
		devcon->GSSetShader(shader.geometryStage.shaderHandle,NULL,0);
		devcon->PSSetShader(shader.pixelStage.shaderHandle, NULL, 0);

		foreach (shader.vertexStage.bindPoints,p)
		{
			//DBG_ASSERT__(p->bufferHandle);
			devcon->VSSetConstantBuffers(p->bindPoint,1,p->bufferHandle.Reference());
		}
		foreach (shader.vertexStage.texBindPoints,p)
		{
			ID3D11ShaderResourceView*const*view = p->quickSlot == InvalidIndex ? p->viewHandle.Reference() : quickSlots[p->quickSlot].view.Reference();
			//DBG_ASSERT_NOT_NULL__(*view);
			devcon->VSSetShaderResources(p->bindPoint,1,view);
		}
		foreach (shader.vertexStage.samplerBindPoints,p)
		{
			//DBG_ASSERT__(p->bufferHandle);
			devcon->VSSetSamplers(p->bindPoint,1,p->samplerHandle.Reference());
		}
		foreach (shader.geometryStage.bindPoints,p)
		{
			//DBG_ASSERT__(p->bufferHandle);
			devcon->GSSetConstantBuffers(p->bindPoint,1,p->bufferHandle.Reference());
		}
		foreach (shader.geometryStage.texBindPoints,p)
		{
			ID3D11ShaderResourceView*const*view = p->quickSlot == InvalidIndex ? p->viewHandle.Reference() : quickSlots[p->quickSlot].view.Reference();
			//DBG_ASSERT_NOT_NULL__(*view);
			devcon->GSSetShaderResources(p->bindPoint,1,view);
		}
		foreach (shader.geometryStage.samplerBindPoints,p)
		{
			//DBG_ASSERT__(p->bufferHandle);
			devcon->GSSetSamplers(p->bindPoint,1,p->samplerHandle.Reference());
		}
		foreach (shader.pixelStage.bindPoints,p)
		{
			//DBG_ASSERT__(p->bufferHandle);
			devcon->PSSetConstantBuffers(p->bindPoint,1,p->bufferHandle.Reference());
		}
		foreach (shader.pixelStage.texBindPoints,p)
		{
			ID3D11ShaderResourceView*const*view = p->quickSlot == InvalidIndex ? p->viewHandle.Reference() : quickSlots[p->quickSlot].view.Reference();
			//DBG_ASSERT_NOT_NULL__(*view);
			devcon->PSSetShaderResources(p->bindPoint,1,view);
		}
		foreach (shader.pixelStage.samplerBindPoints,p)
		{
			//DBG_ASSERT__(p->bufferHandle);
			devcon->PSSetSamplers(p->bindPoint,1,p->samplerHandle.Reference());
		}
	}


	DX11::Shader::IncludeHandler::IncludeHandler(const PathString&relativeTo)
	{
		path = FileSystem::ExtractFileDir(relativeTo);
		if (relativeTo == path)	 //no slashes
			path = ".";
	}


	/*static*/ bool			DX11::Shader::hasHeaderDates = false;
	/*static*/ FileSystem::ftime_t	DX11::Shader::headerDate = 0;


	static char _ToHex(BYTE v)
	{
		if (v < 10)
			return '0'+v;
		return 'a' + (v-10);
	}

	void DX11::Shader::_CompileBlob(Handle<ID3DBlob>&result, const PathString&filename, const String&functionName, const char*profile, const D3D_SHADER_MACRO*macros, bool forceRecompilation /*= false*/)
	{
		PathString compiledPath = FileSystem::ExtractFileDir(filename) + "/compiled/";
		if (!FileSystem::IsDirectory(compiledPath))
			FileSystem::CreateFolder(compiledPath);

		String name = String(FileSystem::ExtractFileName(filename))+"."+functionName;
		//compiledPath += FileSystem::ExtractFileName(filename)+"."+functionName;
		if (macros != NULL)
		{
			const D3D_SHADER_MACRO*m = macros;
			while (m->Name)
			{
				name += '.'+String(m->Name)+'_'+String(m->Definition);
				m++;
			}
		}

		name += "."+String(profile);

		SHA256::HashContainer	container;
		SHA256::Hash(name.c_str(),name.length(),container);
		for (index_t i = 0; i < SHA256::HashContainer::NumBytes/2; i++)
		{
			compiledPath += _ToHex(container.bytes[i]>>4);
			compiledPath += _ToHex(container.bytes[i]&0xF);
		}

		if (forceRecompilation || !FileSystem::IsFile(compiledPath) || FileSystem::GetModificationTime(compiledPath)<FileSystem::GetModificationTime(filename) || (hasHeaderDates && FileSystem::GetModificationTime(compiledPath)<headerDate))
		{
			Handle<ID3DBlob> errors;
			FILE*f = FOPEN(filename.c_str(),"rb");
			if (!f)
				throw Except::IO::DriveAccess::FileOpenFault("Unable to open '"+filename+"'");
			fseek(f,0,SEEK_END);
			long len = ftell(f);
			fseek(f,0,SEEK_SET);
			Array<char> code(len);
			fread(code.pointer(),len,1,f);
			fclose(f);

			DX11::Shader::IncludeHandler inc(filename);
			
			HRESULT hr = D3DCompile(code.pointer(),code.length(),String(FileSystem::ExtractFileNameExt(filename)).c_str(),macros, &inc , functionName.c_str(), profile,  0, 0, result.Init(), errors.Init());
			if (errors)
			{
				String msg = String((const char*)errors->GetBufferPointer(),errors->GetBufferSize());
				ErrMessage(msg);
			}
			if (hr != S_OK)
			{
				DirectX11::globalInstance->ReportError(hr, CLOCATION);
				throw Except::Renderer::ShaderRejected();
			}
			f = FOPEN(compiledPath.c_str(),"wb");
			fwrite(result->GetBufferPointer(),result->GetBufferSize(),1,f);
			fclose(f);
			return;
		}
		FILE*f = FOPEN(compiledPath.c_str(),"rb");
		if (!f)
			throw Except::IO::DriveAccess::FileOpenFault("Unable to open '"+compiledPath+"'");
		fseek(f,0,SEEK_END);
		long len = ftell(f);
		fseek(f,0,SEEK_SET);
		D3DCreateBlob(len,result.Init());
		fread(result->GetBufferPointer(),len,1,f);
		fclose(f);
	}

	HRESULT __stdcall DX11::Shader::IncludeHandler::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
	{
		PathString filename;
		if(IncludeType == D3D_INCLUDE_LOCAL)
		{
			filename = path;
			filename += '/';
			filename += pFileName;
		}
		else if(IncludeType == D3D_INCLUDE_SYSTEM)
		{
			filename = "include/";
			filename += pFileName;
		}
		else
		{
			*ppData = NULL;
			*pBytes = 0;
			return S_OK;
		}
		FILE*f = FOPEN(filename.c_str(),"rb");
		if (!f)
		{
			*ppData = NULL;
			*pBytes = 0;
			//if (pFileName[0] != '.')	//workaround to a strange compiler bug
			//{
			//	String name = "../"+String(pFileName);
			//	return Open(IncludeType,name.c_str(),pParentData,ppData,pBytes);
			//}
			return D3D11_ERROR_FILE_NOT_FOUND;
		}
		fseek(f,0,SEEK_END);
		long len = ftell(f);
		fseek(f,0,SEEK_SET);
		BYTE* data = new BYTE[len];
		fread(data,len,1,f);
		fclose(f);
		*ppData = data;
		*pBytes = len;
		return S_OK;
	}


	HRESULT __stdcall DX11::Shader::IncludeHandler::Close(LPCVOID pData)
	{
		//if (pData)
			delete[] (BYTE*)pData;
		return S_OK;
	}






	void	DX11::VBO::Create(const void*data, size_t numVertices, size_t vertexSize, BufferAccess access)
	{
		Super::Create(data,numVertices*vertexSize,access,D3D11_BIND_VERTEX_BUFFER);
		vertexCount = (UINT)numVertices;
	}

	void	DX11::ArrayBuffer::Create(const void*data, size_t numBytes, count_t numEntries, BufferAccess access, UINT bindFlags)
	{
		Clear();
		Super::Create(data,numBytes,access,bindFlags);
		this->numEntries = numEntries;
	}

	void	DX11::BaseBuffer::Create(const void*data, size_t numBytes, BufferAccess access, UINT bindFlags)
	{
		Clear();
		if (!numBytes)
			return;
		this->access = access;
		this->numBytes = numBytes;
		D3D11_BUFFER_DESC bd;
		ZeroMemory(&bd, sizeof(bd));

		switch (access)
		{
			case BufferAccess::Dynamic:
				bd.Usage = D3D11_USAGE_DYNAMIC;
				bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			break;
			case BufferAccess::Immutable:
				bd.Usage = D3D11_USAGE_IMMUTABLE;
			break;
			case BufferAccess::Default:
				bd.Usage = D3D11_USAGE_DEFAULT;
				//bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			break;
			case BufferAccess::Staging:
				bd.Usage = D3D11_USAGE_STAGING;
				bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
			break;
		};

		bd.ByteWidth = (UINT)numBytes;
		bd.BindFlags = bindFlags;

		if (bindFlags & D3D11_BIND_STREAM_OUTPUT)
		{
			//bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			//bd.StructureByteStride = 4;
		}

		D3D11_SUBRESOURCE_DATA sd;
		sd.pSysMem = data;
		sd.SysMemPitch = 0;
		sd.SysMemSlicePitch = 0;

		HRESULT rs = DirectX11::globalInstance->GetDevice()->CreateBuffer(&bd,data?&sd:NULL,handle.Init());
		if (rs != S_OK)
		{
			DirectX11::globalInstance->ReportError(rs, CLOCATION);
			Clear();
		}
	}


	void		DX11::BaseBuffer::Update(const void*data, size_t numBytes)
	{
		ASSERT_EQUAL__(numBytes,this->numBytes);
		PartialUpdate(data,numBytes);
	}

	void		DX11::BaseBuffer::PartialUpdate(const void*data, size_t numBytes)
	{
		DBG_ASSERT_NOT_EQUAL__(access,BufferAccess::Immutable);
		DBG_ASSERT_NOT_EQUAL__(access,BufferAccess::Undefined);
		DBG_ASSERT_LESS_OR_EQUAL__(numBytes,this->numBytes);

		D3D11_MAPPED_SUBRESOURCE ms;
		HRESULT rs = DirectX11::globalInstance->GetDeviceContext()->Map(handle, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);   // map the buffer
		if (rs != S_OK)
		{
			DirectX11::globalInstance->ReportError(rs, CLOCATION);
			return;
		}
		//memcpy(ms.pData, data, numBytes);
		volatile BYTE*write = (volatile BYTE*)ms.pData;
		const volatile BYTE*read = (const volatile BYTE*)data;
		size_t remaining = numBytes;
		while (remaining >= 8)
		{
			(*(volatile UINT64*)write) = (*(const volatile UINT64*)read);
			write += 8; read += 8; remaining -= 8;
		}
		while (remaining > 0)
		{
			(*write) = (*read);
			write ++; read ++; remaining --;
		}

		//memcpy((volatile void*)ms.pData, (volatile const void*)data, numBytes);
		DirectX11::globalInstance->GetDeviceContext()->Unmap(handle, NULL);
	}


	void				DX11::StreamOutBuffer::Create(size_t byteSize, usage_t usage/*=NoUsage*/)
	{
		this->usage = usage;
		UINT usageFlags = 0;
		switch(usage)
		{
			case VertexBufferUsage:
				usageFlags |= D3D11_BIND_VERTEX_BUFFER;
			break;
			case IndexBufferUsage:
				usageFlags |= D3D11_BIND_INDEX_BUFFER;
			break;
		}
		//usageFlags |= D3D11_BIND_SHADER_RESOURCE;
		Super::Create(NULL,byteSize,BufferAccess::Default,D3D11_BIND_STREAM_OUTPUT |usageFlags);

		//D3D11_SHADER_RESOURCE_VIEW_DESC resDesc;
		//ZeroMemory(&resDesc,sizeof(resDesc));
		//resDesc.Format = DXGI_FORMAT_R8_UINT;
		//resDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		//resDesc.Buffer.ElementWidth = 1;
		//resDesc.Buffer.NumElements = byteSize;

		//HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateShaderResourceView(handle,&resDesc,view.Init());
		//if (hr != S_OK)
		//{
		//	Clear();
		//	DirectX11::globalInstance->ReportError(hr);
		//	return;
		//}

	}

	DX11::VBO					DX11::StreamOutBuffer::ToVBO(count_t numVertices)	const
	{
		ASSERT__(usage==VertexBufferUsage);
		DX11::VBO rs;
		rs.Override(*this,numVertices);
		return rs;
	}


	void				DX11::StageTexture::CreateOrResize(const Resolution3D&resolution, DXGI_FORMAT format, UINT mipmapLayers)
	{
		if (handle.IsSet() && format == this->format && this->resolution == resolution)
			return;
		this->resolution = resolution;
		this->format = format;
		this->rowSize = /*Align32*/(resolution.width * SizeOf(format));
		this->sliceSize = this->rowSize * resolution.height;

		if (resolution.height == 1 && resolution.depth == 1)
		{
			D3D11_TEXTURE1D_DESC texDesc;
			texDesc.ArraySize = 1;
			texDesc.BindFlags = 0;
			texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			texDesc.Format = format;
			texDesc.Width = resolution.width;
			texDesc.MipLevels = mipmapLayers;
			texDesc.MiscFlags = 0;
			texDesc.Usage = D3D11_USAGE_STAGING;

			HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateTexture1D(&texDesc,NULL,(ID3D11Texture1D**)handle.Init());
			if (hr != S_OK)
				DirectX11::globalInstance->ReportError(hr, CLOCATION);
		}
		elif (resolution.depth == 1)
		{
			D3D11_TEXTURE2D_DESC texDesc;
			texDesc.ArraySize = 1;
			texDesc.BindFlags = 0;
			texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			texDesc.Format = format;
			texDesc.Width = resolution.width;
			texDesc.Height = resolution.height;
			texDesc.MipLevels = mipmapLayers;
			texDesc.MiscFlags = 0;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Usage = D3D11_USAGE_STAGING;


			HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateTexture2D(&texDesc,NULL,(ID3D11Texture2D**)handle.Init());
			if (hr != S_OK)
				DirectX11::globalInstance->ReportError(hr, CLOCATION);
		}
		else
		{
			D3D11_TEXTURE3D_DESC texDesc;
			texDesc.BindFlags = 0;
			texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			texDesc.Format = format;
			texDesc.Width = resolution.width;
			texDesc.Height = resolution.height;
			texDesc.Depth = resolution.depth;
			texDesc.MipLevels = mipmapLayers;
			texDesc.MiscFlags = 0;
			texDesc.Usage = D3D11_USAGE_STAGING;

			HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateTexture3D(&texDesc,NULL,(ID3D11Texture3D**)handle.Init());
			if (hr != S_OK)
				DirectX11::globalInstance->ReportError(hr, CLOCATION);
		}

	}
	void				DX11::StageTexture::DownloadData(void*target)
	{
		if (resolution.CountPixels() == 0)
			return;
		D3D11_MAPPED_SUBRESOURCE ms;
		ms.RowPitch = (UINT)rowSize;
		ms.DepthPitch = (UINT)sliceSize;

		HRESULT rs = DirectX11::globalInstance->GetDeviceContext()->Map(handle, NULL, D3D11_MAP_READ, NULL, &ms);   // map the buffer
		if (rs != S_OK)
		{
			DirectX11::globalInstance->ReportError(rs, CLOCATION);
			return;
		}
		BYTE*to = (BYTE*)target;
		BYTE*from = (BYTE*)ms.pData;
		for (UINT32 i = 0; i < resolution.depth; i++)
		{
			for (UINT32 j = 0; j < resolution.height; j++)
			{
				memcpy(to,from,rowSize);
				to += rowSize;
				from += ms.RowPitch;
			}
		}
		//memcpy(target,ms.pData, ms.DepthPitch * resolution.depth);
		DirectX11::globalInstance->GetDeviceContext()->Unmap(handle, NULL);
	}




	void				DX11::StageBuffer::DownloadData(void*target, size_t numBytes)
	{
		if (!numBytes)
			return;
		D3D11_MAPPED_SUBRESOURCE ms;
		HRESULT rs = DirectX11::globalInstance->GetDeviceContext()->Map(handle, NULL, D3D11_MAP_READ, NULL, &ms);   // map the buffer
		if (rs != S_OK)
		{
			DirectX11::globalInstance->ReportError(rs, CLOCATION);
			return;
		}
		memcpy(target,ms.pData, std::min(numBytes,this->GetByteSize()));
		DirectX11::globalInstance->GetDeviceContext()->Unmap(handle, NULL);
	}

	void				DX11::StreamOutBuffer::DownloadData(void*target, size_t numBytes)
	{
		if (!numBytes)
			return;
		if (stageBuffer.GetByteSize() != GetByteSize())
		{
			stageBuffer.Create(GetByteSize());
		}
		DirectX11::globalInstance->GetDeviceContext()->CopyResource(stageBuffer.GetHandle(),handle);
		stageBuffer.DownloadData(target,numBytes);
	}


	static inline void Translate(D3D11_INPUT_ELEMENT_DESC&desc, const Engine::TVertexSection&section, const char*semanticName, UINT index=0)
	{
		desc.SemanticName = semanticName;
		desc.SemanticIndex = index;
		switch (section.length)
		{
			case 1:
				desc.Format = DXGI_FORMAT_R32_FLOAT;
			break;
			case 2:
				desc.Format = DXGI_FORMAT_R32G32_FLOAT;
			break;
			case 3:
				desc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
			break;
			case 4:
				desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			break;
			default:
				FATAL__("Unexpected float count");
			break;
		}
		desc.InputSlot = 0;
		desc.AlignedByteOffset = sizeof(float) * section.offset;
		desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		desc.InstanceDataStepRate = 0;
	}


	void	DX11::VertexDescription::SetTo(const VertexBinding&binding)
	{
		desc.clear();
		if (binding.vertex.isSet())
			Translate(desc.append(), binding.vertex,"POSITION");
		if (binding.color.isSet())
			Translate(desc.append(), binding.color,"COLOR");
		if (binding.normal.isSet())
			Translate(desc.append(), binding.normal,"NORMAL");
		if (binding.tangent.isSet())
			Translate(desc.append(), binding.tangent,"TANGENT");
		for (index_t i = 0; i < binding.texcoords.Count() && desc.count() < 64; i++)
		{
			if (binding.texcoords[i].isSet())
				Translate(desc.append(), binding.texcoords[i],"TEXCOORD",(UINT)i);
		}
		stride[0] = binding.floats_per_vertex * 4;
		atStream = 0;
	}


	DX11::VertexDescription&	DX11::VertexDescription::Add(const char*semanticName, UINT semanticIndex, DXGI_FORMAT format, UINT byteOffset, UINT inputSlot/*=0*/, D3D11_INPUT_CLASSIFICATION inputSlotClass/*=D3D11_INPUT_PER_VERTEX_DATA*/, UINT instanceDataStepRate/*=0*/)
	{
		D3D11_INPUT_ELEMENT_DESC&desc = this->desc.append();
		desc.AlignedByteOffset = byteOffset;
		desc.Format = format;
		desc.InputSlot = inputSlot;
		desc.InputSlotClass = inputSlotClass;
		desc.InstanceDataStepRate = instanceDataStepRate;
		desc.SemanticIndex = semanticIndex;
		desc.SemanticName = semanticName;
		stride[atStream] = std::max(byteOffset + SizeOf(format),stride[atStream]);
		return *this;
	}
	DX11::VertexDescription&	DX11::VertexDescription::Append(const char*semanticName, UINT semanticIndex, DXGI_FORMAT format)
	{
		D3D11_INPUT_ELEMENT_DESC&desc = this->desc.append();
		desc.AlignedByteOffset = stride[atStream];
		desc.Format = format;
		desc.InputSlot = atStream;
		desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		desc.InstanceDataStepRate = 0;
		desc.SemanticIndex = semanticIndex;
		desc.SemanticName = semanticName;
		stride[atStream] += SizeOf(format);
		return *this;
	}

	DX11::VertexDescription&	DX11::VertexDescription::AppendInstance(const char*semanticName, UINT semanticIndex, DXGI_FORMAT format)
	{
		D3D11_INPUT_ELEMENT_DESC&desc = this->desc.append();
		desc.AlignedByteOffset = stride[atStream];
		desc.Format = format;
		desc.InputSlot = atStream;
		desc.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
		desc.InstanceDataStepRate = 1;
		desc.SemanticIndex = semanticIndex;
		desc.SemanticName = semanticName;
		stride[atStream] += SizeOf(format);
		return *this;
	}


	static inline void Translate(D3D11_SO_DECLARATION_ENTRY&desc, const Engine::TVertexSection&section, const char*semanticName, UINT index=0)
	{
		desc.SemanticName = semanticName;
		desc.SemanticIndex = index;
		desc.OutputSlot = 0;
		desc.ComponentCount = section.length;
		desc.StartComponent = 0;
		desc.Stream = 0;
	}

	/*static*/ UINT	DX11::Shader::_CompileStreamLayout(const Engine::VertexBinding&binding,D3D11_SO_DECLARATION_ENTRY*desc, UINT descLen )
	{
		UINT current = 0;
		if (binding.vertex.isSet())
			Translate(desc[current++], binding.vertex,"POSITION");
		if (binding.color.isSet())
			Translate(desc[current++], binding.color,"COLOR");
		if (binding.normal.isSet())
			Translate(desc[current++], binding.normal,"NORMAL");
		if (binding.tangent.isSet())
			Translate(desc[current++], binding.tangent,"TANGENT");
		for (index_t i = 0; i < binding.texcoords.Count() && current < descLen; i++)
		{
			if (binding.texcoords[i].isSet())
				Translate(desc[current++], binding.texcoords[i],"TEXCOORD",(UINT)i);
		}
		return current;
	}

	DX11::Handle<ID3D11InputLayout>	DX11::Shader::BuildLayout(const VertexDescription&binding)
	{
		DX11::Handle<ID3D11InputLayout> rs;
		if (binding.desc.IsEmpty())
			return rs;
		if (!vertexStage.blob)
			FATAL__("Trying to build vertex layout using empty shader");
		HRESULT h = DirectX11::globalInstance->GetDevice()->CreateInputLayout(binding.desc.pointer(), (UINT)binding.desc.length(), vertexStage.blob->GetBufferPointer(), vertexStage.blob->GetBufferSize(), rs.Init());
		if (h != S_OK)
		{
			DirectX11::globalInstance->ReportError(h, CLOCATION);
			rs.Clear();
			return rs;
		}
		return rs;
	}



	void				DX11::PrimitiveCountQuery::Create()
	{
		Super::_Create(D3D11_QUERY_SO_STATISTICS);
	}

	void				DX11::Query::_Create(D3D11_QUERY query)
	{
		Clear();

		D3D11_QUERY_DESC desc;
		desc.MiscFlags = 0;
		desc.Query = query;
		HRESULT h = DirectX11::globalInstance->GetDevice()->CreateQuery(&desc, handle.Init());
		if (h != S_OK)
		{
			DirectX11::globalInstance->ReportError(h, CLOCATION);
			Clear();
		}
	}

	bool				DX11::Query::_Resolve(void*data,size_t dataSize)	const
	{
		return DirectX11::globalInstance->GetDeviceContext()->GetData(handle, data, (UINT)dataSize, 0) == S_OK;
	}



	void				DX11::SampleCountQuery::Create()
	{
		Super::_Create(D3D11_QUERY_OCCLUSION);
	}

	bool	DX11::PrimitiveCountQuery::Resolve(count_t&result)	const
	{
		D3D11_QUERY_DATA_SO_STATISTICS data;
		if (!_Resolve(&data,sizeof(data)))
			return false;
		result = data.NumPrimitivesWritten;
		return true;
	}

	bool	DX11::SampleCountQuery::Resolve(count_t&result)	const
	{
		UINT64 data;
		if (!_Resolve(&data,sizeof(data)))
			return false;
		result = data;
		return true;
	}


	void					DirectX11::BindVertices(const DX11::VBO&buffer, UINT stride, const DX11::VertexLayout&layout)
	{
		UINT //stride = binding.stride,
				offset = 0;
		devcon->IASetVertexBuffers(0,
                        1,
						buffer.handle.Reference(),
                        &stride,
                        &offset);
		
	}

	void					DirectX11::BindVertices(const DX11::VBO&buffer0, UINT stride0, const DX11::VBO&buffer1, UINT stride1/*,const DX11::VertexLayout&layout*/)
	{
		UINT //stride = binding.stride,
			offset[] = {0,0};
		ID3D11Buffer * buffers[] = {buffer0.handle,buffer1.handle};
		UINT stride[] = {stride0,stride1};
		devcon->IASetVertexBuffers(0,
                        2,
						buffers,
                        stride,
                        offset);
		//devcon->IASetInputLayout(layout);
	}

	static inline void _BindIndices(ID3D11DeviceContext*context, const DX11::Handle<ID3D11Buffer>&buffer, DXGI_FORMAT format)
	{
		DBG_ASSERT__(buffer.IsSet());
		context->IASetIndexBuffer(buffer,format,0);
	}


	void					DirectX11::BindIndices(const DX11::IBO16&buffer)
	{
		_BindIndices(devcon,buffer.handle,DXGI_FORMAT_R16_UINT);
	}

	void					DirectX11::BindIndices(const DX11::IBO32&buffer)
	{
		_BindIndices(devcon,buffer.handle,DXGI_FORMAT_R32_UINT);
	}

	void					DirectX11::LinkShader(const DX11::ShaderInstructionBlock&block, bool lazy, bool lock)
	{
		if (shaderLockLevel)
			return;
		if (lock)
			shaderLockLevel ++;
		DBG_ASSERT__(block.shader.IsSet());
		if (lazy)
		{
			if (lazyPixelShaderHandle == block.shader.pixelStage.shaderHandle)
				return;
		}
		lazyPixelShaderHandle = block.shader.pixelStage.shaderHandle;


		SetShader(block.shader,block.quickSlots);
		devcon->IASetPrimitiveTopology(block.topology); 
		devcon->IASetInputLayout(block.layout);
	}



	void						DirectX11::Link(const DX11::BaseInstructionBlock&block, bool lazy)
	{
		//DBG_ASSERT__(tolerateNoVBO || block.vbo.IsSet());	//must tolerate this ... sadly
		BindVertices(block.vbo,block.stride,block.layout);
		LinkShader(block,lazy,false);
		if (block.streamOutTarget.IsSet())
		{
			UINT offset = 0;
			devcon->SOSetTargets(1,block.streamOutTarget.GetHandle().Reference(),&offset);
		}

	}


	void						DirectX11::Render(const DX11::InstructionBlock16&block, bool lazy/*=false*/)
	{
		Render(block,0,(UINT)block.ibo.GetEntryCount(),lazy);
	}

	void						DirectX11::Render(const DX11::InstructionBlock16&block, UINT32 offset, UINT32 count, bool lazy/*=false*/)
	{
		if (!count)
			return;
		Link(block,lazy);
		BindIndices(block.ibo);
		devcon->DrawIndexed(count,offset,0);
	}
	void						DirectX11::Render(const DX11::InstructionBlock32&block, bool lazy/*=false*/)
	{
		Render(block,0,(UINT)block.ibo.GetEntryCount(),lazy);
	}

	void						DirectX11::Render(const DX11::InstructionBlock32&block, UINT32 offset, UINT32 count, bool lazy/*=false*/)
	{
		if (!count)
			return;
		Link(block,lazy);
		BindIndices(block.ibo);

		devcon->DrawIndexed(count,offset,0);
		if (block.streamOutTarget.IsSet())
			devcon->SOSetTargets(0,NULL,NULL);
	}


	void						DirectX11::LazyRenderVertices(const DX11::BaseInstructionBlock&block)
	{
		LazyRenderVertices(block,0,block.vbo.GetVertexCount());
	}

	void						DirectX11::RenderVertices(const DX11::BaseInstructionBlock&block)
	{
		RenderVertices(block,0,block.vbo.GetVertexCount());
	}

	void						DirectX11::RenderVertices(const DX11::BaseInstructionBlock&block, const DX11::Query&query)
	{
		devcon->Begin(query.handle);
		RenderVertices(block);
		devcon->End(query.handle);
	}

	void						DirectX11::RenderVertices(const DX11::BaseInstructionBlock&block, UINT32 offset, UINT32 count, const DX11::Query&query)
	{
		devcon->Begin(query.handle);
		RenderVertices(block,offset,count);
		devcon->End(query.handle);
	}

	
	void						DirectX11::AutoRenderVertices(const DX11::BaseInstructionBlock&block, const DX11::Query&query)
	{
		devcon->Begin(query.handle);
		AutoRenderVertices(block);
		devcon->End(query.handle);
	}
	void						DirectX11::AutoRenderVertices(const DX11::BaseInstructionBlock&block)
	{
		Link(block,false);

		devcon->DrawAuto();
		if (block.streamOutTarget.IsSet())
			devcon->SOSetTargets(0,NULL,NULL);
	}

	//static DX11::SampleCountQuery	_sampleQuery;
	

	void						DirectX11::RenderVertices(const DX11::BaseInstructionBlock&block, UINT32 offset, UINT32 count)
	{
		Link(block,false);

		devcon->Draw(count,offset);
		if (block.streamOutTarget.IsSet())
			devcon->SOSetTargets(0,NULL,NULL);
	}

	void						DirectX11::LazyRenderVertices(const DX11::BaseInstructionBlock&block, UINT32 offset, UINT32 count)
	{
		Link(block,true);

		devcon->Draw(count,offset);
		if (block.streamOutTarget.IsSet())
			devcon->SOSetTargets(0,NULL,NULL);
	}



	void					DX11::FBO::Create(Resolution res, DXGI_FORMAT format, bool depthStencil/*=false*/, mips_t mips /*=NoMips*/)
	{
		Create(res,&format,1,depthStencil,mips);
	}

	void					DX11::FBO::Clear()
	{
		for (index_t i = 0; i < numTargets; i++)
			colorTarget[i].Clear();
		depthStencilTarget.Clear();
		numTargets = 0;
	}

	size_t					DX11::FBO::GetByteSize() const
	{
		size_t byteSize = depthStencilTarget.GetByteSize();
		for (index_t i = 0; i < numTargets; i++)
			byteSize += colorTarget[i].GetByteSize();
		return byteSize;
	}


	void					DX11::FBO::Append(const Texture&tex)
	{
		if (numTargets > 0)
		{
			ASSERT__(colorTarget[0].GetResolution()==tex.GetResolution());
		}
		else if (depthStencilTarget.IsNotEmpty())
			ASSERT__(depthStencilTarget.GetResolution()==tex.GetResolution());
		ASSERT_LESS__(numTargets,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
		colorTarget[numTargets++] = tex;
	}

	void				DX11::FBO::Append(const DepthStencilTexture&ds)
	{
		if (numTargets > 0)
			ASSERT__(colorTarget[0].GetResolution()==ds.GetResolution());
		depthStencilTarget = ds;
	}
	
	static UINT _CountMipLevels(Resolution3D res)
	{
		UINT rs = 1;
		while(res.width > 1 || res.height > 1 || res.depth > 1)
		{
			res /=2;
			rs ++;
		}
		return rs;
	}
	static UINT _CountMipLevels(Resolution res)
	{
		UINT rs = 1;
		while(res.width > 1 || res.height > 1)
		{
			res /=2;
			rs ++;
		}
		return rs;
	}

	void					DX11::FBO::RecreateColorTarget(index_t targetIndex, DXGI_FORMAT format)
	{
		ASSERT_LESS__(targetIndex,numTargets);

		Resolution3D res = colorTarget[targetIndex].GetResolution();
		bool formatChange = colorTarget[targetIndex].IsEmpty() || colorTarget[targetIndex].format != format;

		D3D11_TEXTURE2D_DESC	desc;
		memset(&desc,0,sizeof(desc));
		desc.Width = res.width;
		desc.Height = res.height;
		desc.MipLevels = 1;
		if (supportsMips)
		{
			desc.MipLevels = _CountMipLevels(res);
			colorTarget[targetIndex]._SetMipLevels(desc.MipLevels);
		}
		else
			colorTarget[targetIndex]._SetMipLevels(1);
		desc.ArraySize = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		if (supportsMips)
			desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

		desc.CPUAccessFlags = 0;
		desc.SampleDesc.Count = 1;		

		desc.Format = format;

		HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateTexture2D(&desc,NULL,colorTarget[targetIndex].handle2D.Init());
		if (hr != S_OK)
		{
			Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
			return;
		}
		D3D11_RENDER_TARGET_VIEW_DESC resDesc;
		ZeroMemory(&resDesc,sizeof(resDesc));
		resDesc.Format = desc.Format;
		resDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		resDesc.Texture2D.MipSlice = 0;

		hr = DirectX11::globalInstance->GetDevice()->CreateRenderTargetView(colorTarget[targetIndex].handle2D,&resDesc,colorTarget[targetIndex].rtView.Init());
		if (hr != S_OK)
		{
			Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
			return;
		}
		//colorTarget[targetIndex].resolution = res;
		colorTarget[targetIndex]._CreateView(desc);
	}

	void					DX11::FBO::Create(Resolution res, const DXGI_FORMAT*format, count_t numTargets, bool depthStencil/*=false*/, mips_t mips /*=NoMips*/)
	{
		Clear();

		supportsMips = mips == PrepareMips;

		D3D11_TEXTURE2D_DESC	desc;
		memset(&desc,0,sizeof(desc));
		desc.Width = res.width;
		desc.Height = res.height;
		desc.MipLevels = 1;
		if (supportsMips)
			desc.MipLevels = _CountMipLevels(res);

		desc.ArraySize = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		if (supportsMips)
			desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		desc.CPUAccessFlags = 0;
		desc.SampleDesc.Count = 1;

		
		for (index_t i = 0; i < numTargets && i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		{
			this->numTargets++;
			desc.Format = format[i];

			HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateTexture2D(&desc,NULL,colorTarget[i].handle2D.Init());
			if (hr != S_OK)
			{
				Clear();
				DirectX11::globalInstance->ReportError(hr, CLOCATION);
				return;
			}
			D3D11_RENDER_TARGET_VIEW_DESC resDesc;
			ZeroMemory(&resDesc,sizeof(resDesc));
			resDesc.Format = desc.Format;
			resDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			resDesc.Texture2D.MipSlice = 0;

			hr = DirectX11::globalInstance->GetDevice()->CreateRenderTargetView(colorTarget[i].handle2D,&resDesc,colorTarget[i].rtView.Init());
			if (hr != S_OK)
			{
				Clear();
				DirectX11::globalInstance->ReportError(hr, CLOCATION);
				return;
			}
			colorTarget[i].resolution = Resolution3D(res,1);
			colorTarget[i]._CreateView(desc);
			colorTarget[i]._SetMipLevels(desc.MipLevels);
		}

		if (depthStencil)
			depthStencilTarget.Create(res);
	}

	void					DX11::FBO::GenerateMips()
	{
		for (index_t i = 0; i < numTargets && i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		{
			DirectX11::globalInstance->GetDeviceContext()->GenerateMips(this->colorTarget[i].view);
		}
	}


	void					DX11::FBO::Create(Resolution3D res, DXGI_FORMAT format, mips_t mips /*=NoMips*/)
	{
		Create(res,&format,1,mips);
	}

	void					DX11::FBO::Create(Resolution3D res, const DXGI_FORMAT*format, count_t numTargets, mips_t mips /*=NoMips*/)
	{
		if (res.depth == 1)
		{
			Create(Resolution(res.width,res.height),format,numTargets,false,mips);
			return;
		}
		Clear();

		supportsMips = mips == PrepareMips;


		D3D11_TEXTURE3D_DESC	desc;
		memset(&desc,0,sizeof(desc));
		desc.Width = res.width;
		desc.Height = res.height;
		desc.Depth = res.depth;
		desc.MipLevels = 1;
		if (supportsMips)
			desc.MipLevels = _CountMipLevels(res);

		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		if (supportsMips)
			desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

		desc.CPUAccessFlags = 0;
		
		for (index_t i = 0; i < numTargets && i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		{
			this->numTargets++;
			desc.Format = format[i];

			HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateTexture3D(&desc,NULL,colorTarget[i].handle3D.Init());
			if (hr != S_OK)
			{
				Clear();
				DirectX11::globalInstance->ReportError(hr, CLOCATION);
				return;
			}

			D3D11_RENDER_TARGET_VIEW_DESC resDesc;
			ZeroMemory(&resDesc,sizeof(resDesc));
			resDesc.Format = desc.Format;
			resDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
			resDesc.Texture3D.MipSlice = 0;
			//resDesc.Texture3D.WSize = -1;
			resDesc.Texture3D.WSize = res.depth;

			hr = DirectX11::globalInstance->GetDevice()->CreateRenderTargetView(colorTarget[i].handle3D,&resDesc,colorTarget[i].rtView.Init());
			if (hr != S_OK)
			{
				Clear();
				DirectX11::globalInstance->ReportError(hr, CLOCATION);
				return;
			}
			colorTarget[i].resolution = res;
			colorTarget[i]._CreateView(desc);
			colorTarget[i]._SetMipLevels(desc.MipLevels);
		}
	}

	void				DX11::Texture::DownloadData(void*target)
	{
		if (resolution.CountPixels() == 0)
			return;
		stageTexture.CreateOrResize(resolution,format,mipLevels);
		if (handle1D)
			DirectX11::globalInstance->GetDeviceContext()->CopyResource(stageTexture.GetHandle(),handle1D);
		elif (handle2D)
			DirectX11::globalInstance->GetDeviceContext()->CopyResource(stageTexture.GetHandle(),handle2D);
		elif (handle3D)
			DirectX11::globalInstance->GetDeviceContext()->CopyResource(stageTexture.GetHandle(),handle3D);
		else
			FATAL__("texture is undefined");
		stageTexture.DownloadData(target);
	}



	static UINT _CountMipLevels(UINT width)
	{
		UINT rs = 1;
		while(width > 1)
		{
			width = std::max(1U,width/2);
			rs ++;
		}
		return rs;
	}

	//static bool dbgForce255 = false;


	template <typename T, typename SumT>
		struct Scaler
		{
			static T		Avg(T a, T b)	{return T((SumT(a) + SumT(b))/SumT(2));}
			static T		Avg(T a, T b, T c, T d)	{return T((SumT(a) + SumT(b) + SumT(c) + SumT(d))/SumT(4));}
			static T		Avg(T a, T b, T c, T d, T e, T f, T g, T h)	{return T((SumT(a) + SumT(b) + SumT(c) + SumT(d) + SumT(e) + SumT(f) + SumT(g) + SumT(h))/SumT(8));}
		};

	template <typename T, typename SumT>
		struct Scaler4
		{
			typedef Scaler<T,SumT>	S;
			typedef T	Type;
			typedef TVec4<T>	CopyType;
			static const size_t Stride = sizeof(T)*4;

			static void	Mix2(const T*a, const T*b, T*out)
			{
				(*out++) = S::Avg(*a++,*b++);
				(*out++) = S::Avg(*a++,*b++);
				(*out++) = S::Avg(*a++,*b++);
				(*out++) = S::Avg(*a++,*b++);
			}

			static T	Summarize(const T*a, const T*b, const T*c, const T*d, int channel, const SumT&alphaSum)
			{
				return (T)(((SumT)(a[channel]) * a[3] + (SumT)(b[channel]) * b[3] + (SumT)(c[channel]) * c[3] + (SumT)(d[channel]) * d[3])
								/ alphaSum);
			}

			static void	Mix4(const T*a, const T*b, const T*c, const T*d, T*out)
			{
				//if (dbgForce255)
				//{
				//	ASSERT_EQUAL__(a[3],255);
				//	ASSERT_EQUAL__(b[3],255);
				//	ASSERT_EQUAL__(c[3],255);
				//	ASSERT_EQUAL__(d[3],255);
				//}

				SumT alphaSum = ((SumT)a[3] + (SumT)b[3] + (SumT)c[3] + (SumT)d[3]);

				if (alphaSum > 0)
				{
					(*out++) = Summarize(a,b,c,d,0,alphaSum);
					(*out++) = Summarize(a,b,c,d,1,alphaSum);
					(*out++) = Summarize(a,b,c,d,2,alphaSum);
					(*out++) = T(alphaSum/4);
				}
				else
				{
					(*out++) = S::Avg(*a++,*b++,*c++,*d++);
					(*out++) = S::Avg(*a++,*b++,*c++,*d++);
					(*out++) = S::Avg(*a++,*b++,*c++,*d++);
					(*out++) = S::Avg(*a++,*b++,*c++,*d++);
				}

				//if (dbgForce255)
				//{
				//	ASSERT_EQUAL__(out[-1],255);
				//	ASSERT_EQUAL__(out[-1],255);
				//	ASSERT_EQUAL__(out[-1],255);
				//	ASSERT_EQUAL__(out[-1],255);
				//}


			}

			static void	Mix8(const T*a, const T*b, const T*c, const T*d, const T*e, const T*f, const T*g, const T*h, T*out)
			{
				(*out++) = S::Avg(*a++,*b++,*c++,*d++,*e++,*f++,*g++,*h++);
				(*out++) = S::Avg(*a++,*b++,*c++,*d++,*e++,*f++,*g++,*h++);
				(*out++) = S::Avg(*a++,*b++,*c++,*d++,*e++,*f++,*g++,*h++);
				(*out++) = S::Avg(*a++,*b++,*c++,*d++,*e++,*f++,*g++,*h++);
			}
		};

	template <typename T, typename SumT>
		struct Scaler3
		{
			typedef Scaler<T,SumT>	S;
			typedef T	Type;
			typedef TVec3<T>	CopyType;
			static const size_t Stride = sizeof(T)*3;

			static void	Mix2(const T*a, const T*b, T*out)
			{
				(*out++) = S::Avg(*a++,*b++);
				(*out++) = S::Avg(*a++,*b++);
				(*out++) = S::Avg(*a++,*b++);
			}

			static void	Mix4(const T*a, const T*b, const T*c, const T*d, T*out)
			{
				(*out++) = S::Avg(*a++,*b++,*c++,*d++);
				(*out++) = S::Avg(*a++,*b++,*c++,*d++);
				(*out++) = S::Avg(*a++,*b++,*c++,*d++);
			}

			static void	Mix8(const T*a, const T*b, const T*c, const T*d, const T*e, const T*f, const T*g, const T*h, T*out)
			{
				(*out++) = S::Avg(*a++,*b++,*c++,*d++,*e++,*f++,*g++,*h++);
				(*out++) = S::Avg(*a++,*b++,*c++,*d++,*e++,*f++,*g++,*h++);
				(*out++) = S::Avg(*a++,*b++,*c++,*d++,*e++,*f++,*g++,*h++);
			}
		};
	template <typename T, typename SumT>
		struct Scaler2
		{
			typedef Scaler<T,SumT>	S;
			typedef T	Type;
			typedef TVec2<T>	CopyType;
			static const size_t Stride = sizeof(T)*2;

			static void	Mix2(const T*a, const T*b, T*out)
			{
				(*out++) = S::Avg(*a++,*b++);
				(*out++) = S::Avg(*a++,*b++);
			}

			static void	Mix4(const T*a, const T*b, const T*c, const T*d, T*out)
			{
				(*out++) = S::Avg(*a++,*b++,*c++,*d++);
				(*out++) = S::Avg(*a++,*b++,*c++,*d++);
			}

			static void	Mix8(const T*a, const T*b, const T*c, const T*d, const T*e, const T*f, const T*g, const T*h, T*out)
			{
				(*out++) = S::Avg(*a++,*b++,*c++,*d++,*e++,*f++,*g++,*h++);
				(*out++) = S::Avg(*a++,*b++,*c++,*d++,*e++,*f++,*g++,*h++);
			}
		};
	template <typename T, typename SumT>
		struct Scaler1
		{
			typedef Scaler<T,SumT>	S;
			typedef T	Type;
			typedef T	CopyType;
			static const size_t Stride = sizeof(T);

			static void	Mix2(const T*a, const T*b, T*out)
			{
				(*out++) = S::Avg(*a++,*b++);
			}

			static void	Mix4(const T*a, const T*b, const T*c, const T*d, T*out)
			{
				(*out++) = S::Avg(*a++,*b++,*c++,*d++);
			}

			static void	Mix8(const T*a, const T*b, const T*c, const T*d, const T*e, const T*f, const T*g, const T*h, T*out)
			{
				(*out++) = S::Avg(*a++,*b++,*c++,*d++,*e++,*f++,*g++,*h++);
			}
		};


	template <typename S>
		static void _ScaleHalfT(const void*from,Array<BYTE>&field1,const Resolution3D&sourceRes,const Resolution3D&targetRes)
		{
			const BYTE*field0 = (const BYTE*)from;
			//BYTE*field1 = (BYTE*)to;
			const size_t sliceFrom = sourceRes.width * sourceRes.height * S::Stride;
			const size_t sliceTo = targetRes.width * targetRes.height * S::Stride;
			const size_t rowFrom = sourceRes.width * S::Stride;
			const size_t rowTo = targetRes.width * S::Stride;
			const size_t stride = S::Stride;

			typedef typename S::Type ST;
			#undef GETF3
			#define GETF3(x,y,z)	(const ST*)(field0 + sliceFrom * std::min(z,sourceRes.depth-1) + rowFrom * std::min(y,sourceRes.height-1) + stride * std::min(x,sourceRes.width-1))
			//auto Mix = S::Mix8;
			S s;
			Concurrency::parallel_for(UINT(0),targetRes.depth,[=,&field1](UINT z)
			{
				for (UINT y = 0; y < targetRes.height; y++)
					for (UINT x = 0; x < targetRes.width; x++)
					{
						ST*to = (ST*)(field1 + sliceTo * (z+1) + rowTo * y + x*stride);
						s.Mix8(
								GETF3(x*2,y*2,z*2),GETF3(x*2+1,y*2,z*2),GETF3(x*2,y*2+1,z*2),GETF3(x*2+1,y*2+1,z*2),
								GETF3(x*2,y*2,z*2+1),GETF3(x*2+1,y*2,z*2+1),GETF3(x*2,y*2+1,z*2+1),GETF3(x*2+1,y*2+1,z*2+1),
								to);
					}
			});
		}

	template <typename S>
		static void _ScaleHalfT(const void*from,Array<BYTE>&field1,const Resolution&sourceRes,const Resolution&targetRes)
		{
			const BYTE*field0 = (const BYTE*)from;
			//BYTE*field1 = (BYTE*)to;
			const size_t rowFrom = sourceRes.width * S::Stride;
			const size_t rowTo = targetRes.width * S::Stride;
			const size_t stride = S::Stride;

			typedef typename S::Type ST;
			#undef GETF2
			#define GETF2(x,y)	(const ST*)(field0 + rowFrom * std::min(y,sourceRes.height-1) + stride * std::min(x,sourceRes.width-1))

			S s;
			Concurrency::parallel_for(UINT(0),targetRes.height,[=,&field1](UINT y)
			{
				for (UINT x = 0; x < targetRes.width; x++)
				{
					ST*to = (ST*)(field1 + rowTo * y + x*stride);
					s.Mix4(
							GETF2(x*2,y*2),GETF2(x*2+1,y*2),GETF2(x*2,y*2+1),GETF2(x*2+1,y*2+1),
							to);
				}
			});
		}

	template <typename S>
		static void _ScaleHalfT(const void*from,Array<BYTE>&field1,const UINT&sourceRes,const UINT&targetRes)
		{
			const BYTE*field0 = (const BYTE*)from;
			//BYTE*field1 = (BYTE*)to;

			typedef typename S::Type ST;
			const size_t stride = S::Stride;
			#undef GETF1
			#define GETF1(x)	(const ST*)(field0 + stride * (x))

			S s;
			Concurrency::parallel_for(UINT(0),targetRes,[=,&field1](UINT x)
			{
				{
					ST*to = (ST*)(field1 + x*stride);
					s.Mix2(
							GETF1(x*2),GETF1(x*2+1),
							to);
				}
			});
		}


	template <typename Res>
	static void _ScaleHalf(const void*from,Array<BYTE>&to,const Res&sourceRes,const Res&targetRes,DXGI_FORMAT fmt)
	{
		switch (fmt)
		{
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
				_ScaleHalfT<Scaler4<float,float> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R32G32B32A32_UINT:
				_ScaleHalfT<Scaler4<UINT,UINT64> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R32G32B32A32_SINT:
				_ScaleHalfT<Scaler4<int,INT64> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R32G32B32_FLOAT:
				_ScaleHalfT<Scaler3<float,float> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R32G32B32_UINT:
				_ScaleHalfT<Scaler3<UINT,UINT64> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R32G32B32_SINT:
				_ScaleHalfT<Scaler3<int,INT64> >(from,to,sourceRes,targetRes);
			break;
			//case DXGI_FORMAT_R16G16B16A16_FLOAT:
			case DXGI_FORMAT_R16G16B16A16_UNORM:
			case DXGI_FORMAT_R16G16B16A16_UINT:
				_ScaleHalfT<Scaler4<USHORT,UINT> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R16G16B16A16_SNORM:
			case DXGI_FORMAT_R16G16B16A16_SINT:
				_ScaleHalfT<Scaler4<SHORT,INT> >(from,to,sourceRes,targetRes);
			break;

			case DXGI_FORMAT_R32G32_FLOAT:
				_ScaleHalfT<Scaler2<float,float> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R32G32_UINT:
				_ScaleHalfT<Scaler2<UINT,UINT64> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R32G32_SINT:
				_ScaleHalfT<Scaler2<int,UINT64> >(from,to,sourceRes,targetRes);
			break;
			//case DXGI_FORMAT_R10G10B10A2_UNORM:
			//case DXGI_FORMAT_R10G10B10A2_UINT:
			//case DXGI_FORMAT_R11G11B10_FLOAT:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_R8G8B8A8_UINT:
			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
				_ScaleHalfT<Scaler4<BYTE,USHORT> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R8G8B8A8_SNORM:
			case DXGI_FORMAT_R8G8B8A8_SINT:
				_ScaleHalfT<Scaler4<char,short> >(from,to,sourceRes,targetRes);
			break;
			//case DXGI_FORMAT_R16G16_FLOAT:
			case DXGI_FORMAT_R16G16_UNORM:
			case DXGI_FORMAT_R16G16_UINT:
				_ScaleHalfT<Scaler2<USHORT,UINT> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R16G16_SNORM:
			case DXGI_FORMAT_R16G16_SINT:
				_ScaleHalfT<Scaler2<SHORT,INT> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R32_FLOAT:
				_ScaleHalfT<Scaler1<float,float> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R32_UINT:
				_ScaleHalfT<Scaler1<UINT,UINT64> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R32_SINT:
				_ScaleHalfT<Scaler1<int,INT64> >(from,to,sourceRes,targetRes);
			break;

			case DXGI_FORMAT_R8G8_UNORM:
			case DXGI_FORMAT_R8G8_UINT:
				_ScaleHalfT<Scaler2<BYTE,USHORT> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R8G8_SNORM:
			case DXGI_FORMAT_R8G8_SINT:
				_ScaleHalfT<Scaler2<char,short> >(from,to,sourceRes,targetRes);
			break;
			//case DXGI_FORMAT_R16_FLOAT:
			case DXGI_FORMAT_R16_UNORM:
			case DXGI_FORMAT_R16_UINT:
				_ScaleHalfT<Scaler1<USHORT,UINT> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R16_SNORM:
			case DXGI_FORMAT_R16_SINT:
				_ScaleHalfT<Scaler1<short,int> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R8_UNORM:
			case DXGI_FORMAT_R8_UINT:
			case DXGI_FORMAT_A8_UNORM:
				_ScaleHalfT<Scaler1<BYTE,USHORT> >(from,to,sourceRes,targetRes);
			break;
			case DXGI_FORMAT_R8_SNORM:
			case DXGI_FORMAT_R8_SINT:
				_ScaleHalfT<Scaler1<char,short> >(from,to,sourceRes,targetRes);
			break;
			default:
				FATAL__("Format not supported for mipmap-generation");
		}
	}


	void					DX11::Texture::Load(const Resolution3D&res, DXGI_FORMAT fmt, const void*data, bool generateMipMaps/*=false*/)
	{
		if (res.depth == 1)
		{
			Load(Resolution(res),fmt,data,generateMipMaps);
			return;
		}
		Clear();
		resolution = res;

		D3D11_TEXTURE3D_DESC	desc;
		memset(&desc,0,sizeof(desc));
		desc.Width = res.width;
		desc.Height = res.height;
		desc.Depth = res.depth;
		desc.MipLevels = 1;
		desc.Format = fmt;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		UINT layers = _CountMipLevels(res);

		HRESULT hr;
		if (!generateMipMaps || layers==1)
		{
			D3D11_SUBRESOURCE_DATA data_;
			data_.pSysMem = data;
			data_.SysMemPitch = res.width * SizeOf(fmt);
			data_.SysMemSlicePitch = data_.SysMemPitch * res.height;
			hr = DirectX11::globalInstance->GetDevice()->CreateTexture3D(&desc,&data_,handle3D.Init());
		}
		else
		{

			size_t byteSize = 0;
			desc.MipLevels = layers;
			Array<Array<BYTE> >	levels(layers-1);
			Array<D3D11_SUBRESOURCE_DATA> data_(layers);
			Resolution3D prev = res, res2 = res/2;

			data_[0].pSysMem = data;
			data_[0].SysMemPitch = res.width * SizeOf(fmt);
			data_[0].SysMemSlicePitch = data_[0].SysMemPitch * res.height;

			for (UINT i = 0; i+1 < layers; i++)
			{
				levels[i].SetSize(res2.CountPixels() * SizeOf(fmt));
				_ScaleHalf(i?levels[i-1].pointer():data,levels[i],prev,res2,fmt);

				data_[i+1].pSysMem = levels[i].pointer();
				data_[i+1].SysMemPitch = res2.width * SizeOf(fmt);
				data_[i+1].SysMemSlicePitch = data_[i+1].SysMemPitch * res2.height;

				prev = res2;
				res2/=2;
			}
			hr = DirectX11::globalInstance->GetDevice()->CreateTexture3D(&desc,data_.pointer(),handle3D.Init());
		}
		if (hr != S_OK)
		{
			Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
			return;
		}
		_CreateView(desc);
		mipLevels = layers;
	}
	void					DX11::Texture::Load(const Resolution&res, DXGI_FORMAT fmt, const void*data, bool generateMipMaps/*=false*/)
	{
		if (res.height == 1)
		{
			Load(res.width,fmt,data,generateMipMaps);
			return;
		}
		Clear();
		resolution = Resolution3D(res,1);
		D3D11_TEXTURE2D_DESC	desc;
		memset(&desc,0,sizeof(desc));
		desc.Width = res.width;
		desc.Height = res.height;
		desc.ArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = fmt;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.SampleDesc.Count = 1;

		UINT layers = _CountMipLevels(res);

		HRESULT hr;
		if (!generateMipMaps || layers==1)
		{
			D3D11_SUBRESOURCE_DATA data_;
			data_.pSysMem = data;
			data_.SysMemPitch = res.width * SizeOf(fmt);
			data_.SysMemSlicePitch = data_.SysMemPitch * res.height;
			hr = DirectX11::globalInstance->GetDevice()->CreateTexture2D(&desc,&data_,handle2D.Init());
		}
		else
		{
			size_t byteSize = 0;
			desc.MipLevels = layers;
			Array<Array<BYTE> >	levels(layers-1);
			Array<D3D11_SUBRESOURCE_DATA> data_(layers);
			Resolution prev = res, res2 = res/2;

			data_[0].pSysMem = data;
			data_[0].SysMemPitch = res.width * SizeOf(fmt);
			data_[0].SysMemSlicePitch = data_[0].SysMemPitch * res.height;

			for (UINT i = 0; i+1 < layers; i++)
			{
				levels[i].SetSize(res2.CountPixels() * SizeOf(fmt));
				_ScaleHalf(i?levels[i-1].pointer():data,levels[i],prev,res2,fmt);

				data_[i+1].pSysMem = levels[i].pointer();
				data_[i+1].SysMemPitch = res2.width * SizeOf(fmt);
				data_[i+1].SysMemSlicePitch = data_[i+1].SysMemPitch * res2.height;

				prev = res2;
				res2/=2;
			}
			hr = DirectX11::globalInstance->GetDevice()->CreateTexture2D(&desc,data_.pointer(),handle2D.Init());
		}
		if (hr != S_OK)
		{
			Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
			return;
		}
		_CreateView(desc);
		mipLevels = layers;
	}

	void					DX11::Texture::LoadCube(const Resolution&res, DXGI_FORMAT fmt, const void*data[6], bool generateMipMaps/*=false*/)
	{
		Clear();
		resolution = Resolution3D(res,1);
		D3D11_TEXTURE2D_DESC	desc;
		memset(&desc,0,sizeof(desc));
		desc.Width = res.width;
		desc.Height = res.height;
		desc.ArraySize = 6;
		desc.MipLevels = 1;
		desc.Format = fmt;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		desc.CPUAccessFlags = 0;
		desc.SampleDesc.Count = 1;

		UINT layers = _CountMipLevels(res);

		HRESULT hr;
		if (!generateMipMaps || layers==1)
		{
			D3D11_SUBRESOURCE_DATA data_[6];
			for (int i = 0; i < 6; i++)
			{
				data_[i].pSysMem = data[i];
				data_[i].SysMemPitch = res.width * SizeOf(fmt);
				data_[i].SysMemSlicePitch = data_[i].SysMemPitch * res.height;
			}
			hr = DirectX11::globalInstance->GetDevice()->CreateTexture2D(&desc,data_,handle2D.Init());
		}
		else
		{
			size_t byteSize = 0;
			desc.MipLevels = layers;
			Array<Array<BYTE> >	levels[6];
			Array<D3D11_SUBRESOURCE_DATA> data_(layers*6);
			for (int j = 0; j < 6; j++)
			{
				levels[j].SetSize(layers-1);
				
				Resolution prev = res, res2 = res/2;

				D3D11_SUBRESOURCE_DATA*dat = data_+j*layers;

				dat[0].pSysMem = data[j];
				dat[0].SysMemPitch = res.width * SizeOf(fmt);
				dat[0].SysMemSlicePitch = dat[0].SysMemPitch * res.height;

				for (UINT i = 0; i+1 < layers; i++)
				{
					levels[j][i].SetSize(res2.CountPixels() * SizeOf(fmt));
					_ScaleHalf(i?levels[j][i-1].pointer():data[j],levels[j][i],prev,res2,fmt);

					dat[i+1].pSysMem = levels[j][i].pointer();
					dat[i+1].SysMemPitch = res2.width * SizeOf(fmt);
					dat[i+1].SysMemSlicePitch = dat[i+1].SysMemPitch * res2.height;

					prev = res2;
					res2/=2;
				}
			}
			hr = DirectX11::globalInstance->GetDevice()->CreateTexture2D(&desc,data_.pointer(),handle2D.Init());
		}
		if (hr != S_OK)
		{
			Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
			return;
		}
		_CreateCubeView(desc);
		mipLevels = layers;
	}

	void					DX11::Texture::Load(UINT width, DXGI_FORMAT fmt, const void*data, bool generateMipMaps/*=false*/)
	{
		Clear();
		resolution = Resolution3D(width,1,1);
		D3D11_TEXTURE1D_DESC	desc;
		memset(&desc,0,sizeof(desc));
		desc.Width = width;
		desc.ArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = fmt;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;

		UINT layers = _CountMipLevels(width);

		HRESULT hr;
		if (!generateMipMaps || layers==1)
		{
			D3D11_SUBRESOURCE_DATA data_;
			data_.pSysMem = data;
			data_.SysMemPitch = 0;
			data_.SysMemSlicePitch = 0;
			hr = DirectX11::globalInstance->GetDevice()->CreateTexture1D(&desc,&data_,handle1D.Init());
		}
		else
		{
			size_t byteSize = 0;
			desc.MipLevels = layers;
			Array<Array<BYTE> >	levels(layers-1);
			Array<D3D11_SUBRESOURCE_DATA> data_(layers);
			UINT prev = width, res2 = std::max(1U,width/2);


			data_[0].pSysMem = data;
			data_[0].SysMemPitch = width * SizeOf(fmt);
			data_[0].SysMemSlicePitch = 0;

			for (UINT i = 0; i+1 < layers; i++)
			{
				levels[i].SetSize(res2 * SizeOf(fmt));
				_ScaleHalf(i?levels[i-1].pointer():data,levels[i],prev,res2,fmt);

				data_[i+1].pSysMem = levels[i].pointer();
				data_[i+1].SysMemPitch = 0;
				data_[i+1].SysMemSlicePitch = 0;

				prev = res2;
				res2/=2;
			}
			hr = DirectX11::globalInstance->GetDevice()->CreateTexture1D(&desc,data_.pointer(),handle1D.Init());
		}

		if (hr != S_OK)
		{
			Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
			return;
		}
		_CreateView(desc);
		mipLevels = layers;
	}

	void					DX11::Texture::LoadInts(UINT width, UINT height, BYTE channels, const BYTE*data)
	{
		DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
		switch (channels)
		{
			case 1:
				fmt = DXGI_FORMAT_R8_UINT;
			break;
			case 2:
				fmt = DXGI_FORMAT_R8G8_UINT;
			break;
			case 3:
				FATAL__("rgb not supported");
			return;
			case 4:
				fmt = DXGI_FORMAT_R8G8B8A8_UINT;
			break;
			default:
				FATAL__("unsupported channel count");
			return;
		}

		Load(Resolution(width,height),fmt,data);
	}


	static const DXGI_FORMAT formatMap[4] = {DXGI_FORMAT_R8_UNORM,DXGI_FORMAT_R8G8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM };

	void					DX11::Texture::Load(const Image&image, bool generateMipMaps/*=true*/)
	{
		switch (image.GetChannels())
		{
			case 1:
			case 2:
			case 4:
				this->Load(Resolution(image.GetWidth(),image.GetHeight()),formatMap[image.GetChannels()-1],image.GetData(),generateMipMaps);
			break;
			case 3:
			{
				Image tmp = image;
				tmp.SetChannels(4,BaseImage::CA_MAX);
				//dbgForce255 = true;
				this->Load(Resolution(image.GetWidth(),image.GetHeight()),DXGI_FORMAT_R8G8B8A8_UNORM,tmp.GetData(),generateMipMaps);
			}
			break;
			default:
				FATAL__("Unsupported number of channels");
		}
	}

	void					DX11::Texture::LoadCube(const Image image[6], bool generateMipMaps/*=true*/)
	{
		for (int i = 1; i < 6; i++)
		{
			ASSERT_EQUAL1__(image[0].GetChannels(),image[i].GetChannels(),i);
			ASSERT_EQUAL1__(image[0].GetWidth(),image[i].GetWidth(),i);
			ASSERT_EQUAL1__(image[0].GetHeight(),image[i].GetHeight(),i);
		}
		if (image[0].GetChannels()<1 || image[0].GetChannels() > 4)
			FATAL__("Unsupported number of channels");

		Image tmp[6];
		for (int i = 0; i < 6; i++)
		{
			tmp[i] = image[i];
			tmp[i].FlipVertically();
			if (image[i].GetChannels() == 3)
				tmp[i].SetChannels(4,BaseImage::CA_MAX);
		}

		const void*data[6] = {tmp[0].GetData(),tmp[1].GetData(),tmp[2].GetData(),tmp[3].GetData(),tmp[4].GetData(),tmp[5].GetData()};
		this->LoadCube(Resolution(image[0].GetWidth(),image[0].GetHeight()),formatMap[image[0].GetChannels()-1],data,generateMipMaps);
	}

	void					DX11::Texture::Load(const UnclampedFloatImage&image, bool generateMipMaps/*=true*/)
	{
		switch (image.GetChannels())
		{
			case 1:
				this->Load(Resolution(image.GetWidth(),image.GetHeight()),DXGI_FORMAT_R32_FLOAT,image.GetData(),generateMipMaps);
			break;
			case 2:
				this->Load(Resolution(image.GetWidth(),image.GetHeight()),DXGI_FORMAT_R32G32_FLOAT,image.GetData(),generateMipMaps);
			break;
			case 3:
				this->Load(Resolution(image.GetWidth(),image.GetHeight()),DXGI_FORMAT_R32G32B32_FLOAT,image.GetData(),generateMipMaps);
			break;
			case 4:
				this->Load(Resolution(image.GetWidth(),image.GetHeight()),DXGI_FORMAT_R32G32B32A32_FLOAT,image.GetData(),generateMipMaps);
			break;
			default:
				FATAL__("Unsupported number of channels");
		}
	}



	void DX11::Texture::_CreateView(D3D11_TEXTURE1D_DESC desc)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC resDesc;
		ZeroMemory(&resDesc,sizeof(resDesc));
		resDesc.Format = desc.Format;
		resDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		resDesc.Texture1D.MipLevels = desc.MipLevels;
		resDesc.Texture1D.MostDetailedMip = 0;

		byteSize = SizeOf(desc.Format) * desc.Width;
		format = desc.Format;

		HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateShaderResourceView(handle1D,&resDesc,view.Init());
		if (hr != S_OK)
		{
			handle1D.Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
			return;
		}
	}

	void DX11::Texture::_CreateView(D3D11_TEXTURE2D_DESC desc)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC resDesc;
		ZeroMemory(&resDesc,sizeof(resDesc));
		resDesc.Format = desc.Format;
		resDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		resDesc.Texture2D.MipLevels = desc.MipLevels;
		resDesc.Texture2D.MostDetailedMip = 0;

		byteSize = Align32(SizeOf(desc.Format) * desc.Width) * desc.Height;
		format = desc.Format;

		HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateShaderResourceView(handle2D,&resDesc,view.Init());
		if (hr != S_OK)
		{
			handle2D.Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
			return;
		}
	}
	void DX11::Texture::_CreateCubeView(D3D11_TEXTURE2D_DESC desc)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC resDesc;
		ZeroMemory(&resDesc,sizeof(resDesc));
		resDesc.Format = desc.Format;
		resDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		resDesc.Texture2D.MipLevels = desc.MipLevels;
		resDesc.Texture2D.MostDetailedMip = 0;

		byteSize = Align32(SizeOf(desc.Format) * desc.Width) * desc.Height * 6;
		format = desc.Format;

		HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateShaderResourceView(handle2D,&resDesc,view.Init());
		if (hr != S_OK)
		{
			handle2D.Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
			return;
		}
	}

	void DX11::Texture::_CreateView(D3D11_TEXTURE3D_DESC desc)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC resDesc;
		ZeroMemory(&resDesc,sizeof(resDesc));
		resDesc.Format = desc.Format;
		resDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		resDesc.Texture3D.MipLevels = desc.MipLevels;
		resDesc.Texture3D.MostDetailedMip = 0;

		byteSize = Align32(SizeOf(desc.Format) * desc.Width) * desc.Height * desc.Depth;
		format = desc.Format;

		HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateShaderResourceView(handle3D,&resDesc,view.Init());
		if (hr != S_OK)
		{
			handle3D.Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
			return;
		}
	}

	void					DX11::DepthStencilTexture::Create(const Resolution&res)
	{

	/*
		You would create the Texture2D with DXGI_FORMAT_R24G8_TYPELESS, create the depth stencil view with DXGI_FORMAT_D24_UNORM_S8_UINT, and create the shader resource view with DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	*/

		Clear();
		resolution = Resolution3D(res,1);
		D3D11_TEXTURE2D_DESC	desc;
		memset(&desc,0,sizeof(desc));
		desc.Width = res.width;
		desc.Height = res.height;
		desc.ArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		//DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.SampleDesc.Count = 1;

		//UINT layers = _CountMipLevels(res);

		HRESULT hr;
		{
			D3D11_SUBRESOURCE_DATA data_;
			data_.pSysMem = NULL;
			data_.SysMemPitch = res.width * SizeOf(desc.Format);
			data_.SysMemSlicePitch = data_.SysMemPitch * res.height;
			hr = DirectX11::globalInstance->GetDevice()->CreateTexture2D(&desc,/*&data_*/NULL,handle2D.Init());
		}
		if (hr != S_OK)
		{
			Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
			return;
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
		descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		descDSV.Texture2D.MipSlice = 0;
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Flags = 0;

		// Create the depth stencil view
		hr = DirectX11::globalInstance->GetDevice()->CreateDepthStencilView( handle2D, // Depth stencil texture
												 &descDSV, // Depth stencil desc
												 dsView.Init() );  // [out] Depth stencil view

		if (hr != S_OK)
		{
			Clear();
			DirectX11::globalInstance->ReportError(hr, CLOCATION);
		}

		{
			D3D11_SHADER_RESOURCE_VIEW_DESC resDesc;
			ZeroMemory(&resDesc,sizeof(resDesc));
			resDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			//DXGI_FORMAT_R32_FLOAT;
			resDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			resDesc.Texture2D.MipLevels = desc.MipLevels;
			resDesc.Texture2D.MostDetailedMip = 0;

			byteSize = Align32(SizeOf(desc.Format) * desc.Width) * desc.Height;
			format = desc.Format;

			HRESULT hr = DirectX11::globalInstance->GetDevice()->CreateShaderResourceView(handle2D,&resDesc,view.Init());
			if (hr != S_OK)
			{
				Clear();
				DirectX11::globalInstance->ReportError(hr, CLOCATION);
				return;
			}
		}
		mipLevels = 1;
	}

	bool	DirectX11::_CreateDepthStencilTarget(const Resolution&res)
	{
		DX11::Handle<ID3D11Texture2D> depthStencilTexture;
		D3D11_TEXTURE2D_DESC descDepth;
		descDepth.Width = res.width;
		descDepth.Height = res.height;
		descDepth.MipLevels = 1;
		descDepth.ArraySize = 1;
		descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		//descDepth.Format = DXGI_FORMAT_D32_FLOAT;
		descDepth.SampleDesc = fsaa;
		//descDepth.SampleDesc.Quality = 0;
		//descDepth.SampleDesc.Count = 1;
		descDepth.Usage = D3D11_USAGE_DEFAULT;
		descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		descDepth.CPUAccessFlags = 0;
		descDepth.MiscFlags = 0;
		HRESULT hr = dev->CreateTexture2D( &descDepth, NULL, depthStencilTexture.Init() );
		if (hr != S_OK)
		{
			ReportError(hr, CLOCATION);
			return false;
		}
		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
		descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		descDSV.Texture2D.MipSlice = 0;
		descDSV.ViewDimension = fsaa.Count > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Flags = 0;

		// Create the depth stencil view
		hr = dev->CreateDepthStencilView( depthStencilTexture, // Depth stencil texture
												 &descDSV, // Depth stencil desc
												 backbufferDepthView.Init() );  // [out] Depth stencil view

		if (hr != S_OK)
		{
			ReportError(hr, CLOCATION);
			return false;
		}
		return true;
	}

	void		DirectX11::SetCameraMatrices(const TMatrix4<>&view, const TMatrix4<>&projection, const TMatrix4<>&viewInvert)
	{
		CameraData data;
		data.projectionMatrix = projection;
		data.viewMatrix = view;
		Mat::transpose(viewInvert,data.worldCamera); //dx orientation change
		Mat::mult(data.projectionMatrix,data.viewMatrix,data.viewProjectionMatrix);
		//data.worldCamera = viewInvert;
		cameraBuffer.Update(data);
	}

	void		DirectX11::SetObjectMatrix(const TMatrix4<>&matrix)
	{
		objectBuffer.Update(matrix);
	}
	void		DirectX11::SetObjectPosition(const TVec4<>&pos)
	{
		objectPositionBuffer.Update(pos);
	}


	void		DirectX11::ScanShaderHeader(const PathString&path)
	{
		FileSystem::Folder folder(path);
		if (!folder.IsValidLocation())
			return;
		folder.Rewind();
		FileSystem::File file;
		while (folder.NextFile(file))
		{
			if (file.IsExtension("inc.hlsl") || file.IsExtension("hlsli"))
				DX11::Shader::_IncludeHeaderDate(file.GetModificationTime());
		};
	}



	DX11::Shader::Binding::Binding(const BaseBuffer&buffer, const String&name):bufferName(name),bufferHandle(buffer.handle),textureQuickSlot(InvalidIndex)
	{
		ASSERT__(bufferHandle);
	}
	DX11::Shader::Binding::Binding(const Texture&texture, const String&name):bufferName(name),resourceView(texture.view),textureQuickSlot(InvalidIndex)
	{
		ASSERT__(resourceView);
	}
	DX11::Shader::Binding::Binding(const DepthStencilTexture&texture, const String&name):bufferName(name),resourceView(texture.view),textureQuickSlot(InvalidIndex)
	{
		ASSERT__(resourceView);
	}
	DX11::Shader::SamplerBinding::SamplerBinding(bool clamp, Engine::TextureFilter filter, UINT maxAnisotropy, const String&name):Binding()
	{
		bufferName = name;
		
		D3D11_SAMPLER_DESC desc;
		memset(&desc,0,sizeof(desc));
		desc.AddressU = desc.AddressV = desc.AddressW = clamp ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

		switch (filter)
		{
			case TextureFilter::None:
				desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			break;
			case TextureFilter::Linear:
				desc.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			break;
			case TextureFilter::Bilinear:
				desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			break;
			case TextureFilter::Trilinear:
				desc.Filter = maxAnisotropy > 1 ? D3D11_FILTER_ANISOTROPIC :  D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			break;
			default:
				FATAL__("Unexpected filter");
		}
		desc.MaxAnisotropy = maxAnisotropy;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		desc.MinLOD = 0.f;


		DirectX11::globalInstance->GetDevice()->CreateSamplerState(&desc,samplerHandle.Init());
	}
	DX11::Shader::SamplerBinding::SamplerBinding(Engine::TextureFilter filter, UINT32 clampFlags, UINT maxAnisotropy, const String&name)
	{
		bufferName = name;
		
		D3D11_SAMPLER_DESC desc;
		memset(&desc,0,sizeof(desc));
		desc.AddressU = (clampFlags&ClampU) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = (clampFlags&ClampV) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = (clampFlags&ClampW) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

		switch (filter)
		{
			case TextureFilter::None:
				desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			break;
			case TextureFilter::Linear:
				desc.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			break;
			case TextureFilter::Bilinear:
				desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			break;
			case TextureFilter::Trilinear:
				desc.Filter = maxAnisotropy > 1 ? D3D11_FILTER_ANISOTROPIC :  D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			break;
			default:
				FATAL__("Unexpected filter");
		}
		desc.MaxAnisotropy = maxAnisotropy;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		desc.MinLOD = 0.f;


		DirectX11::globalInstance->GetDevice()->CreateSamplerState(&desc,samplerHandle.Init());
	}


	void						DirectX11::Copy(const DX11::Handle<ID3D11Texture2D>&source, Image&destination)
	{
		D3D11_TEXTURE2D_DESC desc;
		source->GetDesc(&desc);
		DXGI_FORMAT format = desc.Format;
		destination.SetContentType(PixelType::StrictColor);

		BYTE channels = 0;
		switch (format)
		{
			case DXGI_FORMAT_R8_UNORM:
				channels = 1;
			break;
			case DXGI_FORMAT_R8G8_UNORM:
				channels = 2;
			break;
			case DXGI_FORMAT_R8G8B8A8_UNORM:
				channels = 4;
			break;
			default:
				FATAL__("Unable to capture this format");
		}
		destination.SetSize(desc.Width,desc.Height,channels);
		DX11::StageTexture	stageTexture;
		DX11::FBO	fbo;
		
		Resolution3D res(desc.Width,desc.Height,1);
		
		stageTexture.CreateOrResize(res,format,desc.MipLevels);

		if (desc.SampleDesc.Count > 1)	
		{
			fbo.Create(res,format);

			devcon->ResolveSubresource(fbo.GetColorTarget().GetHandle2D(),D3D11CalcSubresource(0,0,1),source,D3D11CalcSubresource(0,0,1),format);
			devcon->CopyResource(stageTexture.GetHandle(),fbo.GetColorTarget().GetHandle2D());

		}
		else
			devcon->CopyResource(stageTexture.GetHandle(),source);
		stageTexture.DownloadData(destination.GetData());
		destination.FlipVertically();
	}


	void						DirectX11::Capture(Image&target)
	{
		Copy(backbufferTexture,target);
	}


}

#endif
