# CA Lab - LabVIEW (Realtime) + EPICS

<p align="center">
  <a href="https://github.com/epics-extensions/CALab/releases">
    <img src="https://github.com/user-attachments/assets/5f7999fb-08ef-40d5-aecc-086edb485a39" alt="Download" width="220" />
  </a>
</p>

> A lightweight, high-performance, user-friendly interface between LabVIEW™ and EPICS.

## Table of contents
- [Overview](#overview)
- [Highlights](#highlights)
- [Compatibility](#compatibility)
- [How it works](#how-it-works)
- [Included tools](#included-tools)
- [Downloads and links](#downloads-and-links)

## Overview
CA Lab connects LabVIEW™ VIs to EPICS PVs and provides a streamlined Channel Access interface.

## Highlights
- Simple create/read/write of EPICS variables and user events
- Consistent data clusters (timestamp, status, severity, optional PV fields)
- Easy to build executables from your VIs
- Requires only LabVIEW™ (no project or external services)
- High performance interface
- Open source

## Compatibility
| Item | Details |
| --- | --- |
| LabVIEW™ | 2019 to current |
| Platforms | Windows®, Linux, NI Linux RT |
| EPICS | EPICS BASE libraries (V7) |

Tested on Windows 11®, Linux (Ubuntu 24.04), and NI Linux RT (2022).

## How it works
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://github.com/user-attachments/assets/6d1d83ab-ba10-4683-8d52-6283d5299da6">
  <source media="(prefers-color-scheme: light)" srcset="https://github.com/user-attachments/assets/f4385b15-c598-4805-ab27-27867f366aa5">
  <img width="1100" height="260" alt="Schema of CA Lab interface" src="https://github.com/user-attachments/assets/f4385b15-c598-4805-ab27-27867f366aa5">
</picture>
<sup>Schema of CA Lab interface</sup>

From any LabVIEW VI, you can directly call these interface VIs:
- `CaLabGet.vi` to read EPICS variables
- `CaLabPut.vi` to write EPICS variables
- `CaLabEvent.vi` to create user events for EPICS variables
- `CaLabInfo.vi` to get context information of the CA Lab library
- `CaLabSoftIOC.vi` to create new EPICS variables and start them

These CA Lab VIs call the interface library `caLab`, which uses EPICS base libraries `ca` and `Com` to provide Channel Access functions.

CA Lab builds an internal PV cache and monitors PVs to improve read/write access and reduce network traffic. Optionally, you can disable caching.

## Included tools
CA Lab includes an EPICS Base package (caget, caput, camonitor, softIOC, and more).

## Downloads and links
- [Releases](https://github.com/epics-extensions/CALab/releases)
- [More info and examples (Helmholtz-Zentrum Berlin)](https://www.helmholtz-berlin.de/zentrum/organisation/it/calab/index_en.html)
