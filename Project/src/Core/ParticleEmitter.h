#pragma once

#define NOMINMAX

#include "glm/glm.hpp"

#include "Common/IGraphicsContext.h"

#include <random>

class Camera;
class IBuffer;
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
};
    // This padding gets the buffer size up to 128 bytes

struct ParticleStorage {
    std::vector<glm::vec4> positions, velocities;
    std::vector<float> ages;
};

class ParticleEmitter
{
public:
    ParticleEmitter(const ParticleEmitterInfo& emitterInfo);
    ~ParticleEmitter();

    bool initialize(IGraphicsContext* pGraphicsContext, const Camera* pCamera);

    void update(float dt);
    void updateGPU(float dt);

    const ParticleStorage& getParticleStorage() const { return m_ParticleStorage; }
    void createEmitterBuffer(EmitterBuffer& emitterBuffer);
    // TODO: Change this to use the emitter's age to calculate the particle count. Using the size doesn't work if particles only exist on the GPU.
    uint32_t getParticleCount() const;

    IBuffer* getPositionsBuffer() { return m_pPositionsBuffer; }
    IBuffer* getVelocitiesBuffer() { return m_pVelocitiesBuffer; }
    IBuffer* getAgesBuffer() { return m_pAgesBuffer; }
    IBuffer* getEmitterBuffer() { return m_pEmitterBuffer; }
    ITexture2D* getParticleTexture() { return m_pTexture; }

    // Whether or not the emitter's settings (above) have been changed during the current frame
    bool m_EmitterUpdated;

private:
    bool createBuffers(IGraphicsContext* pGraphicsContext);

    // Spawns particles before the emitter has created its maximum amount of particles
    void spawnNewParticles();
    void moveParticles(float dt);
    void respawnOldParticles();
    void createParticle(size_t particleIdx, float particleAge);

private:
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

    // GPU-side particle data
    IBuffer* m_pPositionsBuffer;
    IBuffer* m_pVelocitiesBuffer;
    IBuffer* m_pAgesBuffer;

    // Contains particle size
    IBuffer* m_pEmitterBuffer;

    const Camera* m_pCamera;
};