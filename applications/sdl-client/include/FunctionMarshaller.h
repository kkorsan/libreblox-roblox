#pragma once

#include "SDL3/SDL.h"
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include "rbx/threadsafe.h"
#include "rbx/atomic.h"

namespace RBX {

// A very handy class for marshaling a function across threads (sync and async)
class FunctionMarshaller
{
	static SDL_EventType syncEventType;
	static SDL_EventType asyncEventType;

	struct SyncData
	{
		boost::function<void()> func;
		std::promise<void> promise;
	};

	struct StaticData
	{
		std::map<std::thread::id, FunctionMarshaller*> marshallers;
		rbx::safe_queue<SyncData*> syncCalls;
		boost::recursive_mutex criticalSection;
		~StaticData();
	};
	SAFE_STATIC(StaticData, staticData)

	rbx::safe_queue<boost::function<void()>*> asyncCalls;

	rbx::atomic<int> postedAsyncMessage;
	int refCount;
	std::thread::id threadID;

	FunctionMarshaller(std::thread::id threadID);
	~FunctionMarshaller();

public:
	static FunctionMarshaller* GetWindow();
	static void ReleaseWindow(FunctionMarshaller* marshaller);

	// Executes the given function synchronously.
	void Execute(boost::function<void()> job);

	// Submits a function to be executed asynchronously.
	void Submit(boost::function<void()> job);

	// Call this from the main event loop to process async messages.
	void ProcessMessages();

	// Handle SDL user events in the main loop.
	static void HandleSDLEvent(const SDL_Event& event);

private:
	void ProcessSyncMessages();
};

}  // namespace RBX
