/* Copyright 2003-2007 ROBLOX Corporation, All Rights Reserved */

#pragma once

#include "v8datamodel/Mouse.h"
#include "v8datamodel/DataModel.h"
#include "v8tree/Instance.h"
#include "v8datamodel/PartInstance.h"
#include "v8datamodel/InputObject.h"
#include "util/TextureId.h"
#include "rbx/signal.h"
#include "v8datamodel/Filters.h"
#include "v8datamodel/MouseCommand.h"


namespace RBX {

class PartInstance;
class PVInstance;

extern const char *const sPluginMouse;

class PluginMouse : public DescribedNonCreatable<PluginMouse, Mouse, sPluginMouse>
{
private:

public:
	PluginMouse();
	~PluginMouse();

	void fireDragEnterEvent(shared_ptr<const RBX::Instances> instances, shared_ptr<InputObject> input);

	rbx::signal<void(shared_ptr<const Instances>)> dragEnterEventSignal;
};


} // namespace
