#include "Application.h"
#include "Camera.h"
#include "Input.h"
#include "TaskDispatcher.h"
#include "Transform.h"

#include "Common/Profiler.h"
#include "Common/RenderingHandler.hpp"
#include "Common/IImgui.h"
#include "Common/IWindow.h"
#include "Common/IShader.h"
#include "Common/ISampler.h"
#include "Common/IScene.h"
#include "Common/IRenderer.h"
#include "Common/IShader.h"
#include "Common/ITexture2D.h"
#include "Common/ITextureCube.h"
#include "Common/IInputHandler.h"
#include "Common/IGraphicsContext.h"

#include <thread>
#include <chrono>
#include <fstream>

#include <imgui/imgui.h>

#include <glm/gtc/type_ptr.hpp>

#include "LightSetup.h"

#include "Vulkan/CommandPoolVK.h"
#include "Vulkan/DescriptorSetLayoutVK.h"
#include "Vulkan/GraphicsContextVK.h"
#include "Vulkan/RenderPassVK.h"
#include "Vulkan/SwapChainVK.h"

#ifdef max
	#undef max
#endif

#ifdef min
	#undef min
#endif

Application* Application::s_pInstance = nullptr;

constexpr bool	RENDERING_ENABLED = false;
constexpr bool	FORCE_RAY_TRACING_OFF	= true;
constexpr bool	HIGH_RESOLUTION_SPHERE	= false;
constexpr float CAMERA_PAN_LENGTH		= 10.0f;

Application::Application()
	: m_pWindow(nullptr),
	m_pContext(nullptr),
	m_pRenderingHandler(nullptr),
	m_pImgui(nullptr),
	m_pScene(nullptr),
	m_pInputHandler(nullptr),
	m_Camera(),
	m_IsRunning(false),
	m_UpdateCamera(false),
	m_pParticleRenderer(nullptr),
	m_pParticleTexture(nullptr),
	m_pParticleEmitterHandler(nullptr),
	m_NewEmitterInfo(),
	m_CurrentEmitterIdx(0),
	m_CreatingEmitter(false),
	m_KeyInputEnabled(false),
	m_CurrentFrame(0)
{
	ASSERT(s_pInstance == nullptr);
	s_pInstance = this;
}

Application::~Application()
{
	s_pInstance = nullptr;
}

void Application::init(size_t emitterCount, size_t frameCount, bool useMultipleQueues, float particleCount)
{
	LOG("Starting application");
	LOG("Emitters: %d, Frames: %d, Use multiple queues: %d", emitterCount, frameCount, useMultipleQueues);

	m_MaxFrames = frameCount;

	TaskDispatcher::init();

	// Create window
	m_pWindow = IWindow::create("Hello Vulkan", 1440, 900);
	if (m_pWindow)
	{
		m_pWindow->addEventHandler(this);
		m_pWindow->setFullscreenState(false);
	}

	// Create context
	m_pContext = IGraphicsContext::create(m_pWindow, API::VULKAN, useMultipleQueues);

	// Create particlehandler
	m_pParticleEmitterHandler = m_pContext->createParticleEmitterHandler(RENDERING_ENABLED, (uint32_t)frameCount);
	m_pParticleEmitterHandler->initialize(m_pContext, m_pRenderingHandler, &m_Camera);

	// Switch to GPU
	m_pParticleEmitterHandler->toggleComputationDevice();

	ParticleEmitterInfo emitterInfo = {};
	emitterInfo.position			= glm::vec3(0.0f, 0.0f, 0.0f);
	emitterInfo.direction			= glm::normalize(glm::vec3(0.0f, 0.9f, 0.1f));
	emitterInfo.particleSize		= glm::vec2(0.1f, 0.1f);
	emitterInfo.initialSpeed		= 5.5f;
	emitterInfo.particleDuration	= 1.0f;
	emitterInfo.particlesPerSecond	= particleCount;
	emitterInfo.spread				= glm::quarter_pi<float>() / 1.3f;
	emitterInfo.pTexture			= m_pParticleTexture;

	for (size_t emitterNr = 0; emitterNr < emitterCount; emitterNr++) {
		emitterInfo.position.x = (float)emitterNr;
		m_pParticleEmitterHandler->createEmitter(emitterInfo);
	}

	TaskDispatcher::waitForTasks();
}

void Application::run()
{
	GraphicsContextVK* pGraphicsContext	= reinterpret_cast<GraphicsContextVK*>(m_pContext);
	SwapChainVK* pSwapChain				= pGraphicsContext->getSwapChain();
	DeviceVK* pDevice					= pGraphicsContext->getDevice();

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence imageWaitFence = VK_NULL_HANDLE;
	vkCreateFence(pDevice->getDevice(), &fenceInfo, nullptr, &imageWaitFence);

	auto currentTime	= std::chrono::high_resolution_clock::now();
	auto lastTime		= currentTime;

	//HACK to get a non-null deltatime
	std::this_thread::sleep_for(std::chrono::milliseconds(16));

	auto startTime = std::chrono::high_resolution_clock::now();

	while (m_CurrentFrame++ < m_MaxFrames)
	//while (true)
	{
		//pSwapChain->acquireNextImageWaitFence(imageWaitFence);

		lastTime	= currentTime;
		currentTime = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> deltatime = currentTime - lastTime;
		double seconds = deltatime.count() / 1000.0;

		update(seconds);

		pDevice->wait();
		//pSwapChain->presentNoWait();
	}

	reinterpret_cast<GraphicsContextVK*>(m_pContext)->getDevice()->wait();

	auto endTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> totalTime = endTime - startTime;
	double milli = totalTime.count();

	// timestampToMilli is already added
	std::ofstream file;
	file.open("results.txt", std::ios::binary | std::ios::out | std::ios::app | std::ios::ate);

	file.write((char*)(&milli), sizeof(double));

	file.close();
}

void Application::release()
{
	m_pWindow->removeEventHandler(m_pInputHandler);
	m_pWindow->removeEventHandler(m_pImgui);
	m_pWindow->removeEventHandler(this);

	m_pContext->sync();

	SAFEDELETE(m_pRenderingHandler);
	SAFEDELETE(m_pParticleRenderer);
	SAFEDELETE(m_pParticleTexture);
	SAFEDELETE(m_pParticleEmitterHandler);
	SAFEDELETE(m_pImgui);
	SAFEDELETE(m_pScene);

	SAFEDELETE(m_pContext);

	SAFEDELETE(m_pInputHandler);
	Input::setInputHandler(nullptr);

	SAFEDELETE(m_pWindow);

	TaskDispatcher::release();

	LOG("Exiting Application");
}

void Application::onWindowResize(uint32_t width, uint32_t height)
{
	D_LOG("Resize w=%d h%d", width , height);

	if (width != 0 && height != 0)
	{
		if (m_pRenderingHandler)
		{
			m_pRenderingHandler->setViewport((float)width, (float)height, 0.0f, 1.0f, 0.0f, 0.0f);
			m_pRenderingHandler->onWindowResize(width, height);
		}

		m_Camera.setProjection(90.0f, float(width), float(height), 0.01f, 100.0f);
	}
}

void Application::onMouseMove(uint32_t x, uint32_t y)
{
	if (m_UpdateCamera)
	{
		glm::vec2 middlePos = middlePos = glm::vec2(m_pWindow->getClientWidth() / 2.0f, m_pWindow->getClientHeight() / 2.0f);

		float xoffset = middlePos.x - x;
		float yoffset = y - middlePos.y;

		constexpr float sensitivity = 0.25f;
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		glm::vec3 rotation = m_Camera.getRotation();
		rotation += glm::vec3(yoffset, -xoffset, 0.0f);

		m_Camera.setRotation(rotation);
	}
}

void Application::onKeyPressed(EKey key)
{
	//Exit application by pressing escape
	if (key == EKey::KEY_ESCAPE)
	{
		m_IsRunning = false;
	}

	if (m_KeyInputEnabled)
	{
		if (key == EKey::KEY_1)
		{
			m_pWindow->toggleFullscreenState();
		}
		else if (key == EKey::KEY_2)
		{
			m_UpdateCamera = !m_UpdateCamera;
			if (m_UpdateCamera)
			{
				Input::captureMouse(m_pWindow);
			}
			else
			{
				Input::releaseMouse(m_pWindow);
			}
		}
	}
}

void Application::onWindowClose()
{
	D_LOG("Window Closed");
	m_IsRunning = false;
}

Application* Application::get()
{
	return s_pInstance;
}

static glm::vec4 g_Color = glm::vec4(1.0f);

void Application::update(double dt)
{
	m_pParticleEmitterHandler->update((float)dt);
}

void Application::renderUI(double dt)
{
	m_pImgui->begin(dt);

	if (m_ApplicationParameters.IsDirty)
	{
		m_ApplicationParameters.IsDirty = false;
	}

	if (!m_TestParameters.Running)
	{
		// Color picker for mesh
		ImGui::SetNextWindowSize(ImVec2(430, 450), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Color", NULL, ImGuiWindowFlags_NoResize))
		{
			ImGui::ColorPicker4("##picker", glm::value_ptr(g_Color), ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
		}
		ImGui::End();

		// Particle control panel
		ImGui::SetNextWindowSize(ImVec2(420, 210), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Particles", NULL)) {
			ImGui::Text("Toggle Computation Device");
			const char* btnLabel = m_pParticleEmitterHandler->gpuComputed() ? "GPU" : "CPU";
			if (ImGui::Button(btnLabel, ImVec2(40, 25))) {
				m_pParticleEmitterHandler->toggleComputationDevice();
			}

			std::vector<ParticleEmitter*> particleEmitters = m_pParticleEmitterHandler->getParticleEmitters();

			// Emitter creation
			if (ImGui::Button("New emitter")) {
				m_CreatingEmitter = true;
				ParticleEmitter* pLatestEmitter = particleEmitters.back();

				m_NewEmitterInfo = {};
				m_NewEmitterInfo.direction = pLatestEmitter->getDirection();
				m_NewEmitterInfo.initialSpeed = pLatestEmitter->getInitialSpeed();
				m_NewEmitterInfo.particleSize = pLatestEmitter->getParticleSize();
				m_NewEmitterInfo.spread = pLatestEmitter->getSpread();
				m_NewEmitterInfo.particlesPerSecond = pLatestEmitter->getParticlesPerSecond();
				m_NewEmitterInfo.particleDuration = pLatestEmitter->getParticleDuration();
			}

			// Emitter selection
			m_CurrentEmitterIdx = std::min(m_CurrentEmitterIdx, particleEmitters.size() - 1);
			int emitterIdxInt = (int)m_CurrentEmitterIdx;

			if (ImGui::SliderInt("Emitter selection", &emitterIdxInt, 0, int(particleEmitters.size() - 1))) {
				m_CurrentEmitterIdx = (size_t)emitterIdxInt;
			}

			// Get current emitter data
			ParticleEmitter* pEmitter = particleEmitters[m_CurrentEmitterIdx];
			glm::vec3 emitterPos = pEmitter->getPosition();
			glm::vec2 particleSize = pEmitter->getParticleSize();

			glm::vec3 emitterDirection = pEmitter->getDirection();
			float yaw = getYaw(emitterDirection);
			float oldYaw = yaw;
			float pitch = getPitch(emitterDirection);
			float oldPitch = pitch;

			float emitterSpeed = pEmitter->getInitialSpeed();
			float spread = pEmitter->getSpread();

			if (ImGui::SliderFloat3("Position", glm::value_ptr(emitterPos), -10.0f, 10.0f)) {
				pEmitter->setPosition(emitterPos);
			}
			if (ImGui::SliderFloat("Yaw", &yaw, 0.01f - glm::pi<float>(), glm::pi<float>() - 0.01f)) {
				applyYaw(emitterDirection, yaw - oldYaw);
				pEmitter->setDirection(emitterDirection);
			}
			if (ImGui::SliderFloat("Pitch", &pitch, 0.01f - glm::half_pi<float>(), glm::half_pi<float>() - 0.01f)) {
				applyPitch(emitterDirection, pitch - oldPitch);
				pEmitter->setDirection(emitterDirection);
			}
			if (ImGui::SliderFloat3("Direction", glm::value_ptr(emitterDirection), -1.0f, 1.0f)) {
				pEmitter->setDirection(glm::normalize(emitterDirection));
			}
			if (ImGui::SliderFloat2("Size", glm::value_ptr(particleSize), 0.0f, 1.0f)) {
				pEmitter->setParticleSize(particleSize);
			}
			if (ImGui::SliderFloat("Speed", &emitterSpeed, 0.0f, 20.0f)) {
				pEmitter->setInitialSpeed(emitterSpeed);
			}
			if (ImGui::SliderFloat("Spread", &spread, 0.0f, glm::pi<float>())) {
				pEmitter->setSpread(spread);
			}

			ImGui::Text("Particles: %d", pEmitter->getParticleCount());
		}
		ImGui::End();

		// Emitter creation window
		if (m_CreatingEmitter) {
			// Open a new window
			ImGui::SetNextWindowSize(ImVec2(420, 210), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Emitter Creation", NULL)) {
				float yaw = getYaw(m_NewEmitterInfo.direction);
				float oldYaw = yaw;
				float pitch = getPitch(m_NewEmitterInfo.direction);
				float oldPitch = pitch;

				ImGui::SliderFloat3("Position", glm::value_ptr(m_NewEmitterInfo.position), -10.0f, 10.0f);
				if (ImGui::SliderFloat("Yaw", &yaw, 0.01f - glm::pi<float>(), glm::pi<float>() - 0.01f)) {
					applyYaw(m_NewEmitterInfo.direction, yaw - oldYaw);
				}
				if (ImGui::SliderFloat("Pitch", &pitch, 0.01f - glm::half_pi<float>(), glm::half_pi<float>() - 0.01f)) {
					applyPitch(m_NewEmitterInfo.direction, pitch - oldPitch);
				}
				if (ImGui::SliderFloat3("Direction", glm::value_ptr(m_NewEmitterInfo.direction), -1.0f, 1.0f)) {
					m_NewEmitterInfo.direction = glm::normalize(m_NewEmitterInfo.direction);
				}
				ImGui::SliderFloat2("Particle Size", glm::value_ptr(m_NewEmitterInfo.particleSize), 0.0f, 1.0f);
				ImGui::SliderFloat("Speed", &m_NewEmitterInfo.initialSpeed, 0.0f, 20.0f);
				ImGui::SliderFloat("Spread", &m_NewEmitterInfo.spread, 0.0f, glm::pi<float>());
				ImGui::InputFloat("Particle duration", &m_NewEmitterInfo.particleDuration);
				ImGui::InputFloat("Particles per second", &m_NewEmitterInfo.particlesPerSecond);
				ImGui::Text("Emitted particles: %d", int(m_NewEmitterInfo.particlesPerSecond * m_NewEmitterInfo.particleDuration));

				if (ImGui::Button("Create")) {
					m_CreatingEmitter = false;
					m_NewEmitterInfo.pTexture = m_pParticleTexture;

					m_pParticleEmitterHandler->createEmitter(m_NewEmitterInfo);
				}
				if (ImGui::Button("Cancel")) {
					m_CreatingEmitter = false;
				}
				ImGui::End();
			}
		}

		// Draw profiler UI
		ImGui::SetNextWindowSize(ImVec2(430, 450), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Profiler", NULL))
		{
			m_pParticleEmitterHandler->drawProfilerUI();
			m_pRenderingHandler->drawProfilerUI();
		}
		ImGui::End();

		// Draw Scene UI
		m_pScene->renderUI();
	}

	// Draw Application UI
	ImGui::SetNextWindowSize(ImVec2(430, 450), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Application", NULL))
	{
		if (!m_TestParameters.Running)
		{
			//ImGui::Checkbox("Camera Spline Enabled", &m_CameraSplineEnabled);
			//ImGui::SliderFloat("Camera Timer", &m_CameraSplineTimer, 0.0f, m_CameraPositionSpline->getMaxT());

			//Input Parameters
			if (ImGui::Button("Toggle Key Input"))
			{
				m_KeyInputEnabled = !m_KeyInputEnabled;
			}
			ImGui::SameLine();
			if (m_KeyInputEnabled)
				ImGui::Text("Key Input Enabled");
			else
				ImGui::Text("Key Input Disabled");

			ImGui::NewLine();

			//Test Parameters
			ImGui::InputText("Test Name", m_TestParameters.TestName, 256);
			ImGui::SliderInt("Number of Test Rounds", &m_TestParameters.NumRounds, 1, 5);

			if (ImGui::Button("Start Test"))
			{
				m_TestParameters.Running = true;
				m_TestParameters.FrameTimeSum = 0.0f;
				m_TestParameters.FrameCount = 0.0f;
				m_TestParameters.AverageFrametime = 0.0f;
				m_TestParameters.WorstFrametime = std::numeric_limits<float>::min();
				m_TestParameters.BestFrametime = std::numeric_limits<float>::max();
				m_TestParameters.CurrentRound = 0;
			}

			ImGui::Text("Previous Test: %s : %d ", m_TestParameters.TestName, m_TestParameters.NumRounds);
			ImGui::Text("Average Frametime: %f", m_TestParameters.AverageFrametime);
			ImGui::Text("Worst Frametime: %f", m_TestParameters.WorstFrametime);
			ImGui::Text("Best Frametime: %f", m_TestParameters.BestFrametime);
			ImGui::Text("Frame count: %f", m_TestParameters.FrameCount);
		}
		else
		{
			if (ImGui::Button("Stop Test"))
			{
				m_TestParameters.Running = false;
			}

			ImGui::Text("Round: %d / %d", m_TestParameters.CurrentRound, m_TestParameters.NumRounds);
			ImGui::Text("Average Frametime: %f", m_TestParameters.AverageFrametime);
			ImGui::Text("Worst Frametime: %f", m_TestParameters.WorstFrametime);
			ImGui::Text("Best Frametime: %f", m_TestParameters.BestFrametime);
			ImGui::Text("Frame Count: %f", m_TestParameters.FrameCount);
		}

	}
	ImGui::End();

	m_pImgui->end();
}

void Application::render(double dt)
{
	UNREFERENCED_PARAMETER(dt);
	m_pRenderingHandler->render(m_pScene);
}

void Application::testFinished()
{
	m_TestParameters.Running = false;

	sanitizeString(m_TestParameters.TestName, 256);

	std::ofstream fileStream;
	fileStream.open("Results/test_" + std::string(m_TestParameters.TestName) + ".txt");

	if (fileStream.is_open())
	{
		fileStream << "Avg. FT\tWorst FT\tBest FT\tFrame Count" << std::endl;
		fileStream << m_TestParameters.AverageFrametime << "\t";
		fileStream << m_TestParameters.WorstFrametime << "\t";
		fileStream << m_TestParameters.BestFrametime << "\t";
		fileStream << (uint32_t)m_TestParameters.FrameCount;
	}

	fileStream.close();
}

void Application::sanitizeString(char string[], uint32_t numCharacters)
{
	static std::string illegalChars = "\\/:?\"<>|";
	for (uint32_t i = 0; i < numCharacters; i++)
	{
		if (illegalChars.find(string[i]) != std::string::npos)
		{
			string[i] = '_';
		}
	}
}
