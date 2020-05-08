#include "Common/Debug.h"
#include "Core/Application.h"

// Arg 0: Emitter count
// Arg 1: Frame count
// Arg 2: Enable/Disable multiple queues (1 or 0)
int main(int argc, const char* argv[])
{
	size_t emitterCount = 2;
	size_t frameCount = 3;
	bool multipleQueues = true;

	if (argc > 1) {
		emitterCount = std::stoi(argv[1]);
	}

	if (argc > 2) {
		frameCount = std::stoi(argv[2]);
	}

	if (argc > 3) {
		multipleQueues = std::stoi(argv[3]);
	}

#if defined(_DEBUG) && defined(_WIN32)
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Application app;
	app.init(emitterCount, frameCount, multipleQueues);
	app.run();
	app.release();
	return 0;
}
