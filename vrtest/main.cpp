#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>

#include "PresentationEngine.h"
#include "GraphicsDevice.h"
#include "Renderer.h"

class VRTestApp {
public:
    void run() {
        init();
        mainLoop();
        cleanup();
    }

private:
    PresentationEngine* present = nullptr;
    GraphicsDevice* graphics_device = nullptr;
    Renderer* renderer = nullptr;

    void init() {
        present = new PresentationEngine(1024, 768, nullptr, "vrtest");
        graphics_device = new GraphicsDevice(present, nullptr);
        renderer = new Renderer(graphics_device, present, nullptr);
        renderer->createCommandBuffer();
    }
 
    void mainLoop() {
        while (!present->shouldExit()) {
            present->pollEvents();
            renderer->drawFrame();
        }

        vkDeviceWaitIdle(graphics_device->device());
    }

    void cleanup() {
        delete renderer;
        delete present;
        delete graphics_device;
    }
};

int main() {
    VRTestApp app;

    try {
        app.run();
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        getchar();
        return EXIT_FAILURE;
    }

    std::cout << "Press enter to exit.";
    getchar();

    return EXIT_SUCCESS;
}
