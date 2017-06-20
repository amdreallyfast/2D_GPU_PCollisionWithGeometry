#include "Include/Buffers/PersistentAtomicCounterBuffer.h"

#include "Shaders/ShaderHeaders/SsboBufferBindings.comp"

#include "ThirdParty/glload/include/glload/gl_4_4.h"


/*------------------------------------------------------------------------------------------------
Description:
    Gives members initial values.
    Generates a persistently bound atomic counter buffer.  This can be used by compute shaders 
    that specify the atomic counter with the binding location ATOMIC_COUNTER_BUFFER_BINDING 
    (from SsboBufferBindings.comp).
Parameters: None
Returns:    None
Creator:    John Cox, 4/2017
------------------------------------------------------------------------------------------------*/
PersistentAtomicCounterBuffer::PersistentAtomicCounterBuffer() :
    _bufferId(0),
    _bufferPtr(0)
{
    // call glBufferStorage(...) to set up an immutably-sized buffer with certain contracts 
    // Note: 
    // - Write (why?)
    // - Coherent (writes are immediately visible to GPU)
    // - Persistent (allow for performance optimizations magically somehow)
    // - See my source material in the class header
    // Also Note: 
    // using 
    // Note: Thanks to user qartar on the OpenGL subreddit for telling me that glBufferData(...) 
    // is unnecessary prior to glBufferStorage(...).  
    // Also Also Note: Credit to user Yan An on the stackoverflow question 
    // "What is the difference between glBufferStorage and glBufferData?"
    // http://stackoverflow.com/questions/27810542/what-is-the-difference-between-glbufferstorage-and-glbufferdata/27812200#27812200
    // Calling glBufferStorage(...), which will set up the buffer with immutable storage, while 
    // glBufferData(...) sets up the buffer with mutable storage.  I experimented with using 
    // both of these functions and checked out the messages spit out by OpenGL's debug message 
    // callback (debugging must have been difficult prior to OpenGL 4.3).  I glBufferData(...) 
    // is called first, then glBufferStorage(...), then the OpenGL debug message callback 
    // reports the following (the buffer object numbers are unique to my application):
    // Following glBufferData(...):
    //  "Buffer object 1 (bound to GL_ATOMIC_COUNTER_BUFER, *blah blah blah*) will use VIDEO 
    //  memory as the source for buffer object operations."
    // Then glBufferStorage(...):
    //  "Buffer object 1 (bound to GL_ATOMIC_COUNTER_BUFFER, *blah blah blah*) stored in SYSTEM 
    // HEAP memory has been updated."
    // 
    // However, if I do glBufferStorage(...) first and then glBufferData(...), I get the 
    // following OpenGL debug messages:
    // Following glBufferStorage(...):
    //  "Buffer object 1 (bound to GL_ATOMIC_COUNTER_BUFFER, "blah blah blah*) will use SYSTEM 
    //  HEAP memory as the source for buffer object operations."
    // Following glBufferData(...):
    //  "GL_INVALID_OPERATION error generated.  Cannot modify immutable buffer."
    // 
    // So it looks like glBufferStorage(...) will run over glBufferData(...), but the opposite 
    // is not true (at least for my implementation through GLLoad; I don't know if it is 
    // implementation dependent).
    //
    // Also Also Also Note: The use of SYSTEM HEAP memory is because I am only using the 
    // GL_MAP_WRITE_BIT.  If I use GL_MAP_WRITE_BIT | GL_MAP_READ_BIT, then the debug message 
    // that follows glBufferStorage(...) says that it will use DMA memory, and then I loose 
    // ~10fps on my GTX 560M and my i7-2630QM.  Apparently I can still read from it though if I 
    // only use GL_MAP_WRITE_BIT and use fencing properly, so I can read without loosing the 
    // ~10fps.  I suspect that the use of GL_MAP_READ_BIT makes a contract between the CPU and 
    // the GPU that requires more synchronization than just using the write bit.  That would 
    // slow things down.

    // ??what is the "system" in "SYSTEM HEAP"? CPU heap or GPU heap??
    // ??why can I still read from it if I don't have GL_MAP_READ_BIT set??

    glGenBuffers(1, &_bufferId);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, ATOMIC_COUNTER_BUFFER_BINDING, _bufferId);

    GLuint atomicCounterResetValue = 0;
    GLuint flags = GL_MAP_WRITE_BIT| GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glBufferStorage(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), &atomicCounterResetValue, flags);

    // force cast to unsigned int pointer because I know that it is a buffer of unsigned integers
    void *voidPtr = glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), flags);
    _bufferPtr = static_cast<unsigned int *>(voidPtr);
    _bufferPtr[0] = 0;
}

/*------------------------------------------------------------------------------------------------
Description:
    Unmaps the atomic counter buffer and destroys it.

    ??better way to do this? Note from 2D_GPU_PCollisionWithZOrderCurveRadixSorting??
    Note: Thanks to user concerned-cynix on the OpenGL subreddit for letting me know that this static stuff causes an OpenGL error on program exit on Linux.  In VS2015 (at least at the time that this function was made), no error is printed via the debug message callback.  That may be because, if the OpenGL context is destroyed by the time that static variables are destroyed, then the debug message callback won't be called.  Huh.  I want there to only be one instance of this calss so that I only need one buffer binding, but is it possible to do a single buffer binding without static?

Parameters: None
Returns:    None
Creator:    John Cox, 4/2017
------------------------------------------------------------------------------------------------*/
PersistentAtomicCounterBuffer::~PersistentAtomicCounterBuffer()
{
    // unsynchronize
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
    glDeleteBuffers(1, &_bufferId);
}

/*------------------------------------------------------------------------------------------------
Description:
    Returns a const shared pointer to a unique instance of the PersistentAtomicCounterBuffer.

    If this is the first call of the program, it generates a new instance.

    Note: Returning a const reference to a const shared pointer because I don't want the class' 
    instance pointer being messed with on the outside.
Parameters: None
Returns:    
    A const reference to a const shared pointer of the class instance.  
Creator:    John Cox, 4/2017
------------------------------------------------------------------------------------------------*/
const PersistentAtomicCounterBuffer& PersistentAtomicCounterBuffer::GetInstance()
{
    // it is okay to make this static initializer because the static value will not be 
    // initialized until the first call to this method (unlike static globals, which are 
    // initialized at program start), and by then the OpenGL context will be initialized
    static PersistentAtomicCounterBuffer instance;

    return instance;
}

/*------------------------------------------------------------------------------------------------
Description:
    Waits for the GPU to finish whatever it is doing, then puts a 0 into the atomic counter 
    buffer.  Because of GL_MAP_WRITE_BIT and GL_MAP_COHERENT_BIT, this 0 will become immediately 
    visible to GPU.
    
    Note: I discovered after much frustration that you should NOT do this:
    - fence
    - _bufferPtr[0] = 0
    - client wait sync (wait for GPU to finish)

    That just doesn't work.  Must do this:
    - fence
    - client wait sync (wait for GPU to finish)
    - _bufferPtr[0] = 0

    The wait for GPU to finish seems to be pretty much nonexistent on my hardware and for this 
    implementation.
Parameters: None
Returns:    None
Creator:    John Cox, 4/2017
------------------------------------------------------------------------------------------------*/
void PersistentAtomicCounterBuffer::ResetCounter() const
{
    GLsync writeSyncFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    // Note: Thanks to this article for the idea for the wait sync loop.
    // https://www.codeproject.com/Articles/872417/Persistent-Mapped-Buffers-in-OpenGL
    GLenum waitReturn = GL_UNSIGNALED;
    while (waitReturn != GL_ALREADY_SIGNALED && waitReturn != GL_CONDITION_SATISFIED)
    {
        waitReturn = glClientWaitSync(writeSyncFence, GL_SYNC_FLUSH_COMMANDS_BIT, 1);
    }
    _bufferPtr[0] = 0;

    // if it has already synchronized (it has to be in order to reach this point), then the sync 
    // object will be immediately deleted and there should be no performance problem
    // Note: See https://www.khronos.org/opengl/wiki/GLAPI/glDeleteSync.
    glDeleteSync(writeSyncFence);
}

/*------------------------------------------------------------------------------------------------
Description:
    Waits for the GPU to finish whatever it is doing, then reads the atomic counter value.

    Like with ResetCounter, the wait for GPU to finish seems to be pretty much nonexistent on my 
    hardware and for this implementation.

    ??why can I still read from it if I don't have GL_MAP_READ_BIT set??
    
    Note: Failure to call glMemoryBarrier(...) with GL_ATOMIC_COUNTER_BARRIER_BIT doesn't seem 
    to make this not work.  I can read the value regardless.  I don't know why.

Parameters: None
Returns:    None
Creator:    John Cox, 4/2017
------------------------------------------------------------------------------------------------*/
unsigned int PersistentAtomicCounterBuffer::GetCounterValue() const
{
    GLsync readSyncFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    GLenum waitReturn = GL_UNSIGNALED;
    while (waitReturn != GL_ALREADY_SIGNALED && waitReturn != GL_CONDITION_SATISFIED)
    {
        waitReturn = glClientWaitSync(readSyncFence, GL_SYNC_FLUSH_COMMANDS_BIT, 1);
    }
    glDeleteSync(readSyncFence);

    return *_bufferPtr;
}

