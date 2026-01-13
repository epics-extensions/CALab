# CA Lab - LabVIEW (Realtime) + EPICS<br/>[![Download](https://github-production-user-asset-6210df.s3.amazonaws.com/17197773/256530946-ddda9486-4d74-4e74-b729-1843c2a9ea0d.png "Download")](https://github.com/epics-extensions/CALab/releases "Download")


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

**CA Lab works with Windows®, Linux and NI Linux RT.**<br/>
This interface requires only LabVIEW™.

To use this interface, it's **not** necessary to create any LabVIEW™ project nor to use external services. CA Lab can be used directly.

CA Lab is **open source** and works with all LabVIEW™ versions from 2019 up to the current version.<br/>
It has been tested under Windows 11®, Linux (Ubuntu 24.04) and NI Linux RT (2022).

<img width="1100" height="260" alt="schema of CA Lab interface" src="https://github.com/user-attachments/assets/218adaed-2403-4fc9-90d6-ccc1225ad8b7" />
<sup>schema of CA Lab interface</sup>

Any VI can use caLabGet.vi to read or caLabPut.vi to write EPICS variables.<br/>
Use caLabEvent.vi to create user events for any EPICS variables.<br/>
Call CaLabInfo.vi to get context information of the CA Lab library.<br/>
You can use CaLabSoftIOC.vi to create new EPICS variables and start them.

These CA Lab VIs call the interface library 'caLab', which uses EPICS base libraries 'ca' and 'Com' to provide Channel Access functions.

CA Lab library builds an internal PV cache and monitors PVs to improve the read and write access and reduce network traffic. Optionally, you can disable caching.

CA Lab includes an EPICS Base package (caget, caput, camonitor, softIOC and more).


<a href="https://github.com/epics-extensions/CALab/releases">download Windows® setups and source code releases</a><br/>
<a href="https://www.helmholtz-berlin.de/zentrum/organisation/it/calab/index_en.html">more info and examples</a>
