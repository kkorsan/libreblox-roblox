#include "GameVerbs.h"

#include "Document.h"
#include "util/Http.h"
#include "util/Statistics.h"
#include "v8datamodel/Game.h"
#include "v8datamodel/GameBasicSettings.h"


#include "util/SoundService.h"
#include "View.h"
#include "Application.h"
#include "LogManager.h"
// Replaced WebBrowserAxDialog with direct HTTP for cross-platform


namespace RBX {

LeaveGameVerb::LeaveGameVerb(View& view, VerbContainer* container)
	: Verb(container, "Exit")
	, v(view)
{
}

void LeaveGameVerb::doIt(IDataState* dataState)
{
	MainLogManager::getMainLogManager()->setLeaveGame();
    v.CloseWindow();
}

ScreenshotVerb::ScreenshotVerb(const Document& doc,
							   ViewBase* view,
							   Game* game)
	: Verb(game->getDataModel().get(), "Screenshot")
	, doc(doc)
	, game(game)
	, view(view)
{
	boost::shared_ptr<DataModel> dataModel = game->getDataModel();
	dataModel->screenshotReadySignal.connect(
		boost::bind(&ScreenshotVerb::screenshotFinished, this, _1));
}

void ScreenshotVerb::doIt(IDataState* dataState)
{
	boost::shared_ptr<DataModel> dataModel = game->getDataModel();

	if (!dataModel)
		return;

	dataModel->submitTask(boost::bind(&DataModel::TakeScreenshotTask,
		boost::weak_ptr<DataModel>(dataModel)), DataModelJob::Write);
}

void ScreenshotVerb::screenshotFinished(const std::string &filename)
{
	view->getAndClearDoScreenshot();

	boost::shared_ptr<DataModel> dataModel = game->getDataModel();
	dataModel->submitTask(boost::bind(&DataModel::ShowMessage,
		weak_ptr<DataModel>(dataModel), 0, "Screenshot saved", 2.0),
		DataModelJob::Write);
}

ToggleFullscreenVerb::ToggleFullscreenVerb(View& view, VerbContainer* container)
	: Verb(container, "ToggleFullScreen")
	, view(view)
{}

bool ToggleFullscreenVerb::isChecked() const
{
	return view.IsFullscreen();
}

bool ToggleFullscreenVerb::isEnabled() const
{
	return true;
}

void ToggleFullscreenVerb::doIt(RBX::IDataState* dataState)
{
	FASTLOG(FLog::Verbs, "Gui:ToggleFullscreen");

	view.SetFullscreen(!view.IsFullscreen());
}


}  // namespace RBX
