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
#include "DataElementVertex.h"

namespace DevOpcua {

// Constructed from Leaf or Vertex
DataElementVertex::DataElementVertex (const std::string &nname,
                                      ItemUaSdk *pitem,
                                      std::weak_ptr<DataElementUaSdk> child)
    : DataElementUaSdk(nname, pitem)
{
    elements.push_back(child);
}

void
DataElementVertex::show (const int level, const unsigned int indent) const
{
    const std::string ind(indent*2, ' ');
    std::cout << ind
              << "node=" << name << " children=" << elements.size()
              << " mapped=" << (mapped ? "y" : "n") << "\n";
    for (auto it : elements)
        if (auto pelem = it.lock())
            pelem->show(level, indent + 1);
}

// Helper: create the index-to-element map for child elements
void
DataElementVertex::mapChildren (const UaStructureDefinition &definition)
{
    if (debug() >= 5)
        std::cout << " ** creating index-to-element map for child elements" << std::endl;
    for (auto &it : elements) {
        auto pelem = it.lock();
        for (int i = 0; i < definition.childrenCount(); i++)
            if (pelem->name == definition.child(i).name().toUtf8())
                elementMap.insert({i, it});
    }
    if (debug() >= 5)
        std::cout << " ** " << elementMap.size() << "/" << elements.size()
                  << " child elements mapped to a "
                  << "structure of " << definition.childrenCount() << " elements" << std::endl;
    mapped = true;
}

void
DataElementVertex::setIncomingEvent (ProcessReason reason, const UaVariant &value)
{
    // Cache a copy of this element
    incomingData = value;

    if (debug() >= 5)
        std::cout << "Element " << name << " splitting structured data to "
                  << elements.size() << " child elements" << std::endl;

    if (value.type() == OpcUaType_ExtensionObject) {
        UaExtensionObject extensionObject;
        value.toExtensionObject(extensionObject);

        // Get the structure definition from the dictionary
        UaStructureDefinition definition = item->structureDefinition(extensionObject.encodingTypeId());
        if (!definition.isNull()) {
            if (!definition.isUnion()) {
                // ExtensionObject is a structure
                // Decode the ExtensionObject to a UaGenericValue to provide access to the structure fields
                UaGenericStructureValue genericValue;
                genericValue.setGenericValue(extensionObject, definition);

                if (!mapped)
                    mapChildren(definition);
                for (auto &it : elementMap) {
                    auto pelem = it.second.lock();
                    pelem->setIncomingEvent(reason, genericValue.value(it.first));
                }
            }

        } else {
            errlogPrintf("Cannot get a structure definition for %s - check access to type dictionary\n",
                         extensionObject.dataTypeId().toString().toUtf8());
        }
    }
}

void
DataElementVertex::setIncomingEvent (ProcessReason reason)
{
    for (auto &it : elements) {
        auto pelem = it.lock();
        pelem->setIncomingEvent(reason);
    }
}

// Helper: update one data structure element from pointer to child
bool
DataElementVertex::updateDataInGenericValue (UaGenericStructureValue &value,
                                             const int index,
                                             std::shared_ptr<DataElementUaSdk> pelem)
{
    bool updated = false;
    { // Scope of Guard G
        Guard G(pelem->outgoingLock);
        if (pelem->isdirty) {
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

const UaVariant &
DataElementVertex::getOutgoingData ()
{
    if (debug() >= 4)
        std::cout << "Element " << name << " updating structured data from "
                  << elements.size() << " child elements" << std::endl;

    outgoingData = incomingData;
    isdirty = false;
    if (outgoingData.type() == OpcUaType_ExtensionObject) {
        UaExtensionObject extensionObject;
        outgoingData.toExtensionObject(extensionObject);

        // Get the structure definition from the dictionary
        UaStructureDefinition definition = item->structureDefinition(extensionObject.encodingTypeId());
        if (!definition.isNull()) {
            if (!definition.isUnion()) {
                // ExtensionObject is a structure
                // Decode the ExtensionObject to a UaGenericValue to provide access to the structure fields
                UaGenericStructureValue genericValue;
                genericValue.setGenericValue(extensionObject, definition);

                if (!mapped)
                    mapChildren(definition);
                for (auto &it : elementMap) {
                    auto pelem = it.second.lock();
                    if (updateDataInGenericValue(genericValue, it.first, pelem))
                        isdirty = true;
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

        } else {
            errlogPrintf("Cannot get a structure definition for %s - check access to type dictionary\n",
                         extensionObject.dataTypeId().toString().toUtf8());
        }
    }
    return outgoingData;
}

void
DataElementVertex::requestRecordProcessing (const ProcessReason reason) const
{
    for (auto &it : elementMap) {
        auto pelem = it.second.lock();
        pelem->requestRecordProcessing(reason);
    }
}

} // namespace DevOpcua
