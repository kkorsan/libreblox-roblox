#include "Util.h"
#include "v8world/ContactManagerSpatialHash.h"
#include "raknet/RakNetTypes.h"

#include <boost/static_assert.hpp>

namespace RBX { 
namespace Network {

	const RBX::SystemAddress RakNetToRbxAddress(const RakNet::SystemAddress& raknetAddress) 
	{
		return RBX::SystemAddress(raknetAddress.GetBinaryAddress(), raknetAddress.GetPort());
	}

	std::string RakNetAddressToString(const RakNet::SystemAddress& raknetAddress, bool writePort, char portDelineator)
	{
		char buffer[128];
		raknetAddress.ToString(writePort, buffer, portDelineator);
		return buffer;
	}

}}	// namespace

