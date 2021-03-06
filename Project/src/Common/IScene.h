#pragma once
#include "Vulkan/VulkanCommon.h"

#include "Core/Camera.h"
#include "Core/LightSetup.h"

class IMesh;
class Material;

class IScene
{
public:
	DECL_INTERFACE(IScene);

	virtual bool loadFromFile(const std::string& dir, const std::string& fileName) = 0;

	virtual bool init() = 0;
	virtual bool finalize() = 0;

	virtual void updateMeshesAndGraphicsObjects() = 0;
	virtual void updateMaterials() = 0;

	virtual void updateCamera(const Camera& camera) = 0;

	virtual uint32_t submitGraphicsObject(const IMesh* pMesh, const Material* pMaterial, const glm::mat4& transform = glm::mat4(1.0f), uint8_t customMask = 0x80) = 0;
	virtual void updateGraphicsObjectTransform(uint32_t index, const glm::mat4& transform) = 0;

	virtual LightSetup& getLightSetup() = 0;

	//Debug
	virtual void renderUI() = 0;
	virtual void updateDebugParameters() = 0;
};