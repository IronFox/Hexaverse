#ifdef HAS_WINDOW

#include <global_root.h>
#include "texture_font.h"
#include <io/xml.h>
#include <image/converter/magic.h>
#include <io/file_system.h>

namespace Engine
{
	namespace DX11
	{
		TextureFont::TextureFont():scale(1.0f)
		{
			clear();
		}

		TextureFont::~TextureFont()
		{
			clear();
		}
	
		void				TextureFont::clear()
		{
			atlas.Clear();
			for (index_t i = 0; i < ARRAYSIZE(chars); i++)
				chars[i].isset = false;
		}

		template <typename T>
			static bool			get(const XML::Node&node, const String&name, T&out)
			{
				String val;
				return node.Query(name,val) && convert(val.c_str(),out);
			}

	
	
		void				TextureFont::LoadFromFile(const PathString&filename, float scale_)
		{
			clear();

			XML::Container	container;
			container.LoadFromFile(filename);
			PathString root = FileSystem::ExtractFileDir(filename) + FOLDER_SLASH;


			const XML::Node*xfont = container.Find("font");
			if (!xfont)
				throw IO::DriveAccess::FileFormatFault(globalString("XML font file lacks <font> tag"));

			if (!get(*xfont,"scale",scale))
				throw IO::DriveAccess::FileFormatFault(globalString("XML <font> attribute lacks 'scale' parameter"));
			scale*=scale_;
			String imagefile;
			if (!xfont->Query("image",imagefile))
				throw IO::DriveAccess::FileFormatFault(globalString("XML <font> attribute lacks 'image' parameter"));
			
			Image image;
			Magic::LoadFromFile(image,root + imagefile);
			if (image.getChannels() == 4)
				for (UINT32 x = 0; x < image.getWidth(); x++)
					for (UINT32 y = 0; y < image.getHeight(); y++)
					{
						BYTE*px = image.get(x,y);
						if (px[3] == 0)
							px[0] = px[1] = px[2] = 0;
					}
			atlas.Load(image);
			pixelOffset = 0.5f * float2(1.f / image.width(), 1.f / image.height());

			String shaderfile;
			if (!xfont->Query("shader",shaderfile))
				throw IO::DriveAccess::FileFormatFault(globalString("XML <font> attribute lacks 'shader' parameter"));

			colorBuffer.Create(float4());

			Shader::Binding	bindings[] =
			{
				Shader::Binding(atlas,"FontAtlas"),
				Shader::SamplerBinding(true,Engine::TextureFilter::Trilinear,1,"FontSampler"),
				Shader::Binding(colorBuffer,"Color"),
			};

			{
				auto macroCopy = DX11::Shader::macroDefinitions;
				DX11::Shader::macroDefinitions.clear();
				instruction.shader.LoadFromFile(root + shaderfile,"vsMain", "gsMain", "psMain",bindings,ARRAYSIZE(bindings));
				DX11::Shader::macroDefinitions = macroCopy;
			}
			instruction.binding.Append("Texcoords",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
			instruction.binding.Append("Attributes",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
			instruction.UpdateLayout();


			if (image.channels() != 4)
				throw IO::DriveAccess::FileFormatFault("XML font file specifies unusable font image. Image must have 4 channels (RGBA) but has "+String(image.channels()));

	
			height = 0;
			unsigned lowest = UNSIGNED_UNDEF;
		
			for (index_t i = 0; i < xfont->children.count(); i++)
			{
				const XML::Node&xchar = xfont->children[i];
				if (xchar.name != "char")
					continue;
			
				BYTE id;
				if (!get(xchar,"id",id))
					continue;
				unsigned left, right,bottom,top;
				if (!get(xchar,"left",left))
					continue;
				if (!get(xchar,"right",right))
					continue;
				if (!get(xchar,"bottom",bottom))
					continue;
				if (!get(xchar,"top",top))
					continue;
				chars[id].isset = true;
				chars[id].width = right-left+1;
				chars[id].height = top-bottom+1;
				chars[id].yoffset = bottom;
				if (bottom < lowest)
					lowest = bottom;
				if (chars[id].height > height)
					height = chars[id].height;
				chars[id].texcoords.Set(left,bottom,right,top);
				chars[id].texcoords.x /= (float)(image.width()-1);
				chars[id].texcoords.y /= (float)(image.height()-1);
			}
			for (index_t i = 0; i < ARRAYSIZE(chars); i++)
				chars[i].yoffset -= lowest;
			
			//ShowMessage(String(lower_edge)+" . "+String(upper_edge));


			if (chars[(BYTE)'i'].isset && !chars[(BYTE)' '].isset)
			{
				TCharacter&space = chars[(BYTE)' '];
				space.isset = true;
				space.texcoords.SetMinAndMax(0);
				space.width = chars[(BYTE)'i'].width;
				space.height = 1;
				space.yoffset = 0;
			}

			/*
				glTexCoord2f(chars[i].texcoords.x.min,chars[i].texcoords.y.min);   	glVertex2f(0,scale*chars[i].yoffset);
				glTexCoord2f(chars[i].texcoords.x.max,chars[i].texcoords.y.min);	glVertex2f(scale*chars[i].width,scale*chars[i].yoffset);
				glTexCoord2f(chars[i].texcoords.x.max,chars[i].texcoords.y.max);   	glVertex2f(scale*chars[i].width,scale*(chars[i].yoffset+chars[i].height));
				glTexCoord2f(chars[i].texcoords.x.min,chars[i].texcoords.y.max);   	glVertex2f(0,scale*(chars[i].yoffset+chars[i].height));
			glEnd();
			*/
			stream.Create(MaxStream,sizeof(TVertex));
		}


		float TextureFont::CharWidth(char c)
		{
			if (tagMode && tagRange.Contains(c))
			{
				return tagWidth(c);
			}

			if (chars[(BYTE)c].isset)
				return chars[(BYTE)c].width;

			return 0.f;
		}

		float TextureFont::getWidth(const char*str)
		{
			float rs = 0;
			while (*str)
			{
				rs += CharWidth(*str);
				str++;
			}
			return rs*scale;
		}

		float TextureFont::getWidth(const char*str, size_t len)
		{
			float rs = 0;
			for (index_t i = 0; i < len; i++)
			{
				rs += CharWidth(str[i]);
			}
			return rs*scale;
		}

		float TextureFont::getWidth(char c)
		{
			return chars[(BYTE)c].isset?scale*chars[(BYTE)c].width:0;
		}

		void TextureFont::begin(const TFontState&state)
		{
			Mat::eye(m);
			m.w.xyz = float3(state.left+state.indent,
			state.top 
			- 
			//(state.line) * state.y_scale * height * scale
			state.lineOffset 
			- state.y_scale * height * scale
			,
			state.depth);
			//m.w.xyz = float3(state.left+state.indent,state.top - (state.line+1) * state.y_scale * height,state.depth);
			m.x.x = state.x_scale*scale;
			m.y.y = state.y_scale*scale;
			at = 0;

			DirectX11::globalInstance->SetObjectMatrix(m);
			colorBuffer.Update(state);
			//stateCopy = state;
		}

		void TextureFont::alterColor(const TFontColor&color)
		{
			//glColor4fv(color.v);
			colorBuffer.Update(color);
		}

		void TextureFont::write(const char*str)
		{
			write(str,strlen(str));
		}

		void TextureFont::Render(count_t numVertices)
		{
			stream.PartialUpdate(vertices,numVertices,sizeof(TVertex));

			instruction.vbo = stream;

			DirectX11::globalInstance->RenderVertices(instruction);
		}

		void TextureFont::write(const char*str, size_t len)
		{
			index_t written = 0;
			for (index_t i = 0; i < len; i++)
			{
				if (written == MaxStream)
				{
					Render(written);
					written = 0;
				}

				if (tagMode && tagRange.Contains(str[i]))
				{
					if (written)
					{
						Render(written);
						written = 0;
					}

					m.w.x += m.x.x  * at;
					DirectX11::globalInstance->SetObjectMatrix(m);

					m.w.x += onTag(m.w.xyz,str[i],m.x.x,m.y.y);

					at = 0;

					DirectX11::globalInstance->SetObjectMatrix(m);
		
					continue;
				}

				TVertex&vtx = vertices[written++];
				const TCharacter&c = chars[(BYTE)str[i]];
				vtx.x = c.texcoords.x.min - pixelOffset.x;
				vtx.y = c.texcoords.y.min - pixelOffset.y;
				vtx.z = c.texcoords.x.max - pixelOffset.x;
				vtx.w = c.texcoords.y.max - pixelOffset.y;
				vtx.v[4] = c.width;
				vtx.v[5] = c.height;
				vtx.v[6] = at;
				vtx.v[7] = c.yoffset;
				at += c.width;// + 2;
			}
			if (written)
				Render(written);
				//glCallLists(GLuint(len),GL_UNSIGNED_BYTE,str);
		}

		void TextureFont::end()
		{
		}
	}
}

#endif
