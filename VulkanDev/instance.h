#pragma once
#include "config.h"

//namespace for creation functions/definitions etc.
namespace vkInit {
	
	bool supported(std::vector<const char*>& extensions, std::vector<const char*>& layers, bool debug);

	vk::Instance make_instance(bool debug, const char* applicationName);
}
