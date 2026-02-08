#include "SDL3/SDL_main.h"
#include <SDL3/SDL_video.h>
#ifdef nullptr
#undef nullptr
#endif

#include <iostream>
#include <string>
#include <boost/filesystem.hpp>
#include <unistd.h>
#include <cstdlib>

#include "Application.h"
#include "FunctionMarshaller.h"
#include "InitializationError.h"

#include "v8datamodel/ContentProvider.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

LOGGROUP(HangDetection)
LOGGROUP(RobloxWndInit)

int main(int argc, char* argv[])
{
    // This was compiled with sse2 support.  This should be first as any floats
    // will cause issues.
    if (!G3D::System::hasSSE2())
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ROBLOX", "This platform lacks SSE2 support.", NULL);
        return 1;
    }

    // force X11
    // #if defined(__linux__)
    //     setenv("SDL_VIDEODRIVER", "x11", 1);
    // #endif

    // so that our custom cursor doesnt scale up unexpectedly.
    SDL_SetHint(SDL_HINT_MOUSE_DPI_SCALE_CURSORS, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    RBX::Application app;

    // Parse command line arguments first to set paths
    if (!app.ParseArguments(argc, argv))
        return 1;

    // Get module path from executable path
    boost::filesystem::path exePath;
    char buffer[1024];
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, buffer, sizeof(buffer));
    if (len > 0) {
        exePath = boost::filesystem::path(buffer);
    } else {
        exePath = boost::filesystem::path(argv[0]);
    }
#else
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer)-1);
    if (len != -1) {
        buffer[len] = '\0';
        exePath = boost::filesystem::path(buffer);
    } else {
        exePath = boost::filesystem::path(argv[0]);
    }
#endif
    std::string modulePath = exePath.string();

    if (!app.LoadAppSettings(modulePath))
        return 1;

    // need client settings here before we create window
    std::string clientSettingsString;
    FetchClientSettingsData(CLIENT_APP_SETTINGS_STRING, CLIENT_SETTINGS_API_KEY, &clientSettingsString);
    // Apply client settings
    LoadClientSettingsFromString(CLIENT_APP_SETTINGS_STRING, clientSettingsString, &RBX::ClientAppSettings::singleton());

    std::vector<SDL_WindowFlags> windowFlagsToTry;
    #ifdef __linux__
    windowFlagsToTry = {SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY};
    #else
    windowFlagsToTry = {SDL_WINDOW_HIGH_PIXEL_DENSITY, SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY};
    #endif

    SDL_Window* window = nullptr;
    bool initialized = false;
    for (auto flag : windowFlagsToTry) {
        window = SDL_CreateWindow("ROBLOX", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE | flag);
        if (!window) {
            std::cerr << "SDL_CreateWindow with flag " << flag << " failed: " << SDL_GetError() << std::endl;
            continue;
        }

        SDL_SetWindowMinimumSize(window, 800, 600);

        try {
            if (app.Initialize(window, modulePath)) {
                initialized = true;
                break;
            } else {
                SDL_DestroyWindow(window);
                window = nullptr;
            }
        } catch (const RBX::graphics_initialization_error& e) {
            const char* const errorMessage = e.what();
            FASTLOG2(FLog::RobloxWndInit, "Graphics initialization failed with flag %d. User message = %s", flag, errorMessage);
            SDL_DestroyWindow(window);
            window = nullptr;
        } catch (const RBX::initialization_error& e) {
            // Other initialization error, re-throw
            throw;
        }
    }

    if (!initialized) {
        std::cerr << "Failed to initialize with any window configuration" << std::endl;
        SDL_Quit();
        return 1;
    }

    // Timer for keep alive (simulate with a thread or SDL timer)
    // For simplicity, use a simple loop with SDL_Delay for now

    SDL_Event event;
    bool running = true;
    Uint32 lastKeepAlive = SDL_GetTicks();

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                app.AboutToShutdown();
                running = false;
                break;
            default:
                app.HandleSDLEvent(&event);
                break;
            }
            // Handle FunctionMarshaller events
            if (event.type >= SDL_EVENT_USER)
            {
                RBX::FunctionMarshaller::HandleSDLEvent(event);
            }
        }

        // Keep alive check every 10 seconds
        if (SDL_GetTicks() - lastKeepAlive > 10000)
        {
            if (MainLogManager* manager = LogManager::getMainLogManager())
            {
                FASTLOG(FLog::HangDetection, "SDLPlayer: keep alive");
                manager->NotifyFGThreadAlive();
            }
            lastKeepAlive = SDL_GetTicks();
        }
        //SDL_Delay(10); // Small delay to prevent busy loop
    }

    // at this point, threads will still be around.
    // On Linux, no VirtualProtect equivalent, skip or handle differently

    app.Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
