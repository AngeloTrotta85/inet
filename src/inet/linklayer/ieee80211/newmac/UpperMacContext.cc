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

#include "UpperMacContext.h"
#include "ITx.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/contract/packetlevel/RadioControlInfo_m.h"

namespace inet {
namespace ieee80211 {

UpperMacContext::UpperMacContext(const MACAddress& address,
        const IIeee80211Mode *dataFrameMode, const IIeee80211Mode *basicFrameMode, const IIeee80211Mode *controlFrameMode,
        int shortRetryLimit,  int rtsThreshold, ITx *tx) :
                    address(address),
                    dataFrameMode(dataFrameMode), basicFrameMode(basicFrameMode), controlFrameMode(controlFrameMode),
                    shortRetryLimit(shortRetryLimit), rtsThreshold(rtsThreshold), tx(tx)
{
}

const MACAddress& UpperMacContext::getAddress() const
{
    return address;
}

simtime_t UpperMacContext::getSlotTime() const
{
    return dataFrameMode->getSlotTime();
}

simtime_t UpperMacContext::getAIFS() const
{
    return dataFrameMode->getAifsTime(2); //TODO!!!
}

simtime_t UpperMacContext::getSIFS() const
{
    return dataFrameMode->getSifsTime();
}

simtime_t UpperMacContext::getDIFS() const
{
    return dataFrameMode->getDifsTime();
}

simtime_t UpperMacContext::getEIFS() const
{
    return dataFrameMode->getEifsTime(basicFrameMode, LENGTH_ACK);  //TODO ???
}

simtime_t UpperMacContext::getPIFS() const
{
    return dataFrameMode->getPifsTime();
}

simtime_t UpperMacContext::getRIFS() const
{
    return dataFrameMode->getRifsTime();
}

int UpperMacContext::getMinCW() const
{
    return dataFrameMode->getCwMin(); //TODO naming
}

int UpperMacContext::getMaxCW() const
{
    return dataFrameMode->getCwMax(); //TODO naming
}

int UpperMacContext::getShortRetryLimit() const
{
    return shortRetryLimit;
}

int UpperMacContext::getRtsThreshold() const
{
    return rtsThreshold;
}

simtime_t UpperMacContext::getAckTimeout() const
{
    return basicFrameMode->getPhyRxStartDelay() + getSIFS() + getAckDuration();
}

simtime_t UpperMacContext::getCtsTimeout() const
{
    return basicFrameMode->getPhyRxStartDelay() + getSIFS() +  getCtsDuration();
}

simtime_t UpperMacContext::getAckDuration() const
{
    return basicFrameMode->getDuration(LENGTH_ACK) + basicFrameMode->getSlotTime();
}

simtime_t UpperMacContext::getCtsDuration() const
{
    return basicFrameMode->getDuration(LENGTH_CTS) + basicFrameMode->getSlotTime();
}

Ieee80211RTSFrame *UpperMacContext::buildRtsFrame(Ieee80211DataOrMgmtFrame *frame) const
{
    Ieee80211RTSFrame *rtsFrame = new Ieee80211RTSFrame("RTS");
    rtsFrame->setTransmitterAddress(address);

    rtsFrame->setReceiverAddress(frame->getReceiverAddress());
    rtsFrame->setDuration(3 * getSIFS() + basicFrameMode->getDuration(LENGTH_CTS) +
            dataFrameMode->getDuration(frame->getBitLength()) +  //TODO maybe not always with dataFrameMode
            basicFrameMode->getDuration(LENGTH_ACK));
    return rtsFrame;
}

Ieee80211CTSFrame *UpperMacContext::buildCtsFrame(Ieee80211RTSFrame *rtsFrame) const
{
    Ieee80211CTSFrame *frame = new Ieee80211CTSFrame("CTS");
    setBasicBitrate(rtsFrame);
    frame->setReceiverAddress(rtsFrame->getTransmitterAddress());
    frame->setDuration(rtsFrame->getDuration() - getSIFS() - basicFrameMode->getDuration(LENGTH_CTS));
    return frame;
}

Ieee80211ACKFrame *UpperMacContext::buildAckFrame(Ieee80211DataOrMgmtFrame *frameToACK) const
{
    Ieee80211ACKFrame *ackFrame = new Ieee80211ACKFrame("ACK");
    setBasicBitrate(ackFrame);
    ackFrame->setReceiverAddress(frameToACK->getTransmitterAddress());

    if (!frameToACK->getMoreFragments())
        ackFrame->setDuration(0);
    else
        ackFrame->setDuration(frameToACK->getDuration() - getSIFS() - basicFrameMode->getDuration(LENGTH_ACK));
    return ackFrame;
}

Ieee80211DataOrMgmtFrame *UpperMacContext::buildBroadcastFrame(Ieee80211DataOrMgmtFrame *frameToSend) const //FIXME completely misleading name, random functionality
{
    Ieee80211DataOrMgmtFrame *frame = (Ieee80211DataOrMgmtFrame *)frameToSend->dup();
    frame->setDuration(0);
    return frame;
}

double UpperMacContext::computeFrameDuration(int bits, double bitrate) const
{
    const IIeee80211Mode *modType = modeSet->getMode(bps(bitrate));
    double duration = SIMTIME_DBL(modType->getDuration(bits));
    EV_DEBUG << " duration=" << duration * 1e6 << "us(" << bits << "bits " << bitrate / 1e6 << "Mbps)" << endl;
    return duration;
}

Ieee80211Frame *UpperMacContext::setControlBitrate(Ieee80211Frame *frame) const
{
    return setBitrate(frame, controlFrameMode);
}

Ieee80211Frame *UpperMacContext::setBasicBitrate(Ieee80211Frame *frame) const
{
    return setBitrate(frame, basicFrameMode);
}

Ieee80211Frame *UpperMacContext::setDataBitrate(Ieee80211Frame *frame) const
{
    return setBitrate(frame, dataFrameMode);
}

Ieee80211Frame *UpperMacContext::setBitrate(Ieee80211Frame* frame, const IIeee80211Mode* mode) const
{
    ASSERT(frame->getControlInfo() == nullptr);
    TransmissionRequest *ctrl = new TransmissionRequest();
    ctrl->setBitrate(bps(mode->getDataMode()->getNetBitrate()));
    frame->setControlInfo(ctrl);
    return frame;
}

bool UpperMacContext::isForUs(Ieee80211Frame *frame) const
{
    return frame->getReceiverAddress() == address;
}

bool UpperMacContext::isBroadcast(Ieee80211Frame *frame) const
{
    return frame && frame->getReceiverAddress().isBroadcast();
}

bool UpperMacContext::isCts(Ieee80211Frame *frame) const
{
    return dynamic_cast<Ieee80211CTSFrame *>(frame);
}

bool UpperMacContext::isAck(Ieee80211Frame *frame) const
{
    return dynamic_cast<Ieee80211ACKFrame *>(frame);
}

void UpperMacContext::transmitContentionFrame(int txIndex, Ieee80211Frame *frame, simtime_t ifs, simtime_t eifs, int cwMin, int cwMax, simtime_t slotTime, int retryCount, ITx::ICallback *completionCallback) const
{
    tx->transmitContentionFrame(txIndex, frame, ifs, eifs, cwMin, cwMax, slotTime, retryCount, completionCallback);
}

void UpperMacContext::transmitImmediateFrame(Ieee80211Frame *frame, simtime_t ifs, ITx::ICallback *completionCallback) const
{
    tx->transmitImmediateFrame(frame, ifs, completionCallback);
}

} // namespace ieee80211
} // namespace inet

