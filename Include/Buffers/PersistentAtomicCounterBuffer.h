#pragma once

#include <memory>

/*------------------------------------------------------------------------------------------------
Description:
    At this time (4-28-2017) there are a couple compute shaders ("reset particles" and 
    "update particles") that use an atomic counter.  Up until now I have been using an atomic 
    counter for each shader, binding the buffer prior to use and using glBufferSubData(...) to 
    reset the counter to 0, and maybe using glMapBufferSubRange(...) later to get the value of 
    the counter.

    Now I want to using a persistently mapped buffer.  The GL_ATOMIC_COUNTER_BUFFER is not a 
    common buffer target, and it is only used in compute shaders, and I want to write to it and 
    read from frequently (albeit only 1 uint on each write or read), so I will try to make a 
    single atomic counter buffer that any compute shader can use.

    I am taking my glBufferStorage(...) information from the Steam Dev Days 2014 talk, 
    "Beyond Porting: How Modern OpenGL Can Radically Reduce Driver Overhead".  
    https://www.youtube.com/watch?v=-bCeNzgiJ8I&index=21&list=PLckFgM6dUP2hc4iy-IdKFtqR9TeZWMPjm
    Start @8:30
    
    It can also be found under GDC 2014 "Approach Zero Driver Overhead".
    https://www.youtube.com/watch?v=K70QbvzB6II
    Start @11:08

    Note: I had to tinker with everything else that wasn't glBufferStorage(...) until I got it 
    to work.

    Also Note: This is a singleton structure because I want to use the same atomic counter 
    buffer binding pint in each of my compute shaders, so by design it is meant to be used by 
    multiple compute shader controller objects.  I could make multiple persistently bound atomic 
    counter buffers (I think; I haven't tried that yet), but then I'd have to use multiple 
    atomic counter buffer binding points.  I want to use one because I want to see if I can :).

    Also Also Note: I don't need to bother with multithread design because the OpenGL context 
    only runs one thread anyway.
Creator:    John Cox, 4/2017
------------------------------------------------------------------------------------------------*/
class PersistentAtomicCounterBuffer
{
private:
    // no constructing except through GetInstance()
    PersistentAtomicCounterBuffer();
    PersistentAtomicCounterBuffer(const PersistentAtomicCounterBuffer &) = delete;
    PersistentAtomicCounterBuffer &operator=(const PersistentAtomicCounterBuffer &) = delete;

public:
    ~PersistentAtomicCounterBuffer();
    static const PersistentAtomicCounterBuffer &GetInstance();

    void ResetCounter() const;
    unsigned int GetCounterValue() const;

private:
    unsigned int _bufferId;
    unsigned int *_bufferPtr;
};
