#pragma once
#include "config.h"
#include "memory.h"

struct vertexBufferFinalizationChunk {
	vk::Device logicalDevice;
	vk::PhysicalDevice physicalDevice;
	vk::CommandBuffer commandBuffer;
	vk::Queue queue;
};

class VertexMenagerie {
public:
	VertexMenagerie();
	~VertexMenagerie();
	void consume(meshTypes type, std::vector<float>& vertexData, std::vector<uint32_t>& indexData);
	void finalize(vertexBufferFinalizationChunk finalizationChunk);
	Buffer vertexBuffer, indexBuffer;
	std::unordered_map<meshTypes, int> firstIndices;
	std::unordered_map<meshTypes, int> indexCounts;

private:
	int indexOffset;
	vk::Device logicDevice;
	std::vector<float> vertexlump;
	std::vector<uint32_t> indexLump;
};