#pragma once

/*------------------------------------------------------------------------------------------------
Description:
A convenience structure for BvhNode.  Must match the corresponding structure in
ParticleBvhNodeBuffer.comp.
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
struct BoundingBox
{
    BoundingBox() :
        _left(0.0f),
        _right(0.0f),
        _top(0.0f),
        _bottom(0.0f)
    {
    }

    float _left;
    float _right;
    float _top;
    float _bottom;
};

/*------------------------------------------------------------------------------------------------
Description:
Must match the corresponding structure in ParticleBvhNodeBuffer.comp.
Stores info about a single node in the BVH.  Can be either an internal node or a leaf node.
If internal, then its children are either leaf nodes or other internal nodes.  If a leaf
node, then it will have _data to analyze.
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
struct BvhNode
{
    BvhNode() :
        _isLeaf(0),
        _isNull(0),
        _parentIndex(-1),
        _threadEntranceCounter(0),
        _leftChildIndex(-1),
        _rightChildIndex(-1)
    {
    }

    BoundingBox _boundingBox;

    // if 0, then it is an internal node
    int _isLeaf;

    // used during tree construction to prevent merging bounding boxes from inactive leaves (see 
    // GuaranteeMortonCodeUniqueness.comp for explanation)
    int _isNull;

    // used for merging bounding boxes up to the root
    int _parentIndex;

    // used to prevent the first thread that reads this internal node from trying to merge the 
    // bounding boxes of its child, one of which may not be finished yet
    int _threadEntranceCounter;

    // used when traversing down the tree during collision detection
    int _leftChildIndex;
    int _rightChildIndex;

    // Note: Lesson learned about buffer padding.  It is only necessary if the structure defines 
    // a vec* (yes, vec2 included; I tested it) or mat*.  The CPU side can declare whatever it 
    // wants, but if the GLSL structure contains one of them, then the shader compiler will 
    // expect the buffer that contains those structures to be 16-byte aligned, as if it were an 
    // entire array of the things.  Th Particle structure has a couple vec4's, so it is under 
    // this restriction and the CPU-side structure must be padded to a 16-byte alignment.  If 
    // there are only primitives in the structure definition, like this BvhNode structure, then 
    // no padding necessary.
};

