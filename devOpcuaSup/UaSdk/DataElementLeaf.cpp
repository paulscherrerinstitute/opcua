/*************************************************************************\
* Copyright (c) 2018-2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 *  and example code from the Unified Automation C++ Based OPC UA Client SDK
 */

#include <iostream>
#include <memory>
#include <string>
#include <cstring>
#include <cstdlib>

#include <uadatetime.h>
#include <uaextensionobject.h>
#include <uaarraytemplates.h>
#include <opcua_builtintypes.h>
#include <statuscode.h>

#include <errlog.h>
#include <epicsTime.h>
#include <alarm.h>

#include "ItemUaSdk.h"
#include "DataElementLeaf.h"
#include "UpdateQueue.h"
#include "RecordConnector.h"

namespace DevOpcua {

DataElementLeaf::DataElementLeaf (const std::string &nname,
                                  ItemUaSdk *pitem,
                                  RecordConnector *connector)
    : DataElementUaSdk(nname, pitem)
    , DataElement(connector)
    , incomingQueue(connector->plinkinfo->clientQueueSize, connector->plinkinfo->discardOldest)
{}

void
DataElementLeaf::show (const int level, const unsigned int indent) const
{
    std::string ind(indent*2, ' ');
    std::cout << ind
              << "leaf=" << name << " record(" << pconnector->getRecordType() << ")="
              << pconnector->getRecordName()
              << " type=" << variantTypeString(incomingData.type())
              << " timestamp=" << (pconnector->plinkinfo->useServerTimestamp ? "server" : "source")
              << " bini=" << linkOptionBiniString(pconnector->plinkinfo->bini)
              << " monitor=" << (pconnector->plinkinfo->monitor ? "y" : "n") << "\n";
}

// Getting the timestamp and status information from the Item assumes that only one thread
// is pushing data into the Item's DataElement structure at any time.
void
DataElementLeaf::setIncomingEvent (ProcessReason reason, const UaVariant &value)
{
    // Cache incoming data
    incomingData = value;

    // Ignore data updates during initial read -> wait for the readComplete
    if ((item->state() == ConnectionStatus::initialRead && reason == ProcessReason::readComplete) ||
            (item->state() == ConnectionStatus::up)) {
        Guard(pconnector->lock);
        bool wasFirst = false;
        // Make a copy of the value for this element and put it on the queue
        UpdateUaSdk *u(new UpdateUaSdk(getIncomingTimeStamp(), reason, value, getIncomingReadStatus()));
        incomingQueue.pushUpdate(std::shared_ptr<UpdateUaSdk>(u), &wasFirst);
        if (debug() >= 5)
            std::cout << "Element " << name << " set data ("
                      << processReasonString(reason)
                      << ") for record " << pconnector->getRecordName()
                      << " (queue use " << incomingQueue.size()
                      << "/" << incomingQueue.capacity() << ")" << std::endl;
        if (wasFirst)
            pconnector->requestRecordProcessing(reason);
    }
}

void
DataElementLeaf::setIncomingEvent (ProcessReason reason)
{
    Guard(pconnector->lock);
    bool wasFirst = false;
    // Put the event on the queue
    UpdateUaSdk *u(new UpdateUaSdk(getIncomingTimeStamp(), reason));
    incomingQueue.pushUpdate(std::shared_ptr<UpdateUaSdk>(u), &wasFirst);
    if (debug() >= 5)
        std::cout << "Element " << name << " set event ("
                  << processReasonString(reason)
                  << ") for record " << pconnector->getRecordName()
                  << " (queue use " << incomingQueue.size()
                  << "/" << incomingQueue.capacity() << ")" << std::endl;
    if (wasFirst)
        pconnector->requestRecordProcessing(reason);
}

void
DataElementLeaf::dbgReadScalar (const UpdateUaSdk *upd,
                                const std::string &targetTypeName,
                                const size_t targetSize) const
{
    if (debug()) {
        char time_buf[40];
        upd->getTimeStamp().strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f");
        ProcessReason reason = upd->getType();

        std::cout << pconnector->getRecordName() << ": ";
        if (reason == ProcessReason::incomingData || reason == ProcessReason::readComplete) {
            std::cout << "(" << ( pconnector->plinkinfo->useServerTimestamp ? "server" : "device")
                      << " time " << time_buf << ") read " << processReasonString(reason) << " ("
                      << UaStatus(upd->getStatus()).toString().toUtf8() << ") ";
            UaVariant &data = upd->getData();
            if (data.type() == OpcUaType_String)
                std::cout << "'" << data.toString().toUtf8() << "'";
            else
                std::cout << data.toString().toUtf8();
            std::cout << " (" << variantTypeString(data.type()) << ")"
                      << " as " << targetTypeName;
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
DataElementLeaf::readScalar (epicsInt32 *value,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readScalar<epicsInt32, OpcUa_Int32>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readScalar (epicsInt64 *value,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readScalar<epicsInt64, OpcUa_Int64>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readScalar (epicsUInt32 *value,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readScalar<epicsUInt32, OpcUa_UInt32>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readScalar (epicsFloat64 *value,
                             dbCommon *prec,
                             ProcessReason *nextReason,
                             epicsUInt32 *statusCode,
                             char *statusText,
                             const epicsUInt32 statusTextLen)
{
    return readScalar<epicsFloat64, OpcUa_Double>(value, prec, nextReason, statusCode, statusText, statusTextLen);
}

// CString type needs specialization
long
DataElementLeaf::readScalar (char *value, const size_t num,
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
    std::shared_ptr<UpdateUaSdk> upd = incomingQueue.popUpdate(&nReason);
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
            OpcUa_StatusCode stat = upd->getStatus();
            if (OpcUa_IsNotGood(stat)) {
                // No valid OPC UA value
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                // Valid OPC UA value, so copy over
                if (OpcUa_IsUncertain(stat)) {
                    (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                }
                strncpy(value, upd->getData().toString().toUtf8(), num);
                value[num-1] = '\0';
                prec->udf = false;
            }
            if (statusCode) *statusCode = stat;
            if (statusText) {
                strncpy(statusText, UaStatus(stat).toString().toUtf8(), statusTextLen);
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
DataElementLeaf::dbgReadArray (const UpdateUaSdk *upd,
                               const epicsUInt32 targetSize,
                               const std::string &targetTypeName) const
{
    if (debug()) {
        char time_buf[40];
        upd->getTimeStamp().strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S.%09f");
        ProcessReason reason = upd->getType();

        std::cout << pconnector->getRecordName() << ": ";
        if (reason == ProcessReason::incomingData || reason == ProcessReason::readComplete) {
            std::cout << "(" << ( pconnector->plinkinfo->useServerTimestamp ? "server" : "device")
                      << " time " << time_buf << ") read " << processReasonString(reason) << " ("
                      << UaStatus(upd->getStatus()).toString().toUtf8() << ") ";
            UaVariant &data = upd->getData();
            std::cout << " array of " << variantTypeString(data.type())
                      << "[" << upd->getData().arraySize() << "]"
                      << " into " << targetTypeName << "[" << targetSize << "]";
        } else {
            std::cout << "(client time "<< time_buf << ") " << processReasonString(reason);
        }
        std::cout << " --- remaining queue " << incomingQueue.size()
                  << "/" << incomingQueue.capacity() << std::endl;
    }
}

// Read array for EPICS String / OpcUa_String
long int
DataElementLeaf::readArray (char **value, const epicsUInt32 len,
                            const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            OpcUa_BuiltInType expectedType,
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
    std::shared_ptr<UpdateUaSdk> upd = incomingQueue.popUpdate(&nReason);
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
            OpcUa_StatusCode stat = upd->getStatus();
            if (OpcUa_IsNotGood(stat)) {
                // No valid OPC UA value
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                // Valid OPC UA value, so try to convert
                UaVariant &data = upd->getData();
                if (!data.isArray()) {
                    errlogPrintf("%s : incoming data is not an array\n", prec->name);
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else if (data.type() != expectedType) {
                    errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                                 prec->name, variantTypeString(data.type()), epicsTypeString(**value));
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else {
                    if (OpcUa_IsUncertain(stat)) {
                        (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                    }
                    UaStringArray arr;
                    UaVariant_to(upd->getData(), arr);
                    elemsWritten = num < arr.length() ? num : arr.length();
                    for (epicsUInt32 i = 0; i < elemsWritten; i++) {
                        strncpy(value[i], UaString(arr[i]).toUtf8(), len);
                        value[i][len-1] = '\0';
                    }
                    prec->udf = false;
                }
            }
            if (statusCode) *statusCode = stat;
            if (statusText) {
                strncpy(statusText, UaStatus(stat).toString().toUtf8(), statusTextLen);
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

// Specialization for epicsUInt8 / OpcUa_Byte
//   (needed because UaByteArray API is different from all other UaXxxArray classes)
// CAVEAT: changes in the template (in DataElementLeaf.h) must be reflected here
template<>
long
DataElementLeaf::readArray<epicsUInt8, UaByteArray> (epicsUInt8 *value, const epicsUInt32 num,
                                                     epicsUInt32 *numRead,
                                                     OpcUa_BuiltInType expectedType,
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
    std::shared_ptr<UpdateUaSdk> upd = incomingQueue.popUpdate(&nReason);
    dbgReadArray(upd.get(), num, epicsTypeString(*value));

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
            OpcUa_StatusCode stat = upd->getStatus();
            if (OpcUa_IsNotGood(stat)) {
                // No valid OPC UA value
                (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                ret = 1;
            } else {
                // Valid OPC UA value, so try to convert
                UaVariant &data = upd->getData();
                if (!data.isArray()) {
                    errlogPrintf("%s : incoming data is not an array\n", prec->name);
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else if (data.type() != expectedType) {
                    errlogPrintf("%s : incoming data type (%s) does not match EPICS array type (%s)\n",
                                 prec->name, variantTypeString(data.type()), epicsTypeString(*value));
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else {
                    if (OpcUa_IsUncertain(stat)) {
                        (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                    }
                    UaByteArray arr;
                    UaVariant_to(upd->getData(), arr);
                    elemsWritten = static_cast<epicsUInt32>(arr.size());
                    if (num < elemsWritten) elemsWritten = num;
                    memcpy(value, arr.data(), sizeof(epicsUInt8) * elemsWritten);
                    prec->udf = false;
                }
            }
            if (statusCode) *statusCode = stat;
            if (statusText) {
                strncpy(statusText, UaStatus(stat).toString().toUtf8(), statusTextLen);
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
    if (num)
        *numRead = elemsWritten;
    return ret;
}

long
DataElementLeaf::readArray (epicsInt8 *value, const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            dbCommon *prec,
                            ProcessReason *nextReason,
                            epicsUInt32 *statusCode,
                            char *statusText,
                            const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt8, UaSByteArray>(value, num, numRead, OpcUaType_SByte, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readArray (epicsUInt8 *value, const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            dbCommon *prec,
                            ProcessReason *nextReason,
                            epicsUInt32 *statusCode,
                            char *statusText,
                            const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt8, UaByteArray>(value, num, numRead, OpcUaType_Byte, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readArray (epicsInt16 *value, const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            dbCommon *prec,
                            ProcessReason *nextReason,
                            epicsUInt32 *statusCode,
                            char *statusText,
                            const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt16, UaInt16Array>(value, num, numRead, OpcUaType_Int16, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readArray (epicsUInt16 *value, const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            dbCommon *prec,
                            ProcessReason *nextReason,
                            epicsUInt32 *statusCode,
                            char *statusText,
                            const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt16, UaUInt16Array>(value, num, numRead, OpcUaType_UInt16, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readArray (epicsInt32 *value, const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            dbCommon *prec,
                            ProcessReason *nextReason,
                            epicsUInt32 *statusCode,
                            char *statusText,
                            const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt32, UaInt32Array>(value, num, numRead, OpcUaType_Int32, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readArray (epicsUInt32 *value, const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            dbCommon *prec,
                            ProcessReason *nextReason,
                            epicsUInt32 *statusCode,
                            char *statusText,
                            const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt32, UaUInt32Array>(value, num, numRead, OpcUaType_UInt32, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readArray (epicsInt64 *value, const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            dbCommon *prec,
                            ProcessReason *nextReason,
                            epicsUInt32 *statusCode,
                            char *statusText,
                            const epicsUInt32 statusTextLen)
{
    return readArray<epicsInt64, UaInt64Array>(value, num, numRead, OpcUaType_Int64, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readArray (epicsUInt64 *value, const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            dbCommon *prec,
                            ProcessReason *nextReason,
                            epicsUInt32 *statusCode,
                            char *statusText,
                            const epicsUInt32 statusTextLen)
{
    return readArray<epicsUInt64, UaUInt64Array>(value, num, numRead, OpcUaType_UInt64, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readArray (epicsFloat32 *value, const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            dbCommon *prec,
                            ProcessReason *nextReason,
                            epicsUInt32 *statusCode,
                            char *statusText,
                            const epicsUInt32 statusTextLen)
{
    return readArray<epicsFloat32, UaFloatArray>(value, num, numRead, OpcUaType_Float, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readArray (epicsFloat64 *value, const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            dbCommon *prec,
                            ProcessReason *nextReason,
                            epicsUInt32 *statusCode,
                            char *statusText,
                            const epicsUInt32 statusTextLen)
{
    return readArray<epicsFloat64, UaDoubleArray>(value, num, numRead, OpcUaType_Double, prec, nextReason, statusCode, statusText, statusTextLen);
}

long
DataElementLeaf::readArray (char *value, const epicsUInt32 len,
                            const epicsUInt32 num,
                            epicsUInt32 *numRead,
                            dbCommon *prec,
                            ProcessReason *nextReason,
                            epicsUInt32 *statusCode,
                            char *statusText,
                            const epicsUInt32 statusTextLen)
{
    return readArray(&value, len, num, numRead, OpcUaType_String, prec, nextReason, statusCode, statusText, statusTextLen);
}

inline
void
DataElementLeaf::dbgWriteScalar () const
{
    if (debug()) {
        std::cout << pconnector->getRecordName() << ": set outgoing data ("
                  << variantTypeString(outgoingData.type()) << ") to value ";
        if (outgoingData.type() == OpcUaType_String)
            std::cout << "'" << outgoingData.toString().toUtf8() << "'";
        else
            std::cout << outgoingData.toString().toUtf8();
        std::cout << std::endl;
    }
}

long
DataElementLeaf::writeScalar (const epicsInt32 &value, dbCommon *prec)
{
    return writeScalar<epicsInt32>(value, prec);
}

long
DataElementLeaf::writeScalar (const epicsUInt32 &value, dbCommon *prec)
{
    return writeScalar<epicsUInt32>(value, prec);
}

long
DataElementLeaf::writeScalar (const epicsInt64 &value, dbCommon *prec)
{
    return writeScalar<epicsInt64>(value, prec);
}

long
DataElementLeaf::writeScalar (const epicsFloat64 &value, dbCommon *prec)
{
    return writeScalar<epicsFloat64>(value, prec);
}

long
DataElementLeaf::writeScalar (const char *value, const epicsUInt32 len, dbCommon *prec)
{
    long ret = 0;
    long l;
    unsigned long ul;
    double d;

    switch (incomingData.type()) {
    case OpcUaType_String:
    { // Scope of Guard G
        Guard G(outgoingLock);
        isdirty = true;
        outgoingData.setString(static_cast<UaString>(value));
        break;
    }
    case OpcUaType_Boolean:
    { // Scope of Guard G
        Guard G(outgoingLock);
        isdirty = true;
        if (strchr("YyTt1", *value))
            outgoingData.setBoolean(true);
        else
            outgoingData.setBoolean(false);
        break;
    }
    case OpcUaType_Byte:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<OpcUa_Byte>(ul)) {
            Guard G(outgoingLock);
            isdirty = true;
            outgoingData.setByte(static_cast<OpcUa_Byte>(ul));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_SByte:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<OpcUa_SByte>(l)) {
            Guard G(outgoingLock);
            isdirty = true;
            outgoingData.setSByte(static_cast<OpcUa_SByte>(l));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_UInt16:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<OpcUa_UInt16>(ul)) {
            Guard G(outgoingLock);
            isdirty = true;
            outgoingData.setUInt16(static_cast<OpcUa_UInt16>(ul));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_Int16:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<OpcUa_Int16>(l)) {
            Guard G(outgoingLock);
            isdirty = true;
            outgoingData.setInt16(static_cast<OpcUa_Int16>(l));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_UInt32:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<OpcUa_UInt32>(ul)) {
            Guard G(outgoingLock);
            isdirty = true;
            outgoingData.setUInt32(static_cast<OpcUa_UInt32>(ul));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_Int32:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<OpcUa_Int32>(l)) {
            Guard G(outgoingLock);
            isdirty = true;
            outgoingData.setInt32(static_cast<OpcUa_Int32>(l));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_UInt64:
        ul = strtoul(value, nullptr, 0);
        if (isWithinRange<OpcUa_UInt64>(ul)) {
            Guard G(outgoingLock);
            isdirty = true;
            outgoingData.setUInt64(static_cast<OpcUa_UInt64>(ul));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_Int64:
        l = strtol(value, nullptr, 0);
        if (isWithinRange<OpcUa_Int64>(l)) {
            Guard G(outgoingLock);
            isdirty = true;
            outgoingData.setInt64(static_cast<OpcUa_Int64>(l));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_Float:
        d = strtod(value, nullptr);
        if (isWithinRange<OpcUa_Float>(d)) {
            Guard G(outgoingLock);
            isdirty = true;
            outgoingData.setFloat(static_cast<OpcUa_Float>(d));
        } else {
            (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            ret = 1;
        }
        break;
    case OpcUaType_Double:
    {
        d = strtod(value, nullptr);
        Guard G(outgoingLock);
        isdirty = true;
        outgoingData.setDouble(static_cast<OpcUa_Double>(d));
        break;
    }
    default:
        errlogPrintf("%s : unsupported conversion for outgoing data\n",
                     prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
    }

    dbgWriteScalar();
    return ret;
}

inline
void
DataElementLeaf::dbgWriteArray (const epicsUInt32 targetSize, const std::string &targetTypeName) const
{
    if (debug()) {
        std::cout << pconnector->getRecordName() << ": writing array of "
                  << targetTypeName << "[" << targetSize << "] as "
                  << variantTypeString(outgoingData.type()) << "["<< outgoingData.arraySize() << "]"
                  << std::endl;
    }
}

// Write array for EPICS String / OpcUa_String
long
DataElementLeaf::writeArray (const char **value, const epicsUInt32 len,
                             const epicsUInt32 num,
                             OpcUa_BuiltInType targetType,
                             dbCommon *prec)
{
    long ret = 0;

    if (!incomingData.isArray()) {
        errlogPrintf("%s : OPC UA data type is not an array\n", prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else if (incomingData.type() != targetType) {
        errlogPrintf("%s : OPC UA data type (%s) does not match expected type (%s) for EPICS array (%s)\n",
                     prec->name,
                     variantTypeString(incomingData.type()),
                     variantTypeString(targetType),
                     epicsTypeString(**value));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else {
        UaStringArray arr;
        arr.create(static_cast<OpcUa_UInt32>(num));
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
            UaString(pval).copyTo(&arr[i]);
            delete[] val;
        }
        { // Scope of Guard G
            Guard G(outgoingLock);
            isdirty = true;
            UaVariant_set(outgoingData, arr);
        }

        dbgWriteArray(num, epicsTypeString(**value));
    }
    return ret;
}

// Specialization for epicsUInt8 / OpcUa_Byte
//   (needed because UaByteArray API is different from all other UaXxxArray classes)
// CAVEAT: changes in the template (in DataElementLeaf.h) must be reflected here
template<>
long
DataElementLeaf::writeArray<epicsUInt8, UaByteArray, OpcUa_Byte> (const epicsUInt8 *value, const epicsUInt32 num,
                                                                  OpcUa_BuiltInType targetType,
                                                                  dbCommon *prec)
{
    long ret = 0;

    if (!incomingData.isArray()) {
        errlogPrintf("%s : OPC UA data type is not an array\n", prec->name);
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else if (incomingData.type() != targetType) {
        errlogPrintf("%s : OPC UA data type (%s) does not match expected type (%s) for EPICS array (%s)\n",
                     prec->name,
                     variantTypeString(incomingData.type()),
                     variantTypeString(targetType),
                     epicsTypeString(*value));
        (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        ret = 1;
    } else {
        UaByteArray arr(reinterpret_cast<const char *>(value), static_cast<OpcUa_Int32>(num));
        { // Scope of Guard G
            Guard G(outgoingLock);
            isdirty = true;
            UaVariant_set(outgoingData, arr);
        }

        dbgWriteArray(num, epicsTypeString(*value));
    }
    return ret;
}

long
DataElementLeaf::writeArray (const epicsInt8 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt8, UaSByteArray, OpcUa_SByte>(value, num, OpcUaType_SByte, prec);
}

long
DataElementLeaf::writeArray (const epicsUInt8 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt8, UaByteArray, OpcUa_Byte>(value, num, OpcUaType_Byte, prec);
}

long
DataElementLeaf::writeArray (const epicsInt16 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt16, UaInt16Array, OpcUa_Int16>(value, num, OpcUaType_Int16, prec);
}

long
DataElementLeaf::writeArray (const epicsUInt16 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt16, UaUInt16Array, OpcUa_UInt16>(value, num, OpcUaType_UInt16, prec);
}

long
DataElementLeaf::writeArray (const epicsInt32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt32, UaInt32Array, OpcUa_Int32>(value, num, OpcUaType_Int32, prec);
}

long
DataElementLeaf::writeArray (const epicsUInt32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt32, UaUInt32Array, OpcUa_UInt32>(value, num, OpcUaType_UInt32, prec);
}

long
DataElementLeaf::writeArray (const epicsInt64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsInt64, UaInt64Array, OpcUa_Int64>(value, num, OpcUaType_Int64, prec);
}

long
DataElementLeaf::writeArray (const epicsUInt64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsUInt64, UaUInt64Array, OpcUa_UInt64>(value, num, OpcUaType_UInt64, prec);
}

long
DataElementLeaf::writeArray (const epicsFloat32 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsFloat32, UaFloatArray, OpcUa_Float>(value, num, OpcUaType_Float, prec);
}

long
DataElementLeaf::writeArray (const epicsFloat64 *value, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray<epicsFloat64, UaDoubleArray, OpcUa_Double>(value, num, OpcUaType_Double, prec);
}

long
DataElementLeaf::writeArray (const char *value, const epicsUInt32 len, const epicsUInt32 num, dbCommon *prec)
{
    return writeArray(&value, len, num, OpcUaType_String, prec);
}

void
DataElementLeaf::requestRecordProcessing (const ProcessReason reason) const
{
    pconnector->requestRecordProcessing(reason);
}

} // namespace DevOpcua
