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

#ifndef DEVOPCUA_DATAELEMENTVERTEX_H
#define DEVOPCUA_DATAELEMENTVERTEX_H

#include <string>
#include <memory>
#include <unordered_map>

#include <uadatavalue.h>
#include <statuscode.h>

#include <errlog.h>

#include "DataElementUaSdk.h"
#include "devOpcua.h"

namespace DevOpcua {

/**
 * @brief The implementation dependent structural node of an DataElement tree.
 *
 * Inherits the low-level interface (with UA SDK types).
 *
 */
class DataElementVertex : public DataElementUaSdk
{
public:
    virtual ~DataElementVertex() override;

    /**
     * @brief Constructor for DataElementVertex from child element.
     *
     * Creates a Vertex (intermediate node) element of the data structure. The child holds
     * a shared pointer, while the parent has a weak pointer in its list/map of child
     * nodes, to facilitate traversing the structure.
     *
     * @param nname  node name
     * @param pitem  relevant item
     * @param child  weak pointer to child
     */
    DataElementVertex(const std::string &nname,
                      ItemUaSdk *pitem,
                      std::weak_ptr<DataElementUaSdk> child);

    // Implement DataElementUaSdk (OPCUA side) interface
    virtual void show(const int level, const unsigned int indent) const override;
    virtual void setIncomingEvent(const ProcessReason reason, const UaVariant &value) override;
    virtual void setIncomingEvent(const ProcessReason reason) override;
    virtual const UaVariant &getOutgoingData() override;
    virtual void requestRecordProcessing(const ProcessReason reason) const override;
    virtual int debug() const override { return item->debug(); }

private:
    // Helpers
    void mapChildren(const UaStructureDefinition &definition);
    bool updateDataInGenericValue(UaGenericStructureValue &value,
                                  const int index,
                                  std::shared_ptr<DataElementUaSdk> pelem);

    std::vector<std::weak_ptr<DataElementUaSdk>> elements;                 /**< children */

    std::unordered_map<int, std::weak_ptr<DataElementUaSdk>> elementMap;   /**< index-to-child map of children */
    bool mapped;                                                           /**< child name to index mapping done */
};

} // namespace DevOpcua

#endif // DEVOPCUA_DATAELEMENTVERTEX_H
