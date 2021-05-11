#ifdef HAS_WINDOW

#include <global_root.h>
#include "texture_font.h"
#include <io/xml.h>
#include <image/converter/magic.h>
#include <io/file_system.h>
#include <string/encoding.h>

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
				throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, Except::globalString("XML font file lacks <font> tag"));

			if (!get(*xfont,"scale",scale))
				throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, Except::globalString("XML <font> attribute lacks 'scale' parameter"));
			scale*=scale_;
			String imagefile;
			if (!xfont->Query("image",imagefile))
				throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, Except::globalString("XML <font> attribute lacks 'image' parameter"));
			
			Image image;
			Magic::LoadFromFile(image,root + FileSystem::StringToPath(imagefile));
			if (image.GetChannels() == 4)
				for (UINT32 x = 0; x < image.GetWidth(); x++)
					for (UINT32 y = 0; y < image.GetHeight(); y++)
					{
						BYTE*px = image.Get(x,y);
						if (px[3] == 0)
							px[0] = px[1] = px[2] = 0;
					}
			atlas.Load(image);
			pixelOffset = 0.5f * float2(1.f / image.GetWidth(), 1.f / image.GetHeight());

			String shaderfile;
			if (!xfont->Query("shader",shaderfile))
				throw Except::IO::DriveAccess::FileFormatFault(CLOCATION, Except::globalString("XML <font> attribute lacks 'shader' parameter"));

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
				instruction.shader.LoadFromFile(root + FileSystem::StringToPath(shaderfile),"vsMain", "gsMain", "psMain",bindings,ARRAYSIZE(bindings));
				DX11::Shader::macroDefinitions = macroCopy;
			}
			instruction.binding.Append("Texcoords",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
			instruction.binding.Append("Attributes",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
			instruction.UpdateLayout();


			if (image.channels() != 4)
				throw Except::IO::DriveAccess::FileFormatFault("XML font file specifies unusable font image. Image must have 4 channels (RGBA) but has "+String(image.channels()));

	
			height = 0;
			unsigned lowest = UNSIGNED_UNDEF;
		
			for (index_t i = 0; i < xfont->children.Count(); i++)
			{
				const XML::Node&xchar = xfont->children[i];
				if (xchar.name != "char")
					continue;
			
				BYTE id;
				if (!get(xchar,"id",id))	//<- CP1252/ANSI encoded character
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
				chars[id].texcoords.Set(left,bottom,right+1,top+1);
				chars[id].texcoords.x /= (float)(image.GetWidth()-1);
				chars[id].texcoords.y /= (float)(image.GetHeight()-1);
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


		float TextureFont::CharWidth(char32_t c32)
		{
			if (!c32)
				return 0;
			if (isTag && isTag(c32))
			{
				return tagWidth(c32);
			}

			char c;
			if (StringEncoding::UTF32::ToCP1252(c32,c) && chars[(BYTE)c].isset)
				return chars[(BYTE)c].width;

			return chars[(BYTE)'?'].width;
		}


		float TextureFont::GetWidth(const UTF32StringRef&str)
		{
			float rs = 0;
			const auto*c = str.pointer(),
					*const end = c + str.GetLength();
			for (;c != end; ++c)
			{
				rs += CharWidth(*c);
			}
			return rs*scale;
		}

		float TextureFont::GetWidth(const StringRef&str)
		{
			float rs = 0;
			StringEncoding::UTF8::Scanner scanner(str);
			while (scanner.HasNext())
			{
				rs += CharWidth(scanner.Next());
			}
			return rs*scale;
		}

		float TextureFont::GetWidth(char c)
		{
			return CharWidth(c)*scale;
		}

		void TextureFont::Begin(const TFontState&state)
		{
			Mat::Eye(m);
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

		void TextureFont::AlterColor(const TFontColor&color)
		{
			//glColor4fv(color.v);
			colorBuffer.Update(color);
		}

		void TextureFont::Render(count_t numVertices)
		{
			stream.PartialUpdate(vertices,numVertices,sizeof(TVertex));

			instruction.vbo = stream;

			DirectX11::globalInstance->RenderVertices(instruction);
		}

		count_t TextureFont::WriteChar(count_t written, char32_t c32)
		{
			if (written == MaxStream)
			{
				Render(written);
				written = 0;
			}

			if (isTag && isTag(c32))
			{
				if (written)
				{
					Render(written);
					written = 0;
				}

				m.w.x += m.x.x  * at;
				DirectX11::globalInstance->SetObjectMatrix(m);

				m.w.x += onTag(m.w.xyz,c32,m.x.x,m.y.y);

				at = 0;

				DirectX11::globalInstance->SetObjectMatrix(m);
		
				return written;
			}

			char c1252;
			if (!StringEncoding::UTF32::ToCP1252(c32,c1252) || !chars[(BYTE)c1252].isset)
				c1252 = '?';

			TVertex&vtx = vertices[written++];
			const TCharacter&ch = chars[(BYTE)c1252];
			vtx.x = ch.texcoords.x.min - pixelOffset.x;
			vtx.y = ch.texcoords.y.min - pixelOffset.y;
			vtx.z = ch.texcoords.x.max - pixelOffset.x;
			vtx.w = ch.texcoords.y.max - pixelOffset.y;
			vtx.v[4] = ch.width;
			vtx.v[5] = ch.height;
			vtx.v[6] = at;
			vtx.v[7] = ch.yoffset;
			at += ch.width;// + 2;
			return written;
		}

		void TextureFont::Write(const StringRef&str)
		{
			count_t written = 0;
			const char*current = str.pointer(),
					*const end = current + str.GetLength();
			while (current != end)
			{
				char32_t c32;
				if (!StringEncoding::UTF8::ToUTF32(current, end, c32))
					break;
				written = WriteChar(written,c32);
			}
			if (written)
				Render(written);
		}
		
		void TextureFont::Write(const UTF32StringRef&str)
		{
			index_t written = 0;
			const char32_t*current = str.pointer(),
					*const end = current + str.GetLength();
			while (current != end)
			{
				char32_t c32 = *current;
				written = WriteChar(written,c32);
				current++;
			}
			if (written)
				Render(written);
		}

		void TextureFont::End()
		{
		}
	}
}

#endif
