#pragma once

#define NOMINMAX

#include "glm/glm.hpp"

#include "Common/IGraphicsContext.h"
#include "Vulkan/ProfilerVK.h"
#include "Vulkan/VulkanCommon.h"

#include <random>

class Camera;
class IBuffer;
class IDescriptorSet;
class IGraphicsContext;
class IMesh;
class ITexture2D;

struct ParticleEmitterInfo {
    glm::vec3 position, direction;
    glm::vec2 particleSize;
    float particleDuration, initialSpeed, particlesPerSecond;
    // The angle by which spawned particles' directions can diverge from the emitter's direction, [0,pi], where pi means particles can be fired in any direction
    float spread;
    ITexture2D* pTexture;
};

struct EmitterBuffer {
    glm::mat4x4 centeringRotMatrix;
    glm::vec4 position, direction;
    glm::vec2 particleSize;
    float particleDuration, initialSpeed, spread;
    uint32_t particleCount;
};

struct ParticleStorage {
    std::vector<glm::vec4> positions, velocities;
    std::vector<float> ages;
};

class CommandBufferVK;
class CommandPoolVK;

class ParticleEmitter
{
public:
    ParticleEmitter(const ParticleEmitterInfo& emitterInfo);
    ~ParticleEmitter();

    bool initialize(IGraphicsContext* pGraphicsContext);

    void update(float dt);
    void updateGPU(float dt);

    CommandBufferVK* getCommandBuffer(uint32_t frameIndex)  { return m_ppCommandBuffers[frameIndex]; }
    CommandPoolVK* getCommandPool(uint32_t frameIndex)      { return m_ppCommandPools[frameIndex]; }
    ProfilerVK* getProfiler()                               { return m_pProfiler; }

    void saveLatestTimestamps();

    const ParticleStorage& getParticleStorage() const { return m_ParticleStorage; }
    void createEmitterBuffer(EmitterBuffer& emitterBuffer);

    uint32_t getParticleCount() const;

    glm::vec3 getPosition() const { return m_Position; }
    glm::vec3 getDirection() const { return m_Direction; }
    glm::vec2 getParticleSize() const { return m_ParticleSize; }
    float getInitialSpeed() const { return m_InitialSpeed; }
    float getParticlesPerSecond() const { return m_ParticlesPerSecond; }
    float getParticleDuration() const { return m_ParticleDuration; }
    float getSpread() const { return m_Spread; }

    void setPosition(const glm::vec3& position);
    void setDirection(const glm::vec3& direction);
    void setParticleSize(const glm::vec2& size);
    void setInitialSpeed(float initialSpeed);
    void setParticlesPerSecond(float particlesPerSecond);
    void setParticleDuration(float particleDuration);
    void setSpread(float spread);

    IDescriptorSet* getDescriptorSetCompute() { return m_pDescriptorSetCompute; }
    IDescriptorSet* getDescriptorSetRender() { return m_pDescriptorSetRender; }
    void setDescriptorSetCompute(IDescriptorSet* pDescriptorSet)    { m_pDescriptorSetCompute = pDescriptorSet; }
    void setDescriptorSetRender(IDescriptorSet* pDescriptorSet)     { m_pDescriptorSetRender = pDescriptorSet; }

    IBuffer* getPositionsBuffer() { return m_pPositionsBuffer; }
    IBuffer* getVelocitiesBuffer() { return m_pVelocitiesBuffer; }
    IBuffer* getAgesBuffer() { return m_pAgesBuffer; }
    IBuffer* getEmitterBuffer() { return m_pEmitterBuffer; }
    ITexture2D* getParticleTexture() { return m_pTexture; }

    // Whether or not the emitter's settings (above) have been changed during the current frame
    bool m_EmitterUpdated;

private:
    bool createBuffers(IGraphicsContext* pGraphicsContext);
    bool createCommandBuffers(IGraphicsContext* pGraphicsContext);
    void createProfiler(IGraphicsContext* pGraphicsContext);

    void ageEmitter(float dt);
    void moveParticles(float dt);
    void respawnOldParticles();
    void createParticle(size_t particleIdx, float particleAge);

    // Calculate rotation quaternion for spawning new particles in the desired direction
    void createCenteringQuaternion();
    void resizeParticleStorage(size_t newSize);

    void saveTimestampsToFile();

private:
    CommandBufferVK* m_ppCommandBuffers[MAX_FRAMES_IN_FLIGHT];
    CommandPoolVK* m_ppCommandPools[MAX_FRAMES_IN_FLIGHT];

    ProfilerVK* m_pProfiler;
    std::vector<uint64_t> m_Timestamps;

    glm::vec3 m_Position, m_Direction;
    glm::vec2 m_ParticleSize;
    float m_ParticleDuration, m_InitialSpeed, m_ParticlesPerSecond, m_Spread;

    // Resources for generating random spread for particle directions
    std::mt19937 m_RandEngine;
    std::uniform_real_distribution<float> m_ZRandomizer;
    std::uniform_real_distribution<float> m_PhiRandomizer;

    const glm::vec3 m_ZVec = glm::vec3(0.0f, 0.0f, 1.0f);
    // Random directions for particle are centered around (0,0,1), this quaternion centers them around the emitter's direction
    glm::quat m_CenteringRotQuat;

    ParticleStorage m_ParticleStorage;
    ITexture2D* m_pTexture;

    // The amount of time since the emitter started emitting particles. Used for spawning particles.
    float m_EmitterAge;

    IDescriptorSet* m_pDescriptorSetCompute;
    IDescriptorSet* m_pDescriptorSetRender;

    // GPU-side particle data
    IBuffer* m_pPositionsBuffer;
    IBuffer* m_pVelocitiesBuffer;
    IBuffer* m_pAgesBuffer;
    IBuffer* m_pEmitterBuffer;
};
