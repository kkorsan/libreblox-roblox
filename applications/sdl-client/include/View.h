#pragma once

#include "SDL3/SDL.h"
#include "GfxBase/ViewBase.h"
#include "UserInput.h"
namespace RBX {

// Forward declarations
class FunctionMarshaller;
class Game;
struct OSContext;
class RenderJob;
class ViewBase;

namespace Tasks { class Sequence; }

// Class responsible for the game view
class View
{
public:
	View(void* window);
	~View();

	void AboutToShutdown();

	void Start(const shared_ptr<Game>& game);
    void Stop();

    void OnResize(int w, int h);
    void ShowWindow();
    void SetVSync(bool enabled);

    void CloseWindow();
	void HandleSDLEvent(SDL_Event* event);

	SDL_Window* GetWindow() const { return (SDL_Window*)context.window; }

	// TODO: refactor verbs so this isn't needed
	ViewBase* GetGfxView() const { return view.get(); }

	CRenderSettings::GraphicsMode GetLatchedGraphicsMode();

	bool IsFullscreen();
	void SetFullscreen(bool value);

	float GetViewScale() const;

	shared_ptr<DataModel> getDataModel();

private:
	// View references, but doesn't own the game
	shared_ptr<Game> game;

	// The OS context used by the view.
	OSContext context;

	// Are we currently fullscreen?
	bool fullscreen;

	// Do we want to become fullscreen at first available opportunity?
	bool desireFullscreen;

	// The view into the game world.
	boost::scoped_ptr<RBX::ViewBase> view;

	FunctionMarshaller* marshaller;
	boost::scoped_ptr<UserInput> userInput;
	boost::shared_ptr<Tasks::Sequence> sequence;
	boost::shared_ptr<RenderJob> renderJob;

	void bindWorkspace();
	void unbindWorkspace();

	void initializeView();
	void initializeSizes();
	void initializeInput();
	void resetScheduler();

    void initializeJobs();
    void RemoveJobs();
};

}  // namespace RBX
