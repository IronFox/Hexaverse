#ifndef engine_dx11_texture_fontH
#define engine_dx11_texture_fontH

/******************************************************************

dx11 texture-font.

******************************************************************/


#include <engine/textout.h>
//#include <d3d11.h>
//#include <d3dx11.h>
//#include <d3dx10.h>
//#include <D3Dcompiler.h>
//#include <D3DCommon.h>
#include "../ExtRenderer/DirectX11.h"

namespace Engine
{
	namespace DX11
	{
		class TextureFont	//! DX11 texture font object usable in combination with the Textout class
		{
		public:
			typedef float (*FOnTag)(const float3&, char, float scaleX, float scaleY);
			typedef float (*FTagWidth)(char);

		protected:
			struct TCharacter
			{
				bool			isset;
				unsigned		width,
								height,
								yoffset;
				Rect<float>		texcoords;
			};

			static const size_t	NumFloatsPerVertex = 8;
			typedef TVec<float,NumFloatsPerVertex>	TVertex;
			static const size_t MaxStream = 64;
			float2				pixelOffset;

			TVertex				vertices[MaxStream];

			Texture					atlas;
			TCharacter				chars[0x100];
			float					height,
									scale;
			ObjectBuffer<float4>	colorBuffer;
			StreamVBO				stream;
			BaseInstructionBlock	instruction;
			unsigned			at = 0;
			bool				tagMode = false;
			TIntRange<char>		tagRange;

			FOnTag				onTag = nullptr;
			FTagWidth			tagWidth = nullptr;


			//TFontState			stateCopy;

			TMatrix4<>			m;

			//float3				beginAt;
								
			void				clear();

			float				CharWidth(char c);

			void				Render(count_t numVertices);
		public:
	                            TextureFont();
		virtual                 ~TextureFont();
			void				LoadFromFile(const PathString&filename, float scale=1.0f);	//!< Loads the local font configuration from the specified XML file.

	        float               getWidth(const char*str);				//!< Calculates the unscaled width of the specified string. For EveGLTextureFont this is identical to the length of the string. \param str String to determine the length of \return Unscaled length of the string
	        float               getWidth(const char*str, size_t len);	//!< Calculates the unscaled width of the specified string. For EveGLTextureFont this is identical to the length of the string. \param str String to determine the length of \param len Number of characters to stop after \return Unscaled length of the string
	        float               getWidth(char c);						//!< Calculates the unscaled width of a single character. For EveGLTextureFont this is always 1. \param c Character to determine the width of \return Unscaled length of the character
			float				getHeight()	{return scale*height;}
	        void                begin(const TFontState&state);		//!< Begins text output starting with the specified state \param state Status to begin with
	        void                alterColor(const TFontColor&color);	//!< Changes the active color while between begin() and end(). \param color Color to use from here on
	        void                write(const char*str);					//!< Print the specified string. Newline characters are overridden  \param str String to print
	        void                write(const char*str, size_t len);	//!< Print the specified string. Newline characters are overridden  \param str String to print \param len Number of characters to print of \b str
	        void                end();									//!< Ends text output

			void				BeginTagMode(FOnTag onTag,FTagWidth tagWidth,char tagRangeBegin,char tagRangeEnd)
			{
				tagMode = true;
				this->onTag = onTag;
				this->tagWidth = tagWidth;
				this->tagRange = IntRange(tagRangeBegin,tagRangeEnd);
			}
			void				EndTagMode()
			{
				tagMode = false;
			}
		};
	}
}

#endif

