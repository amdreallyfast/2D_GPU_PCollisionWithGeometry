Particle resetting has to handle point emitters and bar emitters, and they have several functions in common.  I decided to split the point emitter resetting and bar emitter resetting into their compute shaders, and then the common functions needed to be split into their own files as well so that a composite shader could be constructed.  This created enough parts that I thought that they should be put into their own folder.

- John Cox, 4/2017
