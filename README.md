# CA Lab (LabVIEW + EPICS)

<img src="https://www.helmholtz-berlin.de/media/media/angebote/it/experimentsteuerung/ca-lab/channelaccesspluslabview-hb-logo.jpg"
     alt="CA Lab Logo"
     style="float: left; margin-right: 10px;" />
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

**CA Lab works with Windows® and Linux.**<br/>
This interface requires only LabVIEW™.

To use this interface, it's **not** necessary to create any LabVIEW™ project nor to use external services. CA Lab can be used directly.

CA Lab is **open source** and works with all LabVIEW™ versions from 7.0 up to the current version (32bit/64bit).<br/>
It has been tested under Windows 7®, Windows 10® (Build 18363.836), RedHat® Enterprise Linux® 8.5.0, and Ubuntu® 20.04.

<img src="https://www.helmholtz-berlin.de/media/media/angebote/it/experimentsteuerung/ca-lab/calabinterface.png"
    alt="schema of CA Lab interface"><br/>
<sup>schema of CA Lab interface</sup>

Any VI can use caLabGet.vi to read or caLabPut.vi to write EPICS variables.<br/>
Use caLabEvent.vi to create user events for any EPICS variables.<br/>
Call CaLabInfo.vi to get context information of the CA Lab library.<br/>
In Windows®, you can use CaLabSoftIOC.vi to create new EPICS variables and start them. In Linux, you can use the native soft IOC to do that. It comes with the CA Lab package.

These CA Lab VIs call the interface library 'caLab', which uses EPICS base libraries 'ca' and 'Com' to provide Channel Access functions.

CA Lab library builds an internal PV cache and monitors PVs to improve the read and write access and reduce network traffic. Optional, you can disable caching.

<a href="https://www.helmholtz-berlin.de/zentrum/locations/it/software/exsteuer/calab/index_en.html">🔗more info and examples</a>
