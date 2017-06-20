#include "Include/ShaderControllers/ProfilingWaitToFinish.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"


namespace ShaderControllers
{
    /*--------------------------------------------------------------------------------------------
    Description:
        This function is used to wait for the GPU to finish its commands.  It is useful during 
        profiling.  It was recommended to me by Osbios on the OpenGL subbreddit question 
        https://www.reddit.com/r/opengl/comments/69ri6l/increasing_buffer_size_for_compute_operations/.  
        It was recommended over glFinish() because that function needs to make a round-trip to 
        the GPU and back (??glClientWaitSync(...) doesn't??) and that apparently eats up some 
        extra cycles that can throw off my profiling.

        This version cycles between three synchronization fences so that there is only a wait if 
        the GPU is really backed up.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void WaitOnQueuedSynchronization()
    {
        // okay to declare static; they won't be initialized until the first call to this 
        // function, before which Init() was already called, so the OpenGL context will be 
        // initialized by the time that the code gets here
        static GLsync updateSyncFences[3] =
        {
            glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0),
            glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0),
            glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)
        };

        static int waitSyncFenceIndex = 0;
        glClientWaitSync(updateSyncFences[waitSyncFenceIndex], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
        glDeleteSync(updateSyncFences[waitSyncFenceIndex]);
        updateSyncFences[waitSyncFenceIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        waitSyncFenceIndex++;
        if (waitSyncFenceIndex >= 3) waitSyncFenceIndex = 0;
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This function is used for agressive synchronization.  It is only useful when profiling.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void WaitForComputeToFinish()
    {
        GLsync waitSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glClientWaitSync(waitSync, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
        glDeleteSync(waitSync);
    }
}


