#include "IGraphicsContext.h"
#include "Vulkan/GraphicsContextVK.h"

IGraphicsContext* IGraphicsContext::create(IWindow* pWindow, API api, bool useMultipleQueues, bool useMultipleFamilies, bool useComputeQueue)
{
	switch (api)
	{
		case API::VULKAN:
		{
			GraphicsContextVK* pContext = DBG_NEW GraphicsContextVK(pWindow, useMultipleQueues, useMultipleFamilies, useComputeQueue);
			pContext->init();
			return pContext;
		}
	}

	return nullptr;
}
