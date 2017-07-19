#ifndef renderer_DirectX11H
#define renderer_DirectX11H

#ifdef HAS_WINDOW

#pragma warning(disable:4005)	//macro redefinition


#include <engine/renderer/renderer.h>
#include <math/resolution.h>
#include <math/vclasses.h>
#include <io/file_system.h>

#include <d3d11.h>
//#include <d3dx11.h>
//#include <d3dx10.h>
#include <D3Dcompiler.h>
#include <D3DCommon.h>



#pragma comment (lib, "d3d11.lib")
//#pragma comment (lib, "d3dx11.lib")
//#pragma comment (lib, "d3dx10.lib")
#pragma comment (lib, "d3dcompiler.lib")
//#pragma comment (lib, "DxErr.lib")

//#include "../../io/log.h"
//#include "../../string/string_buffer.h"

namespace Engine
{
	static const UINT InvalidBindPoint = 0xFFFFFFFF;

	//#define ERR_TABLE_ENTRY				10
	//#define ERR_CONFIG_UNSUPPORTED		(-2)
	//#define ERR_CONFIG_REJECTED			(-3)
	//#define ERR_PFD_SET_FAILED			(-4)
	//#define ERR_CONTEXT_CREATION_FAILED (-5)
	//#define ERR_CONTEXT_BINDING_FAILED	(-6)
	//#define ERR_BAD_IMAGE				(-7)
	//#define ERR_GL_WINDOW_CREATION_FAILED	(-8)



	class DirectX11;

	namespace DX11
	{



		template <typename T>
			class Handle
			{
			private:
				T*			_handle;
			public:
				/**/		Handle():_handle(NULL)	{}
				/**/		Handle(const Handle<T>&other):_handle(other._handle)	{if (_handle) _handle->AddRef();}
				/**/		~Handle()	{if (!application_shutting_down) Clear();}
				Handle<T>&	operator=(const Handle<T>&other)	{Clear(); _handle = other._handle; if (_handle) _handle->AddRef(); return *this;}
				T*			operator->() const	{return _handle;}
				operator	T*() const {return _handle;}
				bool		IsSet()	const	{return _handle != NULL;}
				bool		IsNotEmpty() const	{return _handle != NULL;}
				bool		IsEmpty() const	{return _handle == NULL;}
				T**			Init()	{Clear(); return &_handle;}
				T*const*	Reference() const {return &_handle;}
				void		Clear()		{if (_handle) _handle->Release(); _handle = NULL;}
				void		Release()	{Clear();}
				void		swap(Handle<T>&other)	{std::swap(_handle,other._handle);}
				bool		operator==(const Handle<T>&other) const {return _handle == other._handle;}
				bool		operator!=(const Handle<T>&other) const	{return _handle != other._handle;}
				bool		operator!()	{return _handle == NULL;}
			};



		class VertexDescription
		{
		private:
			Buffer<D3D11_INPUT_ELEMENT_DESC,0>	desc;
			UINT	stride[4];
			UINT	atStream;

			friend class Shader;
			friend class DirectX11;

		public:
			typedef VertexDescription	Self;

			/**/	VertexDescription():atStream(0)	{memset(stride,0,sizeof(stride));}
			/**/	VertexDescription(const VertexBinding&binding):atStream(0)	{SetTo(binding);}
			void	SetTo(const VertexBinding&binding);
			Self&	Add(const char*semanticName, UINT semanticIndex, DXGI_FORMAT format, UINT byteOffset, UINT inputSlot=0, D3D11_INPUT_CLASSIFICATION inputSlotClass=D3D11_INPUT_PER_VERTEX_DATA, UINT instanceDataStepRate=0);
			Self&	Append(const char*semanticName, UINT semanticIndex, DXGI_FORMAT format);
			Self&	AppendInstance(const char*semanticName, UINT semanticIndex, DXGI_FORMAT format);
			Self&	NextStream()	{atStream ++; ASSERT_LESS__(atStream,ARRAYSIZE(stride)); return *this;}
			void	SetStride(UINT s, int streamIndex=0)	{ASSERT_LESS__(streamIndex, ARRAYSIZE(stride)); ASSERT_GREATER_OR_EQUAL__(s,stride[streamIndex]); stride[streamIndex] = s;}	//this is usually unnecessary
			UINT	GetStride(int streamIndex=0) const {return stride[streamIndex];}
			void	Clear()	{desc.clear(); memset(stride,0,sizeof(stride)); atStream = 0;}
		};


		typedef DX11::Handle<ID3D11InputLayout>	VertexLayout;

		class BaseBuffer;
		class Texture;
		class DepthStencilTexture;

		class Shader
		{
		public:

			class StreamBinding
			{
			public:
				String			semanticName;
				BYTE			componentCount;
				enum type_t
				{
					Float32,
					Int32
				}				type;

				/**/			StreamBinding():componentCount(4)	{}

				void			Set(const String&semantic, type_t type, BYTE componentCount)
				{
					this->semanticName = semantic;
					this->componentCount = componentCount;
					this->type = type;
				}
				UINT32			GetByteSize()	const
				{
					UINT32 es = 1;
					switch (type)
					{
						case Float32:
						case Int32:
							es = 4;
						break;
					}
					return es * componentCount;
				}
			};



			class IncludeHandler: public ID3DInclude
			{
			public:
				HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes);
				HRESULT __stdcall Close(LPCVOID pData);

				IncludeHandler(const PathString&relativeTo);
			private:
				PathString path;
			};

			struct Binding
			{
				Handle<ID3D11Buffer>	bufferHandle;
				Handle<ID3D11ShaderResourceView>	resourceView;
				Handle<ID3D11SamplerState>	samplerHandle;
				index_t					textureQuickSlot;
				String					bufferName;

				/**/					Binding():textureQuickSlot(InvalidIndex)	{};
				/**/					Binding(const Handle<ID3D11Buffer>&buffer, const String&name):bufferHandle(buffer),bufferName(name),textureQuickSlot(InvalidIndex)	{}
				/**/					Binding(const BaseBuffer&buffer, const String&name);
				/**/					Binding(const Texture&texture, const String&name);
				/**/					Binding(const DepthStencilTexture&texture, const String&name);
				/**/					Binding(index_t textureQuickSlot, const String&name):bufferName(name),textureQuickSlot(textureQuickSlot)	{}

				void					swap(Binding&other)
				{
					bufferHandle.swap(other.bufferHandle);
					samplerHandle.swap(other.samplerHandle);
					resourceView.swap(other.resourceView);
					bufferName.swap(other.bufferName);
					std::swap(textureQuickSlot,other.textureQuickSlot);
				}
			};

			struct SamplerBinding : Binding
			{
				enum clampFlag_t
				{
					ClampU=0x1,
					ClampV=0x2,
					ClampW=0x4,

				};
				/**/					SamplerBinding(bool clamp, Engine::TextureFilter, UINT maxAnisotropy, const String&name);
				/**/					SamplerBinding(Engine::TextureFilter, UINT32 clampFlags, UINT maxAnisotropy, const String&name);
			};

			struct ResolvedBinding
			{
				UINT					bindPoint;
			};

			struct ResolvedBufferBinding : public ResolvedBinding
			{
				Handle<ID3D11Buffer>	bufferHandle;
			};
			struct ResolvedTextureBinding : public ResolvedBinding
			{
				Handle<ID3D11ShaderResourceView>	viewHandle;
				index_t					quickSlot;
			};
			struct ResolvedSamplerBinding : public ResolvedBinding
			{
				Handle<ID3D11SamplerState>	samplerHandle;
			};

			/*union StandardBindPoints
			{
				struct
				{
					UINT		camera,
								objectMatrix,
								objectPosition;
				};
				UINT			value[3];
			};*/
			struct StageReflection
			{
				Handle<ID3D11ShaderReflection>	reflection;
				//StandardBindPoints				bindPoints;
				Buffer<ResolvedBufferBinding,0>	bindPoints;
				Buffer<ResolvedTextureBinding,0>texBindPoints;
				Buffer<ResolvedSamplerBinding,0>samplerBindPoints;
				
				Handle<ID3DBlob>				blob;

				void				swap(StageReflection&other)
				{
					reflection.swap(other.reflection);
					blob.swap(other.blob);
					bindPoints.swap(other.bindPoints);
					texBindPoints.swap(other.texBindPoints);
					samplerBindPoints.swap(other.samplerBindPoints);
				}
				void				Clear()
				{
					texBindPoints.clear();
					bindPoints.clear();
					samplerBindPoints.clear();
					reflection.Clear();
					blob.Clear();
				}
				void					AppendBindings(const Binding*bindings, count_t numBindings);
				void					AppendBindingsAndCompact(const Binding*bindings, count_t numBindings);
			};
		protected:
			template<typename T>
				struct Stage : public StageReflection
				{
					Handle<T>		shaderHandle;

					void			swap(Stage<T>&other)
					{
						StageReflection::swap(other);
						shaderHandle.swap(other.shaderHandle);
					}
					void			Clear()
					{
						StageReflection::Clear();
						shaderHandle.Clear();
					}
					String				ToString() const
					{
						return String((void*)shaderHandle);
					}

				};

			Stage<ID3D11VertexShader>	vertexStage;
			Stage<ID3D11GeometryShader>	geometryStage;
			Stage<ID3D11PixelShader>	pixelStage;

			static bool			hasHeaderDates;
			static FileSystem::ftime_t	headerDate;

			static void			_IncludeHeaderDate(FileSystem::ftime_t d)	{if (!hasHeaderDates) headerDate = d; else headerDate = std::max(d,headerDate); hasHeaderDates = true;}

			friend class DirectX11;

			template<typename T>
				void			Load(Stage<T>&stage, const PathString&filename, const String&functionName, const char*profile, std::function<HRESULT(const void*, SIZE_T, T**)> load, const Binding*b0, count_t numBindings0, const Binding*b1, count_t numBindings1, const D3D_SHADER_MACRO*macros);
			void				_CompileBlob(Handle<ID3DBlob>&result, const PathString&filename, const String&functionName, const char*profile, const D3D_SHADER_MACRO*macros, bool forceRecompilation = false);
			/*static*/ UINT		_CompileStreamLayout(const Engine::VertexBinding&binding,D3D11_SO_DECLARATION_ENTRY*out, UINT descLen );

		public:
			struct Macro
			{
				String			name,
								definition;

				/**/			Macro()	{};
				/**/			Macro(const String&name, const String&def):name(name),definition(def)	{}
				void			swap(Macro&other)	{name.swap(other.name); definition.swap(other.definition);}
			};
			static Buffer<Macro,0,Strategy::Swap>	macroDefinitions;
			static bool			RedefineMacro(const Macro&, bool forceExists = false);

			bool				IsSet() const	{return vertexStage.shaderHandle.IsSet() || geometryStage.shaderHandle.IsSet() || pixelStage.shaderHandle.IsSet();}
			bool				IsEmpty() const	{return !IsSet();}
			void				Clear();
			void				swap(Shader&other)
			{
				vertexStage.swap(other.vertexStage);
				geometryStage.swap(other.geometryStage);
				pixelStage.swap(other.pixelStage);
			}
			VertexLayout		BuildLayout(const VertexDescription&);

			void				LoadFromFile(const PathString&filename, const String&vertexShaderName, const String&pixelShaderName, const Binding*b=NULL, count_t numBindings=0);
			void				LoadFromFile(const PathString&filename, const String&vertexShaderName, const String&geometryShaderName, const String&pixelShaderName, const Binding*b=NULL, count_t numBindings=0);
			void				LoadFromFiles(const PathString&baseFilename, const String&vertexShaderName, const String&pixelShaderName, const Binding*b=NULL, count_t numBindings=0);
			void				LoadFromFiles(const PathString&baseFilename, const String&vertexShaderName, const String&geometryShaderName, const String&pixelShaderName, const Binding*b=NULL, count_t numBindings=0);
			void				LoadGeometryStreamFromFile(const PathString&filename, const String&vertexShaderName, const String&geometryShaderName, const Array<StreamBinding>&outDefinition, const Binding*b=NULL, count_t numBindings=0);

			void				AppendBindings(const Binding*bindings, count_t numBindings)
			{
				vertexStage.AppendBindingsAndCompact(bindings,numBindings);
				geometryStage.AppendBindingsAndCompact(bindings,numBindings);
				pixelStage.AppendBindingsAndCompact(bindings,numBindings);

			}

			String				ToString() const
			{
				return "Shader{" + vertexStage.ToString() + ", " + geometryStage.ToString() + ", " + pixelStage.ToString()+"}";
			}

		};


		CONSTRUCT_ENUMERATION5(BufferAccess, Undefined, Default,Immutable, Dynamic,Staging);



		class BaseBuffer
		{
		protected:
			Handle<ID3D11Buffer> handle;
			BufferAccess		access;
			size_t				numBytes;
			friend struct Shader::Binding;
			friend class DirectX11;
			void				Create(const void*data, size_t numBytes, BufferAccess, UINT bindFlags);
			void				Update(const void*data, size_t numBytes);
			void				PartialUpdate(const void*data, size_t numBytes);

			/**/				BaseBuffer():numBytes(0)	{};
		public:
			void				Clear()	{handle.Clear();numBytes = 0;access = BufferAccess::Undefined;}
			bool				IsSet()	const {return handle.IsSet();}
			bool				IsEmpty() const {return handle.IsEmpty();}
			size_t				GetByteSize() const {return numBytes;}
			void				swap(BaseBuffer&other)	{handle.swap(other.handle); std::swap(access,other.access); std::swap(numBytes,other.numBytes);}
			String				ToString() const { return "handle="+String((void*)handle)+", access="+access.ToString()+", bytes="+String(numBytes); }
		};

		template <typename T>
			class ObjectBuffer : public BaseBuffer
			{
				typedef BaseBuffer	Super;
				static const size_t NumBlocks = sizeof(T)/16 + (sizeof(T)%16 ? 1 : 0);
				static const size_t	NumBytes = NumBlocks*16;
			public:
				
				void			Create(const T&data, BufferAccess access = BufferAccess::Dynamic, UINT bindFlags = D3D11_BIND_CONSTANT_BUFFER)
				{
					if (sizeof(T) != NumBytes)
					{
						BYTE data_[NumBytes];
						memcpy(data_,&data,sizeof(T));
						Super::Create(data_,NumBytes,access,bindFlags);
					}
					else
						Super::Create(&data,sizeof(T),access,bindFlags);
				}
				void			Update(const T&data)
				{
					if (sizeof(T) != NumBytes)
					{
						BYTE data_[NumBytes];
						memcpy(data_,&data,sizeof(T));
						Super::Update(data_,NumBytes);
					}
					else
						Super::Update(&data,sizeof(T));
				}
			};


		class StreamOutBuffer;

		class ArrayBuffer : public BaseBuffer
		{
			typedef BaseBuffer	Super;
			friend class StreamOutBuffer;

			void			Override(const BaseBuffer&buf, count_t numEntries) {((Super&)*this) = buf; this->numEntries = numEntries;}
		protected:
			count_t				numEntries;
			friend class DirectX11;


			void				Create(const void*data, size_t numBytes, count_t numEntries, BufferAccess, UINT bindFlags);
			void				Update(const void*data, size_t numBytes);
		public:
			/**/				ArrayBuffer():numEntries(0)	{}
			count_t				GetEntryCount() const {return numEntries;}
			void				Clear()	{Super::Clear();numEntries = 0;}
			void				swap(ArrayBuffer&other)	{Super::swap(other); std::swap(numEntries,other.numEntries);}
			String				ToString() const { return Super::ToString() + ", entries=" + String(numEntries); }
		};


		template <typename T>
			class TypedBuffer : public ArrayBuffer
			{
				typedef ArrayBuffer	Super;

			public:
				void			Update(const T*data, count_t numEntries)	{Buffer::Update(data,sizeof(T)*numEntries);}
				void			Update(const ArrayData<T>&data)	{Update(data.pointer(),data.length());}
				void			Update(const BasicBuffer<T>&data)	{Update(data.pointer(),data.length());}
				String			ToString() const { return Super::ToString()+", esize=" + String(sizeof(T)); }
			};

		class VBO : public BaseBuffer
		{
			typedef BaseBuffer	Super;

			friend class StreamOutBuffer;

			void			Override(const BaseBuffer&buf, count_t numVertices) { ((Super&)*this) = buf; this->vertexCount = (UINT)numVertices; }
		protected:
			UINT				vertexCount;
		public:
			UINT				GetVertexCount()	const	{return vertexCount;}
			void				Create(const void*data, size_t numVertices, size_t vertexSize, BufferAccess = BufferAccess::Immutable);
			template <typename T>
				void			CreateFromStructs(const T*data, count_t numStructs, BufferAccess access = BufferAccess::Immutable) {Create((const void*)data,numStructs,sizeof(T),access);}
			template <typename T>
				void			CreateFromStructs(const ArrayData<T>&data, BufferAccess access = BufferAccess::Immutable) {CreateFromStructs(data.pointer(),data.length(),access);}
			template <typename T>
				void			CreateFromStructs(const BasicBuffer<T>&data, BufferAccess access = BufferAccess::Immutable) {CreateFromStructs(data.pointer(),data.length(),access);}
			template <typename T>
				void			CreateFromStructs(const std::initializer_list<T>&data, BufferAccess access = BufferAccess::Immutable)	{CreateFromStructs(&*data.begin(),data.size(),access);}
			template <typename T0>
				void			UpdateStructs(const T0*data, count_t numStructs) {Super::Update((const void*)data,numStructs * sizeof(T0));}
			template <typename T0>
				void			UpdateStructs(const ArrayData<T0>&data) {UpdateStructs(data.pointer(),data.length());}
			template <typename T0>
				void			UpdateStructs(const BasicBuffer<T0>&data) {UpdateStructs(data.pointer(),data.length());}


			void				swap(VBO&other)	{Super::swap(other); std::swap(vertexCount,other.vertexCount);}

			String				ToString() const { return Super::ToString() + ", vcnt=" + String(vertexCount); }
		};

		class StreamVBO : public VBO
		{
			typedef VBO			Super;
		public:
			void				Create(size_t numVertices, size_t vertexSize)	{Super::Create(NULL,numVertices,vertexSize,BufferAccess::Dynamic);}
			void				PartialUpdate(const void*data, size_t numVertices, size_t vertexSize)	{Super::PartialUpdate(data,numVertices * vertexSize); vertexCount = (UINT)numVertices;}
		};


		template <typename T>
			class IBO : public TypedBuffer<T>
			{
				typedef TypedBuffer<T>	Super;
			public:
				void			Create(const T*data, count_t numEntries, BufferAccess access = BufferAccess::Immutable)	{Super::Create(data,sizeof(T)*numEntries,numEntries,access,D3D11_BIND_INDEX_BUFFER);}
				void			Create(const ArrayRef<T>&data, BufferAccess access = BufferAccess::Immutable)	{Create(data.pointer(),data.length(),access);}
				void			Create(const BasicBuffer<T>&data, BufferAccess access = BufferAccess::Immutable)	{Create(data.pointer(),data.length(),access);}
				void			Create(const std::initializer_list<T>&data, BufferAccess access = BufferAccess::Immutable)	{Create(&*data.begin(),data.size(),access);}
			};

		typedef IBO<UINT16>	IBO16;
		typedef IBO<UINT32> IBO32;


		class StageBuffer : public BaseBuffer
		{
		private:
			//Handle<ID3D11ShaderResourceView> view;

		public:
			typedef BaseBuffer	Super;
			void				Create(size_t numBytes)
								{
									Super::Create(NULL,numBytes,BufferAccess::Staging,0);
								}
			void				DownloadData(void*target, size_t numBytes);

			void				swap(StageBuffer&other)	{Super::swap(other); /*view.swap(other.view);*/}
			void				Clear()	{Super::Clear();/*view.Clear();*/}
			Handle<ID3D11Buffer>GetHandle() const {return handle;}
		};

		class StageTexture
		{
		private:
			Handle<ID3D11Resource>	handle;
			Resolution3D			resolution;
			size_t					rowSize,
									sliceSize;
			DXGI_FORMAT				format;
		public:
			void				CreateOrResize(const Resolution3D&resolution, DXGI_FORMAT format, UINT mipmapLayers);
			void				Clear()
			{
				handle.Clear();
				resolution = Resolution3D();
				rowSize = 0;
				sliceSize = 0;
			}
			bool				IsSet() const	{return handle.IsSet();}
			bool				IsEmpty() const {return !IsSet();}

			Handle<ID3D11Resource> GetHandle() const {return handle;}

			void				DownloadData(void*out);
			size_t				GetSliceSize() const {return sliceSize;}
			void				swap(StageTexture&other)
			{
				handle.swap(other.handle);
				std::swap(resolution,other.resolution);
				std::swap(rowSize,other.rowSize);
				std::swap(sliceSize,other.sliceSize);
				std::swap(format,other.format);
			}
		};

		class StreamOutBuffer : public BaseBuffer
		{
		private:
		public:
			typedef BaseBuffer	Super;



			enum usage_t
			{
				NoUsage,
				VertexBufferUsage,
				IndexBufferUsage
			};
			/**/				StreamOutBuffer():usage(NoUsage)	{}
			void				Create(size_t byteSize, usage_t usage=NoUsage);

			VBO					ToVBO(count_t vertexCount)	const;
			template<typename T>
				IBO<T>			ToIBO(count_t indexCount) const
				{
					ASSERT__(usage==IndexBufferUsage);
					DX11::IBO<T> rs;
					rs.Override(*this,indexCount);
					return rs;
				}
			Handle<ID3D11Buffer>GetHandle() const{return handle;}

			void				DownloadData(void*target, size_t numBytes);

			void				swap(StreamOutBuffer&other)	{Super::swap(other); std::swap(usage,other.usage);stageBuffer.swap(other.stageBuffer);/*view.swap(other.view);*/}

			void				Clear()	{Super::Clear();/*view.Clear();*/stageBuffer.Clear();usage=NoUsage;}
			//Handle<ID3D11ShaderResourceView>	GetView() const {return view;}
		private:
			usage_t				usage;
			StageBuffer			stageBuffer;
			//Handle<ID3D11ShaderResourceView> view;

		};

		class Query
		{
		protected:
			Handle<ID3D11Query>	handle;
			friend class DirectX11;

			void				_Create(D3D11_QUERY);
			bool				_Resolve(void*data,size_t dataSize)	const;
		public:
			void				Clear()	{handle.Clear();}
			bool				IsSet()	const {return handle.IsSet();}
			bool				IsEmpty() const {return handle.IsEmpty();}
			void				swap(Query&other)	{handle.swap(other.handle);}
		};

		class PrimitiveCountQuery : public Query
		{
			typedef Query		Super;
		public:
			void				Create();
			bool				Resolve(count_t&)	const;
		};

		class SampleCountQuery : public Query
		{
			typedef Query		Super;
		public:
			void				Create();
			bool				Resolve(count_t&)	const;
		};



		struct ShaderInstructionBlock
		{
			Shader					shader;
			D3D11_PRIMITIVE_TOPOLOGY	topology;
			Array<DX11::Texture>	quickSlots;
			VertexLayout			layout;
			UINT					stride;


			/**/					ShaderInstructionBlock():topology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST),stride(0)	{}
			virtual					~ShaderInstructionBlock(){};

			void					SetPoints()	{topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;}
			void					SetLines()	{topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;}
			void					SetTriangles()	{topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;}
		};

		struct BaseInstructionBlock : public ShaderInstructionBlock
		{
			typedef ShaderInstructionBlock	Super;

			VBO						vbo;
			VertexDescription		binding;
			StreamOutBuffer			streamOutTarget;

			/**/					BaseInstructionBlock()	{}
			
			void					SetBinding(const VertexDescription&binding)
			{
				this->binding = binding;
				UpdateLayout();
			}

			void					Clear()
			{
				(*this) = BaseInstructionBlock();
			}

			void					UpdateLayout()
			{
				Super::layout = shader.BuildLayout(binding);
				Super::stride = binding.GetStride();
			}

			const VertexLayout&		GetLayout()	const {return Super::layout;}

		private:
			friend class DirectX11;

		};

		struct InstructionBlock16 : public BaseInstructionBlock
		{
			IBO16					ibo;
			/**/					InstructionBlock16()	{SetTriangles();}
			explicit				InstructionBlock16(const BaseInstructionBlock&base) : BaseInstructionBlock(base)	{SetTriangles();}
			void					SetBase(const BaseInstructionBlock&base)	{BaseInstructionBlock::operator=(base);}

			void					Clear()
			{
				(*this) = InstructionBlock16();
			}

		};

		struct InstructionBlock32 : public BaseInstructionBlock
		{
			IBO32					ibo;
			/**/					InstructionBlock32()	{SetTriangles();}
			explicit				InstructionBlock32(const BaseInstructionBlock&base) : BaseInstructionBlock(base)	{SetTriangles();}
			void					SetBase(const BaseInstructionBlock&base)	{BaseInstructionBlock::operator=(base);}

			void					Clear()
			{
				(*this) = InstructionBlock32();
			}
		};


		class FBO;


		class BaseTexture
		{
		protected:
			size_t					byteSize;
			Resolution3D			resolution;
			DXGI_FORMAT				format;
			UINT					mipLevels=1;

			friend struct Shader::Binding;
			friend class DirectX11;
			friend class FBO;


			void					Clear(){byteSize = 0; mipLevels = 1; resolution = Resolution3D(); format = DXGI_FORMAT_UNKNOWN;}
		public:
			/**/					BaseTexture():byteSize(0),format(DXGI_FORMAT_UNKNOWN)	{};
			const Resolution3D&		GetResolution() const {return resolution;}
			size_t					GetByteSize()	const	{return byteSize;}
			DXGI_FORMAT				GetFormat() const {return format;}
			UINT					GetMipLevels() const {return mipLevels;}
			void					_SetMipLevels(UINT mipLevels) {this->mipLevels = mipLevels;}

			void					swap(BaseTexture&other)
			{
				std::swap(byteSize,other.byteSize);
				std::swap(resolution,other.resolution);
				std::swap(format,other.format);
				std::swap(mipLevels,other.mipLevels);
			}
		};

		class DepthStencilTexture : public BaseTexture
		{
		private:
			friend class DirectX11;
			friend class FBO;
			friend struct Shader::Binding;

			Handle<ID3D11Texture2D>	handle2D;
			Handle<ID3D11DepthStencilView>	dsView;
			Handle<ID3D11ShaderResourceView>	view;
		public:
			void					Create(const Resolution&);
			void					Clear()
			{
				handle2D.Clear();
				dsView.Clear();
				view.Clear();
				BaseTexture::Clear();
			}
			bool					IsNotEmpty()	const {return handle2D.IsNotEmpty();}
			bool					IsEmpty()	const {return handle2D.IsEmpty();}

			void					swap(DepthStencilTexture&other)
			{
				handle2D.swap(other.handle2D);
				dsView.swap(other.dsView);
				view.swap(other.view);
				BaseTexture::swap(other);
			}
		};

		class Texture : public BaseTexture
		{
		private:
			friend class DirectX11;
			friend class FBO;
			friend struct Shader::Binding;

			Handle<ID3D11Texture1D>	handle1D;
			Handle<ID3D11Texture2D>	handle2D;
			Handle<ID3D11Texture3D>	handle3D;
			Handle<ID3D11ShaderResourceView> view;
			Handle<ID3D11RenderTargetView>	rtView;
			StageTexture			stageTexture;
			void					_CreateView(D3D11_TEXTURE3D_DESC desc);
			void					_CreateView(D3D11_TEXTURE2D_DESC desc);
			void					_CreateView(D3D11_TEXTURE1D_DESC desc);
			void					_CreateCubeView(D3D11_TEXTURE2D_DESC desc);

		public:
			void					Clear()
			{
				handle1D.Clear();
				handle2D.Clear();
				handle3D.Clear();
				view.Clear();
				rtView.Clear();
				stageTexture.Clear();
				BaseTexture::Clear();
			}
			Handle<ID3D11Texture2D>	GetHandle2D() const {return handle2D;}
			bool					IsNotEmpty()	const {return view.IsNotEmpty();}
			bool					IsEmpty()	const {return view.IsEmpty();}


			void					Load(const Image&image, bool generateMipMaps=true);
			void					Load(const UnclampedFloatImage&image, bool generateMipMaps=false);
			void					LoadCube(const Image image[6], bool generateMipMaps=true);

			void					LoadInts(UINT width, UINT height, BYTE channels, const BYTE*);
			void					Load(const Resolution3D&, DXGI_FORMAT, const void*data, bool generateMipMaps=false);
			void					Load(const Resolution&, DXGI_FORMAT, const void*data, bool generateMipMaps=false);
			void					LoadCube(const Resolution&, DXGI_FORMAT, const void*data[6], bool generateMipMaps=false);
			void					Load(UINT width, DXGI_FORMAT, const void*data, bool generateMipMaps=false);
			void					DownloadData(void*target);

			void					swap(Texture&other)
			{
				handle1D.swap(other.handle1D);
				handle2D.swap(other.handle2D);
				handle3D.swap(other.handle3D);
				view.swap(other.view);
				rtView.swap(other.rtView);
				stageTexture.swap(other.stageTexture);
				BaseTexture::swap(other);
			}
		};


		class FBO
		{
		private:
			friend class DirectX11;

			Texture				colorTarget[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
			DepthStencilTexture	depthStencilTarget;
			count_t				numTargets;
			bool				supportsMips;
		public:
			/**/				FBO():numTargets(0),supportsMips(false)	{}

			enum mips_t
			{
				NoMips,
				PrepareMips	//!< Signals that layers should be allocated for mip-map generation
			};

			void				Create(Resolution res, DXGI_FORMAT, bool depthStencil=false, mips_t mips = NoMips);	//!< Creates one target, clears rest
			void				Create(Resolution3D res, DXGI_FORMAT, mips_t mips = NoMips);	//!< Creates one target, clears rest
			void				Create(Resolution res, const DXGI_FORMAT*, count_t numTargets, bool depthStencil=false, mips_t mips = NoMips);
			void				Create(Resolution3D res, const DXGI_FORMAT*, count_t numTargets, mips_t mips = NoMips);

			Resolution			GetResolution() const {return numTargets ? Resolution(colorTarget[0].GetResolution()) : Resolution(depthStencilTarget.GetResolution());}
			const Resolution3D&	GetResolution3D() const {return numTargets ? colorTarget[0].GetResolution() : depthStencilTarget.GetResolution();}
			bool				PrimaryHasAlpha() const {return true;}
			bool				IsEmpty() const {return numTargets == 0 && depthStencilTarget.IsEmpty();}
			bool				IsNotEmpty() const {return numTargets != 0 || depthStencilTarget.IsNotEmpty();}

			void				Clear();
			void				Append(const Texture&);
			void				Append(const DepthStencilTexture&);
			bool				HasDepthStencil() const	{return depthStencilTarget.IsNotEmpty();}
			void				ClearDepthStencil()	{depthStencilTarget.Clear();}
			count_t				CountColorTargets() const {return numTargets;}
			const Texture&		GetColorTarget(UINT index=0) const {return colorTarget[index];}
			Texture&			GetColorTarget(UINT index=0) {return colorTarget[index];}
			const DepthStencilTexture&GetDepthStencilTarget() const {return depthStencilTarget;}
			DepthStencilTexture&GetDepthStencilTarget() {return depthStencilTarget;}
			size_t				GetByteSize() const;
			void				RecreateColorTarget(index_t targetIndex, DXGI_FORMAT format);

			/**
			Generate Mips for all color targets. The local fbo must have been created using PrepareMips
			*/
			void				GenerateMips();
		};




		class Mesh
		{
		private:
			Container::Vector<float>	vfield;

		public:
			VertexBinding	binding;
			bool		hasUV;

			/**/		Mesh()	{Reset(true);}

			void		Reset(bool hasUV)
			{
				vfield.clear();
				this->hasUV = hasUV;
				binding.normal.unset();
				if (hasUV)
				{
					binding.texcoords.SetSize(1);
					binding.texcoords.First().set(3,2);
					binding.floats_per_vertex = 5;
				}
				else
				{
					binding.texcoords.free();
					binding.floats_per_vertex = 3;
				}
			}

			void		MakeVertex(const M::TVec3<>&p, const M::TVec2<>&uv)	{vfield << p.x << p.y << p.z; if (hasUV) vfield << uv.x << uv.y; }
			void		MakeVertex(const M::TVec3<>&p){vfield << p.x << p.y << p.z; if (hasUV) vfield << 0 << 0; }
			void		MakeVertex(float x, float y, float z){vfield << x << y << z; if (hasUV) vfield << 0 << 0; }
			VBO			Upload()
			{
				VBO rs;
				rs.Create(vfield.pointer(),vfield.length() / binding.floats_per_vertex,binding.floats_per_vertex*sizeof(float), BufferAccess::Default); 
				return rs;
			}
		};


	}

	//namespace V2 = GL::V2;

	CONSTRUCT_ENUMERATION4(BlendMode,None,AlphaBlend,FlareBlend,Add);



	class DirectX11: public VisualInterface
	{
	public:
	private:

		struct CameraData
		{
			M::TMatrix4<>	projectionMatrix,
							viewMatrix,
							viewProjectionMatrix,
							worldCamera;
		};

		

		HWND				window;		//!< Handle of the active window
		//HDC					device_context;		//!< Handle of the device context
		IDXGISwapChain		*swapchain;             // the pointer to the swap chain interface
		ID3D11Device		*dev;                     // the pointer to our Direct3D device interface
		ID3D11DeviceContext *devcon;           // the pointer to our Direct3D device context
		DX11::Handle<ID3D11Texture2D>			backbufferTexture;
		DX11::Handle<ID3D11RenderTargetView>	backbufferView;
		DX11::Handle<ID3D11DepthStencilView>	backbufferDepthView;
		DX11::Handle<ID3D11DepthStencilState>	depthStencilState[2][2];
		DX11::Handle<ID3D11RasterizerState>		rasterizerState[2][2];
		DX11::Handle<ID3D11BlendState>			blendState[BlendMode::NumberOfPossibilities];
		DX11::ObjectBuffer<CameraData>			cameraBuffer;
		DX11::ObjectBuffer<M::TMatrix4<> >		objectBuffer;
		DX11::ObjectBuffer<M::TVec4<> >			objectPositionBuffer;	//quicker version
		DX11::ObjectBuffer<float>				timeBuffer;
		Buffer<DX11::Shader::Binding,0,Swap>	baseBindings;
		M::float4			clearColor;

		DXGI_SAMPLE_DESC	fsaa;

		int					errorCode;
		String				errorString;
		D3D_FEATURE_LEVEL	featureLevel;

		UINT				vsync, 
							knownQualityLevels;

		ID3D11PixelShader	*lazyPixelShaderHandle = nullptr;
		int					shaderLockLevel = 0;


		struct TRendererState
		{
			BlendMode		blendMode;
			bool			fill,cull,depthTest,depthWrite;
		};

		TRendererState		current;
		Buffer0<TRendererState>	stack;

		void				SetDepthTest0(bool depthTest, bool depthWrite);
		void				SetRasterizer0(bool fill, bool cull);
		void				SetBlendMode0(BlendMode);
		void				UnbindShaderResources();

	protected:
		//static			bool				bindFrameBufferObject(const GL::FBO&);
			//			void				unbindFrameBufferObject(const Resolution&new_resolution);
		inline			void				onModelviewChange();

		virtual	void				enableLight(const PLight&)	override	{};
		virtual	void				disableLight(const PLight&)	override	{};
		virtual	void				updateLight(const PLight&)	override	{};
		virtual	void				updateLightPosition(const PLight&)	override	{};


	public:
		void			ReportError(HRESULT, const TCodeLocation&);
		//static void					BuildMipMaps(const GL::FBO&, unsigned target, const GLShader::Instance&mipMapShader);
		//static void					BuildMipMaps(const GL::Texture::Reference&, const GLShader::Instance&mipMapShader, GLenum format=GL_RGBA8);

		typedef	float				FloatType;					//!< Float type best used for float fields to achive top render speed
		typedef	UINT32				IndexType;					//!< Index type best used for index fields to achive top render speed

		typedef DX11::Texture		Texture;
		typedef	DX11::VBO			VBO;
		typedef	DX11::IBO32			IBO;
		typedef DX11::FBO			FBO;
		//typedef		GL::Texture						Texture;
		//typedef		GL::Query						Query;			//!< Handle for pixel visibility queries
		//typedef		GL::Shader						Shader;			//!< Handle to a shader instance
		//typedef		MaterialComposition<Texture::Reference>
		//											Material;		//!< Material-Container
		//typedef		GL::ContextLock					ContextLock;

		static DirectX11			*globalInstance;


		virtual bool				pickLightScene(index_t  scenario) override	{return false;};		//!< Picks a light scenario as active scenario (this may be slow at times). \param scenario Index of an existing scenario that should be used from now on (0 = default scenario). \return true on success

		void						NextFrameNoClr();																				//!< Swaps buffers
		void						NextFrame();																					//!< Swaps buffers and clears the back buffer

		//static String				renderState();																					//!< Queries the current render state \return String containing the current OpenGL render state

		//static void						capture(Image&target);																			//!< Copies the current visible image to the specified image
		//static			void						capture(FloatImage&target);																			//!< Copies the current visible image to the specified image
		//static			void						capture(Texture&target, unsigned width, unsigned height);										//!< Captures the current back buffer content to the specified texture
		//static			void						CaptureDepth(Texture&target, unsigned width, unsigned height);									//!< Captures the current back buffer depth content to the specified texture

		/**/						DirectX11();

		const BasicBuffer<DX11::Shader::Binding,Swap>&	GetBaseBindings()	{return baseBindings;}

		const char*					GetVertexShaderProfile() const;
		const char*					GetPixelShaderProfile() const;
		const char*					GetGeometryShaderProfile() const;

		ID3D11Device*				GetDevice() const	{return dev;}
		ID3D11DeviceContext*		GetDeviceContext() const	{return devcon;}
		bool						CreateContext(HWND hWnd, TVisualConfig&config, const Resolution&res);
		void						DestroyContext();																				//!< Destroys the active rendering context
		void						SetViewport(const RECT&region, const Resolution&windowRes);																//!< Sets a new rendering region	\param region Rectangle specifying the new rendering region in the active window
		const char*					GetErrorStr() const {return errorString.c_str();}																					//!< Returns a string-representation of the last occured error \return String-representation of the last occured error
		int							GetErrorCode()	const {return errorCode;}															//!< Returns an index representation of the last occured error \return Error code of the last occured error
		const char*					GetName()	{return "DirectX11";}
		void						SignalWindowResize(const Resolution&);

		void						SetShader(const DX11::Shader&, const Array<DX11::Texture>&);
		void						BindVertices(const DX11::VBO&, UINT stride, const DX11::VertexLayout&);
		void						BindVertices(const DX11::VBO&, UINT stride0, const DX11::VBO&, UINT stride1/*,const DX11::VertexLayout&*/);
		void						BindIndices(const DX11::IBO16&);
		void						BindIndices(const DX11::IBO32&);
		void						RenderVertices(UINT32 offset, UINT32 count, D3D11_PRIMITIVE_TOPOLOGY topology)	{devcon->IASetPrimitiveTopology(topology); devcon->Draw(count,offset);}
		void						RenderIndexed(UINT32 offset, UINT32 count, D3D11_PRIMITIVE_TOPOLOGY topology)	{devcon->IASetPrimitiveTopology(topology); devcon->DrawIndexed(count,offset,0);}
		void						RenderIndexedInstanced(UINT32 indexCount, UINT32 instanceCount, D3D11_PRIMITIVE_TOPOLOGY topology)	{devcon->IASetPrimitiveTopology(topology); devcon->DrawIndexedInstanced(indexCount,instanceCount,0,0,0);}

		void						LinkShader(const DX11::ShaderInstructionBlock&, bool lazy, bool lock);
		void						UnlockShader()	{shaderLockLevel--;ASSERT__(shaderLockLevel >= 0);}
		void						Link(const DX11::BaseInstructionBlock&, bool lazy);
		inline void					DrawVertices(UINT32 offset, UINT32 count)	{devcon->Draw(count,offset);}

		void						Render(const DX11::InstructionBlock16&, bool lazy=false);
		void						Render(const DX11::InstructionBlock16&, UINT32 offset, UINT32 count, bool lazy=false);
		void						Render(const DX11::InstructionBlock32&, bool lazy=false);
		void						Render(const DX11::InstructionBlock32&, UINT32 offset, UINT32 count, bool lazy=false);
		void						RenderVertices(const DX11::BaseInstructionBlock&, const DX11::Query&);
		void						RenderVertices(const DX11::BaseInstructionBlock&, UINT32 offset, UINT32 count, const DX11::Query&);
		void						RenderVertices(const DX11::BaseInstructionBlock&);
		void						RenderVertices(const DX11::BaseInstructionBlock&, UINT32 offset, UINT32 count);
		void						LazyRenderVertices(const DX11::BaseInstructionBlock&, UINT32 offset, UINT32 count);
		void						LazyRenderVertices(const DX11::BaseInstructionBlock&);
		void						AutoRenderVertices(const DX11::BaseInstructionBlock&, const DX11::Query&);
		void						AutoRenderVertices(const DX11::BaseInstructionBlock&);


		void						SetBackbufferClearColor(float r, float g, float b, float a)	{clearColor.Set(r,g,b,a);}
		void						SetBackbufferClearColor(const M::TVec4<>&c)					{clearColor = c;}
		void						SetBackbufferClearColor(const M::TVec3<>&c)					{clearColor.rgb = c;}

		/**
		@brief Pushes the current render state (depth-test,rasterizer,blend-mode) to the stack
		*/
		void						StoreRendererState()	{ASSERT_LESS__(stack.Count(), 16); stack << current;}
		/**
		@brief Pops and restores the last pushed render state from the stack
		*/
		void						RestoreRendererState();
		void						SetDepthTest(bool depthTest, bool depthWrite);
		void						SetRasterizer(bool fill, bool cull);
		void						SetBlendMode(BlendMode);
		inline void					setDepthTest(Engine::VisualEnum::eDepthTest test)	{ SetDepthTest(test != Engine::VisualEnum::NoDepthTest,true);}


		void						SetCameraMatrices(const M::TMatrix4<>&view, const M::TMatrix4<>&projection, const M::TMatrix4<>&viewInvert);
		void						SetObjectMatrix(const M::TMatrix4<>&matrix);
		void						SetObjectPosition(const M::TVec4<>&pos);

		DX11::Handle<ID3D11DepthStencilState>	CreateDepthStencilState(bool depthTest, bool depthWrite);

		bool						TargetFBO(const FBO&fbo);
		void						TargetBackbuffer();
		void						ClearTarget(const FBO&fbo, const M::TVec4<>&color = M::Vector4<>::zero);
		void						ClearTargetColor(const FBO&fbo, const M::TVec4<>&color = M::Vector4<>::zero);
		void						ClearTargetDepth(const FBO&fbo);
		void						ClearDepth();

		void						Copy(const DX11::Handle<ID3D11Texture2D>&source, Image&destination);

		void						Capture(Image&target);


		void						ScanShaderHeader(const PathString&path);

		const Resolution&			GetResolution()	const	{return _currentResolution;}


		D3D_FEATURE_LEVEL			GetFeatureLevel() const {return featureLevel;}
	private:
		bool						_CreateDepthStencilTarget(const Resolution&res);
		void						_CreateBlendState(BlendMode mode,bool enable,D3D11_BLEND srcColor, D3D11_BLEND dstColor, D3D11_BLEND_OP colorOp,  D3D11_BLEND srcAlpha, D3D11_BLEND dstAlpha, D3D11_BLEND_OP alphaOp);

		Resolution					_currentResolution;
		


	};


	template<typename T>
		void	DX11::Shader::Load(Stage<T>&stage, const PathString&filename, const String&functionName, const char*profile, std::function<HRESULT(const void*, SIZE_T, T**)> load, 
				const Binding*b0, count_t numBindings0, const Binding*b1, count_t numBindings1,
				const D3D_SHADER_MACRO*macros)
		{
			_CompileBlob(stage.blob,filename,functionName,profile,macros);

		
			HRESULT hr = load(stage.blob->GetBufferPointer(),stage.blob->GetBufferSize(),stage.shaderHandle.Init());
			if (hr != S_OK)
			{
				_CompileBlob(stage.blob,filename,functionName,profile,macros,true);
				hr = load(stage.blob->GetBufferPointer(),stage.blob->GetBufferSize(),stage.shaderHandle.Init());
				if (hr != S_OK)
				{
					DirectX11::globalInstance->ReportError(hr, CLOCATION);
					Clear();
					throw Except::Renderer::ShaderRejected();
				}
			}

			hr = D3DReflect( stage.blob->GetBufferPointer(), stage.blob->GetBufferSize(), 
				IID_ID3D11ShaderReflection, (void**) stage.reflection.Init());
			if (hr != S_OK)
			{
				Clear();
				DirectX11::globalInstance->ReportError(hr, CLOCATION);
				throw Except::Renderer::ShaderRejected();
			}

			stage.bindPoints.clear();
			stage.AppendBindings(b0,numBindings0);
			stage.AppendBindings(b1,numBindings1);
			stage.bindPoints.compact();
			stage.texBindPoints.compact();
			stage.samplerBindPoints.compact();
		}


}



#endif

#endif
