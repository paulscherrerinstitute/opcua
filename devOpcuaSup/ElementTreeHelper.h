/*************************************************************************\
* Copyright (c) 2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#ifndef DEVOPCUA_ELEMENTTREEHELPER_H
#define DEVOPCUA_ELEMENTTREEHELPER_H

#include <memory>
#include <vector>
#include <string>

#include "Item.h"
#include "RecordConnector.h"

namespace DevOpcua {

/**
 * @brief Manipulators for a tree of DataElements representing structured OPC UA data.
 *
 * Principle of operation for the DataElement tree
 *
 * A node can be a Vertex (structural node), having children nodes, or a Leaf,
 * pointing to a RecordConnector.
 * Each node has a single vertex parent node. (The root node has no parent.)
 *
 * Vertex and Leaf both inherit from a common Node class that carries the low-level
 * API (push/pull structured data and events).
 * The leaf class also inherits the generic high-level DataElement API (read/write
 * through the RecordConnector).
 *
 * The tree implementation makes heavy use of C++ smart pointers:
 * Each Node has a shared_ptr to its parent.
 * Each Vertex has a std::vector of weak_ptr to its children.
 * That way any Leaf can freely be added or removed and all intermediate
 * nodes are properly ref-counted and deleted.
 *
 * The template types used (all specific to the implementation):
 * V is the Vertex class,
 * B is the base class of all DataElements (Leaf and Vertex),
 * I is the class of the Item.
 */

namespace ElementTreeHelper {

static const char defaultSeparator = '.';

/**
 * @brief Split off the last part of a path (after last separator)
 *
 * @param[out] fullpath  full path (last separator and returned part will be erased)
 * @param separator  hierarchy separator character
 *
 * @return last part of the path (after last separator)
 */
std::string
splitLastName(std::string &fullpath, const char separator = defaultSeparator)
{
    std::string lastname;
    size_t sep = fullpath.find_last_of(separator);
    // allow escaping separators
    while (sep != std::string::npos && fullpath[sep-1] == '\\') {
        fullpath.erase(--sep, 1);
        sep = fullpath.find_last_of(separator, --sep);
    }
    if (sep == std::string::npos) {
        std::swap(lastname, fullpath);
    } else {
        lastname = fullpath.substr(sep + 1);
        fullpath.erase(sep);
    }
    if (lastname.empty())
        lastname = "<ROOT>";
    return lastname;
}

/**
 * @brief Split off the first part of a path (before first separator)
 *
 * @param[out] fullpath  full path (returned part and first separator will be erased)
 * @param separator  hierarchy separator character
 *
 * @return last part of the path (after last separator)
 */
std::string
splitFirstName(std::string &fullpath, const char separator = defaultSeparator)
{
    std::string firstname;
    size_t sep = fullpath.find_first_of(separator);
    // allow escaping separators
    while (sep != std::string::npos && fullpath[sep-1] == '\\') {
        fullpath.erase(sep-1, 1);
        sep = fullpath.find_first_of(separator, sep);
    }
    if (sep == std::string::npos) {
        std::swap(firstname, fullpath);
    } else {
        firstname = fullpath.substr(0, sep);
        fullpath.erase(0, sep + 1);
    }
    return firstname;
}

// beginning of path will be shortened by the existing sequence of nodes
// root must point to the root node

template<class V, class B>
std::weak_ptr<V> closestExistingVertex(std::weak_ptr<V> &root, std::string &path)
{
    if (root.expired()) return nullptr;

    // Starting from root element...
    std::weak_ptr<V> nextelem = root;
    bool found;

    do {
        found = false;
        std::string name(splitFirstName(path));

        // Search for name in list of children
        if (!name.empty()) {
            if (auto elem = nextelem.lock()) {
                for (auto it : elem->elements) {
                    if (auto pit = it.lock()) {
                        if (pit->name == name) {
                            found = true;
                            nextelem = it;
                            break;
                        }
                    }
                }
            }
        }
    } while (found && !path.empty());
    return nextelem;
}

#if 0



template<class V, class B, class I>
void
addElementToTree (I *item,
                  std::weak_ptr<B> leaf,
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

    auto chainelem = std::make_shared<DataElementLeaf>(leafname, item, pcon);
    pcon->setDataElement(chainelem);

    // Starting from item...
    std::weak_ptr<DataElementLeaf> topelem = item->rootElement;

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

        chainelem->parent = std::make_shared<DataElementLeaf>(name, item, chainelem);
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
        chainelem->parent = std::make_shared<DataElementLeaf>("[ROOT]", item, chainelem);
        chainelem = chainelem->parent;
        item->rootElement = chainelem;
    }
}





#endif











} // namespace ElementTreeHelper
} // namespace DevOpcua

#endif // DEVOPCUA_ELEMENTTREEHELPER_H
