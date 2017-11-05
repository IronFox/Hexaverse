#pragma once

#ifdef HAS_WINDOW

#include "../display.h"
#include <general/enumeration.h>
#include <global_string.h>
#include "../../clientEvents.h"


class ShadedInstruction
{
public:
	CONSTRUCT_ENUMERATION4(Quality,
				Filtered6x6,
				Filtered3x3,
				Unfiltered,
				Off);
	typedef Engine::DX11::BaseInstructionBlock	Base;
	typedef DeltaWorks::PathString	PathString;
	typedef DeltaWorks::String		String;

	Base					quality[Quality::N];
	static Quality			chosenQuality;
	Engine::DX11::VertexDescription	binding;

	const Base&				Current() const {return quality[chosenQuality];}
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
	void					SetTopology(D3D_PRIMITIVE_TOPOLOGY);
	void					SetPoints()	{SetTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);}
	void					SetLines()	{SetTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);}
	void					SetTriangles()	{SetTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);}
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
		((Engine::DX11::ShaderInstructionBlock&)*this) = shader.Current();
	}

	void					operator=(const ShadedInstruction&inst)
	{
		((Engine::DX11::BaseInstructionBlock&)*this) = inst.Current();
		shader = inst;
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
		((Engine::DX11::ShaderInstructionBlock&)*this) = shader.Current();
	}

	void					operator=(const ShadedInstruction&inst)
	{
		((Engine::DX11::BaseInstructionBlock&)*this) = inst.Current();
		shader = inst;
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
		((Engine::DX11::ShaderInstructionBlock&)*this) = shader.Current();
	}

	void					operator=(const ShadedInstruction&inst)
	{
		((Engine::DX11::BaseInstructionBlock&)*this) = inst.Current();
		shader = inst;
	}
	void					Clear()
	{
		Super::Clear();
		shader.Clear();
	}
};


//






#endif
