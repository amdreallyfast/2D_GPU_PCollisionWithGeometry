#pragma once

#include <memory>

#include "Include/ParticleEmitters/IParticleEmitter.h"
#include "Include/Buffers/Particle.h"
#include "ThirdParty/glm/vec2.hpp"

/*------------------------------------------------------------------------------------------------
Description:
    This particle emitter provides information for resetting particles to a position along a 
    vector with a new velocity within the range [min,max].  
Creator:    John Cox (7-2-2016)
------------------------------------------------------------------------------------------------*/
class ParticleEmitterBar : public IParticleEmitter
{
public:
    ParticleEmitterBar(const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &emitDir,
        const float minVel, const float maxVel);
    using SHARED_PTR = std::shared_ptr<ParticleEmitterBar>;
    using CONST_SHARED_PTR = std::shared_ptr<const ParticleEmitterBar>;

    virtual void SetTransform(const glm::mat4 &emitterTransform) override;

    glm::vec4 GetBarStart() const;
    glm::vec4 GetBarEnd() const;
    glm::vec4 GetEmitDir() const;
    float GetMinVelocity() const;
    float GetMaxVelocity() const;

private:
    glm::vec4 _start;
    glm::vec4 _end;
    glm::vec4 _emitDir;
    float _minVel;
    float _maxVel;

    glm::vec4 _transformedStart;
    glm::vec4 _transformedEnd;
    glm::vec4 _transformedEmitDir;
};

