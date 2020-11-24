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

#ifndef DEVOPCUA_DATAELEMENTLEAF_H
#define DEVOPCUA_DATAELEMENTLEAF_H

// Avoid problems on Windows (macros min, max clash with numeric_limits<>)
#ifdef _WIN32
#  define NOMINMAX
#endif

#include <unordered_map>
#include <limits>

#include <uadatavalue.h>
#include <statuscode.h>

#include <errlog.h>

#include "DataElement.h"
#include "DataElementUaSdk.h"
#include "devOpcua.h"
#include "RecordConnector.h"
#include "Update.h"
#include "UpdateQueue.h"
#include "ItemUaSdk.h"

namespace DevOpcua {

class ItemUaSdk;

typedef Update<UaVariant, OpcUa_StatusCode> UpdateUaSdk;

inline const char *epicsTypeString (const epicsInt8 &) { return "epicsInt8"; }
inline const char *epicsTypeString (const epicsUInt8 &) { return "epicsUInt8"; }
inline const char *epicsTypeString (const epicsInt16 &) { return "epicsInt16"; }
inline const char *epicsTypeString (const epicsUInt16 &) { return "epicsUInt16"; }
inline const char *epicsTypeString (const epicsInt32 &) { return "epicsInt32"; }
inline const char *epicsTypeString (const epicsUInt32 &) { return "epicsUInt32"; }
inline const char *epicsTypeString (const epicsInt64 &) { return "epicsInt64"; }
inline const char *epicsTypeString (const epicsUInt64 &) { return "epicsUInt64"; }
inline const char *epicsTypeString (const epicsFloat32 &) { return "epicsFloat32"; }
inline const char *epicsTypeString (const epicsFloat64 &) { return "epicsFloat64"; }
inline const char *epicsTypeString (const char* &) { return "epicsString"; }

inline const char *
variantTypeString (const OpcUa_BuiltInType type)
{
    switch(type) {
        case OpcUaType_Null:            return "OpcUa_Null";
        case OpcUaType_Boolean:         return "OpcUa_Boolean";
        case OpcUaType_SByte:           return "OpcUa_SByte";
        case OpcUaType_Byte:            return "OpcUa_Byte";
        case OpcUaType_Int16:           return "OpcUa_Int16";
        case OpcUaType_UInt16:          return "OpcUa_UInt16";
        case OpcUaType_Int32:           return "OpcUa_Int32";
        case OpcUaType_UInt32:          return "OpcUa_UInt32";
        case OpcUaType_Int64:           return "OpcUa_Int64";
        case OpcUaType_UInt64:          return "OpcUa_UInt64";
        case OpcUaType_Float:           return "OpcUa_Float";
        case OpcUaType_Double:          return "OpcUa_Double";
        case OpcUaType_String:          return "OpcUa_String";
        case OpcUaType_DateTime:        return "OpcUa_DateTime";
        case OpcUaType_Guid:            return "OpcUa_Guid";
        case OpcUaType_ByteString:      return "OpcUa_ByteString";
        case OpcUaType_XmlElement:      return "OpcUa_XmlElement";
        case OpcUaType_NodeId:          return "OpcUa_NodeId";
        case OpcUaType_ExpandedNodeId:  return "OpcUa_ExpandedNodeId";
        case OpcUaType_StatusCode:      return "OpcUa_StatusCode";
        case OpcUaType_QualifiedName:   return "OpcUa_QualifiedName";
        case OpcUaType_LocalizedText:   return "OpcUa_LocalizedText";
        case OpcUaType_ExtensionObject: return "OpcUa_ExtensionObject";
        case OpcUaType_DataValue:       return "OpcUa_DataValue";
        case OpcUaType_Variant:         return "OpcUa_Variant";
        case OpcUaType_DiagnosticInfo:  return "OpcUa_DiagnosticInfo";
    }
    return "Illegal Value";
}

// Template for range check when writing
template<typename TO, typename FROM>
inline bool isWithinRange (const FROM &value) {
    return !(value < std::numeric_limits<TO>::lowest() || value > std::numeric_limits<TO>::max());
}

// Specializations for unsigned to signed to avoid compiler warnings
template<>
inline bool isWithinRange<OpcUa_SByte, epicsUInt32> (const epicsUInt32 &value) {
    return !(value > static_cast<OpcUa_UInt32>(std::numeric_limits<OpcUa_SByte>::max()));
}

template<>
inline bool isWithinRange<OpcUa_Int16, epicsUInt32> (const epicsUInt32 &value) {
    return !(value > static_cast<OpcUa_UInt32>(std::numeric_limits<OpcUa_Int16>::max()));
}

template<>
inline bool isWithinRange<OpcUa_Int32, epicsUInt32> (const epicsUInt32 &value) {
    return !(value > static_cast<OpcUa_UInt32>(std::numeric_limits<OpcUa_Int32>::max()));
}

template<>
inline bool isWithinRange<OpcUa_SByte, epicsUInt64> (const epicsUInt64 &value) {
    return !(value > static_cast<OpcUa_UInt64>(std::numeric_limits<OpcUa_SByte>::max()));
}

template<>
inline bool isWithinRange<OpcUa_Int16, epicsUInt64> (const epicsUInt64 &value) {
    return !(value > static_cast<OpcUa_UInt64>(std::numeric_limits<OpcUa_Int16>::max()));
}

template<>
inline bool isWithinRange<OpcUa_Int32, epicsUInt64> (const epicsUInt64 &value) {
    return !(value > static_cast<OpcUa_UInt64>(std::numeric_limits<OpcUa_Int32>::max()));
}

template<>
inline bool isWithinRange<OpcUa_Int64, epicsUInt64> (const epicsUInt64 &value) {
    return !(value > static_cast<OpcUa_UInt64>(std::numeric_limits<OpcUa_Int64>::max()));
}

// Simple-check specializations for converting signed to unsigned of same or wider type
template<> inline bool isWithinRange<OpcUa_UInt32, epicsInt8> (const epicsInt8 &value) { return !(value < 0); }
template<> inline bool isWithinRange<OpcUa_UInt32, epicsInt16> (const epicsInt16 &value) { return !(value < 0); }
template<> inline bool isWithinRange<OpcUa_UInt32, epicsInt32> (const epicsInt32 &value) { return !(value < 0); }
template<> inline bool isWithinRange<OpcUa_UInt64, epicsInt8> (const epicsInt8 &value) { return !(value < 0); }
template<> inline bool isWithinRange<OpcUa_UInt64, epicsInt16> (const epicsInt16 &value) { return !(value < 0); }
template<> inline bool isWithinRange<OpcUa_UInt64, epicsInt32> (const epicsInt32 &value) { return !(value < 0); }
template<> inline bool isWithinRange<OpcUa_UInt64, epicsInt64> (const epicsInt64 &value) { return !(value < 0); }

// No-check-needed specializations for converting to same or wider type
template<> inline bool isWithinRange<OpcUa_Int32, epicsInt32> (const epicsInt32 &) { return true; }
template<> inline bool isWithinRange<OpcUa_Int64, epicsInt32> (const epicsInt32 &) { return true; }
template<> inline bool isWithinRange<OpcUa_Float, epicsInt32> (const epicsInt32 &) { return true; }
template<> inline bool isWithinRange<OpcUa_Double, epicsInt32> (const epicsInt32 &) { return true; }

template<> inline bool isWithinRange<OpcUa_UInt32, epicsUInt32> (const epicsUInt32 &) { return true; }
template<> inline bool isWithinRange<OpcUa_Int64, epicsUInt32> (const epicsUInt32 &) { return true; }
template<> inline bool isWithinRange<OpcUa_UInt64, epicsUInt32> (const epicsUInt32 &) { return true; }
template<> inline bool isWithinRange<OpcUa_Float, epicsUInt32> (const epicsUInt32 &) { return true; }
template<> inline bool isWithinRange<OpcUa_Double, epicsUInt32> (const epicsUInt32 &) { return true; }

template<> inline bool isWithinRange<OpcUa_Int64, epicsInt64> (const epicsInt64 &) { return true; }
template<> inline bool isWithinRange<OpcUa_Float, epicsInt64> (const epicsInt64 &) { return true; }
template<> inline bool isWithinRange<OpcUa_Double, epicsInt64> (const epicsInt64 &) { return true; }

template<> inline bool isWithinRange<OpcUa_Double, epicsFloat64> (const epicsFloat64 &) { return true; }

/**
 * @brief The UA SDK implementation of a single piece of data, connected to a record.
 *
 * For the low-level UA SDK side API, see DevOpcua::DataElementUaSdk
 * For the high-level EPICS side API, see DevOpcua::DataElement
 */
class DataElementLeaf : public DataElementUaSdk, public DataElement
{
public:
    virtual ~DataElementLeaf() override {};

    /**
     * @brief Constructor for DataElement from record connector.
     *
     * Creates the final (leaf) element of the data structure. The record connector
     * holds a shared pointer to its leaf, while the data element has a weak pointer
     * to the record connector.
     *
     * @param nname  name of the element (empty for root element)
     * @param pitem  pointer to corresponding ItemUaSdk
     * @param pconnector  pointer to record connector to link to
     */
    DataElementLeaf(const std::string &nname,
                    ItemUaSdk *pitem,
                    RecordConnector *pconnector);

    // Implement DataElementUaSdk (OPCUA side) interface
    virtual void show(const int level, const unsigned int indent) const override;
    virtual void setIncomingEvent(const ProcessReason reason, const UaVariant &value) override;
    virtual void setIncomingEvent(const ProcessReason reason) override;
    virtual const UaVariant &getOutgoingData() override { return outgoingData; }
    virtual void requestRecordProcessing(const ProcessReason reason) const override;
    virtual int debug() const override { return pconnector->debug(); }

    // Implement DataElement (EPICS side) interface
    virtual long int readScalar(epicsInt32 *value,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32 *statusCode = nullptr,
                                char *statusText = nullptr,
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readScalar(epicsInt64 *value,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32 *statusCode = nullptr,
                                char *statusText = nullptr,
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readScalar(epicsUInt32 *value,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32 *statusCode = nullptr,
                                char *statusText = nullptr,
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readScalar(epicsFloat64 *value,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32 *statusCode = nullptr,
                                char *statusText = nullptr,
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    // Classic C string
    virtual long int readScalar(char *value, const size_t num,
                                dbCommon *prec,
                                ProcessReason *nextReason = nullptr,
                                epicsUInt32 *statusCode = nullptr,
                                char *statusText = nullptr,
                                const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readArray(epicsInt8 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readArray(epicsUInt8 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readArray(epicsInt16 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readArray(epicsUInt16 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readArray(epicsInt32 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readArray(epicsUInt32 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readArray(epicsInt64 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readArray(epicsUInt64 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readArray(epicsFloat32 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readArray(epicsFloat64 *value, const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int readArray(char *value, const epicsUInt32 len,
                               const epicsUInt32 num,
                               epicsUInt32 *numRead,
                               dbCommon *prec,
                               ProcessReason *nextReason = nullptr,
                               epicsUInt32 *statusCode = nullptr,
                               char *statusText = nullptr,
                               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1) override;

    virtual long int writeScalar(const epicsInt32 &value,
                                 dbCommon *prec) override;

    virtual long int writeScalar(const epicsInt64 &value,
                                 dbCommon *prec) override;

    virtual long int writeScalar(const epicsUInt32 &value,
                                 dbCommon *prec) override;

    virtual long int writeScalar(const epicsFloat64 &value,
                                 dbCommon *prec) override;

    virtual long int writeScalar(const char *value,
                                 const epicsUInt32 num,
                                 dbCommon *prec) override;

    virtual long int writeArray(const epicsInt8 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    virtual long int writeArray(const epicsUInt8 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    virtual long int writeArray(const epicsInt16 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    virtual long int writeArray(const epicsUInt16 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    virtual long int writeArray(const epicsInt32 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    virtual long int writeArray(const epicsUInt32 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    virtual long int writeArray(const epicsInt64 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    virtual long int writeArray(const epicsUInt64 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    virtual long int writeArray(const epicsFloat32 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    virtual long int writeArray(const epicsFloat64 *value,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    virtual long int writeArray(const char *value, const epicsUInt32 len,
                                const epicsUInt32 num,
                                dbCommon *prec) override;

    virtual void clearOutgoingData() override { outgoingData.clear(); }

private:
    // Helpers: Print debug information
    void dbgWriteScalar () const;
    void dbgReadScalar(const UpdateUaSdk *upd,
                       const std::string &targetTypeName,
                       const size_t targetSize = 0) const;
    void dbgReadArray(const UpdateUaSdk *upd,
                      const epicsUInt32 targetSize,
                      const std::string &targetTypeName) const;
    void dbgWriteArray(const epicsUInt32 targetSize, const std::string &targetTypeName) const;
    // Helper:
    void checkWriteArray(OpcUa_BuiltInType expectedType, const std::string &targetTypeName) const;

    // Helpers: Get additional information for incoming data
    // Note: Only call these from the context of data being pushed.
    //       Yields wrong results if called later on (from record processing context).

    // Get the time stamp from the incoming object
    const epicsTime &getIncomingTimeStamp() const {
        ProcessReason reason = item->getReason();
        if (reason == ProcessReason::incomingData || reason == ProcessReason::readComplete)
            if (pconnector->plinkinfo->useServerTimestamp)
                return item->tsServer;
            else
                return item->tsSource;
        else
            return item->tsClient;
    }

    // Get the read status from the incoming object
    OpcUa_StatusCode getIncomingReadStatus() const {
        return item->getLastStatus().code();
    }

    // Overloaded helper functions that wrap the UaVariant::toXxx() and UaVariant::setXxx methods
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, OpcUa_Int32 &value) { return variant.toInt32(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, OpcUa_UInt32 &value) { return variant.toUInt32(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, OpcUa_Int64 &value) { return variant.toInt64(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, OpcUa_Double &value) { return variant.toDouble(value); }

    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, UaSByteArray &value) { return variant.toSByteArray(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, UaByteArray &value) { return variant.toByteArray(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, UaInt16Array &value) { return variant.toInt16Array(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, UaUInt16Array &value) { return variant.toUInt16Array(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, UaInt32Array &value) { return variant.toInt32Array(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, UaUInt32Array &value) { return variant.toUInt32Array(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, UaInt64Array &value) { return variant.toInt64Array(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, UaUInt64Array &value) { return variant.toUInt64Array(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, UaFloatArray &value) { return variant.toFloatArray(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, UaDoubleArray &value) { return variant.toDoubleArray(value); }
    OpcUa_StatusCode UaVariant_to(const UaVariant &variant, UaStringArray &value) { return variant.toStringArray(value); }

    void UaVariant_set(UaVariant &variant, UaSByteArray &value) { variant.setSByteArray(value, OpcUa_True); }
    void UaVariant_set(UaVariant &variant, UaByteArray &value) { variant.setByteArray(value, OpcUa_True); }
    void UaVariant_set(UaVariant &variant, UaInt16Array &value) { variant.setInt16Array(value, OpcUa_True); }
    void UaVariant_set(UaVariant &variant, UaUInt16Array &value) { variant.setUInt16Array(value, OpcUa_True); }
    void UaVariant_set(UaVariant &variant, UaInt32Array &value) { variant.setInt32Array(value, OpcUa_True); }
    void UaVariant_set(UaVariant &variant, UaUInt32Array &value) { variant.setUInt32Array(value, OpcUa_True); }
    void UaVariant_set(UaVariant &variant, UaInt64Array &value) { variant.setInt64Array(value, OpcUa_True); }
    void UaVariant_set(UaVariant &variant, UaUInt64Array &value) { variant.setUInt64Array(value, OpcUa_True); }
    void UaVariant_set(UaVariant &variant, UaFloatArray &value) { variant.setFloatArray(value, OpcUa_True); }
    void UaVariant_set(UaVariant &variant, UaDoubleArray &value) { variant.setDoubleArray(value, OpcUa_True); }
    void UaVariant_set(UaVariant &variant, UaStringArray &value) { variant.setStringArray(value, OpcUa_True); }

    // Read scalar value as templated function on EPICS type and OPC UA type
    // value == nullptr is allowed and leads to the value being dropped (ignored),
    // including the extended status
    template<typename ET, typename OT>
    long
    readScalar (ET *value,
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
        dbgReadScalar(upd.get(), epicsTypeString(*value));

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
            if (value) {
                OpcUa_StatusCode stat = upd->getStatus();
                if (OpcUa_IsNotGood(stat)) {
                    // No valid OPC UA value
                    (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    ret = 1;
                } else {
                    // Valid OPC UA value, so try to convert
                    OT v;
                    if (OpcUa_IsNotGood(UaVariant_to(upd->getData(), v))) {
                        errlogPrintf("%s : incoming data (%s) out-of-bounds\n",
                                     prec->name,
                                     upd->getData().toString().toUtf8());
                        (void) recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
                    } else {
                        if (OpcUa_IsUncertain(stat)) {
                            (void) recGblSetSevr(prec, READ_ALARM, MINOR_ALARM);
                        }
                        *value = v;
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
        return ret;
    }

    // Read array value as templated function on EPICS type and OPC UA type
    // (latter *must match* OPC UA type enum argument)
    // CAVEAT: changes must also be reflected in specializations (in DataElementUaSdk.cpp)
    template<typename ET, typename OT>
    long
    readArray (ET *value, const epicsUInt32 num,
               epicsUInt32 *numRead,
               OpcUa_BuiltInType expectedType,
               dbCommon *prec,
               ProcessReason *nextReason,
               epicsUInt32 *statusCode,
               char *statusText,
               const epicsUInt32 statusTextLen = MAX_STRING_SIZE+1)
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
                        OT arr;
                        UaVariant_to(upd->getData(), arr);
                        elemsWritten = num < arr.length() ? num : arr.length();
                        memcpy(value, arr.rawData(), sizeof(ET) * elemsWritten);
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

    // Read array value for EPICS String / OpcUa_String
    long
    readArray (char **value, const epicsUInt32 len,
               const epicsUInt32 num,
               epicsUInt32 *numRead,
               OpcUa_BuiltInType expectedType,
               dbCommon *prec,
               ProcessReason *nextReason,
               epicsUInt32 *statusCode,
               char *statusText,
               const epicsUInt32 statusTextLen);

    // Write scalar value as templated function on EPICS type
    template<typename ET>
    long
    writeScalar (const ET &value,
                 dbCommon *prec)
    {
        long ret = 0;

        switch (incomingData.type()) {
        case OpcUaType_Boolean:
        { // Scope of Guard G
            Guard G(outgoingLock);
            isdirty = true;
            if (value == 0)
                outgoingData.setBoolean(false);
            else
                outgoingData.setBoolean(true);
            break;
        }
        case OpcUaType_Byte:
            if (isWithinRange<OpcUa_Byte>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                outgoingData.setByte(static_cast<OpcUa_Byte>(value));
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case OpcUaType_SByte:
            if (isWithinRange<OpcUa_SByte>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                outgoingData.setSByte(static_cast<OpcUa_SByte>(value));
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case OpcUaType_UInt16:
            if (isWithinRange<OpcUa_UInt16>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                outgoingData.setUInt16(static_cast<OpcUa_UInt16>(value));
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case OpcUaType_Int16:
            if (isWithinRange<OpcUa_Int16>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                outgoingData.setInt16(static_cast<OpcUa_Int16>(value));
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case OpcUaType_UInt32:
            if (isWithinRange<OpcUa_UInt32>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                outgoingData.setUInt32(static_cast<OpcUa_UInt32>(value));
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case OpcUaType_Int32:
            if (isWithinRange<OpcUa_Int32>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                outgoingData.setInt32(static_cast<OpcUa_UInt32>(value));
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case OpcUaType_UInt64:
            if (isWithinRange<OpcUa_UInt64>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                outgoingData.setUInt64(static_cast<OpcUa_UInt64>(value));
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case OpcUaType_Int64:
            if (isWithinRange<OpcUa_Int64>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                outgoingData.setInt64(static_cast<OpcUa_Int64>(value));
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case OpcUaType_Float:
            if (isWithinRange<OpcUa_Float>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                outgoingData.setFloat(static_cast<OpcUa_Float>(value));
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case OpcUaType_Double:
            if (isWithinRange<OpcUa_Double>(value)) {
                Guard G(outgoingLock);
                isdirty = true;
                outgoingData.setDouble(static_cast<OpcUa_Double>(value));
            } else {
                (void) recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
                ret = 1;
            }
            break;
        case OpcUaType_String:
        { // Scope of Guard G
            Guard G(outgoingLock);
            isdirty = true;
            outgoingData.setString(static_cast<UaString>(std::to_string(value).c_str()));
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

    // Write array value for EPICS String / OpcUa_String
    long
    writeArray (const char **value, const epicsUInt32 len,
                const epicsUInt32 num,
                OpcUa_BuiltInType targetType,
                dbCommon *prec);

    // Write array value as templated function on EPICS type, OPC UA container and simple (element) types
    // (latter *must match* OPC UA type enum argument)
    // CAVEAT: changes must also be reflected in the specialization (in DataElementLeaf.cpp)
    template<typename ET, typename CT, typename ST>
    long
    writeArray (const ET *value, const epicsUInt32 num,
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
            // The array methods must cast away the constness of their value argument
            // as the UA SDK API uses non-const parameters
            ST *val = const_cast<ST *>(reinterpret_cast<const ST *>(value));
            CT arr(static_cast<OpcUa_Int32>(num), val);
            { // Scope of Guard G
                Guard G(outgoingLock);
                isdirty = true;
                UaVariant_set(outgoingData, arr);
            }

            dbgWriteArray(num, epicsTypeString(*value));
        }
        return ret;
    }

    UpdateQueue<UpdateUaSdk> incomingQueue;  /**< queue of incoming values */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTLEAF_H
