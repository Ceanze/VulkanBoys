#include "IGraphicsContext.h"
#include "Vulkan/GraphicsContextVK.h"

IGraphicsContext* IGraphicsContext::create(IWindow* pWindow, API api, bool useMultipleQueues)
{
	switch (api)
	{
		case API::VULKAN:
		{
			GraphicsContextVK* pContext = DBG_NEW GraphicsContextVK(pWindow, useMultipleQueues);
			pContext->init();
			return pContext;
		}
	}

	return nullptr;
}
