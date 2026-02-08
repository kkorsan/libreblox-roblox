#include "stdafx.h"
#include "humanoid/StatusInstance.h"

namespace RBX {

const char* const sStatusInstance = "Status";

StatusInstance::StatusInstance() 
	:Super()
{
	setName(sStatusInstance);
	lockName();
}
bool StatusInstance::askSetParent(const Instance* instance) const
{
	return false;
}

}
