#pragma once
#include "config.h"
#include "memory.h"

/*
	Holds a vertex buffer for a triangle mesh.
*/
class TriangleMesh {
public:
	TriangleMesh(vk::Device logicalDevice, vk::PhysicalDevice phyiscalDevice);
	~TriangleMesh();
	Buffer vertexBuffer;

private:
	vk::Device logicalDevice;
};