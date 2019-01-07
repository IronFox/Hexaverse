#pragma once

#ifdef HAS_WINDOW

#include "../display.h"
#include <general/enumeration.h>
#include <global_string.h>
#include "../../clientEvents.h"

namespace DX11 = Engine::DX11;

class ShadedInstruction
{
public:

	CONSTRUCT_ENUMERATION4(Quality,
				Filtered6x6,
				Filtered3x3,
				Unfiltered,
				Off);

	struct TQuality
	{
		DX11::Shader		shader;
		DX11::VertexLayout	vertexLayout;

		void				Clear()
		{
			shader.Clear();
			vertexLayout.Clear();
		}

		void				ApplyTo(DX11::ShaderInstructionBlock&block)	const
		{
			block.shader = shader;
			block.layout = vertexLayout;
		}

		void				ApplyTo(DX11::InstructionBlock32I&block) const
		{
			block.shader = shader;
			block.generatedData.combinedLayout = vertexLayout;
		}
	};

	//typedef Engine::DX11::BaseInstructionBlock	Base;
	typedef DeltaWorks::PathString	PathString;
	typedef DeltaWorks::String		String;

	TQuality				quality[Quality::N];
	static Quality			chosenQuality;
	Engine::DX11::VertexDescription	binding, instanceBinding;
	UINT					stride,instanceStride;

	const TQuality&			Current() const {return quality[chosenQuality];}
	void					ApplyCurrentTo(DX11::ShaderInstructionBlock&block)	const
	{
		block.stride = stride;
		Current().ApplyTo(block);
	}
	void					ApplyCurrentTo(DX11::InstructionBlock32I&block)	const
	{
		block.generatedData.protoStride = stride;
		block.generatedData.instanceStride = instanceStride;
		Current().ApplyTo(block);
	}
	void					LoadQualitiesFromFile(const PathString&filename, const String&vsMain, const String&psMain, const Engine::DX11::Shader::Binding*b=NULL, count_t numBindings=0);
	void					LoadQualitiesFromFile(const PathString&filename, const String&vsMain, const String&gsMain, const String&psMain, const Engine::DX11::Shader::Binding*b=NULL, count_t numBindings=0);
	void					Clear()
	{
		for (index_t i = 0; i < Quality::N; i++)
			quality[i].Clear();
	}
	bool					IsEmpty() const {return quality[0].shader.IsEmpty();}
	bool					IsSet() const {return quality[0].shader.IsSet();}
	void					UpdateLayout();
};

class ShadedVertexInstructionBlock : public Engine::DX11::BaseInstructionBlock
{
	typedef Engine::DX11::BaseInstructionBlock	Super;
	index_t					callback=InvalidIndex;
public:
	ShadedInstruction		shader;//shadow base

	/**/					ShadedVertexInstructionBlock()
	{
		callback = GlobalEvents::shadowQualityChanged.Register([this](){SignalShadowQualityChange();});
	}
	/**/					ShadedVertexInstructionBlock(const ShadedVertexInstructionBlock&other):Super(other)
	{
		shader = other.shader;
		callback = GlobalEvents::shadowQualityChanged.Register([this](){SignalShadowQualityChange();});
	}
	virtual					~ShadedVertexInstructionBlock()
	{
		if (!application_shutting_down)
			GlobalEvents::shadowQualityChanged.Unregister(callback);
		callback = InvalidIndex;
	}

	void					operator=(const ShadedVertexInstructionBlock&other)
	{
		((Super&)*this) = other;
		shader = other.shader;
	}

	void					SignalShadowQualityChange()
	{
		shader.ApplyCurrentTo(*this);
	}

	void					operator=(const ShadedInstruction&inst)
	{
		shader = inst;
		SignalShadowQualityChange();
	}
	void					Clear()
	{
		Super::Clear();
		shader.Clear();
	}
};

class ShadedInstructionBlock16 : public Engine::DX11::InstructionBlock16
{
	typedef Engine::DX11::InstructionBlock16	Super;
	index_t					callback=InvalidIndex;
public:
	typedef ShadedInstructionBlock16	Self;
	ShadedInstruction		shader;//shadow base


	/**/					ShadedInstructionBlock16()
	{
		callback = GlobalEvents::shadowQualityChanged.Register([this](){SignalShadowQualityChange();});
	}
	/**/					ShadedInstructionBlock16(const Self&other):Super(other)
	{
		shader = other.shader;
		callback = GlobalEvents::shadowQualityChanged.Register([this](){SignalShadowQualityChange();});
	}
	virtual					~ShadedInstructionBlock16()
	{
		if (!application_shutting_down)
			GlobalEvents::shadowQualityChanged.Unregister(callback);
		callback = InvalidIndex;
	}

	void					operator=(const Self&other)
	{
		((Super&)*this) = other;
		shader = other.shader;
	}

	void					SignalShadowQualityChange()
	{
		shader.ApplyCurrentTo(*this);
	}

	void					operator=(const ShadedInstruction&inst)
	{
		shader = inst;
		SignalShadowQualityChange();
	}
	void					Clear()
	{
		Super::Clear();
		shader.Clear();
	}
};

class ShadedInstructionBlock32 : public Engine::DX11::InstructionBlock32
{
	typedef Engine::DX11::InstructionBlock32	Super;
	index_t					callback=InvalidIndex;
public:
	typedef ShadedInstructionBlock32	Self;
	ShadedInstruction		shader;



	/**/					ShadedInstructionBlock32()
	{
		callback = GlobalEvents::shadowQualityChanged.Register([this](){SignalShadowQualityChange();});
	}
	/**/					ShadedInstructionBlock32(const Self&other):Super(other)
	{
		shader = other.shader;
		callback = GlobalEvents::shadowQualityChanged.Register([this](){SignalShadowQualityChange();});
	}
	virtual					~ShadedInstructionBlock32()
	{
		if (!application_shutting_down)
			GlobalEvents::shadowQualityChanged.Unregister(callback);
		callback = InvalidIndex;
	}

	void					operator=(const Self&other)
	{
		((Super&)*this) = other;
		shader = other.shader;
	}

	void					SignalShadowQualityChange()
	{
		shader.ApplyCurrentTo(*this);
	}

	void					operator=(const ShadedInstruction&inst)
	{
		shader = inst;
		SignalShadowQualityChange();
	}
	void					Clear()
	{
		Super::Clear();
		shader.Clear();
	}
};


class ShadedInstructionBlock32I : public DX11::InstructionBlock32I
{
	typedef Engine::DX11::InstructionBlock32I	Super;
	index_t					callback=InvalidIndex;
public:
	typedef ShadedInstructionBlock32I	Self;
	ShadedInstruction		shader;



	/**/					ShadedInstructionBlock32I()
	{
		callback = GlobalEvents::shadowQualityChanged.Register([this](){SignalShadowQualityChange();});
	}
	/**/					ShadedInstructionBlock32I(const Self&other):Super(other)
	{
		shader = other.shader;
		callback = GlobalEvents::shadowQualityChanged.Register([this](){SignalShadowQualityChange();});
	}
	virtual					~ShadedInstructionBlock32I()
	{
		if (!application_shutting_down)
			GlobalEvents::shadowQualityChanged.Unregister(callback);
		callback = InvalidIndex;
	}

	void					operator=(const Self&other)
	{
		((Super&)*this) = other;
		shader = other.shader;
	}

	void					SignalShadowQualityChange()
	{
		shader.ApplyCurrentTo(*this);
	}

	void					operator=(const ShadedInstruction&inst)
	{
		shader = inst;
		SignalShadowQualityChange();
	}
	void					Clear()
	{
		Super::Clear();
		shader.Clear();
	}
};







#endif
