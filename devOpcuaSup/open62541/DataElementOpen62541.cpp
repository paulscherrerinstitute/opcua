/*************************************************************************\
* Copyright (c) 2018-2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 *
 *  based on the UaSdk implementation by Ralph Lange <ralph.lange@gmx.de>
 */

 // Avoid problems on Windows (macros min, max clash with numeric_limits<>)
#ifdef _WIN32
#  define NOMINMAX
#endif

#include <iostream>
#include <memory>
#include <string>
#include <cstring>
#include <cstdlib>

#include <errlog.h>
#include <epicsTime.h>
#include <alarm.h>

#include "ItemOpen62541.h"
#include "DataElementOpen62541.h"
#include "UpdateQueue.h"
#include "RecordConnector.h"

namespace DevOpcua {

/* Specific implementation of DataElement's "factory" method */
void
DataElement::addElementToTree (Item *item,
                               RecordConnector *pconnector,
                               const std::string &fullpath)
{
    DataElementOpen62541::addElementToTree(static_cast<ItemOpen62541*>(item), pconnector, fullpath);
}

DataElementOpen62541::DataElementOpen62541 (const std::string &name,
                                    ItemOpen62541 *item,
                                    RecordConnector *pconnector)
    : DataElement(pconnector, name)
    , pitem(item)
    , mapped(false)
    , incomingQueue(pconnector->plinkinfo->clientQueueSize, pconnector->plinkinfo->discardOldest)
    , isdirty(false)
{}

DataElementOpen62541::DataElementOpen62541 (const std::string &name,
                                    ItemOpen62541 *item,
                                    std::weak_ptr<DataElementOpen62541> child)
    : DataElement(name)
    , pitem(item)
    , mapped(false)
    , incomingQueue(0ul)
    , isdirty(false)
{
    elements.push_back(child);
}

void
DataElementOpen62541::show (const int level, const unsigned int indent) const
{
    std::string ind(indent*2, ' ');
    std::cout << ind;
    if (isLeaf()) {
        std::cout << "leaf=" << name << " record(" << pconnector->getRecordType() << ")="
                  << pconnector->getRecordName()
                  << " type=" << variantTypeString(incomingData)
                  << " timestamp=" << (pconnector->plinkinfo->useServerTimestamp ? "server" : "source")
                  << " bini=" << linkOptionBiniString(pconnector->plinkinfo->bini)
                  << " monitor=" << (pconnector->plinkinfo->monitor ? "y" : "n") << "\n";
    } else {
        std::cout << "node=" << name << " children=" << elements.size()
                  << " mapped=" << (mapped ? "y" : "n") << "\n";
        for (auto it : elements) {
            if (auto pelem = it.lock()) {
                pelem->show(level, indent + 1);
            }
        }
    }
}

void
DataElementOpen62541::addElementToTree (ItemOpen62541 *item,
                                    RecordConnector *pcon,
                                    const std::string &fullpath)
{
    bool hasRootElement = true;
    // Create final path element as leaf and link it to connector
    std::string path(fullpath);
    std::string restpath;
    size_t sep = path.find_last_of(separator);
    // allow escaping separators
    while (path[sep-1] == '\\') {
        path.erase(--sep, 1);
        sep = path.find_last_of(separator, --sep);
    }
    std::string leafname = path.substr(sep + 1);
    if (leafname.empty()) leafname = "[ROOT]";
    if (sep != std::string::npos)
        restpath = path.substr(0, sep);

    auto chainelem = std::make_shared<DataElementOpen62541>(leafname, item, pcon);
    pcon->setDataElement(chainelem);

    // Starting from item...
    std::weak_ptr<DataElementOpen62541> topelem = item->rootElement;

    if (topelem.expired()) hasRootElement = false;

    // Simple case (leaf is the root element)
    if (leafname == "[ROOT]") {
        if (hasRootElement) throw std::runtime_error(SB() << "root data element already set");
        item->rootElement = chainelem;
        return;
    }

    std::string name;
    if (hasRootElement) {
        // Find the existing part of the path
        bool found;
        do {
            found = false;
            sep = restpath.find_first_of(separator);
            // allow escaping separators
            while (restpath[sep-1] == '\\') {
                restpath.erase(sep-1, 1);
                sep = restpath.find_first_of(separator, sep);
            }
            if (sep == std::string::npos)
                name = restpath;
            else
                name = restpath.substr(0, sep);

            // Search for name in list of children
            if (!name.empty()) {
                if (auto pelem = topelem.lock()) {
                    for (auto it : pelem->elements) {
                        if (auto pit = it.lock()) {
                            if (pit->name == name) {
                                found = true;
                                topelem = it;
                                break;
                            }
                        }
                    }
                }
            }
            if (found) {
                if (sep == std::string::npos)
                    restpath.clear();
                else
                    restpath = restpath.substr(sep + 1);
            }
        } while (found && !restpath.empty());
    }
    // At this point, topelem is the element to add the chain to
    // (otherwise, a root element has to be added), and
    // restpath is the remaining chain that has to be created

    // Create remaining chain, bottom up
    while (restpath.length()) {
        sep = restpath.find_last_of(separator);
        // allow escaping separators
        while (restpath[sep-1] == '\\') {
            restpath.erase(--sep, 1);
            sep = restpath.find_last_of(separator, --sep);
        }
        name = restpath.substr(sep + 1);
        if (sep != std::string::npos)
            restpath = restpath.substr(0, sep);
        else
            restpath.clear();

        chainelem->parent = std::make_shared<DataElementOpen62541>(name, item, chainelem);
        chainelem = chainelem->parent;
    }

    // Add to topelem, or create rootelem and add it to item
    if (hasRootElement) {
        if (auto pelem = topelem.lock()) {
            pelem->elements.push_back(chainelem);
            chainelem->parent = pelem;
        } else {
            throw std::runtime_error(SB() << "previously found top element invalidated");
        }
    } else {
        chainelem->parent = std::make_shared<DataElementOpen62541>("[ROOT]", item, chainelem);
        chainelem = chainelem->parent;
        item->rootElement = chainelem;
    }
}

// Getting the timestamp and status information from the Item assumes that only one thread
// is pushing data into the Item's DataElement structure at any time.
void
DataElementOpen62541::setIncomingData (const UA_Variant &value, ProcessReason reason)
{
    // Make a copy of this element and cache it
    UA_copy(&value, &incomingData, value.type);

    if (isLeaf()) {
        if ((pitem->state() == ConnectionStatus::initialRead && reason == ProcessReason::readComplete) ||
                (pitem->state() == ConnectionStatus::up)) {
            Guard(pconnector->lock);
            bool wasFirst = false;
            // Make a copy of the value for this element and put it on the queue
            UpdateOpen62541 *u(new UpdateOpen62541(getIncomingTimeStamp(), reason, value, getIncomingReadStatus()));
            incomingQueue.pushUpdate(std::shared_ptr<UpdateOpen62541>(u), &wasFirst);
            if (debug() >= 5)
                std::cout << "Element " << name << " set data ("
                          << processReasonString(reason)
                          << ") for record " << pconnector->getRecordName()
                          << " (queue use " << incomingQueue.size()
                          << "/" << incomingQueue.capacity() << ")" << std::endl;
            if (wasFirst)
                pconnector->requestRecordProcessing(reason);
        }
    } else {
        if (debug() >= 5)
            std::cout << "Element " << name << " splitting structured data to "
                      << elements.size() << " child elements" << std::endl;
        if (value.type->typeKind == UA_TYPES_EXTENSIONOBJECT) {
/*
            UA_ExtensionObject *extensionObject = value.data
            value.toExtensionObject(extensionObject);

            // Try to get the structure definition from the dictionary
            UA_StructureDefinition definition = pitem->structureDefinition(extensionObject.encodingTypeId());
            if (!definition.isNull()) {
                if (!definition.isUnion()) {
                    // ExtensionObject is a structure
                    // Decode the ExtensionObject to a UaGenericValue to provide access to the structure fields
                    UaGenericStructureValue genericValue;
                    genericValue.setGenericValue(extensionObject, definition);

                    if (!mapped) {
                        if (debug() >= 5)
                            std::cout << " ** creating index-to-element map for child elements" << std::endl;
                        for (auto &it : elements) {
                            auto pelem = it.lock();
                            for (int i = 0; i < definition.childrenCount(); i++) {
                                if (pelem->name == definition.child(i).name().toUtf8()) {
                                    elementMap.insert({i, it});
                                    pelem->setIncomingData(genericValue.value(i), reason);
                                }
                            }
                        }
                        if (debug() >= 5)
                            std::cout << " ** " << elementMap.size() << "/" << elements.size()
                                      << " child elements mapped to a "
                                      << "structure of " << definition.childrenCount() << " elements" << std::endl;
                        mapped = true;
                    } else {
                        for (auto &it : elementMap) {
                            auto pelem = it.second.lock();
                            pelem->setIncomingData(genericValue.value(it.first), reason);
                        }
                    }
                }

            } else
                errlogPrintf("Cannot get a structure definition for %s - check access to type dictionary\n",
                             extensionObject.dataTypeId().toString().toUtf8());
*/
        }
    }
}

void
DataElementOpen62541::setIncomingEvent (ProcessReason reason)
{
    if (isLeaf()) {
        Guard(pconnector->lock);
        bool wasFirst = false;
        // Put the event on the queue
        UpdateOpen62541 *u(new UpdateOpen62541(getIncomingTimeStamp(), reason));
        incomingQueue.pushUpdate(std::shared_ptr<UpdateOpen62541>(u), &wasFirst);
        if (debug() >= 5)
            std::cout << "Element " << name << " set event ("
                      << processReasonString(reason)
                      << ") for record " << pconnector->getRecordName()
                      << " (queue use " << incomingQueue.size()
                      << "/" << incomingQueue.capacity() << ")"
                      << std::endl;
        if (wasFirst)
            pconnector->requestRecordProcessing(reason);
    } else {
        for (auto &it : elements) {
            auto pelem = it.lock();
            pelem->setIncomingEvent(reason);
        }
    }
}

// Helper to update one data structure element from pointer to child
/*
bool
DataElementOpen62541::updateDataInGenericValue (UaGenericStructureValue &value,
                                            const int index,
                                            std::shared_ptr<DataElementOpen62541> pelem)
{
    bool updated = false;
    { // Scope of Guard G
        Guard G(pelem->outgoingLock);
        if (pelem->isDirty()) {
            value.setField(index, pelem->getOutgoingData());
            pelem->isdirty = false;
            updated = true;
        }
    }
    if (debug() >= 4) {
        if (updated) {
            std::cout << "Data from child element " << pelem->name
                      << " inserted into data structure" << std::endl;
        } else {
            std::cout << "Data from child element " << pelem->name
                      << " ignored (not dirty)" << std::endl;
        }
    }
    return updated;
}
*/


const UA_Variant &
DataElementOpen62541::getOutgoingData ()
{
    if (!isLeaf()) {
        if (debug() >= 4)
            std::cout << "Element " << name << " updating structured data from "
                      << elements.size() << " child elements" << std::endl;

        UA_copy(&incomingData, &outgoingData, incomingData.type);
        isdirty = false;
        if (outgoingData.type->typeKind == UA_TYPES_EXTENSIONOBJECT) {
            UA_ExtensionObject extensionObject;
            UA_ExtensionObject_setValue(&extensionObject, &outgoingData, outgoingData.type);
/*

            // Try to get the structure definition from the dictionary
            UA_StructureDefinition definition = pitem->structureDefinition(extensionObject.encodingTypeId());
            if (!definition.isNull()) {
                if (!definition.isUnion()) {
                    // ExtensionObject is a structure
                    // Decode the ExtensionObject to a UaGenericValue to provide access to the structure fields
                    UaGenericStructureValue genericValue;
                    genericValue.setGenericValue(extensionObject, definition);

                    if (!mapped) {
                        if (debug() >= 5)
                            std::cout << " ** creating index-to-element map for child elements" << std::endl;
                        for (auto &it : elements) {
                            auto pelem = it.lock();
                            for (int i = 0; i < definition.childrenCount(); i++) {
                                if (pelem->name == definition.child(i).name().toUtf8()) {
                                    elementMap.insert({i, it});
                                    if (updateDataInGenericValue(genericValue, i, pelem))
                                        isdirty = true;
                                }
                            }
                        }
                        if (debug() >= 5)
                            std::cout << " ** " << elementMap.size() << "/" << elements.size()
                                      << " child elements mapped to a "
                                      << "structure of " << definition.childrenCount() << " elements" << std::endl;
                        mapped = true;
                    } else {
                        for (auto &it : elementMap) {
                            auto pelem = it.second.lock();
                            if (updateDataInGenericValue(genericValue, it.first, pelem))
                               isdirty = true;
                        }
                    }
                    if (isdirty) {
                        if (debug() >= 4)
                            std::cout << "Encoding changed data structure to outgoingData of element " << name
                                      << std::endl;
                        genericValue.toExtensionObject(extensionObject);
                        outgoingData.setExtensionObject(extensionObject, OpcUa_True);
                    } else {
                        if (debug() >= 4)
                            std::cout << "Returning unchanged outgoingData of element " << name
                                      << std::endl;
                    }
                }

            } else
                errlogPrintf("Cannot get a structure definition for %s - check access to type dictionary\n",
                             extensionObject.dataTypeId().toString().toUtf8());
*/
        }
    }
    return outgoingData;
}

void
DataElementOpen62541::dbgReadScalar (const UpdateOpen62541 *upd,
                                 const std::string &targetTypeName,
                                 const size_t targetSize) const
{
    if (isLeaf() && debug()) {
        char time_buf[40];
        upd->getTimeStamp().strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f");
        ProcessReason reason = upd->getType();

        std::cout << pconnector->getRecordName() << ": ";
        if (reason == ProcessReason::incomingData || reason == ProcessReason::readComplete) {
            UA_Variant &data = upd->getData();
            UA_String datastring;
            UA_String_init(&datastring);
            UA_print(&data, data.type, &datastring);
            std::cout << "(" << ( pconnector->plinkinfo->useServerTimestamp ? "server" : "device")
                      << " time " << time_buf << ") read " << processReasonString(reason) << " ("
                      << UA_StatusCode_name(upd->getStatus()) << ") "
                      << datastring.data
                      << " (" << variantTypeString(data) << ")"
                      << " as " << targetTypeName;
            UA_String_clear(&datastring);
            if (targetSize)
                std::cout << "[" << targetSize << "]";
        } else {
            std::cout << "(client time "<< time_buf << ") " << processReasonString(reason);
        }
        std::cout << " --- remaining queue " << incomingQueue.size()
                  << "/" << incomingQueue.capacity() << std::endl;
    }
}

long
DataElementOpen62541::readScalar (epicsInt32 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              char *statusText,
                              const epicsUInt32 statusTextLen)
{
    return readScalar<epicsInt32>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readScalar (epicsInt64 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              char *statusText,
                              const epicsUInt32 statusTextLen)
{
    return readScalar<epicsInt64>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readScalar (epicsUInt32 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              char *statusText,
                              const epicsUInt32 statusTextLen)
{
    return readScalar<epicsUInt32>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readScalar (epicsFloat64 *value,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              char *statusText,
                              const epicsUInt32 statusTextLen)
{
    return readScalar<epicsFloat64>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

// CString type needs specialization
long
DataElementOpen62541::readScalar (char *value, const size_t num,
                              dbCommon *prec,
                              ProcessReason *nextReason,
                              epicsUInt32 *statusCode,
                              char *statusText,
                              const epicsUInt32 statusTextLen)
{
    long ret = 0;

    if (incomingQueue.empty()) {
        errlogPrintf("%s : incoming data queue empty\n", prec->name);
        return 1;
    }

    ProcessReason nReason;
    std::shared_ptr<UpdateOpen62541> upd = incomingQueue.popUpdate(&nReason);
    dbgReadScalar(upd.get(), "CString", num);

    switch (upd->getType()) {
    case ProcessReason::readFailure:
        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        ret = 1;
        break;
    case ProcessReason::connectionLoss:
        (void) recGblSetSevr(prec, COMM_ALARM, INVALID_ALARM);
        ret = 1;
        break;
    case ProcessReason::incomingData:
    case ProcessReason::readComplete:
    {
        if (num && value) {
            UA_StatusCode stat = upd->getStatus();
            if (UA_STATUS_IS_BAD(stat)) {
                // No valid OPC UA value
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                // Valid OPC UA value, so copy over
                if (UA_STATUS_IS_UNCERTAIN(stat)) {
                    (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                }
                UA_Variant &data = upd->getData();
                if (data.type->typeKind == UA_TYPES_STRING) {
                    strncpy(value, reinterpret_cast<char*>(static_cast<UA_String *>(data.data)->data), num);
                } else {
                    UA_String datastring;
                    UA_String_init(&datastring);
                    UA_print(&data, data.type, &datastring);
                    strncpy(value, reinterpret_cast<char*>(datastring.data), num);
                    UA_String_clear(&datastring);
                }
                value[num-1] = '\0';
                prec->udf = false;
            }
            if (statusCode) *statusCode = stat;
            if (statusText) {
                strncpy(statusText, UA_StatusCode_name(stat), statusTextLen);
                statusText[statusTextLen-1] = '\0';
            }
        }
        break;
    }
    default:
        break;
    }

    prec->time = upd->getTimeStamp();
    if (nextReason) *nextReason = nReason;
    return ret;
}

void
DataElementOpen62541::dbgReadArray (const UpdateOpen62541 *upd,
                                const epicsUInt32 targetSize,
                                const std::string &targetTypeName) const
{
    if (isLeaf() && debug()) {
        char time_buf[40];
        upd->getTimeStamp().strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f");
        ProcessReason reason = upd->getType();

        std::cout << pconnector->getRecordName() << ": ";
        if (reason == ProcessReason::incomingData || reason == ProcessReason::readComplete) {
            std::cout << "(" << ( pconnector->plinkinfo->useServerTimestamp ? "server" : "device")
                      << " time " << time_buf << ") read " << processReasonString(reason) << " ("
                      << UA_StatusCode_name(upd->getStatus()) << ") ";
            UA_Variant &data = upd->getData();
            std::cout << " array of " << variantTypeString(data)
                      << "[" << upd->getData().arrayLength << "]"
                      << " into " << targetTypeName << "[" << targetSize << "]";
        } else {
            std::cout << "(client time "<< time_buf << ") " << processReasonString(reason);
        }
        std::cout << " --- remaining queue " << incomingQueue.size()
                  << "/" << incomingQueue.capacity() << std::endl;
    }
}

// Read array for EPICS String / UA_String
long int
DataElementOpen62541::readArray (char **value, const epicsUInt32 len,
                             const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             const UA_DataType *expectedType,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    long ret = 0;
    epicsUInt32 elemsWritten = 0;

    if (incomingQueue.empty()) {
        errlogPrintf("%s : incoming data queue empty\n", prec->name);
        *numRead = 0;
        return 1;
    }

    ProcessReason nReason;
    std::shared_ptr<UpdateOpen62541> upd = incomingQueue.popUpdate(&nReason);
    dbgReadArray(upd.get(), num, epicsTypeString(**value));

    switch (upd->getType()) {
    case ProcessReason::readFailure:
        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        ret = 1;
        break;
    case ProcessReason::connectionLoss:
        (void) recGblSetSevr(prec, COMM_ALARM, INVALID_ALARM);
        ret = 1;
        break;
    case ProcessReason::incomingData:
    case ProcessReason::readComplete:
    {
        if (num && value) {
            UA_StatusCode stat = upd->getStatus();
            if (UA_STATUS_IS_BAD(stat)) {
                // No valid OPC UA value
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                // Valid OPC UA value, so try to convert
                UA_Variant &data = upd->getData();
                if (UA_Variant_isScalar(&data)) {
                    errlogPrintf("%s : incoming data is not an array\n", prec->name);
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else if (data.type != expectedType) {
                    errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                                 prec->name, variantTypeString(data), epicsTypeString(**value));
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else {
                    if (UA_STATUS_IS_UNCERTAIN(stat)) {
                        (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                    }
                    UA_Variant &data = upd->getData();
                    elemsWritten = num < data.arrayLength ? num : data.arrayLength;
                    for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                        strncpy(value[i], reinterpret_cast<char*>(static_cast<UA_String *>(data.data)[i].data), len);
                        value[i][len-1] = '\0';
                    }
                    prec->udf = false;
                }
            }
            if (statusCode) *statusCode = stat;
            if (statusText) {
                strncpy(statusText, UA_StatusCode_name(stat), statusTextLen);
                statusText[statusTextLen-1] = '\0';
            }
        }
        break;
    }
    default:
        break;
    }

    prec->time = upd->getTimeStamp();
    if (nextReason) *nextReason = nReason;
    if (num && value)
        *numRead = elemsWritten;
    return ret;
}

long
DataElementOpen62541::readArray (epicsInt8 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt8>(value, num, numRead, &UA_TYPES[UA_TYPES_SBYTE], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsUInt8 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt8>(value, num, numRead, &UA_TYPES[UA_TYPES_BYTE], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsInt16 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt16>(value, num, numRead, &UA_TYPES[UA_TYPES_INT16], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsUInt16 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt16>(value, num, numRead, &UA_TYPES[UA_TYPES_UINT16], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsInt32 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt32>(value, num, numRead, &UA_TYPES[UA_TYPES_INT32], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsUInt32 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt32>(value, num, numRead, &UA_TYPES[UA_TYPES_UINT32], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsInt64 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt64>(value, num, numRead, &UA_TYPES[UA_TYPES_INT64], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsUInt64 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt64>(value, num, numRead, &UA_TYPES[UA_TYPES_INT64], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsFloat32 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsFloat32>(value, num, numRead, &UA_TYPES[UA_TYPES_FLOAT], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (epicsFloat64 *value, const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray<epicsFloat64>(value, num, numRead, &UA_TYPES[UA_TYPES_DOUBLE], prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementOpen62541::readArray (char *value, const epicsUInt32 len,
                             const epicsUInt32 num,
                             epicsUInt32 *numRead,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readArray(&value, len, num, numRead, &UA_TYPES[UA_TYPES_STRING], prec, nextReason, statusCode, statusText, statusTextLen);
}

inline
void
DataElementOpen62541::dbgWriteScalar () const
{
    if (isLeaf() && debug()) {
        UA_String datastring;
        UA_String_init(&datastring);
        UA_print(&outgoingData, outgoingData.type, &datastring);
        std::cout << pconnector->getRecordName() << ": set outgoing data ("
                  << variantTypeString(outgoingData) << ") to value "
                  << datastring;
        UA_String_clear(&datastring);
    }
}

long
DataElementOpen62541::writeScalar (const epicsInt32 &value, dbCommon *prec)
{
    return writeScalar<epicsInt32>(value, prec);
}

long
DataElementOpen62541::writeScalar (const epicsUInt32 &value, dbCommon *prec)
{
    return writeScalar<epicsUInt32>(value, prec);
}

long
DataElementOpen62541::writeScalar (const epicsInt64 &value, dbCommon *prec)
{
    return writeScalar<epicsInt64>(value, prec);
}

long
DataElementOpen62541::writeScalar (const epicsFloat64 &value, dbCommon *prec)
{
    return writeScalar<epicsFloat64>(value, prec);
}

long
DataElementOpen62541::writeScalar (const char *value, const epicsUInt32 len, dbCommon *prec)
{
    long ret = 0;
    UA_StatusCode status = UA_STATUSCODE_BADUNEXPECTEDERROR;
    long l;
    unsigned long ul;
    double d;

    switch (incomingData.type->typeKind) {
    case UA_TYPES_STRING:
    {
        UA_String val;
        val.length = strlen(value);
        val.data = const_cast<UA_Byte*>(reinterpret_cast<const UA_Byte*>(value));
        { // Scope of Guard G
            Guard G(outgoingLock);
            isdirty = true;
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_STRING]);
        }
        break;
    }
    case UA_TYPES_BOOLEAN:
    { // Scope of Guard G
        Guard G(outgoingLock);
        isdirty = true;
        UA_Boolean val = strchr("YyTt1", *value);
        status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_BOOLEAN]);
        break;
    }
    case UA_TYPES_BYTE:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<UA_Byte>(ul)) {
            Guard G(outgoingLock);
            isdirty = true;
            UA_Byte val = static_cast<UA_Byte>(ul);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_BYTE]);
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_SBYTE:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<UA_SByte>(l)) {
            Guard G(outgoingLock);
            isdirty = true;
            UA_SByte val = static_cast<UA_Byte>(l);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_SBYTE]);
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_UINT16:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<UA_UInt16>(ul)) {
            Guard G(outgoingLock);
            isdirty = true;
            UA_UInt16 val = static_cast<UA_UInt16>(ul);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT16]);
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_INT16:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<UA_Int16>(l)) {
            Guard G(outgoingLock);
            isdirty = true;
            UA_Int16 val = static_cast<UA_Int16>(l);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT16]);
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_UINT32:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<UA_UInt32>(ul)) {
            Guard G(outgoingLock);
            isdirty = true;
            UA_UInt32 val = static_cast<UA_UInt32>(ul);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT32]);
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_INT32:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<UA_Int32>(l)) {
            Guard G(outgoingLock);
            isdirty = true;
            UA_Int32 val = static_cast<UA_Int32>(l);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT32]);
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_UINT64:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<UA_UInt64>(ul)) {
            Guard G(outgoingLock);
            isdirty = true;
            UA_UInt64 val = static_cast<UA_UInt64>(ul);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_UINT64]);
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_INT64:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<UA_Int64>(l)) {
            Guard G(outgoingLock);
            isdirty = true;
            UA_Int64 val = static_cast<UA_Int64>(l);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_INT64]);
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_FLOAT:
        d = strtod(value, nullptr);
        if (isWithinRange<UA_Float>(d)) {
            Guard G(outgoingLock);
            isdirty = true;
            UA_Float val = static_cast<UA_Float>(d);
            status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_FLOAT]);
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case UA_TYPES_DOUBLE:
    {
        d = strtod(value, nullptr);
        Guard G(outgoingLock);
        isdirty = true;
        UA_Double val = static_cast<UA_Double>(d);
        status = UA_Variant_setScalarCopy(&outgoingData, &val, &UA_TYPES[UA_TYPES_DOUBLE]);
        break;
    }
    default:
        errlogPrintf("%s : unsupported conversion for outgoing data\n",
                     prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
    }
    if (ret == 0 && UA_STATUS_IS_BAD(status)) {
        errlogPrintf("%s : scalar copy failed: %s\n",
                     prec->name, UA_StatusCode_name(status));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    }
    dbgWriteScalar();
    return ret;
}

inline
void
DataElementOpen62541::dbgWriteArray (const epicsUInt32 targetSize, const std::string &targetTypeName) const
{
    if (isLeaf() && debug()) {
        std::cout << pconnector->getRecordName() << ": writing array of "
                  << targetTypeName << "[" << targetSize << "] as "
                  << variantTypeString(outgoingData) << "["<< outgoingData.arrayLength << "]"
                  << std::endl;
    }
}

// Write array for EPICS String / UA_String
long
DataElementOpen62541::writeArray (const char **value, const epicsUInt32 len,
                              const epicsUInt32 num,
                              const UA_DataType *targetType,
                              dbCommon *prec)
{
    long ret = 0;

    if (UA_Variant_isScalar(&incomingData)) {
        errlogPrintf("%s : OPC UA data type is not an array\n", prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else if (incomingData.type != targetType) {
        errlogPrintf("%s : OPC UA data type (%s) does not match expected type (%s) for EPICS array (%s)\n",
                     prec->name,
                     variantTypeString(incomingData),
                     variantTypeString(targetType),
                     epicsTypeString(**value));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else {
        UA_String *arr = static_cast<UA_String *>(UA_Array_new(num, &UA_TYPES[UA_TYPES_STRING]));
        if (!arr) {
            errlogPrintf("%s : out of memory\n", prec->name);
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        } else {
            for (epicsUInt32 i = 0; i < num; i++) {
                char *val = nullptr;
                const char *pval;
                // add zero termination if necessary
                if (memchr(value[i], '\0', len) == nullptr) {
                    val = new char[len+1];
                    strncpy(val, value[i], len);
                    val[len] = '\0';
                    pval = val;
                } else {
                    pval = value[i];
                }
                arr[i] = UA_STRING_ALLOC(pval);
                delete[] val;
            }
            UA_StatusCode status;
            { // Scope of Guard G
                Guard G(outgoingLock);
                isdirty = true;
                status = UA_Variant_setArrayCopy(&outgoingData, arr, num, targetType);
            }
            if (UA_STATUS_IS_BAD(status)) {
                errlogPrintf("%s : array copy failed: %s\n",
                             prec->name, UA_StatusCode_name(status));
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                dbgWriteArray(num, epicsTypeString(**value));
            }
        }
    }
    return ret;
}

long
DataElementOpen62541::writeArray (const epicsInt8 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt8>(value, num, &UA_TYPES[UA_TYPES_SBYTE], prec);
}

long
DataElementOpen62541::writeArray (const epicsUInt8 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt8>(value, num, &UA_TYPES[UA_TYPES_SBYTE], prec);
}

long
DataElementOpen62541::writeArray (const epicsInt16 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt16>(value, num, &UA_TYPES[UA_TYPES_INT16], prec);
}

long
DataElementOpen62541::writeArray (const epicsUInt16 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt16>(value, num, &UA_TYPES[UA_TYPES_UINT16], prec);
}

long
DataElementOpen62541::writeArray (const epicsInt32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt32>(value, num, &UA_TYPES[UA_TYPES_INT32], prec);
}

long
DataElementOpen62541::writeArray (const epicsUInt32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt32>(value, num, &UA_TYPES[UA_TYPES_UINT32], prec);
}

long
DataElementOpen62541::writeArray (const epicsInt64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt64>(value, num, &UA_TYPES[UA_TYPES_INT64], prec);
}

long
DataElementOpen62541::writeArray (const epicsUInt64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt64>(value, num, &UA_TYPES[UA_TYPES_UINT64], prec);
}

long
DataElementOpen62541::writeArray (const epicsFloat32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsFloat32>(value, num, &UA_TYPES[UA_TYPES_FLOAT], prec);
}

long
DataElementOpen62541::writeArray (const epicsFloat64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsFloat64>(value, num, &UA_TYPES[UA_TYPES_DOUBLE], prec);
}

long
DataElementOpen62541::writeArray (const char *value, const epicsUInt32 len, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray(&value, len, num, &UA_TYPES[UA_TYPES_STRING], prec);
}

void
DataElementOpen62541::requestRecordProcessing (const ProcessReason reason) const
{
    if (isLeaf()) {
        pconnector->requestRecordProcessing(reason);
    } else {
        for (auto &it : elementMap) {
            auto pelem = it.second.lock();
            pelem->requestRecordProcessing(reason);
        }
    }
}

} // namespace DevOpcua
