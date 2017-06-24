#pragma once

#include <memory>

#include "Include/ParticleEmitters/IParticleEmitter.h"
#include "Include/Buffers/Particle.h"
#include "ThirdParty/glm/vec2.hpp"

/*------------------------------------------------------------------------------------------------
Description:
    This particle emitter will reset particles to a position at a single point and will set their
    velocity to a random vector anywhere within 360 degrees.
Creator:    John Cox (7-2-2016)
------------------------------------------------------------------------------------------------*/
class ParticleEmitterPoint : public IParticleEmitter
{
public:
    // emits randomly from the origin point
    ParticleEmitterPoint(const glm::vec2 &emitterPos, const float minVel, const float maxVel);
    using SHARED_PTR = std::shared_ptr<ParticleEmitterPoint>;
    using CONST_SHARED_PTR = std::shared_ptr<const ParticleEmitterPoint>;

    virtual void SetTransform(const glm::mat4 &emitterTransform) override;

    glm::vec4 GetPos() const;
    float GetMinVelocity() const;
    float GetMaxVelocity() const;

private:
    glm::vec4 _pos;
    glm::vec4 _transformedPos;
    float _minVel;
    float _maxVel;
};
