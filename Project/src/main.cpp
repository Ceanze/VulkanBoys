#include "Common/Debug.h"
#include "Core/Application.h"

// Arg 1: Emitter count
// Arg 2: Frame count
// Arg 3: Enable/Disable multiple queues (1 or 0)
// Arg 4: Particles per emitter
// Arg 5: Use multiple queue families
// Arg 6: Only use async compute queue family
int main(int argc, const char* argv[])
{
	size_t emitterCount = 2;
	size_t frameCount = 3;
	float particleCount = 100.0f;
	bool multipleQueues = true;
	bool multipleFamilies = false;
	bool useComputeQueue = false;

	if (argc > 1) {
		emitterCount = std::stoi(argv[1]);
	}

	if (argc > 2) {
		frameCount = std::stoi(argv[2]);
	}

	if (argc > 3) {
		multipleQueues = std::stoi(argv[3]);
	}

	if (argc > 4) {
		particleCount = std::stof(argv[4]);
	}

	if (argc > 5) {
		multipleFamilies = std::stoi(argv[5]);
	}

	if (argc > 6) {
		useComputeQueue = std::stoi(argv[6]);
	}

#if defined(_DEBUG) && defined(_WIN32)
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Application app;
	app.init(emitterCount, frameCount, multipleQueues, particleCount, multipleFamilies, useComputeQueue);
	app.run();
	app.release();
	return 0;
}
