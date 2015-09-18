//
// Copyright (C) 2015 Andras Varga
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

#include "Rx.h"
#include "ITx.h"
#include "IUpperMac.h"

namespace inet {
namespace ieee80211 {

Define_Module(Rx);

Rx::Rx()
{
}

Rx::~Rx()
{
    delete cancelEvent(endNavTimer);
}

void Rx::initialize()
{
    tx = check_and_cast<ITx*>(getModuleByPath("^.tx"));  //TODO
    upperMac = check_and_cast<IUpperMac*>(getModuleByPath("^.upperMac")); //TODO
    endNavTimer = new cMessage("NAV");
    recomputeMediumFree();
}

void Rx::handleMessage(cMessage* msg)
{
    if (msg->getContextPointer() != nullptr)
        ((MacPlugin *)msg->getContextPointer())->handleMessage(msg);
    else if (msg == endNavTimer) {
        EV_INFO << "The radio channel has become free according to the NAV" << std::endl;
        recomputeMediumFree();
    }
    else
        throw cRuntimeError("Unexpected self message");
}

void Rx::lowerFrameReceived(Ieee80211Frame* frame)
{
    Enter_Method("lowerFrameReceived()");
    take(frame);

    bool errorFree = isFcsOk(frame);
    tx->lowerFrameReceived(errorFree);
    if (errorFree)
    {
        EV_INFO << "Received message from lower layer: " << frame << endl;
        if (frame->getReceiverAddress() != address)
            setNav(frame->getDuration());
        upperMac->lowerFrameReceived(frame);
    }
    else
    {
        EV_INFO << "Received an erroneous frame. Dropping it." << std::endl;
        delete frame;
    }

}

bool Rx::isFcsOk(Ieee80211Frame* frame) const
{
    return !frame->hasBitError();
}

void Rx::recomputeMediumFree()
{
    bool oldMediumFree = mediumFree;
    // note: the duration of mode switching (rx-to-tx or tx-to-rx) should also count as busy
    mediumFree = receptionState == IRadio::RECEPTION_STATE_IDLE && transmissionState == IRadio::TRANSMISSION_STATE_UNDEFINED && !endNavTimer->isScheduled();
    if (mediumFree != oldMediumFree)
        tx->mediumStateChanged(mediumFree);
}

void Rx::receptionStateChanged(IRadio::ReceptionState state)
{
    Enter_Method("receptionStateChanged()");
    receptionState = state;
    recomputeMediumFree();
}

void Rx::transmissionStateChanged(IRadio::TransmissionState state)
{
    Enter_Method("transmissionStateChanged()");
    transmissionState = state;
    recomputeMediumFree();
}

void Rx::setNav(simtime_t navInterval)    //TODO: this should rather be called setOrExtendNav()!
{
    ASSERT(navInterval >= 0);
    if (navInterval > 0) {
        simtime_t endNav = simTime() + navInterval;
        if (endNavTimer->isScheduled()) {
            simtime_t oldEndNav = endNavTimer->getArrivalTime();
            if (oldEndNav > endNav) // we are only willing to extend the NAV (TODO ok?)
                return;
            cancelEvent(endNavTimer);
        }
        EV_INFO << "Setting NAV to " << navInterval << std::endl;
        scheduleAt(endNav, endNavTimer);
        recomputeMediumFree();
    }
}

} // namespace ieee80211
} // namespace inet

