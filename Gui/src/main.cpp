/**
 * @file main.cpp
 * @brief Entry point for zappy_gui.
 * @details Responsibility:
 *   1. Parse the mandatory -p \<port\> and -h \<host\> arguments from argv.
 *   2. Construct the three top-level subsystems (NetworkClient, WorldState, Renderer).
 *   3. Connect to the server (NetworkClient::connect()).
 *   4. Open a GLFW window and run the event loop until the user presses ESC
 *      or closes the window, draining the network message queue each frame.
 *
 *   Error handling: every ZappyException is caught at the top level and logged
 *   via spdlog before returning EXIT_FAILURE. This ensures the user always sees
 *   a human-readable error message rather than an uncaught-exception crash.
 */

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include "exceptions.hpp"
#include "network/NetworkClient.hpp"
#include "renderer/Renderer.hpp"
#include "world/WorldState.hpp"

/**
 * @brief Parsed command-line arguments.
 * @details Plain data struct — no invariants, no methods. Filled by parseArgs()
 *          and passed to the NetworkClient constructor.
 */
struct Args {
    std::string host; ///< Hostname or IP of the zappy_server.
    uint16_t    port; ///< TCP port the server is listening on.
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
 *          argv[i] is a raw C string; string_view is non-owning — it is safe here
 *          because argv outlives the function call.
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

/**
 * @brief RAII guard that calls glfwTerminate() on destruction.
 * @details Lifetime: created immediately after glfwInit() succeeds; destroyed when
 *          it goes out of scope (either normally or via exception unwinding).
 */
struct GlfwTerminateGuard {
    /**
     * @brief Default constructor — no action needed on creation.
     */
    GlfwTerminateGuard()  = default;

    /**
     * @brief Calls glfwTerminate() to release all GLFW resources.
     */
    ~GlfwTerminateGuard() { glfwTerminate(); }

    GlfwTerminateGuard(const GlfwTerminateGuard&)            = delete; ///< Non-copyable.
    GlfwTerminateGuard& operator=(const GlfwTerminateGuard&) = delete; ///< Non-copyable.
    GlfwTerminateGuard(GlfwTerminateGuard&&)                 = delete; ///< Non-movable.
    GlfwTerminateGuard& operator=(GlfwTerminateGuard&&)      = delete; ///< Non-movable.
};

/**
 * @brief Application entry point. Parses arguments, creates subsystems, and runs the event loop.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return EXIT_SUCCESS on clean exit, EXIT_FAILURE on any error.
 */
int main(int argc, char* argv[])
{
    try {
        // ── Parse arguments ────────────────────────────────────────────────
        const Args args = parseArgs(argc, argv);
        spdlog::info("zappy_gui starting — server: {}:{}", args.host, args.port);

        // ── Construct subsystems ───────────────────────────────────────────
        // Renderer is constructed later, after the GLFW window exists.
        // It needs the window handle to create the Vulkan surface.
        auto world   = std::make_unique<WorldState>();
        auto network = std::make_unique<NetworkClient>(args.host, args.port);

        // ── Connect to server ─────────────────────────────────────────────
        //
        // connect() opens the TCP socket, completes the WELCOME/GRAPHIC handshake,
        // sends bootstrap queries (msz, mct, tna, sgt), and starts the recv thread.
        // If any step fails it throws NetworkConnectException, which propagates to
        // the top-level catch below.
        network->connect();
        spdlog::info("NetworkClient connected and recv thread running.");

        // ── GLFW window setup ──────────────────────────────────────────────
        if (!glfwInit()) {
            throw GlfwInitException("glfwInit failed — is a display available?");
        }

        // GlfwTerminateGuard: calls glfwTerminate() automatically when it goes out of scope.
        GlfwTerminateGuard glfwGuard;

        // GLFW_NO_API: tell GLFW we are not using OpenGL.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // GLFW_RESIZABLE OFF for now — swapchain resize is handled in the renderer feature.
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        // The raw GLFWwindow* from glfwCreateWindow is passed directly to the
        // unique_ptr constructor — it is never stored in a member or returned.
        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window(
            glfwCreateWindow(
                1280, 720,
                "Zappy GUI",
                nullptr,   // monitor = nullptr → windowed mode
                nullptr),  // share = nullptr → no OpenGL context sharing
            glfwDestroyWindow);

        if (!window) {
            throw GlfwWindowException("glfwCreateWindow failed — check display / driver");
        }

        spdlog::info("Window created (1280x720). Press ESC to exit.");

        // Renderer is constructed after the GLFW window because it needs window.get()
        // to create the Vulkan surface (VkSurfaceKHR). The window pointer is used only
        // at C API call sites during construction and is not stored as a member.
        // Each call to drawFrame() passes window.get() again for GLFW input polling.
        // The window unique_ptr is declared before the Renderer in this scope, so the
        // window is guaranteed to outlive the Renderer (C++ destroys in reverse order).
        auto renderer = std::make_unique<Renderer>(window.get());
        spdlog::info("Renderer initialised.");

        // ── Event loop ────────────────────────────────────────────────────
        //
        // glfwWindowShouldClose returns true when:
        //   a) the user clicks the OS close button, or
        //   b) we call glfwSetWindowShouldClose(window.get(), GLFW_TRUE).
        //
        // glfwPollEvents processes all pending OS events (keyboard, mouse, resize,
        // close button) and fires the registered callbacks. Without this call the
        // window would appear frozen to the OS.
        while (!glfwWindowShouldClose(window.get())) {
            glfwPollEvents();

            // ESC closes the window cleanly — sets the should-close flag so the
            // loop exits on the next iteration rather than breaking out directly.
            // This lets any registered close callbacks run normally.
            if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
            }

            // ── Drain the network message queue ───────────────────────────
            //
            // poll() returns one ServerMessage per call and std::nullopt when
            // the queue is empty. We drain everything accumulated since the last
            // frame. Each message is applied to WorldState which updates all
            // game-state fields (tiles, players, eggs, time unit, game over).
            while (auto msg = network->poll()) {
                world->apply(*msg);
            }

            // If the recv thread set the error flag, convert it to an exception
            // here on the main thread (exceptions cannot cross thread boundaries).
            if (network->hasError()) {
                throw NetworkRecvException("server disconnected unexpectedly during run");
            }

            // drawFrame(): acquire a swapchain image, record and submit draw commands,
            // then present the rendered image to the display. The window pointer is
            // passed each frame for GLFW input polling — it is not stored in the Renderer.
            renderer->drawFrame(window.get());
        }

        spdlog::info("zappy_gui exiting cleanly.");
        return EXIT_SUCCESS;

    } catch (const ZappyException& e) {
        // All expected application errors arrive here as ZappyException subclasses.
        // spdlog::error writes to stderr with the ERROR level prefix.
        spdlog::error("Fatal error: {}", e.what());
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        // Catch anything unexpected (e.g. std::bad_alloc) to prevent a bare
        // terminate() with no message.
        spdlog::error("Unexpected error: {}", e.what());
        return EXIT_FAILURE;
    }
}
