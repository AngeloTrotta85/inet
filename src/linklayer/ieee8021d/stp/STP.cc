//
// Copyright (C) 2013 OpenSim Ltd.
// Copyright (C) ANSA Team
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
// Authors: ANSA Team, Benjamin Martin Seregi
//

#include "EtherFrame.h"
#include "STP.h"
#include "InterfaceEntry.h"
#include "STPTester.h"

Define_Module(STP);

void STP::initialize(int stage)
{
    if (stage == 0)
    {
        treeColoring = par("treeColoring").boolValue();
        portCount = this->getParentModule()->gate("ethg$o", 0)->getVectorSize();
        tick = new cMessage("STP_TICK", 0);

        // connection to MACAddressTable for faster aging
        cModule * tmpMacTable = getParentModule()->getSubmodule("macTable");
        macTable = check_and_cast<MACAddressTable *>(tmpMacTable);
        WATCH(bridgeAddress);
    }
    else if (stage == 1)
    {
        NodeStatus * nodeStatus = dynamic_cast<NodeStatus *>(findContainingNode(this)->getSubmodule("status"));
        isOperational = (!nodeStatus) || nodeStatus->getState() == NodeStatus::UP;

        // obtain a bridge address from InterfaceTable
        ifTable = check_and_cast<IInterfaceTable*>(getModuleByPath(par("interfaceTableName")));
        InterfaceEntry * ifEntry = ifTable->getInterface(0);

        if (ifEntry != NULL)
            bridgeAddress = ifEntry->getMacAddress();

        initPortTable();
        helloTime = par("helloTime");
        maxAge = par("maxAge");
        fwdDelay = par("fwdDelay");
        bridgePriority = par("priority");
        ubridgePriority = bridgePriority;

        isRoot = true;
        topologyChangeNotification = false;
        topologyChangeRecvd = true;

        rootPriority = bridgePriority;
        rootAddress = bridgeAddress;
        rootPathCost = 0;
        rootPort = 0;
        cHelloTime = helloTime;
        cMaxAge = maxAge;
        cFwdDelay = fwdDelay;

        helloTimer = 0;
        allDesignated();
        scheduleAt(simTime() + 1, tick);
        if (treeColoring)
            getParentModule()->getDisplayString().setTagArg("i", 1, "#a5ffff");
    }
}

STP::~STP()
{
    cancelAndDelete(tick);
}

void STP::initPortTable()
{
    EV_DEBUG<< "IEE8021D Interface Data initialization. Setting port infos to the protocol defaults." << endl;
    for (unsigned int i = 0; i < portCount; i++)
    {
        Ieee8021DInterfaceData * port = getPortInterfaceData(i);
        port->setDefaultStpPortInfoData();
    }
}

void STP::handleMessage(cMessage * msg)
{
    if (!isOperational)
    {
        EV<< "Message '" << msg << "' arrived when module status is down, dropped it\n";
        delete msg;
        return;
    }

    cMessage * tmp = msg;

    if (!msg->isSelfMessage())
    {
        if (dynamic_cast<BPDU *>(tmp))
        {
            BPDU * bpdu = (BPDU *) tmp;

            if(bpdu->getBpduType() == 0) // configuration BPDU
                handleBPDU(bpdu);
            else if(bpdu->getBpduType() == 1)// TCN BPDU
                handleTCN(bpdu);
        }
        else
            delete msg;
    }
    else
    {
        if(msg == tick)
        {
            handleTick();
            if (treeColoring)
                colorTree();
            scheduleAt(simTime() + 1, tick);
        }
        else
            delete msg;
    }
}

void STP::colorTree()
{
    Ieee8021DInterfaceData * port;
    for (unsigned int i = 0; i < portCount; i++)
    {
        port = getPortInterfaceData(i);
        cGate * outGate = getParentModule()->gate("ethg$o", i);
        cGate * inputGate = getParentModule()->gate("ethg$i", i);
        cGate * outGateNext = outGate->getNextGate();
        cGate * inputGatePrev = inputGate->getPreviousGate();

        if (outGate && inputGate && inputGatePrev && outGateNext)
        {
            if (port->isForwarding())
            {
                if (port->getRole() == Ieee8021DInterfaceData::ROOT)
                {
                    outGate->getDisplayString().setTagArg("ls", 0, "#a5ffff");
                    outGate->getDisplayString().setTagArg("ls", 1, 3);

                    inputGate->getDisplayString().setTagArg("ls", 0, "#a5ffff");
                    inputGate->getDisplayString().setTagArg("ls", 1, 3);

                    outGateNext->getDisplayString().setTagArg("ls", 0, "#a5ffff");
                    outGateNext->getDisplayString().setTagArg("ls", 1, 3);

                    inputGatePrev->getDisplayString().setTagArg("ls", 0, "#a5ffff");
                    inputGatePrev->getDisplayString().setTagArg("ls", 1, 3);
                }
            }
            else
            {
                outGate->getDisplayString().setTagArg("ls", 0, "#000000");
                outGate->getDisplayString().setTagArg("ls", 1, 1);

                inputGate->getDisplayString().setTagArg("ls", 0, "#000000");
                inputGate->getDisplayString().setTagArg("ls", 1, 1);

                outGateNext->getDisplayString().setTagArg("ls", 0, "#000000");
                outGateNext->getDisplayString().setTagArg("ls", 1, 1);

                inputGatePrev->getDisplayString().setTagArg("ls", 0, "#000000");
                inputGatePrev->getDisplayString().setTagArg("ls", 1, 1);
            }
        }
        cModule * puerta = this->getParentModule()->getSubmodule("eth", i);
        if (puerta != NULL)
        {
            char buf[25];
            char sbuf[12];
            char rbuf[13];

            int role = port->getRole();
            switch (role)
            {
                case 0:
                    sprintf(rbuf, "ALTERNATE\n");
                    break;
                case 1:
                    sprintf(rbuf, "NOTASSIGNED\n");
                    break;
                case 2:
                    sprintf(rbuf, "DISABLED\n");
                    break;
                case 3:
                    sprintf(rbuf, "DESIGNATED\n");
                    break;
                case 4:
                    sprintf(rbuf, "BACKUP\n");
                    break;
                case 5:
                    sprintf(rbuf, "ROOT\n");
                    break;
                default:
                    sprintf(rbuf, "UNKNOWN\n");
                    break;
            }

            int state = port->getState();
            switch (state)
            {
                case 0:
                    sprintf(sbuf, "DISCARDING\n");
                    break;
                case 1:
                    sprintf(sbuf, "LEARNING\n");
                    break;
                case 2:
                    sprintf(sbuf, "FORWARDING\n");
                    break;
                default:
                    sprintf(sbuf, "UNKNOWN\n");
                    break;
            }

            sprintf(buf, "%s%s", rbuf, sbuf);
            puerta->getDisplayString().setTagArg("t", 0, buf);
        }
    }

    if (isOperational) //only when the router is working
    {
        if (getRootIndex() == -1)
        {
            // root mark
            this->getParentModule()->getDisplayString().setTagArg("i", 1, "#a5ffff");
        }
        else
        {
            // remove possible root mark
            this->getParentModule()->getDisplayString().setTagArg("i", 1, "");
        }
    }
}

void STP::handleBPDU(BPDU * bpdu)
{
    Ieee802Ctrl * controlInfo = check_and_cast<Ieee802Ctrl *>(bpdu->getControlInfo());
    int arrivalGate = controlInfo->getInterfaceId();
    Ieee8021DInterfaceData * port = getPortInterfaceData(arrivalGate);

    if (bpdu->getTcaFlag())
    {
        topologyChangeRecvd = true;
        topologyChangeNotification = false;
    }

    // get inferior BPDU, reply with superior
    if (!superiorBPDU(arrivalGate, bpdu))
    {
        if (port->getRole() == Ieee8021DInterfaceData::DESIGNATED)
        {
            EV_DETAIL<< "Inferior Configuration BPDU " << bpdu << " arrived on port=" << arrivalGate << " responding to it with a superior BPDU." << endl;
            generateBPDU(arrivalGate);
        }
    }

    // BPDU from root
    else if (port->getRole() == Ieee8021DInterfaceData::ROOT)
    {
        EV_INFO << "Configuration BPDU " << bpdu << " arrived from Root Switch." << endl;

        if(bpdu->getTcFlag())
        {
            EV_DEBUG << "MACAddressTable aging time set to " << cFwdDelay << "." << endl;
            macTable->setAgingTime(cFwdDelay);

            // config BPDU with TC flag
            for (unsigned int i = 0; i < desPorts.size(); i++)
                generateBPDU(desPorts.at(i), MACAddress::STP_MULTICAST_ADDRESS,true,false);
        }
        else
        {
            macTable->resetDefaultAging();

            EV_INFO << "Sending BPDUs on all designated ports." << endl;

            // BPDUs are sent on all designated ports
            for (unsigned int i = 0; i < desPorts.size(); i++)
                generateBPDU(desPorts.at(i));
        }
    }

    tryRoot();
    delete bpdu;
}

void STP::handleTCN(BPDU * tcn)
{
    EV_INFO<< "Topology Change Notification BDPU " << tcn << " arrived." << endl;
    topologyChangeNotification = true;

    Ieee802Ctrl * controlInfo = check_and_cast<Ieee802Ctrl *>(tcn->getControlInfo());
    int arrivalGate = controlInfo->getInterfaceId();
    MACAddress address = controlInfo->getSrc();

    // send ACK to the sender
    EV_INFO << "Sending Topology Change Notification ACK." << endl;
    generateBPDU(arrivalGate,address,false,true);

    controlInfo->setInterfaceId(rootPort);// send TCN to the Root Switch

    if (!isRoot)
        send(tcn, "relayOut");
    else
        delete tcn;
}

void STP::generateBPDU(int port, const MACAddress& address, bool tcFlag, bool tcaFlag)
{
    BPDU * bpdu = new BPDU();
    Ieee802Ctrl * controlInfo = new Ieee802Ctrl();
    controlInfo->setDest(address);
    controlInfo->setInterfaceId(port);

    bpdu->setProtocolIdentifier(0);
    bpdu->setProtocolVersionIdentifier(0);
    bpdu->setBpduType(0); // 0 if configuration BPDU

    bpdu->setBridgeAddress(bridgeAddress);
    bpdu->setBridgePriority(bridgePriority);
    bpdu->setRootPathCost(rootPathCost);
    bpdu->setRootAddress(rootAddress);
    bpdu->setRootPriority(rootPriority);
    bpdu->setPortNum(port);
    bpdu->setPortPriority(getPortInterfaceData(port)->getPriority());
    bpdu->setMessageAge(0);
    bpdu->setMaxAge(cMaxAge);
    bpdu->setHelloTime(cHelloTime);
    bpdu->setForwardDelay(cFwdDelay);

    if (topologyChangeNotification)
    {
        if (isRoot || tcFlag)
        {
            bpdu->setTcFlag(true);
            bpdu->setTcaFlag(false);
        }

        else if (tcaFlag)
        {
            bpdu->setTcFlag(false);
            bpdu->setTcaFlag(true);
        }
    }

    bpdu->setControlInfo(controlInfo);

    send(bpdu, "relayOut");
}

void STP::generateTCN()
{
    // there is something to notify
    if (topologyChangeNotification || !topologyChangeRecvd)
    {
        if (getPortInterfaceData(rootPort)->getRole() == Ieee8021DInterfaceData::ROOT)
        {
            // exist root port to notifying
            topologyChangeNotification = false;
            BPDU * tcn = new BPDU();
            tcn->setProtocolIdentifier(0);
            tcn->setProtocolVersionIdentifier(0);

            // 1 if Topology Change Notification BPDU
            tcn->setBpduType(1);

            Ieee802Ctrl * controlInfo = new Ieee802Ctrl();
            controlInfo->setDest(MACAddress::STP_MULTICAST_ADDRESS);
            controlInfo->setInterfaceId(rootPort);
            tcn->setControlInfo(controlInfo);

            EV_INFO<< "The topology was changed. Sending Topology Change Notification BPDU " << tcn << " to the Root Switch." << endl;
            send(tcn, "relayOut");
        }
    }
}

bool STP::superiorBPDU(int portNum, BPDU * bpdu)
{
    Ieee8021DInterfaceData * port = getPortInterfaceData(portNum);
    Ieee8021DInterfaceData * xBpdu = new Ieee8021DInterfaceData();

    int result;

    xBpdu->setRootPriority(bpdu->getRootPriority());
    xBpdu->setRootAddress(bpdu->getRootAddress());
    xBpdu->setRootPathCost(bpdu->getRootPathCost() + port->getLinkCost());
    xBpdu->setBridgePriority(bpdu->getBridgePriority());
    xBpdu->setBridgeAddress(bpdu->getBridgeAddress());
    xBpdu->setPortPriority(bpdu->getPortPriority());
    xBpdu->setPortNum(bpdu->getPortNum());

    result = superiorTPort(port, xBpdu);

    // port is superior
    if (result > 0)
        return false;

    if (result < 0)
    {
        // BPDU is superior
        port->setFdWhile(0); // renew info
        port->setState(Ieee8021DInterfaceData::DISCARDING);
        setSuperiorBPDU(portNum, bpdu); // renew information
        return true;
    }

    setSuperiorBPDU(portNum, bpdu); // renew information
    delete xBpdu;
    return true;
}

void STP::setSuperiorBPDU(int portNum, BPDU * bpdu)
{
    // BDPU is out-of-date
    if (bpdu->getMessageAge() >= bpdu->getMaxAge())
        return;

    Ieee8021DInterfaceData * portData = getPortInterfaceData(portNum);

    portData->setRootPriority(bpdu->getRootPriority());
    portData->setRootAddress(bpdu->getRootAddress());
    portData->setRootPathCost(bpdu->getRootPathCost() + portData->getLinkCost());
    portData->setBridgePriority(bpdu->getBridgePriority());
    portData->setBridgeAddress(bpdu->getBridgeAddress());
    portData->setPortPriority(bpdu->getPortPriority());
    portData->setPortNum(bpdu->getPortNum());
    portData->setMaxAge(bpdu->getMaxAge());
    portData->setFwdDelay(bpdu->getForwardDelay());
    portData->setHelloTime(bpdu->getHelloTime());

    // we just set new port info so reset the age timer
    portData->setAge(0);

}

void STP::generator()
{
    // only the root switch can generate Hello BPDUs
    if (!isRoot)
        return;

    EV_INFO<< "It is hello time. Root switch sending hello BDPUs on all its ports." << endl;

    // send hello BDPUs on all ports
    for (unsigned int i = 0; i < portCount; i++)
        generateBPDU(i);

}

void STP::handleTick()
{
    // bridge timers
    convergenceTime++;

    // hello BDPU timer
    if (isRoot)
        helloTimer = helloTimer + 1;
    else
        helloTimer = 0;

    for (unsigned int i = 0; i < portCount; i++)
    {
        Ieee8021DInterfaceData * port = getPortInterfaceData(i);

        // disabled ports don't count
        if (port->getRole() == Ieee8021DInterfaceData::DISABLED)
            continue;

        // increment the MessageAge and FdWhile timers
        if (port->getRole() != Ieee8021DInterfaceData::DESIGNATED)
        {
            EV_DEBUG<< "Message Age timer incremented on port=" << i << endl;
            port->setAge(port->getAge() + 1);
        }
        if (port->getRole() == Ieee8021DInterfaceData::ROOT || port->getRole() == Ieee8021DInterfaceData::DESIGNATED)
        {
            EV_DEBUG<< "Forward While timer incremented on port=" << i << endl;
            port->setFdWhile(port->getFdWhile() + 1);
        }

    }
    checkTimers();
    checkParametersChange();
    generateTCN();
}

void STP::checkTimers()
{
    Ieee8021DInterfaceData * port;

    // hello timer check
    if (helloTimer >= cHelloTime)
    {
        generator();
        helloTimer = 0;
    }

    // information age check
    for (unsigned int i = 0; i < portCount; i++)
    {
        port = getPortInterfaceData(i);

        if (port->getAge() >= cMaxAge)
        {
            EV_DETAIL<< "Port=" << i << " reached its maximum age. Setting it to the default port info." << endl;
            if (port->getRole() == Ieee8021DInterfaceData::ROOT)
            {
                port->setDefaultStpPortInfoData();
                lostRoot();
            }
            else
            {
                port->setDefaultStpPortInfoData();
                lostAlternate(i);
            }
        }
    }

    // fdWhile timer
    for (unsigned int i = 0; i < portCount; i++)
    {
        port = getPortInterfaceData(i);

        // ROOT / DESIGNATED, can transition
        if (port->getRole() == Ieee8021DInterfaceData::ROOT || port->getRole() == Ieee8021DInterfaceData::DESIGNATED)
        {
            if (port->getFdWhile() >= cFwdDelay)
            {
                switch (port->getState())
                {
                    case Ieee8021DInterfaceData::DISCARDING:
                        EV_DETAIL<< "Port=" << i << " goes into learning state." << endl;
                        port->setState(Ieee8021DInterfaceData::LEARNING);
                        port->setFdWhile(0);
                        break;
                        case Ieee8021DInterfaceData::LEARNING:
                        EV_DETAIL << "Port=" << i << " goes into forwarding state." << endl;
                        port->setState(Ieee8021DInterfaceData::FORWARDING);
                        port->setFdWhile(0);
                        break;
                        default:
                        port->setFdWhile(0);
                        break;
                    }

                }
            }
            else
            {
                EV_DETAIL << "Port=" << i << " goes into discarding state." << endl;
                port->setFdWhile(0);
                port->setState(Ieee8021DInterfaceData::DISCARDING);
            }
        }

    }

void STP::checkParametersChange()
{
    if (isRoot)
    {
        cHelloTime = helloTime;
        cMaxAge = maxAge;
        cFwdDelay = fwdDelay;
    }
    if (ubridgePriority != bridgePriority)
    {
        ubridgePriority = bridgePriority;
        reset();
    }
}

Ieee8021DInterfaceData * STP::getPortInterfaceData(unsigned int portNum)
{
    cGate * gate = this->getParentModule()->gate("ethg$o", portNum);
    InterfaceEntry * gateIfEntry = ifTable->getInterfaceByNodeOutputGateId(gate->getId());
    Ieee8021DInterfaceData * portData = gateIfEntry->ieee8021DData();

    if (!portData)
        throw cRuntimeError("IEEE8021DInterfaceData not found.");

    return portData;
}

bool STP::checkRootEligibility()
{
    Ieee8021DInterfaceData * port;

    for (unsigned int i = 0; i < portCount; i++)
    {
        port = getPortInterfaceData(i);

        if (superiorID(port->getRootPriority(), port->getRootAddress(), bridgePriority, bridgeAddress) > 0)
            return false;

    }

    return true;
}

void STP::tryRoot()
{
    if (checkRootEligibility())
    {
        EV_DETAIL<< "Switch is elected as root switch." << endl;
        if (treeColoring)
            getParentModule()->getDisplayString().setTagArg("i",1,"#a5ffff");
        isRoot = true;
        allDesignated();
        rootPriority = bridgePriority;
        rootAddress = bridgeAddress;
        rootPathCost = 0;
        cHelloTime = helloTime;
        cMaxAge = maxAge;
        cFwdDelay = fwdDelay;
    }
    else
    {
        if (treeColoring)
            getParentModule()->getDisplayString().setTagArg("i",1,"");

        isRoot = false;
        selectRootPort();
        selectDesignatedPorts();
    }

}

int STP::superiorID(unsigned int aPriority, MACAddress aAddress, unsigned int bPriority, MACAddress bAddress)
{
    if (aPriority < bPriority)
        return 1; // A is superior
    else if (aPriority > bPriority)
        return -1;

    // APR == BPR
    if (aAddress.compareTo(bAddress) == -1)
        return 1; // A is superior
    else if (aAddress.compareTo(bAddress) == 1)
        return -1;

    // A==B
    // (can happen if bridge have two port connected to one not bridged lan,
    // "cable loopback"
    return 0;
}

int STP::superiorPort(unsigned int aPriority, unsigned int aNum, unsigned int bPriority, unsigned int bNum)
{
    if (aPriority < bPriority)
        return 1; // A is superior

    else if (aPriority > bPriority)
        return -1;

    // APR == BPR
    if (aNum < bNum)
        return 1; // A is superior

    else if (aNum > bNum)
        return -1;

    // A==B
    return 0;
}

int STP::superiorTPort(Ieee8021DInterfaceData * portA, Ieee8021DInterfaceData * portB)
{
    int result;

    result = superiorID(portA->getRootPriority(), portA->getRootAddress(), portB->getRootPriority(),
            portB->getRootAddress());

    // not same, so pass result
    if (result != 0)
        return result;

    if (portA->getRootPathCost() < portB->getRootPathCost())
        return 1;

    if (portA->getRootPathCost() > portB->getRootPathCost())
        return -1;

    // designated bridge
    result = superiorID(portA->getBridgePriority(), portA->getBridgeAddress(), portB->getBridgePriority(),
            portB->getBridgeAddress());

    // not same, so pass result
    if (result != 0)
        return result;

    // designated port of designated Bridge
    result = superiorPort(portA->getPortPriority(), portA->getPortNum(), portB->getPortPriority(), portB->getPortNum());

    // not same, so pass result
    if (result != 0)
        return result;

    return 0; // same
}

void STP::selectRootPort()
{
    desPorts.clear();
    unsigned int xRootPort = 0;
    int result;
    Ieee8021DInterfaceData * best = getPortInterfaceData(0);
    Ieee8021DInterfaceData * currentPort;

    for (unsigned int i = 0; i < portCount; i++)
    {
        currentPort = getPortInterfaceData(i);
        currentPort->setRole(Ieee8021DInterfaceData::NOTASSIGNED);
        result = superiorTPort(currentPort, best);
        if (result > 0)
        {
            xRootPort = i;
            best = currentPort;
            continue;
        }
        if (result < 0)
        {
            continue;
        }
        if (currentPort->getPriority() < best->getPriority())
        {
            xRootPort = i;
            best = currentPort;
            continue;
        }
        if (currentPort->getPriority() > best->getPriority())
            continue;
    }

    if (rootPort != xRootPort)
    {
        EV_DETAIL<< "Port=" << xRootPort << " selected as root port." << endl;
        topologyChangeNotification = true;
    }
    rootPort = xRootPort;
    getPortInterfaceData(rootPort)->setRole(Ieee8021DInterfaceData::ROOT);
    rootPathCost = best->getRootPathCost();
    rootAddress = best->getRootAddress();
    rootPriority = best->getRootPriority();

    cMaxAge = best->getMaxAge();
    cFwdDelay = best->getFwdDelay();
    cHelloTime = best->getHelloTime();

}

void STP::selectDesignatedPorts()
{
    // select designated ports
    desPorts.clear();
    Ieee8021DInterfaceData * port;
    Ieee8021DInterfaceData * bridgeGlobal = new Ieee8021DInterfaceData();
    int result;

    bridgeGlobal->setBridgePriority(bridgePriority);
    bridgeGlobal->setBridgeAddress(bridgeAddress);
    bridgeGlobal->setRootAddress(rootAddress);
    bridgeGlobal->setRootPriority(rootPriority);

    for (unsigned int i = 0; i < portCount; i++)
    {
        port = getPortInterfaceData(i);

        if (port->getRole() == Ieee8021DInterfaceData::ROOT || port->getRole() == Ieee8021DInterfaceData::DISABLED)
            continue;

        bridgeGlobal->setPortPriority(port->getPriority());
        bridgeGlobal->setPortNum(i);

        bridgeGlobal->setRootPathCost(rootPathCost + port->getLinkCost());

        result = superiorTPort(bridgeGlobal, port);

        if (result > 0)
        {
            EV_DETAIL<< "Port=" << i << " is elected as designated port." << endl;
            desPorts.push_back(i);
            port->setRole(Ieee8021DInterfaceData::DESIGNATED);
            continue;
        }
        if (result < 0)
        {
            EV_DETAIL<< "Port=" << i << " goes into alternate role." << endl;
            port->setRole(Ieee8021DInterfaceData::ALTERNATE);
            continue;
        }
    }
    delete bridgeGlobal;
}

void STP::allDesignated()
{
    // all ports of the root switch are designated ports
    EV_DETAIL<< "All ports become designated." << endl; // todo

    Ieee8021DInterfaceData * port;
    desPorts.clear();
    for (unsigned int i = 0; i < portCount; i++)
    {
        port = getPortInterfaceData(i);
        if (port->getRole() == Ieee8021DInterfaceData::DISABLED)
            continue;

        port->setRole(Ieee8021DInterfaceData::DESIGNATED);
        desPorts.push_back(i);
    }
}

void STP::lostRoot()
{
    topologyChangeNotification = true;
    tryRoot();
}

void STP::lostAlternate(int port)
{
    selectDesignatedPorts();
    topologyChangeNotification = true;
}

void STP::reset()
{
    // upon booting all switches believe themselves to be the root
    if (treeColoring)
        getParentModule()->getDisplayString().setTagArg("i", 1, "#a5ffff");
    isRoot = true;
    rootPriority = bridgePriority;
    rootAddress = bridgeAddress;
    rootPathCost = 0;
    cHelloTime = helloTime;
    cMaxAge = maxAge;
    cFwdDelay = fwdDelay;
    allDesignated();

    for (unsigned int i = 0; i < portCount; i++)
    {
        Ieee8021DInterfaceData * port = getPortInterfaceData(i);
        port->setDefaultStpPortInfoData();
    }
}

void STP::start()
{
    // obtain a bridge address from InterfaceTable
    ifTable = check_and_cast<IInterfaceTable*>(getModuleByPath(par("interfaceTableName")));
    InterfaceEntry * ifEntry = ifTable->getInterface(0);
    if (ifEntry != NULL)
        bridgeAddress = ifEntry->getMacAddress();
    if (treeColoring)
    {
        getParentModule()->getDisplayString().setTagArg("i", 1, "#a5ffff");
        this->getParentModule()->getDisplayString().removeTag("i2");
    }

    isOperational = true;
    initPortTable();
    ubridgePriority = bridgePriority;
    isRoot = true;
    topologyChangeNotification = true;
    topologyChangeRecvd = true;
    rootPriority = bridgePriority;
    rootAddress = bridgeAddress;
    rootPathCost = 0;
    rootPort = 0;
    cHelloTime = helloTime;
    cMaxAge = maxAge;
    cFwdDelay = fwdDelay;
    helloTimer = 0;
    allDesignated();

    scheduleAt(simTime() + 1, tick);
}

void STP::stop()
{
    isOperational = false;
    isRoot = false;
    topologyChangeNotification = true;

    if (treeColoring)
    {
        for (unsigned int i = 0; i < portCount; i++)
        {
            cGate * outGate = getParentModule()->gate("ethg$o", i);
            cGate * inputGate = getParentModule()->gate("ethg$i", i);
            cGate * outGateNext = outGate->getNextGate();
            cGate * inputGatePrev = inputGate->getPreviousGate();

            if (outGate && inputGate && inputGatePrev && outGateNext)
            {
                outGate->getDisplayString().setTagArg("ls", 0, "#000000");
                outGate->getDisplayString().setTagArg("ls", 1, 1);

                inputGate->getDisplayString().setTagArg("ls", 0, "#000000");
                inputGate->getDisplayString().setTagArg("ls", 1, 1);

                outGateNext->getDisplayString().setTagArg("ls", 0, "#000000");
                outGateNext->getDisplayString().setTagArg("ls", 1, 1);

                inputGatePrev->getDisplayString().setTagArg("ls", 0, "#000000");
                inputGatePrev->getDisplayString().setTagArg("ls", 1, 1);
            }
        }
        getParentModule()->getDisplayString().setTagArg("i", 1, "");
    }
    this->getParentModule()->getDisplayString().setTagArg("i2", 0, "status/stop");
    cancelEvent(tick);
}

bool STP::handleOperationStage(LifecycleOperation * operation, int stage, IDoneCallback * doneCallback)
{
    Enter_Method_Silent();

    if (dynamic_cast<NodeStartOperation *>(operation))
    {
        if (stage == NodeStartOperation::STAGE_LINK_LAYER)
        {
            start();
        }
    }
    else if (dynamic_cast<NodeShutdownOperation *>(operation))
    {
        if (stage == NodeShutdownOperation::STAGE_LINK_LAYER)
        {
            stop();
        }
    }
    else if (dynamic_cast<NodeCrashOperation *>(operation))
    {
        if (stage == NodeCrashOperation::STAGE_CRASH)
        {
            stop();
        }
    }
    else
    {
        throw cRuntimeError("Unsupported operation '%s'", operation->getClassName());
    }
    return true;
}

int STP::getRootIndex()
{
    int result = -1;
    for (unsigned int i = 0; i < portCount; i++)
    {
        Ieee8021DInterfaceData * iPort = getPortInterfaceData(i);
        if (iPort->getRole() == Ieee8021DInterfaceData::ROOT)
        {
            result = i;
            break;
        }
    }
    return result;
}
