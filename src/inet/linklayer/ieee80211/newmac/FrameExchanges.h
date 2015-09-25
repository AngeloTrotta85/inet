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

#ifndef __INET_FRAMEEXCHANGES_H
#define __INET_FRAMEEXCHANGES_H

#include "FrameExchange.h"
#include "inet/physicallayer/ieee80211/mode/IIeee80211Mode.h"

namespace inet {
namespace ieee80211 {

class Ieee80211DataOrMgmtFrame;

//using namespace inet::physicallayer;
using inet::physicallayer::AccessCategory;
//using inet::physicallayer::AccessCategory;
//typedef inet::physicallayer::AccessCategory AccessCategory;

// just to demonstrate the use FsmBasedFrameExchange; otherwise we prefer the step-based because it's simpler
class SendDataWithAckFsmBasedFrameExchange : public FsmBasedFrameExchange
{
    protected:
        Ieee80211DataOrMgmtFrame *frame;
        int txIndex;
        int accessCategory;
        cMessage *ackTimer = nullptr;
        int retryCount = 0;

        enum State { INIT, TRANSMITDATA, WAITACK, SUCCESS, FAILURE };
        State state = INIT;

    protected:
        bool handleWithFSM(EventType event, cMessage *frameOrTimer) override;

        void transmitDataFrame();
        void retryDataFrame();
        void scheduleAckTimeout();
        bool isAck(Ieee80211Frame *frame);

    public:
        SendDataWithAckFsmBasedFrameExchange(cSimpleModule *ownerModule, IUpperMacContext *context, IFinishedCallback *callback, Ieee80211DataOrMgmtFrame *frame, int txIndex, int accessCategory);
        ~SendDataWithAckFsmBasedFrameExchange();
};

class SendDataWithAckFrameExchange : public StepBasedFrameExchange
{
    protected:
        Ieee80211DataOrMgmtFrame *dataFrame = nullptr;
        int retryCount = 0;
    protected:
        virtual void doStep(int step) override;
        virtual bool processReply(int step, Ieee80211Frame *frame) override;
        virtual void processTimeout(int step) override;
        virtual void processInternalCollision(int step) override;
    public:
        SendDataWithAckFrameExchange(cSimpleModule *ownerModule, IUpperMacContext *context, IFinishedCallback *callback, Ieee80211DataOrMgmtFrame *dataFrame, int txIndex, int accessCategory);
        ~SendDataWithAckFrameExchange();
};

class SendDataWithRtsCtsFrameExchange : public StepBasedFrameExchange
{
    protected:
        Ieee80211DataOrMgmtFrame *dataFrame = nullptr;
        int retryCount = 0;
    protected:
        virtual void doStep(int step) override;
        virtual bool processReply(int step, Ieee80211Frame *frame) override;
        virtual void processTimeout(int step) override;
        virtual void processInternalCollision(int step) override;
    public:
        SendDataWithRtsCtsFrameExchange(cSimpleModule *ownerModule, IUpperMacContext *context, IFinishedCallback *callback, Ieee80211DataOrMgmtFrame *dataFrame, int txIndex=0, int accessCategory=AccessCategory::AC_LEGACY);
        ~SendDataWithRtsCtsFrameExchange();
};

class SendMulticastDataFrameExchange : public StepBasedFrameExchange
{
    protected:
        Ieee80211DataOrMgmtFrame *dataFrame = nullptr;
        int retryCount = 0; // internal collisions are still possible (EDCA)
    protected:
        virtual void doStep(int step) override;
        virtual bool processReply(int step, Ieee80211Frame *frame) override;
        virtual void processTimeout(int step) override;
        virtual void processInternalCollision(int step) override;
    public:
        SendMulticastDataFrameExchange(cSimpleModule *ownerModule, IUpperMacContext *context, IFinishedCallback *callback, Ieee80211DataOrMgmtFrame *dataFrame, int txIndex, int accessCategory);
        ~SendMulticastDataFrameExchange();
};

} // namespace ieee80211
} // namespace inet

#endif

