#include "vertex_menagerie.h"

VertexMenagerie::VertexMenagerie() {
	indexOffset = 0;
}

void VertexMenagerie::consume(meshTypes type, std::vector<float>& vertexData, std::vector<uint32_t>& indexData) {

	int indexCount = static_cast<int>(indexData.size());
	int vertexCount = static_cast<int>(vertexData.size() / 7);
	int lastIndex = static_cast<int>(indexLump.size());

	firstIndices.insert(std::make_pair(type, lastIndex));
	indexCounts.insert(std::make_pair(type, indexCount));

	for (float attribute :vertexData)
	{
		vertexlump.push_back(attribute);
	}

	for (uint32_t index : indexData)
	{
		indexLump.push_back(index + indexOffset);
	}

	indexOffset += vertexCount;
}

void VertexMenagerie::finalize(vertexBufferFinalizationChunk finalizationChunk) {
	this->logicDevice = finalizationChunk.logicalDevice;

	BufferInputChunk inputChunk;
	inputChunk.logicalDevice = finalizationChunk.logicalDevice;
	inputChunk.physicalDevice = finalizationChunk.physicalDevice;
	inputChunk.size = sizeof(float) * vertexlump.size();
	inputChunk.usage = vk::BufferUsageFlagBits::eTransferSrc;
	inputChunk.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
	Buffer stagingBuffer = vkUtils::createBuffer(inputChunk);

	void* memoryLocation = logicDevice.mapMemory(stagingBuffer.bufferMemory, 0, inputChunk.size);
	memcpy(memoryLocation, vertexlump.data(), inputChunk.size);
	logicDevice.unmapMemory(stagingBuffer.bufferMemory);

	inputChunk.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
	inputChunk.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;

	vertexBuffer = vkUtils::createBuffer(inputChunk);

	vkUtils::copyBuffer(stagingBuffer, vertexBuffer, inputChunk.size, finalizationChunk.queue, finalizationChunk.commandBuffer);

	logicDevice.destroyBuffer(stagingBuffer.buffer);
	logicDevice.freeMemory(stagingBuffer.bufferMemory);

	//make a staging buffer for indices
	inputChunk.size = sizeof(uint32_t) * indexLump.size();
	inputChunk.usage = vk::BufferUsageFlagBits::eTransferSrc;
	inputChunk.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
	stagingBuffer = vkUtils::createBuffer(inputChunk);

	//fill it with index data
	memoryLocation = logicDevice.mapMemory(stagingBuffer.bufferMemory, 0, inputChunk.size);
	memcpy(memoryLocation, indexLump.data(), inputChunk.size);
	logicDevice.unmapMemory(stagingBuffer.bufferMemory);

	//make the index buffer
	inputChunk.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;
	inputChunk.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
	indexBuffer = vkUtils::createBuffer(inputChunk);

	//copy to it
	vkUtils::copyBuffer(stagingBuffer, indexBuffer, inputChunk.size, finalizationChunk.queue, finalizationChunk.commandBuffer);

	//destroy staging buffer
	logicDevice.destroyBuffer(stagingBuffer.buffer);
	logicDevice.freeMemory(stagingBuffer.bufferMemory);

}

VertexMenagerie::~VertexMenagerie() {

	//destroy vertex buffer
	logicDevice.destroyBuffer(vertexBuffer.buffer);
	logicDevice.freeMemory(vertexBuffer.bufferMemory);

	//destroy index buffer
	logicDevice.destroyBuffer(indexBuffer.buffer);
	logicDevice.freeMemory(indexBuffer.bufferMemory);
}