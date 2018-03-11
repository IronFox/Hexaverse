#include "loading.h"
//#include <container/queue.h>
#include <image/converter/magic.h>
#include <math/vclasses.h>
#include <future>
#include <logEvent.h>

namespace Loading
{

	namespace Detail
	{

		#ifdef HAS_WINDOW

			namespace DX11 = Engine::DX11;

			struct TTask
			{
				String					name;
				std::function<void()>	load;


				friend void swap(TTask&a, TTask&b)
				{
					a.name.swap(b.name);
					a.load.swap(b.load);
				}
			};
			Queue<TTask>	tasks;
		
			RECT									oldWindowLocation;
			UINT32									oldDisplayFlags=0;
			bool									oldWasMaximized=false;
			Engine::DisplayConfig::border_style_t	oldBorderStyle;
			Engine::DisplayConfig::FOnResize		oldOnResize;

			Display	*display=nullptr;
			Engine::VirtualTextout*textout = nullptr;



			float	imageAspect = 1.f;
			DX11::InstructionBlock16	instruction;


			void UpdateTextoutState()
			{
				textout->PushState();
				float s = 3.f;
				textout->SetScale(s / display->GetClientWidth(), s / display->GetClientHeight());
			}

			void LoadResources()
			{
				Image image;
				Magic::LoadFromFile(image,"resources/splash.png");
				imageAspect = image.GetPixelAspect();

				{
					DX11::Texture tex;
					tex.Load(image,true);

					DX11::Shader::Binding bindings[] = 
					{
						DX11::Shader::Binding(tex,"SplashTexture"),
						DX11::Shader::SamplerBinding(true,Engine::VisualEnum::TextureFilter::Trilinear,1,"SplashSampler")
					};

					instruction.shader.LoadFromFile("./shaders/splash.hlsl","vsMain","psMain",bindings,ARRAYSIZE(bindings));
				}


				instruction.binding.Append("POSITION",0,DXGI_FORMAT_R32G32_FLOAT);
				instruction.ibo.Create({0,1,2, 0,2,3});
				instruction.vbo.CreateFromStructs({float2(-1),float2(1,-1),float2(1),float2(-1,1)});
				instruction.SetTriangles();
				instruction.UpdateLayout();

				const auto screen = display->GetScreenSize();

				RECT windowPos;

				const UINT width = screen.width/2;
				const UINT height = UINT(float(width) / imageAspect);

				windowPos.left = screen.width/2 - width/2;
				windowPos.top = screen.height / 2 - height/2;

				windowPos.right = screen.width/2 + width /2;
				windowPos.bottom = screen.height/2 + height/2;

				oldWasMaximized = display->IsMaximized();
				oldDisplayFlags = Engine::Context::GetLastSignalledDisplayFlags();
				oldWindowLocation = oldWasMaximized ? display->GetPreMaxiMinimizeLocation() : display->GetWindowLocation();
				oldBorderStyle = display->GetBorderStyle();
				oldOnResize = Engine::context.ReplaceOnResize(Engine::DisplayConfig::FOnResize());

				display->SetBorderStyle(Engine::DisplayConfig::NoBorder);
				display->PositionWindow(windowPos);

				if (textout)
					UpdateTextoutState();


			}

				TTask t;

			bool	Loop()
			{
				if (Engine::input.pressed[Key::Escape])
				{
					LogEvent(EventSignificance::Important,"User abort during loading stage");
					Logging::End();
					TerminateProcess(GetCurrentProcess(),0);	//self-terminate
					FATAL__("User abort during loading stage");	//should not get here
				}

				if (tasks.Pop(t))
				{
					LogEvent(EventClass::Preset::common,"Entering loading stage: "+t.name);
					t.load();
				}
				else
					return false;

				display->Render(instruction);

				if (textout)
				{
					display->Pick(Engine::Aspect<>());	//all identity
					auto&text = *textout;
					//const float w = text.QueryScaledWidth(t.name);


					text.MoveTo(-0.6f ,-0.15f);
					text.WriteLn(t.name);
				}

		
				return true;
			}

			void	Finish()
			{
				display->StoreRendererState();

				display->SetRasterizer(true,false);
				display->setDepthTest(Engine::VisualEnum::NoDepthTest);
				display->SetBlendMode(Engine::BlendMode::AlphaBlend);

				display->Assign(Detail::Loop);
				display->ExecuteNoClr();	//executes until all tasks are done

				display->RestoreRendererState();

				textout->PopState();
				
				display->PositionWindow(Detail::oldWindowLocation);
				if (oldWasMaximized)
					display->Maximize();
				display->SetBorderStyle(Detail::oldBorderStyle);
				Engine::context.ReplaceOnResize(oldOnResize);
				oldOnResize(display->GetClientSize(),Detail::oldDisplayFlags);

				instruction.Clear();

			}

		#endif

	}


	void		Begin()
	{

	}

	void		Stage(const String&name, const std::function<void()>&loader)
	{
		#ifndef HAS_WINDOW
			loader();
		#else
			Detail::TTask&t = Detail::tasks.Push();
			t.name = name;
			t.load = loader;
		#endif

	}
	
	void		Finish()
	{
		#ifdef HAS_WINDOW

		
			Detail::Finish();

		#endif

	}
	
	
	#ifdef HAS_WINDOW

		void	SetupDisplay(Display&display)
		{
			ASSERT_IS_NULL__(Detail::display);
			Detail::display = &display;
		
			Detail::LoadResources();
		}

	
		void	SetupText(Engine::VirtualTextout&tex)
		{
			using namespace Detail;
			ASSERT_IS_NULL__(textout);
			textout = &tex;
			UpdateTextoutState();
		}

	#endif
	










}

