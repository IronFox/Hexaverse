#ifndef progressBarH
#define progressBarH



#ifdef HAS_WINDOW

#include <math/vclasses.h>

/**
Handles progress bars rendered above entities in the scene
*/
namespace ProgressBar
{
	/**
	@brief Resets status bar rendering, and locates the 'cursor' at the specified location
	*/
	void	SetLocation(const float3&worldCenter, float scale=1.f);

	/**
	@brief Appends and renders a new bar. Multiple bars may be rendered for one location this way
	*/
	void	RenderNewBar(float progress, float max, const float3&color);

	/**
	@brief Signals that all pending operations should finish now
	*/
	void	End();

	void	Create();
	void	Destroy();
}


#endif

#endif
