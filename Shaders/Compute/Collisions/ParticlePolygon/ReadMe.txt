This is just like the particle sorting and BVH generation, hence several of the files are named the same as their particle counterparts.

Unfortunately, in OpenGL compute shaders, there is no such thing as passing a buffer as an argument, so all buffers must be referenced explicitly by name.  I don't like the idea of trying to reuse the particles' sorting buffers (sorting data, prefix scan, potential collisions, BVH nodes), so I want to use polygon-specific buffers.  

Buffers can't be passed by argument, so the polygons' sorting buffers need to be referenced explicitly, which requires their own compute shaders.  

These polygon-sorting compute shaders perform the same functionality that the particle-sorting compute shaders do.

Perhaps Vulkan will allow me more flexibility.
