#pragma once

#include "rbx/Boost.hpp"


namespace RBX {
	class PartInstance;
}


namespace boost {
	std::size_t hash_value(const shared_ptr<RBX::PartInstance>& b);
}

