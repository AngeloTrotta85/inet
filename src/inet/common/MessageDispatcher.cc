//
// Copyright (C) 2013 OpenSim Ltd.
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

#include "inet/applications/common/SocketTag_m.h"
#include "inet/common/MessageDispatcher.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"

namespace inet {

Define_Module(MessageDispatcher);

MessageDispatcher::MessageDispatcher()
{
}

void MessageDispatcher::initialize()
{
    WATCH_MAP(socketIdToUpperLayerGateIndex);
    WATCH_MAP(interfaceIdToLowerLayerGateIndex);
    WATCH_MAP(protocolIdToUpperLayerGateIndex);
    WATCH_MAP(protocolIdToLowerLayerGateIndex);
}

int MessageDispatcher::computeSocketReqSocketId(Packet *packet)
{
    auto *socketReq = packet->findTag<SocketReq>();
    return socketReq != nullptr ? socketReq->getSocketId() : -1;
}

int MessageDispatcher::computeSocketIndSocketId(Packet *packet)
{
    auto *socketInd = packet->findTag<SocketInd>();
    return socketInd != nullptr ? socketInd->getSocketId() : -1;
}

int MessageDispatcher::computeInterfaceId(Packet *packet)
{
    auto interfaceReq = packet->findTag<InterfaceReq>();
    return interfaceReq != nullptr ? interfaceReq->getInterfaceId() : -1;
}

const Protocol *MessageDispatcher::computeProtocol(Packet *packet)
{
    auto protocolReq = packet->findTag<DispatchProtocolReq>();;
    return protocolReq != nullptr ? protocolReq->getProtocol() : nullptr;
}

int MessageDispatcher::computeSocketReqSocketId(Message *message)
{
    auto *socketReq = message->findTag<SocketReq>();
    return socketReq != nullptr ? socketReq->getSocketId() : -1;
}

int MessageDispatcher::computeSocketIndSocketId(Message *message)
{
    auto *socketInd = message->findTag<SocketInd>();
    return socketInd != nullptr ? socketInd->getSocketId() : -1;
}

int MessageDispatcher::computeInterfaceId(Message *message)
{
    auto interfaceReq = message->findTag<InterfaceReq>();
    return interfaceReq != nullptr ? interfaceReq->getInterfaceId() : -1;
}

const Protocol *MessageDispatcher::computeProtocol(Message *message)
{
    auto protocolReq = message->findTag<DispatchProtocolReq>();;
    return protocolReq != nullptr ? protocolReq->getProtocol() : nullptr;
}

void MessageDispatcher::arrived(cMessage *message, cGate *inGate, simtime_t t) {
    Enter_Method_Silent();
    cGate *outGate = nullptr;
    if (!strcmp("upperLayerIn", inGate->getName())) {
        if (message->isPacket())
            outGate = handleUpperLayerPacket(check_and_cast<Packet *>(message), inGate);
        else
            outGate = handleUpperLayerMessage(check_and_cast<Request *>(message), inGate);
    }
    else if (!strcmp("lowerLayerIn", inGate->getName())) {
        if (message->isPacket())
            outGate = handleLowerLayerPacket(check_and_cast<Packet *>(message), inGate);
        else
            outGate = handleLowerLayerMessage(check_and_cast<Indication *>(message), inGate);
    }
    else
        throw cRuntimeError("Message %s(%s) arrived on unknown '%s' gate", message->getName(), message->getClassName(), inGate->getFullName());
    outGate->deliver(message, t);
}

cGate *MessageDispatcher::handleUpperLayerPacket(Packet *packet, cGate *inGate)
{
    int interfaceId = computeInterfaceId(packet);
    auto protocol = computeProtocol(packet);
    if (protocol != nullptr) {
        auto it = protocolIdToLowerLayerGateIndex.find(protocol->getId());
        if (it != protocolIdToLowerLayerGateIndex.end())
            return gate("lowerLayerOut", it->second);
        else
            throw cRuntimeError("handleUpperLayerPacket(): Unknown protocol: id = %d, name = %s", protocol->getId(), protocol->getName());
    }
    else if (interfaceId != -1) {
        auto it = interfaceIdToLowerLayerGateIndex.find(interfaceId);
        if (it != interfaceIdToLowerLayerGateIndex.end())
            return gate("lowerLayerOut", it->second);
        else
            throw cRuntimeError("handleUpperLayerPacket(): Unknown interface: id = %d", interfaceId);
    }
    else
        throw cRuntimeError("handleUpperLayerPacket(): Unknown packet: %s(%s)", packet->getName(), packet->getClassName());
}

cGate *MessageDispatcher::handleLowerLayerPacket(Packet *packet, cGate *inGate)
{
    int socketId = computeSocketIndSocketId(packet);
    auto protocol = computeProtocol(packet);
    if (socketId != -1) {
        auto it = socketIdToUpperLayerGateIndex.find(socketId);
        if (it != socketIdToUpperLayerGateIndex.end())
            return gate("upperLayerOut", it->second);
        else
            throw cRuntimeError("handleLowerLayerPacket(): Unknown socket, id = %d", socketId);
    }
    else if (protocol != nullptr) {
        auto it = protocolIdToUpperLayerGateIndex.find(protocol->getId());
        if (it != protocolIdToUpperLayerGateIndex.end())
            return gate("upperLayerOut", it->second);
        else
            throw cRuntimeError("handleLowerLayerPacket(): Unknown protocol: id = %d, name = %s", protocol->getId(), protocol->getName());
    }
    else
        throw cRuntimeError("handleLowerLayerPacket(): Unknown packet: %s(%s)", packet->getName(), packet->getClassName());
}

cGate *MessageDispatcher::handleUpperLayerMessage(Request *request, cGate *inGate)
{
    int socketId = computeSocketReqSocketId(request);
    int interfaceId = computeInterfaceId(request);
    auto protocol = computeProtocol(request);
    if (socketId != -1) {
        auto it = socketIdToUpperLayerGateIndex.find(socketId);
        if (it == socketIdToUpperLayerGateIndex.end())
            socketIdToUpperLayerGateIndex[socketId] = inGate->getIndex();
        else if (it->first != socketId)
            throw cRuntimeError("handleUpperLayerMessage(): Socket is already registered: id = %d, gate = %d, new gate = %d", socketId, it->second, inGate->getIndex());
    }
    if (protocol != nullptr) {
        auto it = protocolIdToLowerLayerGateIndex.find(protocol->getId());
        if (it != protocolIdToLowerLayerGateIndex.end())
            return gate("lowerLayerOut", it->second);
        else
            throw cRuntimeError("handleUpperLayerMessage(): Unknown protocol: id = %d, name = %s", protocol->getId(), protocol->getName());
    }
    else if (interfaceId != -1) {
        auto it = interfaceIdToLowerLayerGateIndex.find(interfaceId);
        if (it != interfaceIdToLowerLayerGateIndex.end())
            return gate("lowerLayerOut", it->second);
        else
            throw cRuntimeError("handleUpperLayerMessage(): Unknown interface: id = %d", interfaceId);
    }
    else
        throw cRuntimeError("handleUpperLayerMessage(): Unknown message: %s(%s)", request->getName(), request->getClassName());
}

cGate *MessageDispatcher::handleLowerLayerMessage(Indication *indication, cGate *inGate)
{
    int socketId = computeSocketIndSocketId(indication);
    auto protocol = computeProtocol(indication);
    if (socketId != -1) {
        auto it = socketIdToUpperLayerGateIndex.find(socketId);
        if (it != socketIdToUpperLayerGateIndex.end())
            return gate("upperLayerOut", it->second);
        else
            throw cRuntimeError("handleLowerLayerMessage(): Unknown socket, id = %d", socketId);
    }
    else if (protocol != nullptr) {
        auto it = protocolIdToUpperLayerGateIndex.find(protocol->getId());
        if (it != protocolIdToUpperLayerGateIndex.end())
            return gate("uppwerLayerOut", it->second);
        else
            throw cRuntimeError("handleLowerLayerMessage(): Unknown protocol: id = %d", protocol->getId());
    }
    else
        throw cRuntimeError("handleLowerLayerMessage(): Unknown message: %s(%s)", indication->getName(), indication->getClassName());
}

void MessageDispatcher::handleRegisterProtocol(const Protocol& protocol, cGate *protocolGate)
{
    if (!strcmp("upperLayerIn", protocolGate->getName())) {
        if (protocolIdToUpperLayerGateIndex.find(protocol.getId()) != protocolIdToUpperLayerGateIndex.end())
            throw cRuntimeError("handleRegisterProtocol(): Upper layer protocol is already registered: %s", protocol.str().c_str());
        protocolIdToUpperLayerGateIndex[protocol.getId()] = protocolGate->getIndex();
        int size = gateSize("lowerLayerOut");
        for (int i = 0; i < size; i++)
            registerProtocol(protocol, gate("lowerLayerOut", i));
    }
    else if (!strcmp("lowerLayerIn", protocolGate->getName())) {
        if (protocolIdToLowerLayerGateIndex.find(protocol.getId()) != protocolIdToLowerLayerGateIndex.end())
            throw cRuntimeError("handleRegisterProtocol(): Lower layer protocol is already registered: %s", protocol.str().c_str());
        protocolIdToLowerLayerGateIndex[protocol.getId()] = protocolGate->getIndex();
        int size = gateSize("upperLayerOut");
        for (int i = 0; i < size; i++)
            registerProtocol(protocol, gate("upperLayerOut", i));
    }
    else
        throw cRuntimeError("handleRegisterProtocol(): Unknown gate: %s", protocolGate->getName());
}

void MessageDispatcher::handleRegisterInterface(const InterfaceEntry &interface, cGate *interfaceGate)
{
    if (!strcmp("upperLayerIn", interfaceGate->getName()))
        throw cRuntimeError("handleRegisterInterface(): Invalid gate: %s", interfaceGate->getName());
    else if (!strcmp("lowerLayerIn", interfaceGate->getName())) {
        if (interfaceIdToLowerLayerGateIndex.find(interface.getInterfaceId()) != interfaceIdToLowerLayerGateIndex.end())
            throw cRuntimeError("handleRegisterInterface(): Interface is already registered: %s", interface.str().c_str());
        interfaceIdToLowerLayerGateIndex[interface.getInterfaceId()] = interfaceGate->getIndex();
        int size = gateSize("upperLayerOut");
        for (int i = 0; i < size; i++)
            registerInterface(interface, gate("upperLayerOut", i));
    }
    else
        throw cRuntimeError("handleRegisterInterface(): Unknown gate: %s", interfaceGate->getName());
}

} // namespace inet

