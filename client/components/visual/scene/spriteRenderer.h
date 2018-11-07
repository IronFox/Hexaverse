#pragma once

#include <global_string.h>
#include <math/vclasses.h>
#include <container/buffer.h>
#include "../display.h"

namespace Objects
{
	namespace D = DeltaWorks;

	/**
	General purpose sprite renderer, batch-renderable at a variable number of locations
	*/
	class SpriteRenderer
	{
	public:
		struct Attributes
		{
			D::float3	color = D::float3(1);
			/**
			Brightness pulse.
			X and Y denote the two brightness extremes, the Z value the sine frequency when pulsing between the two
			*/
			D::float3	pulse = D::float3(1);
		};

		struct Instance
		{
			D::float3	location;
			D::float2	size;
			Attributes	attributes;
		};

		class Baked
		{
			friend class SpriteRenderer;
		public:
			void	Render() const;
		private:
			Engine::DX11::BaseInstructionBlock	clipped,blended;
		};

		static const count_t MaxInstancesPerRow = 64;

		/**
		Creates or restores a showable sprite from the given standard icon image.
		Standard icon images are expected to either be gray-scale or provide an alpha channel.
		In the end, a single gray-scale image is used as basis to construct an outlined gray-scale+alpha image
		@param source Source image. May be any supported format (jpeg, png, bmp, tga)
		@param cacheTo Caching location. Always PNG
		@param forceRenew Ignored cached version even if up to date
		@param edgeScale Scale to apply to edge effects. The larger, the greater the visible edges
		*/
		void		CreateOrLoadFromIcon(const D::PathString&source, const D::PathString&cacheTo, bool forceRenew, float edgeScale=1.f);
		/**
		Loads the given RGBA image source (typically a png)
		*/
		void		Load(const D::PathString&source);

		/**
		Renders the local icon at the given center.
		The rendering may be delayed.
		Call Flush() to assure no cached sprites remain
		*/
		void		Render(const Instance&);
		/**
		Renders any remaining cached instances.
		Must be called before changing camera
		*/
		void		Flush();

		Baked		Bake(const D::ArrayRef<Instance>&) const;

		/**
		Must be called at least once for the system to work
		*/
		static void	StaticCreate();
	private:
		D::Buffer0<Instance>	instances;
		Engine::DX11::Texture	texture;
	};


}

