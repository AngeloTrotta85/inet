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

#ifndef IEEE80211MACRECEPTION_H_
#define IEEE80211MACRECEPTION_H_

#include "Ieee80211MacPlugin.h"
#include "inet/common/INETDefs.h"
#include "inet/physicallayer/contract/packetlevel/IRadio.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {
namespace ieee80211 {

class Ieee80211NewMac;
class Ieee80211UpperMac;
class IIeee80211MacContext;

class Ieee80211MacReception : public Ieee80211MacPlugin
{
    protected:
        cMessage *nav = nullptr;
        IRadio::ReceptionState receptionState = IRadio::RECEPTION_STATE_UNDEFINED;
        IRadio::TransmissionState transmissionState = IRadio::TRANSMISSION_STATE_UNDEFINED;
        IIeee80211MacContext *context; //TODO initialize!

    protected:
        void handleMessage(cMessage *msg);
        void setNav(simtime_t navInterval);
        bool isFcsOk(Ieee80211Frame *frame) const;

    public:
        Ieee80211MacReception(Ieee80211NewMac *mac);
        ~Ieee80211MacReception();

        void setContext(IIeee80211MacContext *context) { this->context = context; }

        void receptionStateChanged(IRadio::ReceptionState newReceptionState);
        void transmissionStateChanged(IRadio::TransmissionState transmissionState);
        /** @brief Tells if the medium is free according to the physical and virtual carrier sense algorithm. */
        virtual bool isMediumFree() const; //TODO "tx-to-rx switching" state should also count as busy (but not rx-to-tx, otherwise we wont be able to transmit anything with contention)
        void handleLowerFrame(Ieee80211Frame *frame);

};

}

} /* namespace inet */

#endif /* IEEE80211MACRECEPTION_H_ */
