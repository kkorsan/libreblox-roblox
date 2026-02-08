/* Copyright 2003-2006 ROBLOX Corporation, All Rights Reserved */


// third time's the charm
#include "network/api.h"
#include "Client.h"
#include "Server.h"
#include "ServerReplicator.h"
#include "ClientReplicator.h"
#include "Players.h"
#include "Player.h"
#include "Marker.h"

#include "NetworkSettings.h"
#include "v8datamodel/DataModel.h"
#include "v8datamodel/GlobalSettings.h"
#include "v8datamodel/HackDefines.h"
#include "v8datamodel/Workspace.h"
#include "GuidRegistryService.h"
#include "RakNetVersion.h"
#include "util/Utilities.h"
#include "FastLog.h"

// RakNet
#include "StringCompressor.h"
#include "StringTable.h"

#include <boost/algorithm/string.hpp>

FASTSTRINGVARIABLE(ClientExternalBrowserUserAgent, "Roblox/WinInet")

#define ROBLOX_URL_IDENTIFIER "roblox.com/"
#define ROBLOXLABS_URL_IDENTIFIER ".robloxlabs.com/"

#pragma comment (lib, "Ws2_32.lib")

std::string RBX::Network::versionB;
std::string RBX::Network::securityKey;
unsigned int RBX::initialProgramHash = 0;

#if RAKNET_PROTOCOL_VERSION!=5
#error
#endif


RBX_REGISTER_CLASS(RBX::Network::Client);
RBX_REGISTER_CLASS(RBX::Network::Server);
RBX_REGISTER_CLASS(RBX::Network::Player);
RBX_REGISTER_CLASS(RBX::Network::Players);
RBX_REGISTER_CLASS(RBX::NetworkSettings);
RBX_REGISTER_CLASS(RBX::Network::Peer);
RBX_REGISTER_CLASS(RBX::Network::Marker);
RBX_REGISTER_CLASS(RBX::Network::Replicator);
RBX_REGISTER_CLASS(RBX::Network::ServerReplicator);
RBX_REGISTER_CLASS(RBX::Network::ClientReplicator);
RBX_REGISTER_CLASS(RBX::Network::GuidRegistryService);

RBX_REGISTER_ENUM(PacketPriority);
RBX_REGISTER_ENUM(PacketReliability);
RBX_REGISTER_ENUM(RBX::Network::FilterResult);
RBX_REGISTER_ENUM(RBX::Network::Player::MembershipType);
RBX_REGISTER_ENUM(RBX::Network::Player::ChatMode);
RBX_REGISTER_ENUM(RBX::Network::Players::PlayerChatType);
RBX_REGISTER_ENUM(RBX::Network::Players::ChatOption);
RBX_REGISTER_ENUM(RBX::NetworkSettings::PhysicsSendMethod);
RBX_REGISTER_ENUM(RBX::NetworkSettings::PhysicsReceiveMethod);

namespace RBX {
	namespace Network {
		// Prevent string compressors from being created at the same time
		class SafeInitFree
		{
		public:
			SafeInitFree()
			{
				RakNet::StringCompressor::AddReference();
				RakNet::StringTable::AddReference();
			}
			~SafeInitFree()
			{
				RakNet::StringCompressor::RemoveReference();
				RakNet::StringTable::RemoveReference();
			}
		};
	}
}

static bool _isPlayerAuthenticationEnabled;

bool RBX::Network::isPlayerAuthenticationEnabled()
{
	return _isPlayerAuthenticationEnabled;
}

bool RBX::Network::isNetworkClient(const Instance* context)
{
	return ServiceProvider::find<Client>(context) != NULL;
}

#if defined(RBX_RCC_SECURITY)
static shared_ptr<RBX::Network::ServerReplicator> createSecureReplicator(RakNet::SystemAddress a, RBX::Network::Server* s, RBX::NetworkSettings* networkSettings)
{
	return RBX::Creatable<RBX::Instance>::create<RBX::Network::CheatHandlingServerReplicator>(a, s, networkSettings);
}
#endif

static void initVersion1()
{

	// security key: generated externally (version+platform+product+salt), modify per release, send from client to server

#if (defined(_WIN32) && (defined(LOVE_ALL_ACCESS) || defined(_NOOPT) || defined(_DEBUG) || defined(RBX_TEST_BUILD))) || (defined(__APPLE__) && defined(__arm__)) || defined(__ANDROID__) || (defined(__APPLE__) && defined(__aarch64__)) || defined(RBX_PLATFORM_DURANGO)
	// If we are Apple iOS, Android, or if we are Windows noopt/debug/test, use this fixed key:
	// INTERNALiosapp, sha1:0dc7e3e7b0dde768db1640bc6bc682817f0720dd then rot13 and put below
	RBX::Network::securityKey = RBX::rot13("0qp7r3r7o0qqr768qo1640op6op682817s0720qq");
#elif defined(_WIN32)
	//0.1.0pcplayerHregBEighE, sha1: 680f0a3be0dd9b779fe28c0c5f31aede23b86dd8, then rot13 and put below
	RBX::Network::securityKey = RBX::rot13("680s0n3or0qq9o779sr28p0p5s31nrqr23o86qq8");
#elif defined(__APPLE__) && defined(__i386)
	//0.1.0macplayerHregBEighE, sha1: 396f87757c0e7cdd88599c82474cb986b73779c9, then rot13 and put below
	RBX::Network::securityKey = RBX::rot13("396s87757p0r7pqq88599p82474po986o73779p9");
#elif defined(__linux__)
    // for linux!
    //0.1.0linuxplayerHregBEighE, sha1: 4f1e5f53ba9b17b5760dc71e5b0cd2e612f3a746, then rot13 and put below
    RBX::Network::securityKey = RBX::rot13("4s1r5s53on9o17o5760qp71r5o0pq2r612s3n746");
#else
    #error "Unsupported platform"
#endif

	RBX::Network::versionB += '7';
	RBX::Network::versionB += (char)79;
}

static void initVersion2()
{
	// TODO: Obfuscate even more?
	RBX::Network::versionB += "l";
	RBX::Network::versionB += 'E';
}



void RBX::Network::initWithServerSecurity()
{
	initVersion1();
	initWithoutSecurity();
#if defined(RBX_RCC_SECURITY)
	_isPlayerAuthenticationEnabled = true;
	Server::createReplicator = createSecureReplicator;
#endif
	initVersion2();
}


void RBX::Network::initWithPlayerSecurity()
{
	initVersion1();
	initWithoutSecurity();
	initVersion2();
}

void RBX::Network::initWithCloudEditSecurity()
{
	// Keep this in sync with initWithoutSecurity password
	versionB = "";
	versionB += "^";
	versionB += (char)17;
}

void RBX::Network::initWithoutSecurity()
{
	_isPlayerAuthenticationEnabled = false;

	static SafeInitFree safeInitFree;

	// Forces registration of Descriptors
	Client::classDescriptor();
	Server::classDescriptor();
	versionB += "^";
	Player::classDescriptor();
	Players::classDescriptor();
	GlobalAdvancedSettings::classDescriptor();
	NetworkSettings::classDescriptor();
	versionB += (char)17;

	// Force instantiation of NetworkSettings singleton
	NetworkSettings::singleton();
}

void RBX::Network::setVersion(const char* version)
{
	if (version)
		versionB = version;
}

void RBX::Network::setSecurityVersions(const std::vector<std::string>& versions)
{
	Server::setAllowedSecurityVersions(versions);
}

bool RBX::Network::isTrustedContent(const char* url)
{
    if (!RBX::ContentProvider::isUrl(url))
		return false;

	return Http::isStrictlyRobloxSite(url);
}

#if defined(_WIN32) && !defined(RBX_STUDIO_BUILD) && !defined(RBX_PLATFORM_DURANGO)
namespace {
void isDebuggedDirectThreadFunc(weak_ptr<RBX::DataModel> weakDataModel) {
#if !defined(LOVE_ALL_ACCESS) && !defined(_NOOPT) && !defined(_DEBUG)
	static const int kSleepBetweenChecksMillis = 1500;

	while (true) {
		shared_ptr<RBX::DataModel> dataModel = weakDataModel.lock();
		if (!dataModel) { break; }

		// this will be less secure, but, we are trying to be open about our client
		// unsigned int mask = static_cast<unsigned int>(
		// 	VMProtectIsDebuggerPresent(true /*check for kernel debuggers too*/)) *
		// 	HATE_DEBUGGER;
		// dataModel->addHackFlag(mask);

		boost::this_thread::sleep(boost::posix_time::milliseconds(kSleepBetweenChecksMillis));
	}
#endif
}
}

void RBX::spawnDebugCheckThreads(weak_ptr<RBX::DataModel> dataModel) {
	boost::thread t(boost::bind(&isDebuggedDirectThreadFunc, dataModel));
}
#endif

bool RBX::Network::getSystemUrlLocal(DataModel *dataModel)
{
	// Stub implementation - this function appears to be unused or incomplete
	// In the context it's called, it's meant to send a special item to kick users
	// but since it's not implemented, we return false
	return false;
}
