#pragma once
#ifdef HAS_WINDOW

#include "visualObjectCommands.h"
#include <objects/components.h>
#include <al/openal.h>
#include <objects/clientAttachment.h>

namespace Objects
{

	/**
	Used to cache and render the paraboloid above a turret base
	*/
	class ParaboloidBuffer : public ClientAttachment
	{
		TVec3<> lastCenter;
		float lastRange = 0.f;
		float lastExp = 0.f;
		TMatrix4<> matrix;
		DX11::InstructionBlock32	attackInstruction,extendedInstruction;

		static void		MakeHull(DX11::InstructionBlock32&target, const TMatrix4<>&matrix, float flatRange, float explosionRadius, const TVec3<>&center);

	public:
		void			Render(const Grid::LCellIdI&cell, float zOffset, float flatRange, float explosionRadius, const float4&color, bool extended);
	};

	class VisualComponent : public ClientAttachment
	{
	public:
		Array<DX11::InstructionBlock32>				partInstruction;
	};

	class TaskAttachment : public ClientAttachment
	{
	public:
		DX11::InstructionBlock32		instruction;

		struct Shape
		{
			DX11::InstructionBlock32	instruction;
			TMatrix4<>					system;
			bool						depthTest,		//Indicates that the shape should depth-test against the terrain and other complex shapes
										preRenderDepth = true,	//Effective only if @a depthTest is true. Indicates that this is a complex shape and should be depth-only pre-rendered
										flareDisplay = false,	//Indicates that the shape is rendered using an add-blend function, and using a white color when not selected. Otherwise black is used, using regular alpha blending
										selectedOnly = false;	//This shape should only be rendered for selected units
		};

		Buffer0<Shape>					localizableShapes;

		void							SetLocalizableShape(ConstName structureIdentifier, const Grid::LCellIdI&location, const OwnerParameter&);


		static std::shared_ptr<TaskAttachment>	Of(Objects::MobileEntity::Task&);
	};

}

#endif
