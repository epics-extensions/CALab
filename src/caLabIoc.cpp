// This software is copyrighted by the HELMHOLTZ-ZENTRUM BERLIN FUER MATERIALIEN UND ENERGIE G.M.B.H., BERLIN, GERMANY (HZB).
// The following terms apply to all files associated with the software. HZB hereby grants permission to use, copy, and modify
// this software and its documentation for non-commercial educational or research purposes, provided that existing copyright
// notices are retained in all copies. The receiver of the software provides HZB with all enhancements, including complete
// translations, made by the receiver.
// IN NO EVENT SHALL HZB BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING
// OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF, EVEN IF HZB HAS BEEN ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE. HZB SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
// AND HZB HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

//==================================================================================================
// Name        : caLab.cpp
// Author      : Carsten Winkler
// Version     : 1.6.0.0
// Copyright   : HZB
// Description : library for reading, writing and handle events of EPICS variables (PVs) in LabVIEW
//==================================================================================================

// Definitions
#include <extcode.h>
#include <windows.h>
#include <TlHelp32.h>
#include <stdio.h>
#include <dbDefs.h>

#ifdef WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
static bool externalIocInstance = false;
static char** pszNameList = 0x0;
static size_t iListCount = 0;

//extern "C" void disconnectAll(void);

// Typedefs
typedef struct {
    size_t dimSize;
    LStrHandle elt[1];
} sStringArray;
typedef sStringArray **sStringArrayHdl;

// Search for process and optional terminate it
//    wcProcessFileName: name of process
//    kill:              TRUE: if find process terminate it
//    returns TRUE if process found
static bool ProcessByName(wchar_t* wcProcessFileName, bool kill) {
    HANDLE hProcessSnap;
    HANDLE hProcess;
    PROCESSENTRY32 pe32;

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE)
        return false;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return false;
    }
    do {
        if (!wcscmp(pe32.szExeFile, wcProcessFileName)) {
            if (kill) {
                hProcess = OpenProcess(PROCESS_TERMINATE, 0, pe32.th32ProcessID);
                TerminateProcess(hProcess, 0);
                CloseHandle(hProcess);
                CloseHandle(hProcessSnap);
                return true;
            }
            else {
                CloseHandle(hProcessSnap);
                return true;
            }
        }
    } while (Process32Next(hProcessSnap, &pe32));
    CloseHandle(hProcessSnap);
    return false;
}

// Search for "softIoc.exe"
//    ForceOneIOC: TRUE = terminate if existing
//    returns TRUE if ForceOneIOC == FALSE AND found any running "softIoc.exe"
extern "C" EXPORT int softIoc(LVBoolean *ForceOneIOC) {
    if (*ForceOneIOC) {
        ProcessByName(L"softIoc.exe", true);
        return (externalIocInstance = false);
    }
    externalIocInstance = ProcessByName(L"softIoc.exe", false);
    return externalIocInstance;
}

// Callback of LabVIEW when any caLab-VI is unloaded
//		instanceState:	undocumented pointer
extern "C" EXPORT MgErr unreserved(InstanceDataPtr *instanceState)
{
    if (iListCount) {
        //removePVs(pszNameList, iListCount);
        LVBoolean all = true;
        //disconnectAll();
        for (size_t i = 0; i < iListCount; i++) {
            free(pszNameList[i]);
        }
        free(pszNameList);
        iListCount = 0;
        pszNameList = 0x0;
    }
    if (!externalIocInstance)
        ProcessByName(L"softIoc.exe", true);
    return 0;
}

// Callback of LabVIEW when any caLab-VI is aborted
//		instanceState:	undocumented pointer
extern "C" EXPORT MgErr aborted(InstanceDataPtr *instanceState)
{
    return unreserved(instanceState);
}

// Callback of LabVIEW when any caLab-VI is loaded
//		instanceState:	undocumented pointer
extern "C" EXPORT MgErr reserved(InstanceDataPtr *instanceState) {
    return 0;
}

// Adds a PV list to internal PV cache
//    PVList: list of PVs
extern "C" EXPORT void addPVList(sStringArrayHdl* PVList) {
    if (!((***PVList).elt)[0])
        return;
    iListCount = (**PVList)->dimSize;
    pszNameList = (char**)realloc(pszNameList, iListCount * sizeof(char*));
    for (size_t i = 0; i < iListCount; i++) {
        pszNameList[i] = (char*)malloc(PVNAME_STRINGSZ * sizeof(char));
        memset(pszNameList[i], 0, PVNAME_STRINGSZ);
        memcpy_s(pszNameList[i], PVNAME_STRINGSZ, (**((***PVList).elt)[i]).str, (**((***PVList).elt)[i]).cnt);
    }
}
