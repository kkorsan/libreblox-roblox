#include "util/Guid.h"
#include "rbx/atomic.h"
#include "util/Utilities.h"

#include "reflection/Type.h"

#include <string>
#include <algorithm> // For std::min
#include <memory>    // For std::unique_ptr
#include <mutex>     // For std::once_flag, std::call_once
#include <sstream>   // For std::ostringstream

// Boost.UUID is a cross-platform solution and fixes the issues with the previous platform-specific
// implementations while staying compatible with typical project C++ standards (e.g., C++17).
// Standard C++20 <uuid> would be preferred if available, but Boost is a good alternative.
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp> // For boost::uuids::to_string

// Global static generator for Boost UUIDs. Uses boost::random::random_device for better randomness.
static boost::uuids::random_generator gen;

namespace RBX {
	template<>
	bool StringConverter<Guid::Data>::convertToValue(const std::string& text, Guid::Data& value)
	{
		RBXASSERT(false); // not fully implemented
		return false;
	}
	namespace Reflection
	{
		template<>
		Guid::Data& Variant::convert<Guid::Data>(void)
		{
			return genericConvert<Guid::Data>();
		}

		template<>
		const Type& Type::getSingleton<Guid::Data>()
		{
			static TType<Guid::Data> type("GuidData");
			return type;
		}
	}
}

static rbx::atomic<int> nextIndex = 0;
// Use std::unique_ptr for managing localScope's memory, ensuring proper destruction
// if the program were to clean up static objects (though for static, it's often not strictly needed
// for correctness if memory isn't reclaimed, it's good practice).
static std::unique_ptr<RBX::Guid::Scope> localScope;
RBX::Guid::Scope RBX::Guid::Scope::nullScope;

const RBX::Guid::Scope& RBX::Guid::Scope::null()
{
	return nullScope;
}

static void initLocalScope()
{
	RBX::Guid::Scope scope;
	RBX::Guid::generateRBXGUID(scope);

	// Use std::make_unique for safe and idiomatic allocation
	localScope = std::make_unique<RBX::Guid::Scope>(scope);
}

const RBX::Guid::Scope& RBX::Guid::getLocalScope()
{
	// Use std::once_flag and std::call_once for thread-safe one-time initialization
	static std::once_flag flag;
	std::call_once(flag, &initLocalScope);
	return *localScope;
}

RBX::Guid::Guid()
{
	data.scope = getLocalScope();
	data.index = ++nextIndex;
}

void RBX::Guid::generateStandardGUID(std::string& result)
{
	// Creates a string like this: {c200e360-38c5-11ce-ae62-08002b2b79ef}
	// Using Boost.UUID for a cross-platform solution.
	boost::uuids::uuid u = gen();

	// boost::uuids::to_string produces a string like "c200e360-38c5-11ce-ae62-08002b2b79ef" (lowercase, with hyphens, no braces)
	result = boost::uuids::to_string(u);

	// Add braces back as per original desired format "{...}"
	result = "{" + result + "}";
}

void RBX::Guid::generateRBXGUID(RBX::Guid::Scope& result)
{
	std::string tmp;
	generateRBXGUID(tmp);

	result.set( tmp );
}

void RBX::Guid::generateRBXGUID(std::string& result)
{
	// Start with a text character (so it conforms to xs:ID requirement)
	generateStandardGUID(result);
	result = "RBX" + result;

	// result looks like this: RBX{c200e360-38c5-11ce-ae62-08002b2b79ef}

	// strip the {}- characters
	result.erase(40, 1);
	result.erase(27, 1);
	result.erase(22, 1);
	result.erase(17, 1);
	result.erase(12, 1);
	result.erase(3, 1);

	// result looks like this: RBXc200e36038c511ceae6208002b2b79ef

	// TODO: This string could be more compact if we included g-z
}

void RBX::Guid::assign(Data data)
{
	this->data = data;
}


bool RBX::Guid::Data::operator ==(const RBX::Guid::Data& other) const
{
	return index == other.index && scope == other.scope;
}

bool RBX::Guid::Data::operator <(const RBX::Guid::Data& other) const
{
	const int compare = scope.compare(other.scope);

	if (compare < 0)
		return true;
	if (compare > 0)
		return false;

	return index < other.index;
}

int RBX::Guid::compare(const Guid* a, const Guid* b)
{
	const Scope& sa = a ? a->data.scope : Scope::null();
	const Scope& sb = b ? b->data.scope : Scope::null();

	const int compare = sa.compare(sb);
	if (compare)
		return compare;
	int ia = a ? a->data.index : 0;
	int ib = b ? b->data.index : 0;
	return ia - ib;
}

int RBX::Guid::compare(const Guid* a0, const Guid* a1, const Guid* b0, const Guid* b1)
{
	int aComp = compare(a0, a1);
	int bComp = compare(b0, b1);

	const Guid* aMax = (aComp > 0) ? a0 : a1;
	const Guid* bMax = (bComp > 0) ? b0 : b1;

	const int compare = Guid::compare(aMax, bMax);
	if (compare)
		return compare;

	const Guid* aMin = (aComp > 0) ? a1 : a0;
	const Guid* bMin = (bComp > 0) ? b1 : b0;

	return Guid::compare(aMin, bMin);
}

// Removed #pragma warning for sprintf as std::ostringstream is used instead.

std::string RBX::Guid::Data::readableString(int scopeLength) const
{
				std::ostringstream oss; // Use std::ostringstream for modern C++ string formatting

	if (scopeLength > 0)
	{
		std::string s = scope.getName()->toString();
		s = s.substr(std::min((size_t)3, s.size()), std::min(scopeLength, 32));
		oss << s << "_" << index;
	}
	else
	{
		oss << index;
	}
	return oss.str();
}
