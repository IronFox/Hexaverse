#pragma once

#include <global_string.h>

namespace StartConfig
{
	namespace D = DeltaWorks;

	void			Load(const D::PathString&userConfigPath);
	const D::String&GetString(const D::String&path, const D::String&fallback);
	bool			GetBool(const D::String&path, bool fallback = false);
	int				GetInt(const D::String&path, int fallback = 0);
	float			GetFloat(const D::String&path, float fallback = 0);
	void			Finish();


}
