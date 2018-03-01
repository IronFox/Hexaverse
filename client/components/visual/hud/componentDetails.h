#pragma once

#include "guiComponents.h"
#include <engine/aspect.h>

namespace HUD{
namespace Component{
namespace Detail
{
	static const float textZoom = 1.5f;


	extern Engine::OrthographicAspect<>	aspect;
	extern Resolution						lastClientSpace;

	struct Viewport
	{
		M::Rect<>	view;
		float2	offset,
				relativeOffset;


	};
	extern Buffer0<Viewport>	viewportStack;
	extern float2	totalOffset;

	void	LoadView(const M::Rect<>&view, const float2&offset);
	void	PushLoadView(const M::Rect<>&view, const float2&offset=float2());
	void	PopLoadView();
	void	OnInput();
	void	OnCharInput(char);
	void	ActivateInput(const char*allowed, const std::function<void(Key::Name)>&onSpecialKey, Engine::Keyboard::CharacterBuffer&sourceBufferContent);
	void	DeactivateInput(Engine::Keyboard::CharacterBuffer*destBufferContent);
	bool	InputIsActive();


	extern Buffer<Component*>	allComponents;
}
}}

