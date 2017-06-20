#pragma once

#include <memory>
#include "Include/Buffers/Particle.h"
#include "ThirdParty/glm/mat4x4.hpp"

/*------------------------------------------------------------------------------------------------
Description:
    The "particle updater" must be able to easily use multiple particle emitters without much 
    trouble, so use an interface that defines the basic functionality of each particle emitter.
Creator:    John Cox (7-2-2016)
------------------------------------------------------------------------------------------------*/
class IParticleEmitter
{
public:
    virtual void SetTransform(const glm::mat4 &emitterTransform) = 0;
};

