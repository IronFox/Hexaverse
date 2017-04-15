#pragma once

#ifdef HAS_WINDOW

#include <engine/display.h>
#include <engine/textout.h>
#include "../../ExtRenderer/DirectX11.h"
#include "../../DX11/texture_font.h"

extern Engine::Display<Engine::DirectX11>	display;
extern Engine::Textout<Engine::DX11::TextureFont>	textout;

#endif
