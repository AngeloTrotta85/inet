//
// Copyright (C) 2015 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef IEEE_80211_MAC_H
#define IEEE_80211_MAC_H

// uncomment this if you do not want to log state machine transitions
#define FSM_DEBUG

#include <list>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/contract/packetlevel/IRadio.h"
#include "inet/common/queue/IPassiveQueue.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Consts.h"
#include "inet/linklayer/base/MACProtocolBase.h"
#include "inet/physicallayer/ieee80211/mode/IIeee80211Mode.h" //TODO not needed here
#include "inet/physicallayer/ieee80211/mode/Ieee80211ModeSet.h" //TODO not needed here
#include "IIeee80211MacRadioInterface.h"

namespace inet {

namespace ieee80211 {

using namespace physicallayer;

class IIeee80211UpperMacContext;
class IIeee80211MacTx;
class IIeee80211MacRx;
class IIeee80211UpperMac;
class Ieee80211Frame;


/**
 * IEEE 802.11b Media Access Control Layer.
 *
 * Various comments in the code refer to the Wireless LAN Medium Access
 * Control (MAC) and Physical Layer(PHY) Specifications
 * ANSI/IEEE Std 802.11, 1999 Edition (R2003)
 */
class INET_API Ieee80211NewMac : public MACProtocolBase, public IIeee80211MacRadioInterface
{
  protected:
    MACAddress address; // only because createInterfaceEntry() needs it

    IIeee80211UpperMac *upperMac = nullptr;
    IIeee80211MacRx *rx = nullptr;
    IIeee80211MacTx *tx = nullptr;
    IRadio *radio = nullptr;

    IRadio::TransmissionState transmissionState = IRadio::TransmissionState::TRANSMISSION_STATE_UNDEFINED;

    // The last change channel message received and not yet sent to the physical layer, or NULL.
    cMessage *pendingRadioConfigMsg = nullptr;

    /** @name Statistics */
    //@{
    long numRetry;
    long numSentWithoutRetry;
    long numGivenUp;
    long numCollision;
    long numSent;
    long numReceived;
    long numSentBroadcast;
    long numReceivedBroadcast;
    static simsignal_t stateSignal;
    static simsignal_t radioStateSignal;
    //@}

  protected:
    virtual int numInitStages() const {return NUM_INIT_STAGES;}
    virtual void initialize(int);

    void receiveSignal(cComponent *source, simsignal_t signalID, long value);
    void configureRadioMode(IRadio::RadioMode radioMode);
    virtual InterfaceEntry *createInterfaceEntry() override;
    virtual const MACAddress& isInterfaceRegistered();
    void transmissionStateChanged(IRadio::TransmissionState transmissionState);

    /** @brief Handle commands (msg kind+control info) coming from upper layers */
    virtual void handleUpperCommand(cMessage *msg);

    /** @brief Handle timer self messages */
    virtual void handleSelfMessage(cMessage *msg);

    /** @brief Handle messages from upper layer */
    virtual void handleUpperPacket(cPacket *msg);

    /** @brief Handle messages from lower (physical) layer */
    virtual void handleLowerPacket(cPacket *msg);

    virtual bool handleNodeStart(IDoneCallback *doneCallback) override;
    virtual bool handleNodeShutdown(IDoneCallback *doneCallback) override;
    virtual void handleNodeCrash() override;

  public:
    Ieee80211NewMac();
    virtual ~Ieee80211NewMac();

    virtual void sendFrame(Ieee80211Frame *frameToSend);
    virtual void sendDownPendingRadioConfigMsg();
};

} // namespace ieee80211

} // namespace inet

#endif

