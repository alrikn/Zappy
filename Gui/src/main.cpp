/**
 * @file main.cpp
 * @brief Entry point for zappy_gui.
 * @details Responsibility:
 *   1. Parse the mandatory -p \<port\> and -h \<host\> arguments from argv.
 *   2. Construct the three top-level subsystems (NetworkClient, WorldState, Renderer).
 *   3. Open a GLFW window and run the event loop until the user presses ESC
 *      or closes the window.
 *
 *   What is not here yet:
 *   - Vulkan initialisation (renderer bootstrap feature).
 *   - TCP connection (network feature).
 *   - World state parsing (world feature).
 *   The three subsystem objects are stubs that compile but do nothing.
 */

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

// GLFW_NO_API is set via glfwWindowHint before window creation so GLFW does
// NOT try to create an OpenGL context. We are Vulkan-only; an OpenGL context
// would conflict with later Vulkan surface creation and waste driver resources.
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

// Forward-declaration headers for the three top-level subsystems.
// These are stubs for now — they compile but contain no logic.
#include "network/NetworkClient.hpp"
#include "renderer/Renderer.hpp"
#include "world/WorldState.hpp"

// ─── CLI argument parsing ─────────────────────────────────────────────────────

/**
 * @brief Parsed command-line arguments.
 */
struct Args {
    std::string host;  ///< Hostname or IP of the zappy_server
    uint16_t    port;  ///< TCP port the server is listening on
};

/**
 * @brief Write the expected command-line syntax to stderr and exit.
 * @param prog argv[0] — the name of the binary as invoked.
 */
static void printUsage(std::string_view prog)
{
    std::cerr << "USAGE: " << prog << " -p port -h machine\n"
              << "  -p port     TCP port the zappy_server is listening on\n"
              << "  -h machine  Hostname or IP address of the server\n";
    std::exit(EXIT_FAILURE);
}

/**
 * @brief Extract -p and -h from the command line.
 * @details Exits with a usage message if either flag is missing or if -p is not a
 *          valid integer in [1, 65535].
 *
 *          Why std::string_view for comparisons: argv[i] is a char* (C API). Wrapping
 *          it in string_view avoids a heap allocation while allowing == comparison
 *          against string literals. string_view is non-owning — it is safe here because
 *          argv outlives the function call.
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @return Filled Args struct with host and port.
 */
static Args parseArgs(int argc, char* argv[])
{
    std::string host;
    int         portRaw = -1;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};

        if (arg == "-p" && i + 1 < argc) {
            try {
                portRaw = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "error: -p requires a numeric port\n";
                printUsage(argv[0]);
            }
        } else if (arg == "-h" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--help" || arg == "-help") {
            printUsage(argv[0]);
        }
    }

    if (host.empty()) {
        std::cerr << "error: -h machine is required\n";
        printUsage(argv[0]);
    }
    if (portRaw < 1 || portRaw > 65535) {
        std::cerr << "error: -p port must be in [1, 65535]\n";
        printUsage(argv[0]);
    }

    return Args{
        .host = std::move(host),
        .port = static_cast<uint16_t>(portRaw),
    };
}

// ─── Entry point ──────────────────────────────────────────────────────────────

/**
 * @brief Application entry point. Parses arguments, creates subsystems, and runs the event loop.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return EXIT_SUCCESS on clean exit.
 */
int main(int argc, char* argv[])
{
    // ── Parse arguments ────────────────────────────────────────────────────
    const Args args = parseArgs(argc, argv);
    spdlog::info("zappy_gui starting — server: {}:{}", args.host, args.port);

    auto world    = std::make_unique<WorldState>();
    auto network  = std::make_unique<NetworkClient>(args.host, args.port);
    auto renderer = std::make_unique<Renderer>();

    // ── GLFW window setup ──────────────────────────────────────────────────
    if (!glfwInit()) {
        throw std::runtime_error("glfwInit failed — no display available?");
    }

    // GLFW_NO_API: tell GLFW we are NOT using OpenGL. Without this hint,
    // GLFW would call wglCreateContext / glXCreateContext to make an OpenGL
    // context, which wastes driver memory and prevents Vulkan surface creation.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // GLFW_RESIZABLE OFF for now — the swapchain resize path is non-trivial
    // and will be handled in the renderer feature to avoid crashing on resize.
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // Raw GLFWwindow* — GLFW is a C library that manages window lifetime
    // through glfwCreateWindow / glfwDestroyWindow. We must call these
    // manually.
    GLFWwindow* window = glfwCreateWindow(
        1280, 720,
        "Zappy GUI",
        nullptr,   // monitor = nullptr → windowed mode
        nullptr);  // share = nullptr → no OpenGL context sharing
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }

    spdlog::info("Window created (1280x720). Press ESC to exit.");

    // ── Event loop ────────────────────────────────────────────────────────
    //
    // glfwWindowShouldClose returns true when:
    //   a) the user clicks the OS close button, or
    //   b) we call glfwSetWindowShouldClose(window, GLFW_TRUE).
    //
    // glfwPollEvents processes all pending OS events (keyboard, mouse, resize,
    // close button) and fires the registered callbacks. Without this call the
    // window would appear frozen to the OS.
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // ESC closes the window cleanly — sets the should-close flag so the
        // loop exits on the next iteration rather than breaking out directly.
        // This lets any registered close callbacks run normally.
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Renderer stub: no-op until Vulkan rendering is implemented.
        renderer->drawFrame();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────
    //
    // Destroy the window before glfwTerminate — the window must be destroyed
    // while the GLFW library is still active.
    glfwDestroyWindow(window);

    // glfwTerminate releases all GLFW resources (event queues, display
    // connections on Linux/X11 or Wayland). Must be called after all windows
    // are destroyed.
    glfwTerminate();

    spdlog::info("zappy_gui exiting cleanly.");
    return EXIT_SUCCESS;
}
