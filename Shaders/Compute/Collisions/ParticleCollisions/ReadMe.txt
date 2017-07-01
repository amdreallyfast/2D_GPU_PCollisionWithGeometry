O(N^2) collision detection gets out of hand really fast.
- 5 particles: 25 comparisons (easy)
- 50 particles: 2500 comparisons (manageable)
- 500 particles: 250,000 comparisons (ouch)
- 50,000 particles: 2,500,000,000 comparisons (forget it)

Each particle still needs to some examine the entire particle array for potential collisions, but this is doable in O(NlogN).  I will spend the time and effort necessary to generate a binary tree of bounding volumes.  Each particle will then take its own bounding volume and check for overlaps from the root down to the leaves.  This allows a particle to hone in on overlapping bounding boxes with other particles in O(Nlog2N) time.  The binary tree can only be constructed out of sorted data though, so going this route requires sorting particles -> generate binary tree -> generate bounding volumes for the tree -> traverse tree to check for collisions.
- 5 particles: ~12 comparisons (not worth the sorting and tree construction time)
- 50 particles: ~283 comparisons (probably breaking even with sorting and tree construction time)
- 500 particles: ~4,483 comparisons (definitely worth the time)
- 50,000 particles: 780,482 (the tree is the only feasible option)

Quad trees are not constructable on a GPU in any reasonable amount of time.  The problem is the subdivision.  Everyone who wants to put a particle into a quad tree node has to wait while it is being subdivided, and in addition to there being no mutexes in GPU programming in any language (GLSL, CUDA, Vulkan), this causes a major lack of occupancy and is a big performance concern.  

Instead, this program will construct a Bounding Volume Hierarchy.  I am following this "Thinking Parallel" guide from the NVidia devblog:
    https://devblogs.nvidia.com/parallelforall/thinking-parallel-part-i-collision-detection-gpu/
    https://devblogs.nvidia.com/parallelforall/thinking-parallel-part-ii-tree-traversal-gpu/
    https://devblogs.nvidia.com/parallelforall/thinking-parallel-part-iii-tree-construction-gpu/

Binary radix tree construction follows this paper, which is referenced in a link in the 3rd part of that guide.
    http://devblogs.nvidia.com/parallelforall/wp-content/uploads/2012/11/karras2012hpg_paper.pdf

This is a binary radix tree that can be constructed in parallel.  The term "radix" means that the tree is constructed based on the bits of the numbers in the data set that it is building over, unlike a quad tree, which is constructed based on how full each node is.  A quad tree's construction must therefore be sequential, while a radix algorithm is not dependent on the state of other nodes and can therefore be performed in parallel.  However, this algorithm for the construction of the binary radix tree will only work if the data that it is working on is already sorted.

So what is this data and how is it sorted?  For the purposes of collision detection, the data needs to be a function of particle position that somehow places particles that are near each other in space near to each other when sorted.  If the data does not put spacial-proximitous particles near each other, then the bounding volume hierarchy that is built on top of this tree will have bounding volumes all over the place, and that won't reduce the number of collision comparisons to O(Nlog2N) levels.

But how to sort particles in 2D space?  Or 3D space?  By X, then Y, then Z?  Nope.  Use a space-filling curve.  A space-filling curve is (1) capable of reaching every point in a region of space, like a square on the range X [-1,+1] and Y [-1,+1] ("fills" the space if it passes through enough points) and (2) has a start, an end, and never crosses itself (properties of a curve).
Recommended resources:
- 3blue1brown: “Hilbert's Curve, and the usefulness of infinite results in a finite world” https://www.youtube.com/watch?v=DuiryHHTrjU 
- Numberphile: “Space-filling Curves” https://www.youtube.com/watch?v=x-DgL49CFlM 

Specifically, I will be using a Z-Order curve.  The points in a Z-Order curve are called Morton Codes.  According to the Wikipedia page, "In mathematical analysis and computer science, Z-order, Lebesgue curve, Morton order or Morton code is a function which maps multidimensional data to one dimension while preserving locality of the data points. It was introduced in 1966 by Guy Macdonald Morton."  In English, this means a Z-Order curve can be used to take 2D (or more) points and calculate a single number such that points that are near each other in 2D space are usually close to each other on the curve.  This is exactly what I want.  I can sort particles over this data, then use this same data to construct a binary radix tree so that sequential leaf nodes will usually describe particles that are near each other, and the bounding volume hierarchy that is built on top of this tree will have bounding volumes that are localized to nearby particles.  Excellent.

Here is what a Z-Order curve looks like:
https://www.youtube.com/watch?v=hfhNRkmYfuU

In my own visualizations, I found that it usually looked like a letter "Z" that was rotated counterclockwise 90-degrees, and when data is not nicely spaced as it was in that video example, such as with a bunch of particles in 2D or 3D space, the curve does not resemble the letter "Z" at all.  I think that "Z" is short for "zig-zag", which the curve definitely looks like, not the letter "Z".  

Morton Codes are made by some bit-swizzling magic.  In GLSL, only integers can perform bit operations, and integers max out at 32 bits.  When swizzling over 3D space, as this demo is doing (yes, it is only 2D right now, but I don't understand the bit magic to make it exclusive to 2D, and I plan to move to 3D eventually anyway, so I'm staying with this), then an X, Y, and Z value integer value will be required.  Swizzling 3 numbers into 32bit integer means that each number can only be 10 bits, an so the Morton Code is 30bits (11 bits per X, Y, and Z would require a 33bit integer, which is not available in GLSL).  See the 3rd part of the Thinking Parallel article for more detail.  I will use an unsigned integer so that I have 2 more bits to play with if necessary.

Parallel sorting will be accomplished via a "radix" sort.  Like a binary radix tree, this sorting will be performed by analyzing the bits in the numebrs themselves rather than comparing the values of two numbers, as CPU-bound algorithms do.  The core of this sort will be the parallel prefix sum.  The sorting will begin by extracting the 0th bit from all the values being considered (all the Morton Codes), calculating the number of preceding 1s (prefix sum) and preciding 0s (total number of Morton Codes - prefix sum) for that number, then reorganize the values so that all values that had a 0 extracted will be first and all the values that had a 1 extracted will be after that.  The key part of this algorithm is numbers with a 0 extracted are not shoved at random into a collection with the other "0s", and numbers with a 1 extracted are not shoved at random into a collection with the other "1s", but rather numbers with a 0 extracted will retain their relative position to the other "0s", ditto for the "1s".  Repeat this for all the other 29 bits in the Morton Code.

Here is the order of compute shader operations:
(1) Generate Morton Codes for each particle
For bits 0-29 in the Morton Codes
    (2) Get bit for prefix scan
    (3) Clear work group sums
    (4) Parallel prefix scan to create a prefix sum (number of preceding 1s, and thus implicitly the number of preceding 0s)
    (5) Sort Morton Codes according the prefix sums for the current bit
(6) Use the sorted Morton Codes to sort particles to the same positions.  This will help avoid data divergeance when accessing the particle buffer during collision resolution.
(7) Generate the binary radix tree using the sorted Morton Codes
(8) Populate the leaf nodes with bounding boxes
(9) Merge the bounding boxes from the leaves up to the root of the tree (might be able to do (7) in the same shader with barrier())
(10) Have each particle's bounding box (in its corresponding leaf node) traverse the tree and look for possible collisions
(11) Resolve those collisions.

And that is parallel collision detection.  It's a lot of work to change from O(N^2) -> O(Nlog2N), but it is the only feasible way to perform particle collision when N becomes large.
