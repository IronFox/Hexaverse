#pragma once
#ifdef HAS_WINDOW

#include "objectCommands.h"
#include <objects/components.h>
#include <al/openal.h>
#include <objects/clientAttachment.h>
#include <objects/mobileEntity.h>
#include "../../ExtRenderer/DirectX11.h"
#include <objects/structure/tower.h>

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
		static DX11::IBO32			attackIBO;
		static DX11::BaseInstructionBlock	paraboloidShader;

		static void			MakeHull(DX11::InstructionBlock32&target, const TMatrix4<>&matrix, float flatRange, float explosionRadius, const TVec3<>&center);

	public:
		void			Render(const Grid::LCellIdI&cell, float zOffset, float flatRange, float explosionRadius, const float4&color, bool extended);
		static const std::shared_ptr<ParaboloidBuffer>&	Of(Objects::TurretBase&);


		/**
		Releases global GPU resources.
		Called by objectRenderer.cpp:Renderer::Destroy()
		*/
		static void		StaticDestroy();
		static void		StaticCreate();

	};

	class VisualComponent : public ClientAttachment
	{
		void				FinishCreation(Objects::Component::Base&base, bool scaleToVehicleSize=true);
	public:

		Array<DX11::InstructionBlock32>				partInstruction;
		static const std::shared_ptr<VisualComponent>&	Allocate(Objects::Component::Base&);
		static const std::shared_ptr<VisualComponent>&	Of(const Objects::Component::Base&);


		static void					FinalizeAll();
	};


	class StructureHull : public ClientAttachment
	{
	public:
		/**
		Temporary structure-specific rendering instruction. Updated by Recompile(). Can be used to render the local structure.
		*/
		DX11::InstructionBlock32	constShape;


		static const std::shared_ptr<StructureHull>&	Of(const Objects::Structure&);

		static void					LoadFor(const String&geometryName, Objects::Geometry&staticGeometry, PClientAttachment&staticShape);
		static void					LoadFor(const Objects::Structure&prototype, Objects::Geometry&staticGeometry, PClientAttachment&staticShape);

		template <typename T>
			static void				LoadFor()
			{
				LoadFor(*T::globalPrototype, T::geometry, T::staticShape);
			}

		/**
		Loads all static structure geometry and uploads them to the GPU
		*/
		static void					LoadAll();
	};

	class ConnectorHull : public ClientAttachment
	{
	public:
		/**
		Temporary structure-specific rendering instruction. Updated by Recompile(). Can be used to render the local structure.
		*/
		DX11::InstructionBlock32	compiledShape;
		/**
		Container of the geometry of the local structure. Updated by RecompileConnectors()
		*/
		Geometry					localMesh;

		void						RecompileConnectors(Objects::Structure&structure);

		static const std::shared_ptr<ConnectorHull>&	Of(const Objects::Structure&);
		static const std::shared_ptr<ConnectorHull>&	Allocate(Objects::Structure&);	//must be permissive allocation. Return previously allocated if existing
	};


	class TowerAttackVisualization : public ClientAttachment
	{
	public:
		TMatrix4<>					blastSystem;
		DX11::InstructionBlock32	blastVisualization,
									trajectoryVisualization;

		void						Recompile(const Tower&t, const Grid::LCellIdI&target);

		static const std::shared_ptr<TowerAttackVisualization>&	Of(Objects::Tower::AttackTask&);
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
		bool							wasJob=false;

		void							SetLocalizableShape(ConstName structureIdentifier, const Grid::LCellIdI&location, const OwnerParameter&);
		static void						Recompile(const MobileEntity&e, MobileEntity::Task&t,const Grid::LCellIdI&pointOfOrigin);
		void							RecompilePath(const MobileEntity::Task&t, int stepsPerRound, float structureSize=vehicleScale);

		static const std::shared_ptr<TaskAttachment>&	Of(const Objects::MobileEntity::Task&);
		static const std::shared_ptr<TaskAttachment>&	Allocate(Objects::MobileEntity::Task&);
	};


	void	CreateComponents();
	void	DestroyComponents();

}

#endif
