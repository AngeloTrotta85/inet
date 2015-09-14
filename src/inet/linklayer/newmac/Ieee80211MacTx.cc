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

#include "Ieee80211MacTx.h"
#include "Ieee80211MacContentionTx.h"
#include "Ieee80211MacImmediateTx.h"
#include "IIeee80211UpperMac.h"
#include "Ieee80211NewMac.h" //TODO

namespace inet {

namespace ieee80211 {

Ieee80211MacTx::Ieee80211MacTx()
{
    for (int i = 0; i < MAX_NUM_CONTENTIONTX; i++)
        contentionTx[i] = nullptr;
}

Ieee80211MacTx::~Ieee80211MacTx()
{
    for (int i = 0; i < MAX_NUM_CONTENTIONTX; i++)
        delete contentionTx[i];
    delete immediateTx;
}

void Ieee80211MacTx::initialize()
{
    mac = check_and_cast<Ieee80211NewMac*>(getParentModule());  //TODO
    upperMac = check_and_cast<IIeee80211UpperMac*>(getModuleByPath("^.upperMac")); //TODO

    numContentionTx = 4; //TODO
    ASSERT(numContentionTx < MAX_NUM_CONTENTIONTX);
    for (int i = 0; i < numContentionTx; i++)
        contentionTx[i] = new Ieee80211MacContentionTx(mac, i); //TODO factory method
    immediateTx = new Ieee80211MacImmediateTx(mac); //TODO factory method

}

void Ieee80211MacTx::transmitContentionFrame(int txIndex, Ieee80211Frame *frame, simtime_t ifs, simtime_t eifs, int cwMin, int cwMax, simtime_t slotTime, int retryCount, ICallback *completionCallback)
{
    ASSERT(txIndex >= 0 && txIndex < numContentionTx);
    contentionTx[txIndex]->transmitContentionFrame(frame, ifs, eifs, cwMin, cwMax, slotTime, retryCount, completionCallback);
}

void Ieee80211MacTx::transmitImmediateFrame(Ieee80211Frame *frame, simtime_t ifs, ICallback *completionCallback)
{
    immediateTx->transmitImmediateFrame(frame, ifs, completionCallback);
}

void Ieee80211MacTx::mediumStateChanged(bool mediumFree)
{
    for (int i = 0; i < numContentionTx; i++)
        contentionTx[i]->mediumStateChanged(mediumFree);
}

void Ieee80211MacTx::radioTransmissionFinished()
{
    for (int i = 0; i < numContentionTx; i++)
        contentionTx[i]->radioTransmissionFinished();
}

void Ieee80211MacTx::lowerFrameReceived(bool isFcsOk)
{
    for (int i = 0; i < numContentionTx; i++)
        contentionTx[i]->lowerFrameReceived(isFcsOk);
}


}

} //namespace

