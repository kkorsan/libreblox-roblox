#include "SDL3/SDL.h"
#ifdef nullptr
#undef nullptr
#endif
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <chrono>
#include <map>
#include "FunctionMarshaller.h"

#include "rbx/Debug.h"
#include "util/standardout.h"
#include "rbx/Boost.hpp"

namespace RBX {

SDL_EventType FunctionMarshaller::syncEventType = (SDL_EventType)SDL_RegisterEvents(1);
SDL_EventType FunctionMarshaller::asyncEventType = (SDL_EventType)SDL_RegisterEvents(1);

FunctionMarshaller::FunctionMarshaller(std::thread::id threadID)
	: refCount(0), threadID(threadID)
{
}

void FunctionMarshaller::ProcessSyncMessages()
{
	SyncData* data;
	while (staticData().syncCalls.pop_if_present(data))
	{
		try
		{
			data->func();
			data->promise.set_value();
		}
		catch (std::exception& e)
		{
			data->promise.set_exception(std::current_exception());
			StandardOut::singleton()->print(RBX::MESSAGE_ERROR, e);
		}
		delete data;
	}
}

FunctionMarshaller::~FunctionMarshaller()
{
	boost::function<void()>* f;
	while (asyncCalls.pop_if_present(f))
		delete f;

	RBXASSERT(threadID == std::this_thread::get_id());

#ifdef _DEBUG
	{
		boost::recursive_mutex::scoped_lock lock(staticData().criticalSection);
		RBXASSERT(refCount == 0);
		RBXASSERT(staticData().marshallers.find(threadID) == staticData().marshallers.end());
	}
#endif
}

FunctionMarshaller* FunctionMarshaller::GetWindow()
{
	// Share a common FunctionMarshaller in a given Thread

	boost::recursive_mutex::scoped_lock lock(staticData().criticalSection);

	std::thread::id threadID = std::this_thread::get_id();
	auto find = staticData().marshallers.find(threadID);
	if (find != staticData().marshallers.end())
	{
		find->second->refCount++;
		return find->second;
	}
	else
	{
		FunctionMarshaller* marshaller = new FunctionMarshaller(threadID);
		staticData().marshallers[threadID] = marshaller;
		marshaller->refCount++;
		return marshaller;
	}
}

void FunctionMarshaller::ReleaseWindow(FunctionMarshaller* marshaller)
{
	boost::recursive_mutex::scoped_lock lock(staticData().criticalSection);

	marshaller->refCount--;
	if (marshaller->refCount == 0)
	{
		staticData().marshallers.erase(marshaller->threadID);
		delete marshaller;
	}
}

void FunctionMarshaller::HandleSDLEvent(const SDL_Event& event)
{
	FunctionMarshaller* marshaller = FunctionMarshaller::GetWindow();
	if (!marshaller)
		return;

	if (event.type == asyncEventType)
	{
		marshaller->postedAsyncMessage.swap(0);
		boost::function<void()>* f;
		while (marshaller->asyncCalls.pop_if_present(f))
		{
			try
			{
				(*f)();
			}
			catch (std::exception& e)
			{
				delete f;
				StandardOut::singleton()->print(RBX::MESSAGE_ERROR, e);
				throw;
			}
			delete f;
		}
	}
	else if (event.type == syncEventType)
	{
		marshaller->ProcessSyncMessages();
	}
}

void FunctionMarshaller::Execute(boost::function<void()> job)
{
	if (threadID == std::this_thread::get_id())
	{
		job();
	}
	else
	{
		// For cross-thread, use a promise
		SyncData* data = new SyncData{job, std::promise<void>()};
		std::future<void> future = data->promise.get_future();

		staticData().syncCalls.push(data);

		SDL_Event event;
		event.type = syncEventType;
		SDL_PushEvent(&event);

		// Wait for the main thread to process the event
		future.get();
	}
}

void FunctionMarshaller::Submit(boost::function<void()> job)
{
	boost::function<void()>* f = new boost::function<void()>(job);
	asyncCalls.push(f);
	if (postedAsyncMessage.swap(1) == 0)
	{
		SDL_Event event;
		event.type = asyncEventType;
		SDL_PushEvent(&event);
	}
}

void FunctionMarshaller::ProcessMessages()
{
	// Process async calls
	boost::function<void()>* f;
	while (asyncCalls.pop_if_present(f))
	{
		try
		{
			(*f)();
		}
		catch (std::exception& e)
		{
			delete f;
			StandardOut::singleton()->print(RBX::MESSAGE_ERROR, e);
			throw;
		}
		delete f;
	}

	// Process sync calls (essential for preventing deadlocks during shutdown)
	ProcessSyncMessages();
}

FunctionMarshaller::StaticData::~StaticData()
{
	SyncData* data;
	while (syncCalls.pop_if_present(data))
	{
		delete data;
	}
	for (auto& pair : marshallers)
	{
		delete pair.second;
	}
}

}  // namespace RBX
