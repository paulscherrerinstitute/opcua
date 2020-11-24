/*************************************************************************\
* Copyright (c) 2020 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 */

#include <memory>
#include <utility>
#include <gtest/gtest.h>

#include <epicsTime.h>

#include "ElementTreeHelper.h"

namespace {

using namespace DevOpcua;

// Test classes with the minimal required interface
// and test reference counting

class TestNode;

class TestAPI {
public:
    TestAPI() { TestAPI::instances++; }
    ~TestAPI() { TestAPI::instances--; }
    std::shared_ptr<TestNode> parent;
private:
    static unsigned int instances;
};

unsigned int TestAPI::instances = 0;

class TestNode : public TestAPI {
public:
    TestNode() { TestNode::instances++; }
    ~TestNode() { TestNode::instances--; }
    std::vector<std::weak_ptr<TestAPI>> elements;
private:
    static unsigned int instances;
};

unsigned int TestNode::instances = 0;

class TestLeaf : public TestAPI {
public:
    TestLeaf() { TestLeaf::instances++; }
    ~TestLeaf() { TestLeaf::instances--; }
private:
    static unsigned int instances;
};

unsigned int TestLeaf::instances = 0;

/* std::string splitLastName(std::string &fullpath, const char separator = defaultSeparator)
 *
 * @brief Split off the last part of a path (after last separator)
 *
 * @param[out] fullpath  full path (last separator and returned part will be erased)
 * @param separator  hierarchy separator character
 *
 * @return last part of the path (after last separator)
 */

TEST(ElementTreeHelperTest, splitLastName_empty) {
    std::string path("");
    std::string name = ElementTreeHelper::splitLastName(path);
    EXPECT_EQ(path, "") << "path not empty after splitting last name from path ''";
    EXPECT_EQ(name, "<ROOT>") << "name not '<ROOT>' after splitting last name from path ''";
}

TEST(ElementTreeHelperTest, splitLastName_one) {
    std::string path("test123");
    std::string name = ElementTreeHelper::splitLastName(path);
    EXPECT_EQ(path, "") << "path not empty after splitting last name from path 'test123'";
    EXPECT_EQ(name, "test123") << "name not 'test123' after splitting last name from path 'test123'";
}

TEST(ElementTreeHelperTest, splitLastName_two) {
    std::string path("lev1.lev2");
    std::string name = ElementTreeHelper::splitLastName(path);
    EXPECT_EQ(path, "lev1") << "path not 'lev1' after splitting last name from path 'lev1.lev2'";
    EXPECT_EQ(name, "lev2") << "name not 'lev2' after splitting last name from path 'lev1.lev2'";
}

TEST(ElementTreeHelperTest, splitLastName_three) {
    std::string path("lev1.lev2.lev3");
    std::string name = ElementTreeHelper::splitLastName(path);
    EXPECT_EQ(path, "lev1.lev2") << "path not 'lev1.lev2' after splitting last name from path 'lev1.lev2.lev3'";
    EXPECT_EQ(name, "lev3") << "name not 'lev3' after splitting last name from path 'lev1.lev2.lev3'";
}

TEST(ElementTreeHelperTest, splitLastName_escapedSepInName) {
    std::string path("lev1.lev2\\.lev3");
    std::string name = ElementTreeHelper::splitLastName(path);
    EXPECT_EQ(path, "lev1") << "path not 'lev1' after splitting last name from path 'lev1.lev2\\.lev3'";
    EXPECT_EQ(name, "lev2.lev3") << "name not 'lev2.lev3' after splitting last name from path 'lev1.lev2\\.lev3'";
}

TEST(ElementTreeHelperTest, splitLastName_escapedSepInPath) {
    std::string path("lev1\\.lev2.lev3");
    std::string name = ElementTreeHelper::splitLastName(path);
    EXPECT_EQ(path, "lev1\\.lev2") << "path not 'lev1\\.lev2' after splitting last name from path 'lev1\\.lev2.lev3'";
    EXPECT_EQ(name, "lev3") << "name not 'lev3' after splitting last name from path 'lev1\\.lev2.lev3'";
}

TEST(ElementTreeHelperTest, splitLastName_multipleEscapedSep) {
    std::string path("lev1\\.lev2\\.lev3.lev4\\.lev5\\.lev6\\.lev7");
    std::string name = ElementTreeHelper::splitLastName(path);
    EXPECT_EQ(path, "lev1\\.lev2\\.lev3") << "path not 'lev1\\.lev2\\.3' after splitting last name from path 'lev1\\.lev2\\.lev3.lev4\\.lev5\\.lev6\\.lev7'";
    EXPECT_EQ(name, "lev4.lev5.lev6.lev7") << "name not 'lev4.lev5.lev6.lev7' after splitting last name from path 'lev1\\.lev2\\.lev3.lev4\\.lev5\\.lev6\\.lev7'";
}

/* std::string splitFirstName(std::string &fullpath, const char separator = defaultSeparator)
 *
 * @brief Split off the first part of a path (before first separator)
 *
 * @param[out] fullpath  full path (returned part and first separator will be erased)
 * @param separator  hierarchy separator character
 *
 * @return last part of the path (after last separator)
 */

TEST(ElementTreeHelperTest, splitFirstName_empty) {
    std::string path("");
    std::string name = ElementTreeHelper::splitFirstName(path);
    EXPECT_EQ(path, "") << "path not empty after splitting first name from path ''";
    EXPECT_EQ(name, "") << "name not empty after splitting first name from path ''";
}

TEST(ElementTreeHelperTest, splitFirstName_one) {
    std::string path("test123");
    std::string name = ElementTreeHelper::splitFirstName(path);
    EXPECT_EQ(path, "") << "path not empty after splitting first name from path 'test123'";
    EXPECT_EQ(name, "test123") << "name not 'test123' after splitting first name from path 'test123'";
}

TEST(ElementTreeHelperTest, splitFirstName_two) {
    std::string path("lev1.lev2");
    std::string name = ElementTreeHelper::splitFirstName(path);
    EXPECT_EQ(path, "lev2") << "path not 'lev2' after splitting first name from path 'lev1.lev2'";
    EXPECT_EQ(name, "lev1") << "name not 'lev1' after splitting first name from path 'lev1.lev2'";
}

TEST(ElementTreeHelperTest, splitFirstName_three) {
    std::string path("lev1.lev2.lev3");
    std::string name = ElementTreeHelper::splitFirstName(path);
    EXPECT_EQ(path, "lev2.lev3") << "path not 'lev2.lev3' after splitting first name from path 'lev1.lev2.lev3'";
    EXPECT_EQ(name, "lev1") << "name not 'lev1' after splitting first name from path 'lev1.lev2.lev3'";
}

TEST(ElementTreeHelperTest, splitFirstName_escapedSepInPath) {
    std::string path("lev1.lev2\\.lev3");
    std::string name = ElementTreeHelper::splitFirstName(path);
    EXPECT_EQ(path, "lev2\\.lev3") << "path not 'lev2\\.lev3' after splitting first name from path 'lev1.lev2\\.lev3'";
    EXPECT_EQ(name, "lev1") << "name not 'lev1' after splitting first name from path 'lev1.lev2\\.lev3'";
}

TEST(ElementTreeHelperTest, splitFirstName_escapedSepInName) {
    std::string path("lev1\\.lev2.lev3");
    std::string name = ElementTreeHelper::splitFirstName(path);
    EXPECT_EQ(path, "lev3") << "path not 'lev3' after splitting first name from path 'lev1\\.lev2.lev3'";
    EXPECT_EQ(name, "lev1.lev2") << "name not 'lev1.lev2' after splitting first name from path 'lev1\\.lev2.lev3'";
}

TEST(ElementTreeHelperTest, splitFirstName_multipleEscapedSep) {
    std::string path("lev1\\.lev2\\.lev3.lev4\\.lev5\\.lev6\\.lev7");
    std::string name = ElementTreeHelper::splitFirstName(path);
    EXPECT_EQ(path, "lev4\\.lev5\\.lev6\\.lev7") << "path not 'lev4\\.lev5\\.lev6\\.lev7' after splitting first name from path 'lev1\\.lev2\\.lev3.lev4\\.lev5\\.lev6\\.lev7'";
    EXPECT_EQ(name, "lev1.lev2.lev3") << "name not 'lev1.lev2.lev3' after splitting first name from path 'lev1\\.lev2\\.lev3.lev4\\.lev5\\.lev6\\.lev7'";
}

} // namespace
