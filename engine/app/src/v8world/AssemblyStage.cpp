/* Copyright 2003-2005 ROBLOX Corporation, All Rights Reserved */
#include "stdafx.h"

#include "v8world/AssemblyStage.h"
#include "v8world/MovingAssemblyStage.h"
#include "v8world/Assembly.h"
#include "v8world/Edge.h"
#include "v8world/Joint.h"
#include "v8world/Contact.h"
#include "v8world/Primitive.h"

namespace RBX {

#pragma warning(push)
#pragma warning(disable: 4355) // 'this' : used in base member initializer list
AssemblyStage::AssemblyStage(IStage* upstream, World* world)
	: EdgeBuffer(	upstream, 
					new MovingAssemblyStage(this, world), 
					world)		// 2 assemblies needed to move edges past this stage
{}
#pragma warning(pop)

AssemblyStage::~AssemblyStage()
{
}

Assembly* AssemblyStage::onEngineChanging(Primitive* p)
{
	if (Assembly::isAssemblyRootPrimitive(p)) {
		Assembly* a = p->getAssembly();
		if (a->inPipeline() && a->downstreamOfStage(this)) {
			onSimulateAssemblyDescendentRemoving(a);			// basically push and pop from downstream
			return a;
		}
	}
	return NULL;
}

void AssemblyStage::onEngineChanged(Assembly* a)
{
	RBXASSERT(a);
	onSimulateAssemblyDescendentAdded(a);					// basically push and pop from downstream
}



void AssemblyStage::onSimulateAssemblyDescendentAdded(Assembly* a)
{
	a->putInPipeline(this);
	rbx_static_cast<MovingAssemblyStage*>(getDownstreamWS())->onSimulateAssemblyAdded(a);
	afterAssemblyAdded(a);
}


void AssemblyStage::onSimulateAssemblyDescendentRemoving(Assembly* a) 
{
	beforeAssemblyRemoving(a);
	rbx_static_cast<MovingAssemblyStage*>(getDownstreamWS())->onSimulateAssemblyRemoving(a);
	a->removeFromPipeline(this);
}


void AssemblyStage::onSimulateAssemblyRootAdded(Assembly* a)
{
	a->putInStage(this);
	rbx_static_cast<MovingAssemblyStage*>(getDownstreamWS())->onSimulateAssemblyAdded(a);
	afterAssemblyAdded(a);
}


void AssemblyStage::onSimulateAssemblyRootRemoving(Assembly* a) 
{
	beforeAssemblyRemoving(a);
	rbx_static_cast<MovingAssemblyStage*>(getDownstreamWS())->onSimulateAssemblyRemoving(a);
	a->removeFromStage(this);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

void AssemblyStage::onFixedAssemblyRootAdded(Assembly* a)
{
	a->putInStage(this);							
	afterAssemblyAdded(a);
}

void AssemblyStage::onFixedAssemblyRootRemoving(Assembly* a) 
{
	beforeAssemblyRemoving(a);
	a->removeFromStage(this);							// 2.  out of this stage
}

void AssemblyStage::onNoSimulateAssemblyDescendentAdded(Assembly* a)
{
	a->putInPipeline(this);
	afterAssemblyAdded(a);
}

void AssemblyStage::onNoSimulateAssemblyDescendentRemoving(Assembly* a) 
{
	beforeAssemblyRemoving(a);
	a->removeFromPipeline(this);
}




} // namespace
