#pragma once

#include <vector>
#include <memory>

#include "Include/Buffers/SSBOs/ParticleSsbo.h"
#include "Include/Particles/IParticleEmitter.h"
#include "Include/Particles/ParticleEmitterPoint.h"
#include "Include/Particles/ParticleEmitterBar.h"


namespace ShaderControllers
{
    /*--------------------------------------------------------------------------------------------
    Description:
        Encapsulate particle reseting via compute shader.  Resetting involves taking inactive 
        particles and giving them a new position near a particle emitter plus giving them a new 
        velocity.  
        
        Particles can currently (4-15-2017) be emitted from two types of emitters:
        (1) Point emitters eject particles in all directions
        (2) Bar emitters eject particles outwards from a 2D plane 

        These was deemed different enough to justify splitting the once-one shader into two, one
        for each type of emitter.  This shader controller has the info for both.

        Note: Unlike in previous Particle-related demos, shared pointers are used and the 
        ParticleReset emitter is now an "owner" of the emitters.  By using shared pointers, the 
        user is given the option of keeping around an emitter and changing its position or 
        direction at runtime, or they can create an emitter on startup and then leave it to the 
        ParticleReset object to clean them up when it is finished.
    Creator:    John Cox, 4/2017 (origins in 11/2016)
    --------------------------------------------------------------------------------------------*/
    class ParticleReset
    {
    public:
        ParticleReset(const ParticleSsbo::SharedConstPtr &ssboToReset);
        ~ParticleReset();

        // Note: Have to use a copy, not a reference, in order for a shared pointer argument to 
        // be turned into a shared pointer to const data.  A shared pointer is castable to a 
        // shared pointer to const data, but they are two different object types, hence the need
        // for a copy constructor (??I think??).
        void AddEmitter(ParticleEmitterPoint::CONST_SHARED_PTR pointEmitter);
        void AddEmitter(ParticleEmitterBar::CONST_SHARED_PTR barEmitter);

        void ResetParticles(unsigned int particlesPerEmitterPerFrame);

    private:
        unsigned int _totalParticleCount;
        unsigned int _computeProgramIdBarEmitters;
        unsigned int _computeProgramIdPointEmitters;

        // some of these uniforms had to be split into two versions to accomodate both shaders

        // specific to point emitter
        int _unifLocPointEmitterCenter;
        int _unifLocPointMaxParticleEmitCount;
        int _unifLocPointMinParticleVelocity;
        int _unifLocPointMaxParticleVelocity;

        // specific to bar emitter
        int _unifLocBarEmitterP1;
        int _unifLocBarEmitterP2;
        int _unifLocBarEmitterEmitDir;
        int _unifLocBarMaxParticleEmitCount;
        int _unifLocBarMinParticleVelocity;
        int _unifLocBarMaxParticleVelocity;

        // all the updating heavy lifting goes on in the compute shader, so CPU cache coherency 
        // is not a concern for emitter storage on the CPU side and a std::vector<...> is 
        // acceptable
        // Note: The compute shader has no concept of inheritance.  Rather than store a single 
        // collection of IParticleEmitter pointers and cast them to either point or bar emitters 
        // on every update, just store them separately.
        static const int MAX_EMITTERS = 4;
        std::vector<ParticleEmitterPoint::CONST_SHARED_PTR> _pointEmitters;
        std::vector<ParticleEmitterBar::CONST_SHARED_PTR> _barEmitters;
    };
}
