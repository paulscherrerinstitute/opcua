<a target="_blank" href="http://semver.org">![Version][badge.version]</a>
<a target="_blank" href="https://travis-ci.org/ralphlange/opcua">![Travis status][badge.travis]</a>
<a target="_blank" href="https://www.codacy.com/app/ralphlange/opcua">![Codacy grade][badge.codacy]</a>

# opcua - EPICS Device Support for OPC UA

[EPICS](https://epics-controls.org) Device Support module for interfacing
to the OPC UA protocol. The architecture allows supporting different
implementations of the low level client library.

Linux and Windows builds are supported.

## Status

:warning:
This module is under development.
Please contact the author [Ralph Lange](mailto:ralph.lange@gmx.de) for details.
:warning:

The first (and - at this time - only) supported OPC UA client library is the
commercially available Unified Automation C++ Based OPC UA Client SDK.

## Prerequisites

*   A C++ compiler that supports the C++11 standard. \
    Microsoft Visual C++ needs to be from Visual Studio 2015 or newer.
    g++ needs to be 4.6 or above.

*   [EPICS Base](https://epics-controls.org/resources-and-support/base/)
    3.15.5 (and up; EPICS 7 is supported).

*   The [gtest module](https://github.com/epics-modules/gtest) to compile and
    run the Google Test based tests.

### Using the Unified Automation Client SDK

*   Unified Automation C++ Based [OPC UA Client SDK][unified.sdk]
    (1.5/1.6/1.7 are supported, as well as their evaluation bundles).

*   For OPC UA security support (authentication/encryption), you need
    openssl/libcrypto on your system - both when compiling the SDK and when
    generating any binaries (IOCs).

*   For more details, refer to the `README.md` in the
    [`devOpcuaSup/UaSdk`][uasdk.dir] directory.

## Building the module

This is a standard EPICS module.

Inside the `configure` subdirectory or one level above the TOP location
(TOP is where this README file resides), create a file `RELEASE.local`
that sets `EPICS_BASE` and `GTEST` to the absolute paths inside your EPICS
installation. The `GTEST` module is needed to compile and run the tests.
Not defining it produces a clean build, but without any tests.

Configure the compiler on Linux to use the C++11 standard by adding
```makefile
USR_CXXFLAGS_Linux += -std=c++11
```
to the `CONFIG_SITE` file (or one of the host/target specific site
configuration files). \
It is preferable to set this option globally in EPICS Base.

The configuration necessary when building against a specific client library
is documented in the `README.md` file inside the respective subdirectory of
`devOpcuaSup`.

## Using the module

IOC applications that use the module need to

*   add an entry to the Device Support module in their `RELEASE.local` file
*   include `opcua.dbd` when building the IOC's DBD file
*   include `opcua` in the support libraries for the IOC binary.

## Documentation

Sparse.

The documentation folder of the Device Support module contains the
[Requirements Specification (SRS)][requirements.pdf] giving an introduction
and the list of requirements that should convey a good idea of the planned
features.

The [Cheat Sheet][cheatsheet.pdf] explains the configuration in the startup
script and the database links.

## Feedback / Reporting issues

Please use the GitHub project's
[issue tracker](https://github.com/ralphlange/opcua/issues).

## Credits

This module is based on extensive
[prototype work](https://github.com/bkuner/opcUaUnifiedAutomation)
by Bernhard Kuner (HZB/BESSY) and uses ideas and code snippets from
Michael Davidsaver (Osprey DCS).
Additional help from Carsten Winkler (HZB/BESSY).

## License

This module is distributed subject to a Software License Agreement found
in file LICENSE that is included with this distribution.

<!-- Links -->
[badge.version]: https://img.shields.io/github/v/release/ralphlange/opcua?sort=semver
[badge.travis]: https://travis-ci.org/ralphlange/opcua.svg?branch=master
[badge.codacy]: https://api.codacy.com/project/badge/Grade/65b1d28ca5e34a7d853d168f50beaafc

[unified.sdk]: https://www.unified-automation.com/products/client-sdk/c-ua-client-sdk.html

[uasdk.dir]: https://github.com/ralphlange/opcua/tree/master/devOpcuaSup/UaSdk
[requirements.pdf]: https://docs.google.com/viewer?url=https://raw.githubusercontent.com/ralphlange/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20SRS.pdf
[cheatsheet.pdf]: https://docs.google.com/viewer?url=https://raw.githubusercontent.com/ralphlange/opcua/master/documentation/EPICS%20Support%20for%20OPC%20UA%20-%20Cheat%20Sheet.pdf
