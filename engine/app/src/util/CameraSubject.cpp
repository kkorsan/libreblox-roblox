/* Copyright 2003-2005 ROBLOX Corporation, All Rights Reserved */
#include "stdafx.h"

#include "util/CameraSubject.h"
#include "rbx/Debug.h"
#include "v8datamodel/Workspace.h"
#include "v8datamodel/Filters.h"
#include "v8datamodel/Camera.h"
#include "v8world/ContactManager.h"
#include "v8world/World.h"

namespace RBX {

ContactManager* CameraSubject::getContactManager()
{
	if (Instance* instance = dynamic_cast<Instance*>(this)) {
		if (World* world = Workspace::getWorldIfInWorkspace(instance)) {
			return world->getContactManager();
		}
	}
	return NULL;
}

} // namespace