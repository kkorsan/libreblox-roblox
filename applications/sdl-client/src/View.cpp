#include "SDL3/SDL.h"
#ifdef nullptr
#undef nullptr
#endif
#include <iostream>
#include <fstream>
#include <string>

#include "v8datamodel/DebugSettings.h"
#include "v8datamodel/GameBasicSettings.h"
#include "script/ScriptContext.h"
#include "FunctionMarshaller.h"
#include "GameVerbs.h"
#include "LogManager.h"
#include "rbx/Tasks/Coordinator.h"
#include "RenderJob.h"
#include "RenderSettingsItem.h"
#include "util/ScopedAssign.h"
#include "v8datamodel/Game.h"
#include "v8datamodel/UserController.h"
#include "InitializationError.h"
#include "View.h"
#include "format_string.h"

#include "util/RobloxGoogleAnalytics.h"
#include "rbx/SystemUtil.h"

LOGGROUP(PlayerShutdownLuaTimeoutSeconds)
LOGGROUP(RobloxWndInit)
LOGGROUP(Graphics)
FASTFLAGVARIABLE(DirectX11Enable, false)
FASTFLAGVARIABLE(GraphicsReportingInitErrorsToGAEnabled,true)
FASTFLAGVARIABLE(UseNewAppBridgeInputWindows, false)

DYNAMIC_FASTFLAGVARIABLE(FullscreenRefocusingFix, false)


namespace {
    // Removed Windows-specific SetFocusWrapper
}

namespace RBX {

static const char* kSavedScreenSizeRegistryKey =
	"HKEY_CURRENT_USER\\Software\\Roblox Corporation\\ROBLOX\\Settings\\RobloxPlayerV4WindowSizeAndPosition";

View::View(void* window)
	: fullscreen(false)
	, desireFullscreen(false)
	, marshaller(NULL)
{
    desireFullscreen = RBX::GameBasicSettings::singleton().getFullScreen();
    context.window = window;
    marshaller = FunctionMarshaller::GetWindow();

    initializeView();
}

View::~View()
{
    RBXASSERT(!this->game && "Call Stop() before shutting down!");
    view.reset();

    if (marshaller)
        FunctionMarshaller::ReleaseWindow(marshaller);
}


void View::AboutToShutdown()
{
	// Save window settings to file
	int w, h, x, y;
	SDL_GetWindowSize(GetWindow(), &w, &h);
	SDL_GetWindowPosition(GetWindow(), &x, &y);
	std::ofstream out(std::string(getenv("HOME") ? getenv("HOME") : "/tmp") + "/.roblox/window_settings");
	if (out) {
		out << x << " " << y << " " << w << " " << h << " " << (fullscreen ? 1 : 0);
		out.close();
	}
	if (userInput) {
		userInput->unAcquireMousePublic();
	}
}

// Removed rememberWindowSettings and saveWindowSettings, handled in AboutToShutdown

void View::initializeView()
{
    ViewBase::InitPluginModules();

    std::vector<CRenderSettings::GraphicsMode> modes;

    RBX::CRenderSettings::GraphicsMode graphicsMode = CRenderSettingsItem::singleton().getLatchedGraphicsMode();
    switch(graphicsMode)
    {
    case CRenderSettings::NoGraphics:
        break;
    case CRenderSettings::OpenGL:
    // case CRenderSettings::Direct3D9:
    // case CRenderSettings::Direct3D11:
        modes.push_back(graphicsMode);
        break;
    default:
        {
            Uint32 flags = SDL_GetWindowFlags((SDL_Window*)context.window);
            if (flags & SDL_WINDOW_OPENGL) {
                modes.push_back(CRenderSettings::OpenGL);
            }
            // else {
            //     if (FFlag::DirectX11Enable)
            //         modes.push_back(CRenderSettings::Direct3D11);
            //     modes.push_back(CRenderSettings::Direct3D9);
            // }
        }
        break;
    }

	std::string lastMessage;
	bool success = false;
	size_t modei = 0;
	while(!success && modei < modes.size())
	{
		graphicsMode = modes[modei];
		try
		{
			view.reset(ViewBase::CreateView(graphicsMode, &context, &CRenderSettingsItem::singleton()));
			view->initResources();
			success = true;
		}
		catch (std::exception& e)
		{
			RBX::StandardOut::singleton()->printf(RBX::MESSAGE_WARNING, "Mode %d failed: \"%s\"", graphicsMode, e.what());
			lastMessage += e.what();
            lastMessage += " | ";

			modei++;
			if(modei < modes.size())
			{
				RBX::StandardOut::singleton()->printf(RBX::MESSAGE_WARNING, "Trying mode %d...", modes[modei]);
			}
		}
	}

	if(!success)
	{
        if (FFlag::GraphicsReportingInitErrorsToGAEnabled)
        {{{
            lastMessage += SystemUtil::getGPUMake() + " | ";
            lastMessage += SystemUtil::osVer();
            const char* label = modes.size()? "GraphicsInitError" : "GraphicsInitErrorNoModes";
            RobloxGoogleAnalytics::trackEventWithoutThrottling( GA_CATEGORY_GAME, label , lastMessage.c_str(), 0 );
        }}}

        // Removed Windows registry write
		throw graphics_initialization_error(
			"Your graphics drivers seem to be too old for ROBLOX to use.\n\n"
			"Visit http://www.roblox.com/drivers for info on how to perform a driver upgrade.");
	}

    RBXASSERT( view );

    // Removed Windows registry write

	initializeSizes();
}

void View::resetScheduler()
{
	TaskScheduler& taskScheduler = TaskScheduler::singleton();
	taskScheduler.add(renderJob);
}

void View::HandleSDLEvent(SDL_Event* event)
{
	// Handle SDL events, pass to UserInput
	if (userInput)
	{
		// Handle window focus loss for key cleanup
		if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST || event->type == SDL_EVENT_WINDOW_MINIMIZED)
		{
			userInput->onWindowFocusLost();
		}

		userInput->postUserInputMessage(event);
	}
}



void View::initializeJobs()
{
	shared_ptr<DataModel> dataModel = game->getDataModel();
	renderJob.reset(new RenderJob(this, marshaller, dataModel));
}

void View::initializeInput()
{
	userInput.reset(new UserInput(GetWindow(), game, this));

	if (userInput)
	{
		DataModel::LegacyLock lock(game->getDataModel(), DataModelJob::Write);

		ControllerService* service =
			ServiceProvider::create<ControllerService>(game->getDataModel().get());
		service->setHardwareDevice(userInput.get());
	}
}

void View::RemoveJobs()
{
	if (renderJob)
	{
		boost::function<void()> callback =
			boost::bind(&FunctionMarshaller::ProcessMessages, marshaller);
		TaskScheduler::singleton().removeBlocking(renderJob, callback);
	}

    // RenderJob is sure to be completed at this point, since removeBlocking returned - but it might have marshalled
    // renderPerform asynchronously before exiting, which means that we might still have a callback that uses this view
    // in the marshaller queue.
    // This makes sure that all pending marshalled events are processed to avoid a use after free.
    marshaller->ProcessMessages();

    // All render processing is complete; it's safe to reset job pointers now
    renderJob.reset();
}

shared_ptr<DataModel> View::getDataModel()
{
	return game ? game->getDataModel() : shared_ptr<DataModel>();
}

CRenderSettings::GraphicsMode View::GetLatchedGraphicsMode()
{
	return CRenderSettingsItem::singleton().getLatchedGraphicsMode();
}

bool View::IsFullscreen()
{
	return (SDL_GetWindowFlags(GetWindow()) & SDL_WINDOW_FULLSCREEN) != 0;
}

void View::SetFullscreen(bool value)
{
	if (value)
	{
		SDL_SetWindowFullscreen(GetWindow(), SDL_WINDOW_FULLSCREEN);
	}
	else
	{
		SDL_SetWindowFullscreen(GetWindow(), 0);
	}
	desireFullscreen = value;
	RBX::GameBasicSettings::singleton().setFullScreen(value);
}

void View::initializeSizes()
{
	SDL_DisplayID displayIndex = SDL_GetDisplayForWindow(GetWindow());
	if (displayIndex == 0) {
		LogManager::ReportEvent(EVENTLOG_WARNING_TYPE, "Attempt to initialize monitor sizes without valid display");
		return;
	}

	const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(displayIndex);
	if (mode == NULL) {
		LogManager::ReportEvent(EVENTLOG_ERROR_TYPE, "Failed to get desktop display mode");
		return;
	}

	G3D::Vector2int16 currentDisplaySize(mode->w, mode->h);

	G3D::Vector2int16 fullscreenSize, windowSize;

	RBX::CRenderSettings::ResolutionPreset preference = CRenderSettingsItem::singleton().getResolutionPreference();
	if (preference == RBX::CRenderSettings::ResolutionAuto) {
		fullscreenSize = currentDisplaySize;
	} else {
		const RBX::CRenderSettings::RESOLUTIONENTRY& res =
			CRenderSettingsItem::singleton().getResolutionPreset(preference);
		fullscreenSize.x = res.width;
		fullscreenSize.y = res.height;
	}

	// validate mode
	int w, h;
	if (SDL_GetWindowSizeInPixels(GetWindow(), &w, &h) == 0) {
		windowSize = G3D::Vector2int16(w, h);
	} else {
		// Fallback if GetWindowSizeInPixels fails
		windowSize = fullscreenSize;
		windowSize.x = std::min((int)windowSize.x, (int)currentDisplaySize.x);
		windowSize.y = std::min((int)windowSize.y, (int)currentDisplaySize.y);
	}

	// Ensure fullscreen size is also high-res if needed, though usually fullscreenSize logic above is based on desktop mode.
	// We'll trust the desktop mode is correct for fullscreen, but for windowed/render target, we strictly use the pixel size.

	CRenderSettingsItem::singleton().setWindowSize(windowSize);
	CRenderSettingsItem::singleton().setFullscreenSize(fullscreenSize);

	if (view) {
		view->onResize(windowSize.x, windowSize.y);
	}
}

void View::OnResize(int w, int h)
{
	view->onResize(w, h);
}

void View::SetVSync(bool enabled)
{
    if (view)
    {
        view->setVSync(enabled);
    }
}

void View::ShowWindow()
{
	// Load window settings from file
	std::ifstream in(std::string(getenv("HOME") ? getenv("HOME") : "/tmp") + "/.roblox/window_settings");
	int x = SDL_WINDOWPOS_UNDEFINED, y = SDL_WINDOWPOS_UNDEFINED, w = 800, h = 600, fs = 0;
	if (in) {
		in >> x >> y >> w >> h >> fs;
		in.close();
	}
	SDL_SetWindowPosition(GetWindow(), x, y);
	SDL_SetWindowSize(GetWindow(), w, h);
	if (fs) {
		SetFullscreen(true);
	}

	SDL_ShowWindow(GetWindow());

	if (RBX::GameBasicSettings::singleton().getFullScreen())
	{
		SetFullscreen(true);
	}
}

void View::unbindWorkspace()
{
	shared_ptr<DataModel> dm = getDataModel();
	DataModel::LegacyLock lock( dm, DataModelJob::Write);
	view->bindWorkspace(boost::shared_ptr<DataModel>());
}

void View::bindWorkspace()
{
	shared_ptr<DataModel> dm = getDataModel();
	DataModel::LegacyLock lock( dm, DataModelJob::Write);
	view->bindWorkspace(game->getDataModel());
	view->buildGui();
}

void View::Start(const shared_ptr<Game>& game)
{
    RBXASSERT(!this->game);
    this->game = game;

    bindWorkspace();
    initializeJobs();
    initializeInput(); // NOTE: have to do this here, Input requires datamodel access
	resetScheduler();
}

void View::Stop()
{
    RBXASSERT(this->game);

    if (view)
        view->setShuttingDown(true);

    this->RemoveJobs();

    if (game && game->getDataModel())
        if (RBX::ControllerService* service = RBX::ServiceProvider::create<RBX::ControllerService>(game->getDataModel().get()))
            service->setHardwareDevice(NULL);

	if (userInput)
	{
		userInput->removeJobs();
		userInput.reset();
	}

    unbindWorkspace();

    game.reset();
}

void View::CloseWindow()
{
	SDL_Event event;
	event.type = SDL_EVENT_QUIT;
	SDL_PushEvent(&event);
}

float View::GetViewScale() const
{
    if (view)
    {
        return view->getViewScale();
    }
    return 1.0f;
}


}  // namespace RBX
