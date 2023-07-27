# CA Lab - LabVIEW (Realtime) + EPICS

[![Download](https://github.com/epics-extensions/CALab/assets/17197773/ddda9486-4d74-4e74-b729-1843c2a9ea0d)](https://github.com/epics-extensions/CALab/releases)</br>
[Download](https://github.com/epics-extensions/CALab/releases)
## Overview

What is CA Lab?<br/>
**It is a**<br/>
- user-friendly,<br/>
- lightweight and<br/>
- high performance<br/>interface between LabVIEW™ and EPICS.<br/>

**This interface uses**<br/>
- proven EPICS BASE libraries (V7),<br/>
- a CA Lab interface library<br/>
- and polymorphic VIs<br/>
to access EPICS variables.

**Creating, reading and writing EPICS variables** is very simple now. Also, **user events** for EPICS variables can be implemented easily.<br/>
EPICS time stamp, status, severity, and optional PV fields (properties) are bound into a resulting data cluster. You avoid inconsistent data sets.<br/>
It's easy to create an executable of your VI.

**CA Lab works with Windows®, Linux and Realtime Linux.**<br/>
This interface requires only LabVIEW™.

To use this interface, it's **not** necessary to create any LabVIEW™ project nor to use external services. CA Lab can be used directly.

CA Lab is **open source** and works with all LabVIEW™ versions from 2019 up to the current version.<br/>
It has been tested under Windows 7®, Windows 10/11® , Linux (RHEL 8.5, Ubuntu 20.04/22.04) and NI Linux RT (2022).

<img src="https://github.com/epics-extensions/CALab/assets/17197773/a975e0fc-20b8-441d-8013-f0d11c06898b"
    alt="schema of CA Lab interface"><br/>
<sup>schema of CA Lab interface</sup>

Any VI can use caLabGet.vi to read or caLabPut.vi to write EPICS variables.<br/>
Use caLabEvent.vi to create user events for any EPICS variables.<br/>
Call CaLabInfo.vi to get context information of the CA Lab library.<br/>
You can use CaLabSoftIOC.vi to create new EPICS variables and start them.

These CA Lab VIs call the interface library 'caLab', which uses EPICS base libraries 'ca' and 'Com' to provide Channel Access functions.

CA Lab library builds an internal PV cache and monitors PVs to improve the read and write access and reduce network traffic. Optional, you can disable caching.

CA Lab includes a EPICS Base package (caget, caput, camonitor, softIOC and more).

<a href="https://github.com/epics-extensions/CALab/releases">🔗download Windows® setups and source code releases</a></br>
<a href="https://www.helmholtz-berlin.de/zentrum/locations/it/calab">🔗more info and examples</a>
