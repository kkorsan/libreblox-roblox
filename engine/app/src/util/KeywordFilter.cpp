#include "stdafx.h"

#include "util/KeywordFilter.h"
#include "reflection/EnumConverter.h"

namespace RBX 
{
	namespace Reflection
	{
		template<>
		Reflection::EnumDesc<KeywordFilterType>::EnumDesc()
			:RBX::Reflection::EnumDescriptor("KeywordFilterType")
		{
			addPair(INCLUDE_KEYWORDS,"Include");
			addPair(EXCLUDE_KEYWORDS,"Exclude");
		}
	}
}
