//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#ifndef IEEE80211MACPLUGIN_H_
#define IEEE80211MACPLUGIN_H_

#include "inet/common/INETDefs.h"
#include "Ieee80211NewMac.h"

namespace inet {
namespace ieee80211 {

class Ieee80211MacPlugin : public cObject
{
    protected:
        cSimpleModule *ownerModule = nullptr;

    public:
        Ieee80211MacPlugin(cSimpleModule *ownerModule) : ownerModule(ownerModule) {}
        virtual ~Ieee80211MacPlugin() {}

    public:
        virtual void handleMessage(cMessage *msg) = 0;
        virtual void scheduleAt(simtime_t t, cMessage *msg);
        virtual cMessage* cancelEvent(cMessage *msg);
};

}
} /* namespace inet */

#endif /* IEEE80211MACPLUGIN_H_ */
