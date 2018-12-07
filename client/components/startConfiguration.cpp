#include "startConfiguration.h"
#include <io/config.h>
#include <logEvent.h>
#include "../version.h"
#ifdef HAS_WINDOW
	#include "visual/hud/menu.h"
#endif

namespace StartConfig
{
	using namespace DeltaWorks;

	Configuration	sourceConfig,baseConfig,destConfig;
	bool			changed = false;
	PathString		configPath;
	bool			postFinish = false;

	void			Load(const PathString&userConfigPath)
	{
		configPath = userConfigPath;
		baseConfig.LoadFromFile("game.ini");
		if (!sourceConfig.LoadFromFile(userConfigPath))
		{
			LogEvent(EventClass::Preset::configuration,nullptr,"Unable to load user configuration from '"+String(userConfigPath)+"': '"+sourceConfig.Report()+"'. Attempting to load original config");
			sourceConfig = baseConfig;
		}
		destConfig = sourceConfig;

		#ifdef HAS_WINDOW
			HUD::Menu::Configuration::Assign(&baseConfig, &destConfig, userConfigPath);
		#endif
	}


	void Fix(const String&path, const String&fallback)
	{


		StringRef comment,def = fallback.ref();
		const Configuration::Attribute*attrib = baseConfig.GetAttrib(path);
		if (attrib)
		{
			comment = attrib->comment.ref();
			def = attrib->value.ref();
			LogEvent(EventClass::Preset::fixedConfigurationError,"Fixing config error: '"+path+"' not found. Defaulting to '"+(def)+"'");
		}
		else
			LogEvent(EventClass::Preset::fixedConfigurationError,"Fixing config error: '"+path+"' not found in actual or base configuration. Defaulting to '"+(def)+"'");
		destConfig.Define(path,def).comment = comment;
		changed = true;
		if (postFinish)
			Finish();
	}

	const String&	GetString(const String&path, const String&fallback)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		if (attrib)
			return attrib->value;
		Fix(path,fallback);
		static String staticCopy;
		staticCopy = fallback;	//fallback might be a temporary string, so this at least keeps it alive
		return staticCopy;
	}

	
	bool	GetBool(const String&path, bool fallback /*= false*/)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		bool rs;
		if (attrib && convert(attrib->value.c_str(),rs))
			return rs;
		Fix(path,fallback);
		return fallback;
	}
	
	int	GetInt(const String&path, int fallback /*= 0*/)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		int rs;
		if (attrib && convert(attrib->value.c_str(),rs))
			return rs;
		Fix(path,fallback);
		return fallback;
	}
	
	float	GetFloat(const String&path, float fallback /*= 0*/)
	{
		Configuration::Attribute*attrib = sourceConfig.GetAttrib(path);
		float rs;
		if (attrib && convert(attrib->value.c_str(),rs))
			return rs;
		Fix(path,fallback);
		return fallback;
	}

	void	Finish()
	{
		postFinish = true;
		if (changed)
		{
			LogEvent(EventClass::Preset::configuration,"Rewriting configuration ("+String(configPath)+")");
			destConfig.SaveToFile(configPath,PRODUCT_NAME " Client");
			changed = false;
		}
	}


}