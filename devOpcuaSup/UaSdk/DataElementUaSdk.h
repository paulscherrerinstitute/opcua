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

#ifndef DEVOPCUA_DATAELEMENTUASDK_H
#define DEVOPCUA_DATAELEMENTUASDK_H

#include <uadatavalue.h>
#include <statuscode.h>

#include <errlog.h>

#include "DataElement.h"
#include "devOpcua.h"
#include "RecordConnector.h"
#include "ItemUaSdk.h"

namespace DevOpcua {

class DataElementVertex;

/**
 * @brief Base class of Leaf and Vertex DataElements (includes the low-level API)
 *
 */
class DataElementUaSdk
{
public:
    virtual ~DataElementUaSdk();

    /**
     * @brief Print configuration and status.
     */
    virtual void show(const int level, const unsigned int indent) const = 0;

    /**
     * @brief Push an incoming event (with data) down the tree.
     *
     * Called from the OPC UA client worker thread when new data is
     * received from the OPC UA session.
     *
     * @param value  new value for this data element
     * @param reason  reason for this value update
     */
    virtual void setIncomingEvent(const ProcessReason reason, const UaVariant &value) = 0;

    /**
     * @brief Push an incoming event (without data) down the tree.
     *
     * Called from the OPC UA client worker thread when an event without data
     * is received or created (failed read/write, connection loss).
     *
     * @param reason  reason for this value update
     */
    virtual void setIncomingEvent(const ProcessReason reason) = 0;

    /**
     * @brief Get the outgoing data value from the DataElement.
     *
     * Called from the OPC UA client worker thread when data is being
     * assembled in OPC UA session for sending.
     *
     * @return  reference to outgoing data
     */
    virtual const UaVariant &getOutgoingData() = 0;

    /**
     * @brief Clear (discard) the current outgoing data.
     *
     * Called by the low level connection (OPC UA session)
     * after it is done accessing the data in the context of sending.
     *
     * In case an implementation uses a queue, this should remove the
     * oldest element from the queue, allowing access to the next element
     * with the next send.
     */
    virtual void clearOutgoingData() = 0;

    /**
     * @brief Create processing requests for record(s) attached to this element.
     */
    virtual void requestRecordProcessing(const ProcessReason reason) const = 0;

    /**
     * @brief Get debug level.
     * @return debug level
     */
    virtual int debug() const = 0;

    const std::string name;                    /**< element name */

    UaVariant incomingData;                    /**< cache of latest incoming value */

    epicsMutex outgoingLock;                   /**< data lock for outgoing value */
    bool isdirty;                              /**< outgoing value has been (or needs to be) updated */
    UaVariant outgoingData;                    /**< cache of latest outgoing value */

protected:
    DataElementUaSdk(const std::string &name, ItemUaSdk *item)
        : name(name)
        , isdirty(false)
        , item(item)
    {}

    ItemUaSdk *const item;                     /**< pointer to the relevant item */
    std::shared_ptr<DataElementVertex> parent; /**< parent */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTUASDK_H
