#include "Replicator.HashItem.h"

#include "Item.h"
#include "Replicator.h"
#include "ReplicatorStats.h"

#include "FastLog.h"
#include "v8datamodel/DataModel.h"

#include "BitStream.h"
#include "util/ProgramMemoryChecker.h"
#include "v8datamodel/HackDefines.h"
#include "security/FuzzyTokens.h"
#include "security/ApiSecurity.h"
#include "v8datamodel/HackDefines.h"

namespace RBX {
    namespace Network {

        Replicator::HashItem::HashItem(Replicator* replicator, const PmcHashContainer* const hashes, unsigned long long fuzzyToken,
            unsigned long long apiToken, unsigned long long prevApiToken)
            : Item(*replicator)
            , hashes(*hashes)
            , fuzzyToken(fuzzyToken)
            , apiToken(apiToken)
            , prevApiToken(prevApiToken)
        {

        }

        // make sure to _copy_ data to the item and not re-read it from other memory.
        bool Replicator::HashItem::write(RakNet::BitStream& bitStream) {
            // There is a small amount of obscuring that is done to make this slightly harder to
            // analyze.  "nonce" = "number used once"
            writeItemType(bitStream, ItemTypeHash);
            unsigned char numItems;
            numItems = hashes.hash.size();
            bitStream << numItems;
            unsigned int obscuringXorValue = hashes.nonce ^ hashes.hash[numItems-1];
            bitStream << obscuringXorValue;
            for (unsigned char i = 0; i < numItems-1; ++i)
            {
                obscuringXorValue ^= hashes.hash[i];
                bitStream << obscuringXorValue;
            }
            bitStream << hashes.hash[numItems-1];
            bitStream << fuzzyToken;
            bitStream << apiToken;
            bitStream << prevApiToken;
            return true;
        }

    }
}
