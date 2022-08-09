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
// Name        : caLabIoc.cpp
// Authors     : Carsten Winkler, Brian Powell
// Version     : 1.6.1.0
// Copyright   : HZB
// Description : library to handle softIoc.exe (Windows only)
// GitHub      : https://github.com/epics-extensions/CALab
//==================================================================================================

// Definitions
#include <extcode.h>
#include <windows.h>
#include <TlHelp32.h>
#include <stdio.h>
#include <dbDefs.h>

#if defined _WIN32 || defined _WIN64
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
typedef sStringArray** sStringArrayHdl;

// Search for process and optional terminate it
//    wcProcessFileName: name of process
//    kill:              TRUE: if find process terminate it
//    returns TRUE if process found
static bool ProcessByName(const char* procname, bool kill) {
    HANDLE hProcessSnap;
    HANDLE hProcess;
    PROCESSENTRY32 pe32;
    BOOL hResult;
    BOOL retVal = false;

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE)
        return retVal;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    hResult = Process32First(hProcessSnap, &pe32);
    while (hResult) {
        printf("%s\n", pe32.szExeFile);
        if (strcmp(procname, pe32.szExeFile) == 0) {
            if (kill) {
                hProcess = OpenProcess(PROCESS_TERMINATE, 0, pe32.th32ProcessID);
                if (hProcess != NULL) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
            retVal = true;
        }
        hResult = Process32Next(hProcessSnap, &pe32);
    };
    CloseHandle(hProcessSnap);
    return retVal;
}

// Search for "softIoc.exe"
//    ForceOneIOC: TRUE = terminate if existing
//    returns TRUE if ForceOneIOC == FALSE AND found any running "softIoc.exe"
extern "C" EXPORT int softIoc(LVBoolean* ForceOneIOC) {
    if (*ForceOneIOC) {
        ProcessByName("softIoc.exe", true);
        return (externalIocInstance = false);
    }
    externalIocInstance = ProcessByName("softIoc.exe", false);
    return externalIocInstance;
}

// Callback of LabVIEW when any caLab-VI is unloaded
//		instanceState:	undocumented pointer
extern "C" EXPORT MgErr unreserved(InstanceDataPtr* instanceState)
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
        ProcessByName("softIoc.exe", true);
    return 0;
}

// Callback of LabVIEW when any caLab-VI is aborted
//		instanceState:	undocumented pointer
extern "C" EXPORT MgErr aborted(InstanceDataPtr* instanceState)
{
    return unreserved(instanceState);
}

// Callback of LabVIEW when any caLab-VI is loaded
//		instanceState:	undocumented pointer
extern "C" EXPORT MgErr reserved(InstanceDataPtr* instanceState) {
    return 0;
}

// Adds a PV list to internal PV cache
//    PVList: list of PVs
extern "C" EXPORT void addPVList(sStringArrayHdl* PVList) {
    if (!((***PVList).elt)[0])
        return;
    void* tmp = realloc(pszNameList, (**PVList)->dimSize* sizeof(char*));
    if (tmp) {
        iListCount = (**PVList)->dimSize;
        pszNameList = (char**)tmp;
        for (size_t i = 0; i < iListCount; i++) {
            size_t destSize = PVNAME_STRINGSZ* sizeof(char);
            tmp = malloc(destSize);
            if (tmp) {
                memset(tmp, 0, destSize);
                memcpy_s(tmp, destSize, (**((***PVList).elt)[i]).str, (**((***PVList).elt)[i]).cnt);
                pszNameList[i] = (char*)tmp;
            }
            else {
                pszNameList[i] = 0x0;
            }
        }
    }
}
