// Vulkan Renderer.

#define GLFW_INCLUDE_VULKAN

#include <iostream>
#include <stdexcept>

#include "core/engine.h"

int main(int argc, char* argv[])
{
	Engine engine;

	try
	{
		engine.initialize();
		engine.run();
		engine.cleanUp();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
