#include "ParticleEmitterHandlerVK.h"

#include "imgui/imgui.h"

#include "Common/Debug.h"
#include "Common/IGraphicsContext.h"
#include "Common/IShader.h"
#include "Vulkan/BufferVK.h"
#include "Vulkan/CommandPoolVK.h"
#include "Vulkan/CommandBufferVK.h"
#include "Vulkan/DescriptorPoolVK.h"
#include "Vulkan/DescriptorSetVK.h"
#include "Vulkan/DescriptorSetLayoutVK.h"
#include "Vulkan/GBufferVK.h"
#include "Vulkan/GraphicsContextVK.h"
#include "Vulkan/PipelineLayoutVK.h"
#include "Vulkan/PipelineVK.h"
#include "Vulkan/RenderingHandlerVK.h"
#include "Vulkan/SamplerVK.h"
#include "Vulkan/ShaderVK.h"

// Compute shader bindings
#define POSITIONS_BINDING   	0
#define VELOCITIES_BINDING  	1
#define AGES_BINDING        	2
#define EMITTER_BINDING     	3

ParticleEmitterHandlerVK::ParticleEmitterHandlerVK(bool renderingEnabled, uint32_t frameCount)
	:ParticleEmitterHandler(renderingEnabled),
	m_pDescriptorPool(nullptr),
	m_pDescriptorSetLayoutPerEmitter(nullptr),
	m_pPipelineLayout(nullptr),
	m_pPipeline(nullptr),
	m_pCommandPoolGraphics(nullptr),
	m_pGBufferSampler(nullptr),
	m_WorkGroupSize(0),
	m_NextQueueIndexCompute(0),
	m_NextQueueIndexGraphics(0),
	m_CurrentFrame(0),
	m_FrameCount(frameCount)
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_ppCommandPools[i] = nullptr;
    }
}

ParticleEmitterHandlerVK::~ParticleEmitterHandlerVK()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        SAFEDELETE(m_ppCommandPools[i]);
    }

	SAFEDELETE(m_pGBufferSampler);
	SAFEDELETE(m_pDescriptorPool);
	SAFEDELETE(m_pDescriptorSetLayoutPerEmitter);
	SAFEDELETE(m_pPipelineLayout);
	SAFEDELETE(m_pPipeline);
	SAFEDELETE(m_pCommandPoolGraphics);
}

void ParticleEmitterHandlerVK::update(float dt)
{
	if (m_GPUComputed) {
        updateGPU(dt);
    } else {
        for (ParticleEmitter* particleEmitter : m_ParticleEmitters) {
            particleEmitter->update(dt);
        }
    }
}

void ParticleEmitterHandlerVK::updateRenderingBuffers(RenderingHandler* pRenderingHandler)
{
    RenderingHandlerVK* pRenderingHandlerVK = reinterpret_cast<RenderingHandlerVK*>(pRenderingHandler);
    CommandBufferVK* pCommandBuffer = pRenderingHandlerVK->getCurrentGraphicsCommandBuffer();

    for (ParticleEmitter* pEmitter : m_ParticleEmitters) {
		if (!m_GPUComputed) {
			// Update emitter buffer. If GPU computing is enabled, this will already have been updated
			if (pEmitter->m_EmitterUpdated) {
				EmitterBuffer emitterBuffer = {};
				pEmitter->createEmitterBuffer(emitterBuffer);

				BufferVK* pEmitterBuffer = reinterpret_cast<BufferVK*>(pEmitter->getEmitterBuffer());
				pCommandBuffer->updateBuffer(pEmitterBuffer, 0, &emitterBuffer, sizeof(EmitterBuffer));

				pEmitter->m_EmitterUpdated = false;
			}

			// Update particle positions buffer
			const std::vector<glm::vec4>& particlePositions = pEmitter->getParticleStorage().positions;
			BufferVK* pPositionsBuffer = reinterpret_cast<BufferVK*>(pEmitter->getPositionsBuffer());

			pCommandBuffer->updateBuffer(pPositionsBuffer, 0, particlePositions.data(), sizeof(glm::vec4) * particlePositions.size());
		}
    }
}

void ParticleEmitterHandlerVK::drawProfilerUI()
{}

bool ParticleEmitterHandlerVK::initializeGPUCompute()
{
    if (!createCommandPoolAndBuffers()) {
		return false;
	}

	if (!createSamplers()) {
		return false;
	}

	if (!createPipelineLayout()) {
		return false;
	}

	if (!createPipeline()) {
		return false;
	}

	return true;
}

void ParticleEmitterHandlerVK::toggleComputationDevice()
{
	GraphicsContextVK* pGraphicsContext = reinterpret_cast<GraphicsContextVK*>(m_pGraphicsContext);
	DeviceVK* pDevice = pGraphicsContext->getDevice();

	// Disable GPU-side computing
	if (m_GPUComputed) {
		CommandBufferVK* pTempCommandBuffer = m_pCommandPoolGraphics->allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		pTempCommandBuffer->reset(true);
		pTempCommandBuffer->begin(nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		for (ParticleEmitter* pEmitter : m_ParticleEmitters) {
			BufferVK* pPositionsBuffer = reinterpret_cast<BufferVK*>(pEmitter->getPositionsBuffer());
			acquireForGraphics(pPositionsBuffer, pTempCommandBuffer);
		}

		pTempCommandBuffer->end();
		pDevice->executeGraphics(pTempCommandBuffer, nullptr, nullptr, 0, nullptr, 0);

		// Wait for command buffer to finish executing before deleting it
		pTempCommandBuffer->reset(true);
		m_pCommandPoolGraphics->freeCommandBuffer(&pTempCommandBuffer);
	} else {
		// Enable GPU-side computing
		CommandBufferVK* pTempCmdBufferCompute = m_ppCommandPools[0]->allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		pTempCmdBufferCompute->reset(true);
		pTempCmdBufferCompute->begin(nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		for (ParticleEmitter* pEmitter : m_ParticleEmitters) {
			// Since ImGui is what triggered this, and ImGui is handled AFTER particles are updated, the particle buffers will be used
			// for rendering next, and the renderer will try to acquire ownership of the buffers for the rendering queue, the compute queue needs to release them
			BufferVK* pPositionsBuffer = reinterpret_cast<BufferVK*>(pEmitter->getPositionsBuffer());
			releaseFromCompute(pPositionsBuffer, pTempCmdBufferCompute);

			// Copy age and velocity data to the GPU buffers
			const ParticleStorage& particleStorage = pEmitter->getParticleStorage();

			BufferVK* pAgesBuffer = reinterpret_cast<BufferVK*>(pEmitter->getAgesBuffer());
			pTempCmdBufferCompute->updateBuffer(pAgesBuffer, 0, (const void*)particleStorage.ages.data(), particleStorage.ages.size() * sizeof(float));

			BufferVK* pVelocitiesBuffer = reinterpret_cast<BufferVK*>(pEmitter->getVelocitiesBuffer());
			pTempCmdBufferCompute->updateBuffer(pVelocitiesBuffer, 0, (const void*)particleStorage.velocities.data(), particleStorage.ages.size() * sizeof(glm::vec4));

			// Update emitter buffer
			EmitterBuffer emitterBuffer = {};
			pEmitter->createEmitterBuffer(emitterBuffer);

			BufferVK* pEmitterBuffer = reinterpret_cast<BufferVK*>(pEmitter->getEmitterBuffer());
			pTempCmdBufferCompute->updateBuffer(pEmitterBuffer, 0, &emitterBuffer, sizeof(EmitterBuffer));
		}

		pTempCmdBufferCompute->end();

		pDevice->executeCompute(pTempCmdBufferCompute, nullptr, nullptr, 0, nullptr, 0);

		// Wait for command buffer to finish executing before deleting it
		pTempCmdBufferCompute->reset(true);
		m_ppCommandPools[0]->freeCommandBuffer(&pTempCmdBufferCompute);
	}

	m_GPUComputed = !m_GPUComputed;
}

void ParticleEmitterHandlerVK::releaseFromGraphics(BufferVK* pBuffer, CommandBufferVK* pCommandBuffer)
{
	if (!m_RenderingEnabled) {
		return;
	}

	GraphicsContextVK* pGraphicsContext = reinterpret_cast<GraphicsContextVK*>(m_pGraphicsContext);
	DeviceVK* pDevice = pGraphicsContext->getDevice();
	const QueueFamilyIndices& queueFamilyIndices = pDevice->getQueueFamilyIndices();

	pCommandBuffer->releaseBufferOwnership(
		pBuffer,
		VK_ACCESS_SHADER_READ_BIT,
		queueFamilyIndices.GraphicsQueues.value().FamilyIndex,
		queueFamilyIndices.ComputeQueues.value().FamilyIndex,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
	);
}

void ParticleEmitterHandlerVK::releaseFromCompute(BufferVK* pBuffer, CommandBufferVK* pCommandBuffer)
{
	if (!m_RenderingEnabled) {
		return;
	}

	GraphicsContextVK* pGraphicsContext = reinterpret_cast<GraphicsContextVK*>(m_pGraphicsContext);
	DeviceVK* pDevice = pGraphicsContext->getDevice();
	const QueueFamilyIndices& queueFamilyIndices = pDevice->getQueueFamilyIndices();

	pCommandBuffer->releaseBufferOwnership(
		pBuffer,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		queueFamilyIndices.ComputeQueues.value().FamilyIndex,
		queueFamilyIndices.GraphicsQueues.value().FamilyIndex,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
	);
}

void ParticleEmitterHandlerVK::acquireForGraphics(BufferVK* pBuffer, CommandBufferVK* pCommandBuffer)
{
	if (!m_RenderingEnabled) {
		return;
	}

	GraphicsContextVK* pGraphicsContext = reinterpret_cast<GraphicsContextVK*>(m_pGraphicsContext);
	DeviceVK* pDevice = pGraphicsContext->getDevice();
	const QueueFamilyIndices& queueFamilyIndices = pDevice->getQueueFamilyIndices();

	pCommandBuffer->acquireBufferOwnership(
		pBuffer,
		VK_ACCESS_SHADER_READ_BIT,
		queueFamilyIndices.ComputeQueues.value().FamilyIndex,
		queueFamilyIndices.GraphicsQueues.value().FamilyIndex,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
	);
}

void ParticleEmitterHandlerVK::acquireForCompute(BufferVK* pBuffer, CommandBufferVK* pCommandBuffer)
{
	if (!m_RenderingEnabled) {
		return;
	}

	GraphicsContextVK* pGraphicsContext = reinterpret_cast<GraphicsContextVK*>(m_pGraphicsContext);
	DeviceVK* pDevice = pGraphicsContext->getDevice();
	const QueueFamilyIndices& queueFamilyIndices = pDevice->getQueueFamilyIndices();

	pCommandBuffer->acquireBufferOwnership(
		pBuffer,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		queueFamilyIndices.GraphicsQueues.value().FamilyIndex,
		queueFamilyIndices.ComputeQueues.value().FamilyIndex,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
	);
}

void ParticleEmitterHandlerVK::initializeEmitter(ParticleEmitter* pEmitter)
{

	DeviceVK* pDevice = reinterpret_cast<GraphicsContextVK*>(m_pGraphicsContext)->getDevice();
	if (m_IsComputeQueue) {
		pEmitter->initialize(m_pGraphicsContext, m_FrameCount, pDevice->getQueueFamilyIndices().ComputeQueues.value().FamilyIndex, m_NextQueueIndexCompute);
		LOG("Next: %d", m_NextQueueIndexCompute);
		uint32_t computeQueueCount = pDevice->getQueueFamilyIndices().ComputeQueues.value().QueueCount;
		m_NextQueueIndexCompute = (m_NextQueueIndexCompute + 1) % computeQueueCount;
	}
	else {
		pEmitter->initialize(m_pGraphicsContext, m_FrameCount, pDevice->getQueueFamilyIndices().GraphicsQueues.value().FamilyIndex, m_NextQueueIndexGraphics);
		LOG("Next: %d", m_NextQueueIndexGraphics);
		uint32_t graphicsQueueCount = pDevice->getQueueFamilyIndices().GraphicsQueues.value().QueueCount;
		m_NextQueueIndexGraphics = (m_NextQueueIndexGraphics + 1) % graphicsQueueCount;
	}
	m_IsComputeQueue = !m_IsComputeQueue;

	// Create descriptor set for the emitter
	DescriptorSetVK* pEmitterDescriptorSet = m_pDescriptorPool->allocDescriptorSet(m_pDescriptorSetLayoutPerEmitter);
	if (pEmitterDescriptorSet == nullptr) {
		LOG("Failed to create descriptor set for particle emitter");
		return;
	}

	BufferVK* pPositionsBuffer	= reinterpret_cast<BufferVK*>(pEmitter->getPositionsBuffer());
	BufferVK* pVelocitiesBuffer	= reinterpret_cast<BufferVK*>(pEmitter->getVelocitiesBuffer());
	BufferVK* pAgesBuffer		= reinterpret_cast<BufferVK*>(pEmitter->getAgesBuffer());
	BufferVK* pEmitterBuffer	= reinterpret_cast<BufferVK*>(pEmitter->getEmitterBuffer());

	pEmitterDescriptorSet->writeStorageBufferDescriptor(pPositionsBuffer,	POSITIONS_BINDING);
	pEmitterDescriptorSet->writeStorageBufferDescriptor(pVelocitiesBuffer,	VELOCITIES_BINDING);
	pEmitterDescriptorSet->writeStorageBufferDescriptor(pAgesBuffer,		AGES_BINDING);
	pEmitterDescriptorSet->writeUniformBufferDescriptor(pEmitterBuffer,		EMITTER_BINDING);

	pEmitter->setDescriptorSetCompute(pEmitterDescriptorSet);
}

void ParticleEmitterHandlerVK::updateGPU(float dt)
{
	for (ParticleEmitter* pEmitter : m_ParticleEmitters) {
		CommandBufferVK* pCommandBuffer = pEmitter->getCommandBuffer(m_CurrentFrame);
		beginUpdateFrame(pEmitter);

		// Update push-constant
		PushConstant pushConstant = {dt};
		pCommandBuffer->pushConstants(m_pPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant), (const void*)&pushConstant);

		pEmitter->updateGPU(dt);

		DescriptorSetVK* pDescriptorSet = reinterpret_cast<DescriptorSetVK*>(pEmitter->getDescriptorSetCompute());
		pCommandBuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, m_pPipelineLayout, 0, 1, &pDescriptorSet, 0, nullptr);

		uint32_t particleCount = pEmitter->getParticleCount();
		glm::u32vec3 workGroupSize(1 + particleCount / m_WorkGroupSize, 1, 1);

		pCommandBuffer->dispatch(workGroupSize);

		endUpdateFrame(pEmitter);
    }

	m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void ParticleEmitterHandlerVK::beginUpdateFrame(ParticleEmitter* pEmitter)
{
	CommandBufferVK* pCommandBuffer = pEmitter->getCommandBuffer(m_CurrentFrame);
	CommandPoolVK* pCommandPool		= pEmitter->getCommandPool(m_CurrentFrame);
	ProfilerVK* pProfiler			= pEmitter->getProfiler();

	pCommandBuffer->reset(true);
	pCommandPool->reset();

	pCommandBuffer->begin(nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	pProfiler->reset(m_CurrentFrame, pCommandBuffer);

	pProfiler->beginFrame(pCommandBuffer);

	pCommandBuffer->bindPipeline(m_pPipeline);

	// Update emitter's buffers
	if (pEmitter->m_EmitterUpdated) {
		EmitterBuffer emitterBuffer = {};
		pEmitter->createEmitterBuffer(emitterBuffer);

		BufferVK* pEmitterBuffer = reinterpret_cast<BufferVK*>(pEmitter->getEmitterBuffer());
		pCommandBuffer->updateBuffer(pEmitterBuffer, 0, &emitterBuffer, sizeof(EmitterBuffer));

		pEmitter->m_EmitterUpdated = false;
	}
}

void ParticleEmitterHandlerVK::endUpdateFrame(ParticleEmitter* pEmitter)
{
	CommandBufferVK* pCommandBuffer = pEmitter->getCommandBuffer(m_CurrentFrame);
	ProfilerVK* pProfiler			= pEmitter->getProfiler();
	pProfiler->endFrame();

	pCommandBuffer->end();

	GraphicsContextVK* pGraphicsContext = reinterpret_cast<GraphicsContextVK*>(m_pGraphicsContext);
    DeviceVK* pDevice = pGraphicsContext->getDevice();

	// Hardcoded for now that graphics is zero and compute is not
	if (pEmitter->getFamilyIndex() == 0)
		pDevice->executeGraphics(pCommandBuffer, nullptr, nullptr, 0, nullptr, 0, pEmitter->getQueueIndex());
	else
		pDevice->executeCompute(pCommandBuffer, nullptr, nullptr, 0, nullptr, 0, pEmitter->getQueueIndex());
}

bool ParticleEmitterHandlerVK::createCommandPoolAndBuffers()
{
    GraphicsContextVK* pGraphicsContext = reinterpret_cast<GraphicsContextVK*>(m_pGraphicsContext);
    DeviceVK* pDevice = pGraphicsContext->getDevice();

	const QueueFamilyIndices& queueFamilyIndices = pDevice->getQueueFamilyIndices();
	const uint32_t computeQueueIndex = queueFamilyIndices.ComputeQueues.value().FamilyIndex;
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_ppCommandPools[i] = DBG_NEW CommandPoolVK(pDevice, computeQueueIndex);

		if (!m_ppCommandPools[i]->init()) {
			return false;
		}
	}

	m_pCommandPoolGraphics = DBG_NEW CommandPoolVK(pDevice, queueFamilyIndices.GraphicsQueues.value().FamilyIndex);
	return m_pCommandPoolGraphics->init();
}

bool ParticleEmitterHandlerVK::createSamplers()
{
	SamplerParams params = {};
	params.MagFilter = VK_FILTER_NEAREST;
	params.MinFilter = VK_FILTER_NEAREST;
	params.WrapModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	params.WrapModeV = params.WrapModeU;
	params.WrapModeW = params.WrapModeU;

	GraphicsContextVK* pGraphicsContext = reinterpret_cast<GraphicsContextVK*>(m_pGraphicsContext);

	m_pGBufferSampler = DBG_NEW SamplerVK(pGraphicsContext->getDevice());
	return m_pGBufferSampler->init(params);
}

bool ParticleEmitterHandlerVK::createPipelineLayout()
{
	GraphicsContextVK* pGraphicsContext = reinterpret_cast<GraphicsContextVK*>(m_pGraphicsContext);
    DeviceVK* pDevice = pGraphicsContext->getDevice();

	// Descriptor Set Layout
	m_pDescriptorSetLayoutPerEmitter = DBG_NEW DescriptorSetLayoutVK(pDevice);

	m_pDescriptorSetLayoutPerEmitter->addBindingStorageBuffer(VK_SHADER_STAGE_COMPUTE_BIT, POSITIONS_BINDING, 1);
	m_pDescriptorSetLayoutPerEmitter->addBindingStorageBuffer(VK_SHADER_STAGE_COMPUTE_BIT, VELOCITIES_BINDING, 1);
	m_pDescriptorSetLayoutPerEmitter->addBindingStorageBuffer(VK_SHADER_STAGE_COMPUTE_BIT, AGES_BINDING, 1);
	m_pDescriptorSetLayoutPerEmitter->addBindingUniformBuffer(VK_SHADER_STAGE_COMPUTE_BIT, EMITTER_BINDING, 1);

	if (!m_pDescriptorSetLayoutPerEmitter->finalize()) {
		LOG("Failed to finalize particle descriptor set layout");
		return false;
	}

	// Descriptor pool
	DescriptorCounts descriptorCounts	= {};
	descriptorCounts.m_SampledImages	= 128;
	descriptorCounts.m_StorageBuffers	= 128;
	descriptorCounts.m_UniformBuffers	= 128;

	m_pDescriptorPool = DBG_NEW DescriptorPoolVK(pDevice);
	if (!m_pDescriptorPool->init(descriptorCounts, 16)) {
		LOG("Failed to initialize descriptor pool");
		return false;
	}

	m_pPipelineLayout = DBG_NEW PipelineLayoutVK(pDevice);

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.size = sizeof(PushConstant);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;

	return m_pPipelineLayout->init({m_pDescriptorSetLayoutPerEmitter}, {pushConstantRange});
}

bool ParticleEmitterHandlerVK::createPipeline()
{
	// Create pipeline state
	IShader* pComputeShader = m_pGraphicsContext->createShader();
	pComputeShader->initFromFile(EShader::COMPUTE_SHADER, "main", "assets/shaders/particles/update_cs.spv");
	if (!pComputeShader->finalize()) {
        LOG("Failed to create compute shader for particle emitter handler");
		return false;
	}

	// Maximize the work group size
	GraphicsContextVK* pGraphicsContext = reinterpret_cast<GraphicsContextVK*>(m_pGraphicsContext);
    DeviceVK* pDevice = pGraphicsContext->getDevice();

	uint32_t pMaxWorkGroupSize[3];
	pDevice->getMaxComputeWorkGroupSize(pMaxWorkGroupSize);
	m_WorkGroupSize = pMaxWorkGroupSize[0];

	ShaderVK* pComputeShaderVK = reinterpret_cast<ShaderVK*>(pComputeShader);
	pComputeShaderVK->setSpecializationConstant(1, m_WorkGroupSize);

	m_pPipeline = DBG_NEW PipelineVK(pDevice);
	m_pPipeline->finalizeCompute(pComputeShader, m_pPipelineLayout);

	SAFEDELETE(pComputeShader);
	return true;
}
