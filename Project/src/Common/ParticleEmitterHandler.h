#pragma once

#include "Common/ITexture2D.h"
#include "Core/ParticleEmitter.h"

class Camera;
class IGraphicsContext;
class IPipeline;
class IRenderer;
class ITexture2D;
class ParticleEmitter;
class RenderingHandler;
class Texture2DVK;

class ParticleEmitterHandler
{
public:
    ParticleEmitterHandler(bool renderingEnabled);
    virtual ~ParticleEmitterHandler();

    virtual void update(float dt) = 0;
    virtual void updateRenderingBuffers(RenderingHandler* pRenderingHandler) = 0;
    virtual void drawProfilerUI() = 0;

    void initialize(IGraphicsContext* pGraphicsContext, RenderingHandler* pRenderingHandler, const Camera* pCamera);
    // Initializes resources for GPU-side computing of particles
    virtual bool initializeGPUCompute() = 0;

    ParticleEmitter* createEmitter(const ParticleEmitterInfo& emitterInfo);

    std::vector<ParticleEmitter*>& getParticleEmitters() { return m_ParticleEmitters; }

    virtual void onWindowResize() = 0;

    bool gpuComputed() const { return m_GPUComputed; }
    virtual void toggleComputationDevice() = 0;

    bool collisionsEnabled() const { return m_CollisionsEnabled; }
    void toggleCollisions() { m_CollisionsEnabled = !m_CollisionsEnabled; }

protected:
    IGraphicsContext* m_pGraphicsContext;
    RenderingHandler* m_pRenderingHandler;
    const Camera* m_pCamera;

    std::vector<ParticleEmitter*> m_ParticleEmitters;

    bool m_RenderingEnabled;

    // Whether to use the GPU or the CPU for updating particle data
    bool m_GPUComputed;
    bool m_CollisionsEnabled;

private:
    virtual void initializeEmitter(ParticleEmitter* pEmitter) = 0;
};
