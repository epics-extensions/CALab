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
// Authors     : Carsten Winkler, Brian Powell
// Version     : 1.7.3.3
// Copyright   : HZB
// Description : library for reading, writing and handle events of EPICS variables (PVs) in LabVIEW
// GitHub      : https://github.com/epics-extensions/CALab
//==================================================================================================

#include <extcode.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <vector>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <shared_mutex>
#include <epicsVersion.h>

#if defined _WIN32 || defined _WIN64
#include <alarm.h>
#include <alarmString.h>
#include <cadef.h>
#include <envDefs.h>
#include <epicsStdio.h>
#include <windows.h>
#include <libloaderapi.h>
#define EXPORT __declspec(dllexport)
#else
#include <dlfcn.h>
#include <shareLib.h>
#include <limits.h>
#define EXPORT
#define __stdcall
void __attribute__((constructor)) caLabLoad(void);
void __attribute__((destructor))  caLabUnload(void);
void* caLibHandle = 0x0;
void* comLibHandle = 0x0;
#endif
#define CALAB_VERSION       "1.7.3.3"
#define ERROR_OFFSET        7000           // User defined error codes of LabVIEW start at this number
#define MAX_ERROR_SIZE		255

#if defined _WIN32 || defined _WIN64
// Support for the community version of LabVIEW (32 bit) running on Windows
#else
#if UINTPTR_MAX == 0xffffffff
#error "unsupported ProcessorType (32-bit)"
#endif
#endif

#ifndef __GNUC__
#pragma warning(push)
#pragma warning(disable:4996)
#endif

#if  IsOpSystem64Bit
#define uPtr uQ
#else
#define uPtr uL
#endif


#if MSWin && (ProcessorType == kX86)
/* Windows x86 targets use 1-byte structure packing. */
#pragma pack(push,1)
#pragma warning (disable : 4103)
#endif

/* lv_prolog.h and lv_epilog.h set up the correct alignment for LabVIEW data. */
#include "lv_prolog.h"
// undocumented but used LV function
TH_REENTRANT EXTERNC MgErr _FUNCC DbgPrintfv(const char* buf, va_list args);

// Internal typedefs
typedef struct {
	size_t dimSize;
	LStrHandle elt[1];
} sStringArray;
typedef sStringArray** sStringArrayHdl;

typedef struct {
	uInt32 dimSizes[2];
	LStrHandle elt[1];
} sStringArray2D;
typedef sStringArray2D** sStringArray2DHdl;

typedef struct {
	size_t dimSize;
	double elt[1];
} sDoubleArray;
typedef sDoubleArray** sDoubleArrayHdl;

typedef struct {
	uInt32 dimSizes[2];
	double elt[1];
} sDoubleArray2D;
typedef sDoubleArray2D** sDoubleArray2DHdl;

typedef struct {
	size_t dimSize;
	uInt32 elt[1];
} sIntArray;
typedef sIntArray** sIntArrayHdl;

typedef struct {
	size_t dimSize;
	uint64_t elt[1];
} sLongArray;
typedef sLongArray** sLongArrayHdl;

typedef struct {
	uInt32 dimSizes[2];
	int64_t elt[1];
} sLongArray2D;
typedef sLongArray2D** sLongArray2DHdl;

typedef struct {
	LVBoolean status;                  // error status
	uInt32 code;                       // error code
	LStrHandle source;                 // error message
} sError;
typedef sError** sErrorHdl;

typedef struct {
	size_t dimSize;
	sError result[1];
} sErrorArray;
typedef sErrorArray** sErrorArrayHdl;

typedef struct {
	LStrHandle PVName;                 // names of PV as string array
	uInt32 valueArraySize;             // size of value array
	sStringArrayHdl StringValueArray;  // values as string array
	sDoubleArrayHdl ValueNumberArray;  // values as double array
	LStrHandle StatusString;           // status of PV as string
	int16_t StatusNumber;              // status of PV as short
	LStrHandle SeverityString;         // severity of PV as string
	int16_t SeverityNumber;            // severity of PV as short
	LStrHandle TimeStampString;        // time stamp of PV as string
	uInt32 TimeStampNumber;            // severity of PV as integer
	sStringArrayHdl FieldNameArray;    // optional field names as string array
	sStringArrayHdl FieldValueArray;   // field values as string array
	sError ErrorIO;                    // error structure
} sResult;
typedef sResult* sResultPtr;
typedef sResult** sResultHdl;

typedef struct {
	size_t dimSize;
	sResult result[1];
} sResultArray;
typedef sResultArray** sResultArrayHdl;
#include "lv_epilog.h"

typedef enum {
	firstValueAsString = 1,
	firstValueAsNumber = 2,
	valueArrayAsNumber = 4,
	errorOut = 8,
	pviElements = 16,
	pviValuesAsString = 32,
	pviValuesAsNumber = 64,
	pviStatusAsString = 128,
	pviStatusAsNumber = 256,
	pviSeverityAsString = 512,
	pviSeverityAsNumber = 1024,
	pviTimestampAsString = 2048,
	pviTimestampAsNumber = 4096,
	pviFieldNames = 8192,
	pviFieldValues = 16384,
	pviError = 32768
} out_filter;
typedef std::unordered_map<std::string, std::string> propertyMap;
typedef propertyMap::iterator propertyMapIterator;
typedef std::unordered_map<void*, std::atomic<bool>> modifiedMap;
typedef modifiedMap::iterator modifiedMapIterator;

#if defined _WIN32 || defined _WIN64
HMODULE libRef = 0x0;
DWORD dllStatus = DLL_PROCESS_DETACH;
BOOLEAN WINAPI DllMain(HINSTANCE hDllHandle, DWORD nReason, LPVOID Reserved) {
	dllStatus = nReason;
	switch (nReason) {
	case DLL_PROCESS_ATTACH: {
		break;
	}
	case DLL_PROCESS_DETACH: {
		break;
	}
	}
	return 1;
}
#else // Workaround for EPICS library unload issue in Linux
const char** dbr_text = (const char**)dlsym(caLibHandle, "dbr_text");
const char*  dbr_text_invalid = (const char* )dlsym(caLibHandle, "dbr_text_invalid");
const short* dbf_text_dim     = (const short*)dlsym(caLibHandle, "dbf_text_dim");
const short* dbr_text_dim     = (const short*)dlsym(caLibHandle, "dbr_text_dim");
const char* epicsAlarmSeverityStrings[] = {
	"NO_ALARM",
	"MINOR",
	"MAJOR",
	"INVALID"
};
const char* epicsAlarmConditionStrings[] = {
	"NO_ALARM",
	"READ",
	"WRITE",
	"HIHI",
	"HIGH",
	"LOLO",
	"LOW",
	"STATE",
	"COS",
	"COMM",
	"TIMEOUT",
	"HWLIMIT",
	"CALC",
	"SCAN",
	"LINK",
	"SOFT",
	"BAD_SUB",
	"UDF",
	"DISABLE",
	"SIMM",
	"READ_ACCESS",
	"WRITE_ACCESS"
};
#define alarmSeverityString        epicsAlarmSeverityStrings
#define alarmStatusString          epicsAlarmConditionStrings
#define MAX_STRING_SIZE            40
#define MAX_ENUM_STATES            16
#define MAX_ENUM_STRING_SIZE       26
#define epicsThreadPriorityBaseMax 91
#define NO_ALARM                   0
#define CA_K_ERROR                 2
#define CA_K_SUCCESS               1
#define CA_K_WARNING               0
#define CA_V_MSG_NO                0x03
#define CA_V_SEVERITY              0x00
#define CA_M_MSG_NO                0x0000FFF8
#define CA_M_SEVERITY              0x00000007
#define CA_INSERT_MSG_NO(code)     (((code)<< CA_V_MSG_NO)&CA_M_MSG_NO)
#define CA_INSERT_SEVERITY(code)   (((code)<< CA_V_SEVERITY)& CA_M_SEVERITY)
#define CA_OP_CONN_UP              6
#define CA_OP_CONN_DOWN            7
#define dbf_type_to_DBR_TIME(type) (((type) >= 0 && (type) <= *dbf_text_dim-3) ? (type) + 2*(*dbf_text_dim-2) : -1)
#define dbr_type_to_text(type)     (((type) >= 0 && (type) <  *dbr_text_dim)   ? dbr_text[(type)] : dbr_text_invalid)
#define DEFMSG(SEVERITY,NUMBER)    (CA_INSERT_MSG_NO(NUMBER) | CA_INSERT_SEVERITY(SEVERITY))
#define epicsMutexCreate()         epicsMutexOsiCreate(__FILE__,__LINE__)
#define ECA_NORMAL                 DEFMSG(CA_K_SUCCESS,  0) /* success */
#define ECA_ALLOCMEM               DEFMSG(CA_K_WARNING,  6)
#define ECA_BADTYPE                DEFMSG(CA_K_ERROR,   14)
#define ECA_DISCONN                DEFMSG(CA_K_WARNING, 24)
#define ECA_UNRESPTMO              DEFMSG(CA_K_WARNING,   60)
#define DBE_VALUE                  (1<<0)
#define DBE_ALARM                  (1<<2)
#define DBF_STRING                 0
#define	DBF_INT                    1
#define	DBF_SHORT                  1
#define	DBF_FLOAT                  2
#define	DBF_ENUM                   3
#define	DBF_CHAR                   4
#define	DBF_LONG                   5
#define	DBF_DOUBLE                 6
#define DBF_NO_ACCESS              7
#define DBR_STRING	DBF_STRING
#define	DBR_INT		DBF_INT
#define	DBR_SHORT	DBF_INT
#define	DBR_FLOAT	DBF_FLOAT
#define	DBR_ENUM	DBF_ENUM
#define	DBR_CHAR	DBF_CHAR
#define	DBR_LONG	DBF_LONG
#define	DBR_DOUBLE	DBF_DOUBLE
#define DBR_STS_STRING	7
#define	DBR_STS_SHORT	8
#define	DBR_STS_INT	DBR_STS_SHORT
#define	DBR_STS_FLOAT	9
#define	DBR_STS_ENUM	10
#define	DBR_STS_CHAR	11
#define	DBR_STS_LONG	12
#define	DBR_STS_DOUBLE	13
#define	DBR_TIME_STRING	14
#define	DBR_TIME_INT	15
#define	DBR_TIME_SHORT	15
#define	DBR_TIME_FLOAT	16
#define	DBR_TIME_ENUM	17
#define	DBR_TIME_CHAR	18
#define	DBR_TIME_LONG	19
#define	DBR_TIME_DOUBLE	20
#define	DBR_GR_STRING	21
#define	DBR_GR_SHORT	22
#define	DBR_GR_INT	DBR_GR_SHORT
#define	DBR_GR_FLOAT	23
#define	DBR_GR_ENUM	24
#define	DBR_GR_CHAR	25
#define	DBR_GR_LONG	26
#define	DBR_GR_DOUBLE	27
#define	DBR_CTRL_STRING	28
#define DBR_CTRL_SHORT	29
#define DBR_CTRL_INT	DBR_CTRL_SHORT
#define	DBR_CTRL_FLOAT	30
#define DBR_CTRL_ENUM	31
#define	DBR_CTRL_CHAR	32
#define	DBR_CTRL_LONG	33
#define	DBR_CTRL_DOUBLE	34
#define DBR_PUT_ACKT	DBR_CTRL_DOUBLE + 1
#define DBR_PUT_ACKS    DBR_PUT_ACKT + 1
#define DBR_STSACK_STRING DBR_PUT_ACKS + 1
#define DBR_CLASS_NAME DBR_STSACK_STRING + 1
#define	LAST_BUFFER_TYPE	DBR_CLASS_NAME
enum epicsAlarmSeverity {
	epicsSevNone = NO_ALARM,
	epicsSevMinor,
	epicsSevMajor,
	epicsSevInvalid,
	ALARM_NSEV
};

enum epicsAlarmCondition {
	epicsAlarmNone = NO_ALARM,
	epicsAlarmRead,
	epicsAlarmWrite,
	epicsAlarmHiHi,
	epicsAlarmHigh,
	epicsAlarmLoLo,
	epicsAlarmLow,
	epicsAlarmState,
	epicsAlarmCos,
	epicsAlarmComm,
	epicsAlarmTimeout,
	epicsAlarmHwLimit,
	epicsAlarmCalc,
	epicsAlarmScan,
	epicsAlarmLink,
	epicsAlarmSoft,
	epicsAlarmBadSub,
	epicsAlarmUDF,
	epicsAlarmDisable,
	epicsAlarmSimm,
	epicsAlarmReadAccess,
	epicsAlarmWriteAccess,
	ALARM_NSTATUS
};
#define firstEpicsAlarmSev  epicsSevNone
#define MINOR_ALARM         epicsSevMinor
#define MAJOR_ALARM         epicsSevMajor
#define INVALID_ALARM       epicsSevInvalid
#define lastEpicsAlarmSev   epicsSevInvalid

struct ca_client_context;
typedef double ca_real;
typedef long chtype;
typedef unsigned capri;
typedef void(*EPICSTHREADFUNC)(void* parm);
typedef struct oldChannelNotify* chid;
typedef struct oldSubscription* evid;
typedef struct epicsMutexParm* epicsMutexId;
typedef struct epicsThreadOSD* epicsThreadId;
typedef chid chanId;
typedef uint8_t epicsUInt8;
typedef int16_t epicsInt16;
typedef uint16_t epicsUInt16;
typedef int32_t epicsInt32;
typedef uint32_t epicsUInt32;
typedef float epicsFloat32;
typedef double epicsFloat64;
typedef epicsUInt16 dbr_enum_t;
typedef epicsInt16 dbr_short_t;
typedef epicsUInt8 dbr_char_t;
typedef epicsInt32 dbr_long_t;
typedef epicsFloat32 dbr_float_t;
typedef epicsFloat64 dbr_double_t;
typedef char epicsOldString[MAX_STRING_SIZE];
typedef epicsOldString dbr_string_t;
typedef enum { ca_disable_preemptive_callback, ca_enable_preemptive_callback } ca_preemptive_callback_select;
typedef enum { epicsThreadStackSmall, epicsThreadStackMedium, epicsThreadStackBig } epicsThreadStackSizeClass;
typedef enum { cs_never_conn, cs_prev_conn, cs_conn, cs_closed } channel_state;
typedef enum { epicsMutexLockOK, epicsMutexLockTimeout, epicsMutexLockError } epicsMutexLockStatus;
typedef struct epicsTimeStamp {
	epicsUInt32    secPastEpoch;
	epicsUInt32    nsec;
} epicsTimeStamp;
typedef struct envParam {
	char* name;
	char* pdflt;
} ENV_PARAM;
struct  connection_handler_args {
	chanId  chid;
	long    op;
};
typedef struct event_handler_args {
	void* usr;
	chanId          chid;
	long            type;
	long            count;
	const void* dbr;
	int             status;
} evargs;
struct dbr_gr_enum {
	dbr_short_t	status;
	dbr_short_t	severity;
	dbr_short_t	no_str;
	char		strs[MAX_ENUM_STATES][MAX_ENUM_STRING_SIZE];
	dbr_enum_t	value;
};
struct dbr_ctrl_enum {
	dbr_short_t	status;
	dbr_short_t	severity;
	dbr_short_t	no_str;
	char	strs[MAX_ENUM_STATES][MAX_ENUM_STRING_SIZE];
	dbr_enum_t	value;
};
struct dbr_time_short {
	dbr_short_t	status;
	dbr_short_t	severity;
	epicsTimeStamp	stamp;
	dbr_short_t	RISC_pad;
	dbr_short_t	value;
};
struct dbr_time_double {
	dbr_short_t	status;
	dbr_short_t	severity;
	epicsTimeStamp	stamp;
	dbr_long_t	RISC_pad;
	dbr_double_t	value;
};
struct exception_handler_args {
	void* usr;
	chanId		chid;
	long		type;
	long		count;
	void* addr;
	long		stat;
	long		op;
	const char* ctx;
	const char* pFile;
	unsigned	lineNo;
};
typedef channel_state(*ca_state_t) (chid chan);
typedef void caCh(struct connection_handler_args args);
typedef void caEventCallBackFunc(struct event_handler_args);
typedef struct ca_client_context* (*ca_current_context_t) ();
typedef const char* (*ca_message_t)(long ca_status);
typedef const char* (*ca_name_t) (chid chan);
typedef const char* (*envGetConfigParamPtr_t)(const ENV_PARAM* pParam);
typedef const ENV_PARAM* (*env_param_list_t);
typedef const unsigned short(*dbr_value_offset_t);
#define dbr_value_ptr(PDBR, DBR_TYPE) ((void*)(((char*)PDBR)+dbr_value_offset[DBR_TYPE]))
typedef epicsMutexId(*epicsMutexOsiCreate_t)(const char* pFileName, int lineno);
typedef epicsThreadId(*epicsThreadCreate_t) (const char* name, unsigned int priority, unsigned int stackSize, EPICSTHREADFUNC funptr, void* parm);
typedef epicsMutexLockStatus(*epicsMutexLock_t)(epicsMutexId id);
typedef epicsMutexLockStatus(*epicsMutexTryLock_t)(epicsMutexId id);
typedef void caExceptionHandler(struct exception_handler_args);
typedef int(*ca_add_exception_event_t) (caExceptionHandler* pfunc, void* pArg);
typedef int(*ca_array_get_t) (chtype type, unsigned long count, chid pChan, void* pValue);
typedef int(*ca_array_put_t) (chtype type, unsigned long count, chid chanId, const void* pValue);
typedef int(*ca_array_put_callback_t) (chtype type, unsigned long count, chid chanId, const void* pValue, caEventCallBackFunc* pFunc, void* pArg);
typedef int(*ca_attach_context_t) (struct ca_client_context* context);
typedef int(*ca_flush_io_t) (void);
typedef int(*ca_clear_channel_t)(chid chanId);
typedef int(*ca_clear_subscription_t)(evid eventID);
typedef int(*ca_context_create_t) (ca_preemptive_callback_select select);
typedef int(*ca_create_channel_t) (const char* pChanName, caCh* pConnStateCallback, void* pUserPrivate, capri priority, chid* pChanID);
typedef int(*ca_create_subscription_t) (chtype type, unsigned long count, chid chanId, long mask, caEventCallBackFunc* pFunc, void* pArg, evid* pEventID);
typedef int(*ca_array_get_callback_t) (chtype type, unsigned long count, chid chanId, caEventCallBackFunc* pFunc, void* pArg);
typedef int(*ca_pend_io_t) (ca_real timeOut);
#if defined _WIN32 || defined _WIN64
#define EPICS_PRINTF_STYLE(f,a)
typedef int(__stdcall* epicsSnprintf_t)(char* str, size_t size, const char* format, ...) EPICS_PRINTF_STYLE(3, 4);
#else
#define EPICS_PRINTF_STYLE(f,a) __attribute__((format(__printf__,f,a)))
typedef int(*epicsSnprintf_t)(char* str, size_t size, const char* format, ...) __attribute__((format(__printf__, 3, 4)));
#endif
typedef short(*ca_field_type_t) (chid chan);
typedef size_t(__stdcall* epicsTimeToStrftime_t) (char* pBuff, size_t bufLength, const char* pFormat, const epicsTimeStamp* pTS);
typedef unsigned int(*epicsThreadGetStackSize_t)(epicsThreadStackSizeClass size);
typedef unsigned long(*ca_element_count_t) (chid chan);
typedef void* (*ca_puser_t)(chid chan);
typedef void(*ca_context_destroy_t) (void);
typedef void(*ca_detach_context_t) ();
typedef void(*epicsMutexDestroy_t)(epicsMutexId id);
typedef void(*epicsMutexUnlock_t)(epicsMutexId id);
typedef void(*epicsThreadSleep_t)(double seconds);
typedef const char* (*ca_version_t)(void);

ca_add_exception_event_t ca_add_exception_event = 0x0;
ca_attach_context_t ca_attach_context = 0x0;
ca_array_get_t ca_array_get = 0x0;
ca_array_put_t ca_array_put = 0x0;
ca_array_put_callback_t ca_array_put_callback = 0x0;
ca_clear_channel_t ca_clear_channel = 0x0;
ca_clear_subscription_t ca_clear_subscription = 0x0;
ca_context_create_t ca_context_create = 0x0;
ca_context_destroy_t ca_context_destroy = 0x0;
ca_create_channel_t ca_create_channel = 0x0;
ca_create_subscription_t ca_create_subscription = 0x0;
ca_array_get_callback_t ca_array_get_callback = 0x0;
ca_current_context_t ca_current_context = 0x0;
ca_detach_context_t ca_detach_context = 0x0;
ca_element_count_t ca_element_count = 0x0;
ca_field_type_t ca_field_type = 0x0;
ca_flush_io_t ca_flush_io = 0x0;
ca_message_t ca_message = 0x0;
ca_name_t ca_name = 0x0;
ca_pend_io_t ca_pend_io = 0x0;
ca_puser_t ca_puser = 0x0;
ca_state_t ca_state = 0x0;
ca_version_t ca_version = 0x0;
dbr_value_offset_t dbr_value_offset = 0x0;
envGetConfigParamPtr_t envGetConfigParamPtr = 0x0;
env_param_list_t env_param_list = 0x0;
epicsMutexDestroy_t epicsMutexDestroy = 0x0;
epicsMutexLock_t epicsMutexLock = 0x0;
epicsMutexTryLock_t epicsMutexTryLock = 0x0;
epicsMutexOsiCreate_t epicsMutexOsiCreate = 0x0;
epicsMutexUnlock_t epicsMutexUnlock = 0x0;
epicsSnprintf_t epicsSnprintf = 0x0;
epicsThreadCreate_t epicsThreadCreate = 0x0;
epicsThreadGetStackSize_t epicsThreadGetStackSize = 0x0;
epicsThreadSleep_t epicsThreadSleep = 0x0;
epicsTimeToStrftime_t epicsTimeToStrftime = 0x0;

typedef const unsigned short(*dbr_size_t);
typedef const unsigned short(*dbr_value_size_t);
dbr_value_size_t dbr_value_size = 0x0;
dbr_size_t dbr_size = 0x0;
#define dbr_size_n(TYPE,COUNT) ((unsigned)((COUNT)<=0?dbr_size[TYPE]:dbr_size[TYPE]+((COUNT)-1)*dbr_value_size[TYPE]))
#define ca_get_callback(type, chan, pFunc, pArg) ca_array_get_callback (type, 1u, chan, pFunc, pArg)
#endif

#define MAX_NAME_SIZE 61

MgErr DeleteStringArray(sStringArrayHdl array);
void DbgTime(void);
MgErr CaLabDbgPrintf(const char* format, ...);
MgErr CaLabDbgPrintfD(const char* format, ...);
void connectionChanged(connection_handler_args args);
void valueChanged(evargs args);
void putState(evargs args);
void caLabLoad(void);
void caLabUnload(void);
uInt32 _LToCStrN(ConstLStrP source, unsigned char* dest, uInt32 destSize);
std::string myItemsFindEnum(std::string name, dbr_enum_t enumValue);

ca_client_context* pcac = 0x0;            // EPICS context
bool				bCaLabPolling = false; // TRUE: Avoids permanent open network ports. (CompactRIO)
uInt32				globalCounter = 0;     // simple counter for debugging
uInt32				reservedCounter = 0;   // simple counter for debugging
static bool			stopped;               // indicator for closing library
FILE* pCaLabDbgFile = 0x0;   // file handle for optional debug file
std::atomic<int>	tasks(0);			   // number of parallel tasks
static bool			err200 = false;        // send one error 200 message only
uInt32				currentlyConnectedPos = 6 * sizeof(void*) + sizeof(unsigned int); // direct access to connect indicator in channel access object
epicsMutexId		getLock;               // used to protect the getValue() entry point
epicsMutexId		putLock;               // used to protect the putValue() entry point

// https://www.techiedelight.com/trim-string-cpp-remove-leading-trailing-spaces/
const std::string WHITESPACE = " \n\r\t\f\v";
std::string ltrim(const std::string& s)
{
	size_t start = s.find_first_not_of(WHITESPACE);
	return (start == std::string::npos) ? "" : s.substr(start);
}

std::string rtrim(const std::string& s)
{
	size_t end = s.find_last_not_of(WHITESPACE);
	return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string trim(const std::string& s) {
	return rtrim(ltrim(s));
}

// internal data object
class calabItem {
public:
	void* validAddress;							// check number for valid object
	chanId						caID = 0x0;								// channel access ID
	chanId						caIDprec = 0x0;							// channel access ID for display precision
	chtype						nativeType = -1;						// native data type (EPICS)
	uInt32						numberOfValues = 0x0;					// number of values
	char						szName[MAX_NAME_SIZE];					// PV name as null-terminated string
	evid						caEnumEventID = 0x0;					// event ID for subscription of enums
	evid						caEventID = 0x0;						// event ID for subscription of values
	sDoubleArrayHdl				doubleValueArray = 0x0;					// buffer for read values (Doubles)
	sError						ErrorIO;								// error struct buffer
	std::atomic<bool>			hasValue;								// indicator for read value
	std::atomic<bool>			isConnected;							// indicator for successfully connect to server
	std::atomic<bool>			isPassive;								// indicator for polling values instead of monitoring
	std::atomic<bool>			putReadBack;							// indicator for synchronized reading
	modifiedMap					fieldModified;							// indicator for changed field value
	epicsMutexId				myLock;									// object mutex
	LStrHandle					name = 0x0;								// PV name as LV string
	calabItem*					parent = 0x0;							// parent of field object = main object with values
	propertyMap					properties;								// properties / fields
	int16_t						SeverityNumber = epicsSevInvalid;		// number of EPICS severity
	LStrHandle					SeverityString = 0x0;					// LV string of EPICS severity
	int16_t						StatusNumber = epicsAlarmComm;			// number of EPICS status
	LStrHandle					StatusString = 0x0;						// LV string of EPICS status
	sStringArrayHdl				stringValueArray = 0x0;					// buffer for read values (LV strings)
	uInt32						TimeStampNumber = 0;					// number of time stamp
	LStrHandle					TimeStampString = 0x0;					// LV string of time stamp
	dbr_gr_enum					sEnum;									// enumeration String
	std::vector<LVUserEventRef>	RefNum;									// reference number for LV user event
	std::vector<sResult*>		eventResultCluster;						// reference object for LV user event
	void*						writeValueArray = 0x0;					// buffer for output
	uInt32						writeValueArraySize = 0;				// size of output buffer
	std::atomic<bool>			locked;									// indicator of locked object
	std::chrono::high_resolution_clock::time_point timer;				// watch dog timer
	char						className[MAX_STRING_SIZE];				// class name of object
	int32						prec;									// display precision; -1 == not initialized, -2 == not available

	calabItem(void* currentInstance, LStrHandle name, sStringArrayHdl fieldNames = 0x0) {
		hasValue = false;
		isConnected = false;
		isPassive = false;
		if (currentInstance)
			fieldModified[currentInstance] = false;
		validAddress = this;
		locked = false;
		myLock = epicsMutexCreate();
		if ((*name)->cnt < MAX_NAME_SIZE - 1) {
			NumericArrayResize(uB, 1, (UHandle*)&this->name, (*name)->cnt);
			memcpy((*this->name)->str, (*name)->str, (*name)->cnt);
			(*this->name)->cnt = (*name)->cnt;
			memcpy(szName, (*name)->str, (*name)->cnt);
			szName[(*name)->cnt] = 0x0;
		}
		else {
			NumericArrayResize(uB, 1, (UHandle*)&this->name, MAX_NAME_SIZE - 1);
			memcpy((*this->name)->str, (*name)->str, MAX_NAME_SIZE - 1);
			(*this->name)->cnt = MAX_NAME_SIZE - 1;
			memcpy(szName, (*name)->str, MAX_NAME_SIZE - 1);
			szName[MAX_NAME_SIZE - 1] = 0x0;
			CaLabDbgPrintf("%s was cropped to %d characters (max EPICS name size)", szName, MAX_NAME_SIZE - 1);
		}
		// White spaces in PV names are not allowed
		if (strchr(szName, ' ') || strchr(szName, '\t')) {
			if (strchr(szName, ' ')) {
				DbgTime(); CaLabDbgPrintf("white space in PV name \"%s\" detected", szName);
				*(strchr(szName, ' ')) = 0;
			}
			if (strchr(szName, '\t')) {
				DbgTime(); CaLabDbgPrintf("tabulator in PV name \"%s\" detected", szName);
				*(strchr(szName, '\t')) = 0;
			}
			NumericArrayResize(uB, 1, (UHandle*)&this->name, strlen(szName));
			memcpy((*this->name)->str, szName, strlen(szName));
			(*this->name)->cnt = (int32)strlen(szName);
		}
		if (fieldNames && *fieldNames) {
			for (uInt32 i = 0; i < (*fieldNames)->dimSize; i++) {
				if (*(*fieldNames)->elt[i] && (*(*fieldNames)->elt[i])->cnt) {
					std::string fieldName = std::string((char*)(*(*fieldNames)->elt[i])->str, (size_t)(*(*fieldNames)->elt[i])->cnt);
					fieldName = trim(fieldName);
					properties[fieldName] = "";
				}
			}
		}
		ErrorIO.source = 0x0;
		sEnum.no_str = 0x0;
		setError(ECA_DISCONN);
        memcpy(className, "unknown", sizeof("unknown"));
		className[sizeof("unknown")] = 0x0;
		prec = -1;
		timer = std::chrono::high_resolution_clock::now();
	}

	~calabItem() {
#if defined _WIN32 || defined _WIN64
		if (!dllStatus) return; // DLL_PROCESS_DETACH
#endif
		MgErr err = noErr;
		int32 lerr = lock();
		if (lerr) CaLabDbgPrintf("lock failed in ~calabItem");
		szName[0] = 0x0;
		unlock();
		if (RefNum.size() || eventResultCluster.size()) {
			std::vector<LVUserEventRef>::iterator itRefNum = RefNum.begin();
			while (itRefNum != RefNum.end()) {
				itRefNum = RefNum.erase(itRefNum);
			}
			std::vector<sResult*>::iterator itEventResultCluster = eventResultCluster.begin();
			while (itEventResultCluster != eventResultCluster.end()) {
				itEventResultCluster = eventResultCluster.erase(itEventResultCluster);
			}
		}
		err += DSDisposeHandle(name);
		if (doubleValueArray)
			err += DSDisposeHandle(doubleValueArray);
		if (stringValueArray) {
			for (uInt32 i = 0; i < (*stringValueArray)->dimSize; i++)
				err += DSDisposeHandle((*stringValueArray)->elt[i]);
			(*stringValueArray)->dimSize = 0;
			err += DSDisposeHandle(stringValueArray);
		}
		if (StatusString)
			err += DSDisposeHandle(StatusString);
		if (SeverityString)
			err += DSDisposeHandle(SeverityString);
		if (TimeStampString)
			err += DSDisposeHandle(TimeStampString);
		if (ErrorIO.source)
			err += DSDisposeHandle(ErrorIO.source);
		if (err)
			CaLabDbgPrintf("Error: Memory exception in cItem::~Item");
		epicsMutexDestroy(myLock);
		if (writeValueArray)
			free(writeValueArray);
	}

	// lock this instance
	// return non-zero if lock fails
	int32 lock() {
#ifdef _DEBUG
		int32 loopcount = 0;
#endif
		bool havelock = true;
		locked = true;
		std::chrono::duration<double> diff;
		std::chrono::high_resolution_clock::time_point lockTimer = std::chrono::high_resolution_clock::now();
		//epicsMutexLock(myLock);
		while (epicsMutexTryLock(myLock) != epicsMutexLockOK) {
#ifdef _DEBUG
			loopcount++;
#endif
			diff = std::chrono::high_resolution_clock::now() - lockTimer;
			if (diff.count() > 10) {
				havelock = false;
				break;
			}
		}
#ifdef _DEBUG
		if (!havelock)
			CaLabDbgPrintf("%s has delayed mutex after %d tries", szName, loopcount);
#endif
		return !havelock;
	}

	// unlock this instance
	void unlock() {
		epicsMutexUnlock(myLock);
		locked = false;
	}

	// write error struct
	//    iError: error code
	MgErr setError(int32 iError) {
		int32 iSize = 0;
		MgErr err = noErr;
		iSize = (int32)strlen(ca_message(iError));
		if (!ErrorIO.source || (*ErrorIO.source)->cnt != iSize) {
			err += NumericArrayResize(uB, 1, (UHandle*)&ErrorIO.source, iSize);
			(*ErrorIO.source)->cnt = iSize;
		}
		memcpy((*ErrorIO.source)->str, ca_message(iError), iSize);
		if (iError <= ECA_NORMAL)
			ErrorIO.code = 0;
		else
			ErrorIO.code = iError + ERROR_OFFSET;
		ErrorIO.status = 0;
		return err;
	}

	// disconnect instance from server
	void disconnect() {
		int32 err = lock();
		if (err) CaLabDbgPrintf("lock failed in disconnect");
		isPassive = true;
		fieldModified.clear();
		if (RefNum.size() || eventResultCluster.size()) {
			std::vector<LVUserEventRef>::iterator itRefNum = RefNum.begin();
			while (itRefNum != RefNum.end()) {
				itRefNum = RefNum.erase(itRefNum);
			}
			std::vector<sResult*>::iterator itEventResultCluster = eventResultCluster.begin();
			while (itEventResultCluster != eventResultCluster.end()) {
				itEventResultCluster = eventResultCluster.erase(itEventResultCluster);
			}
		}
		unlock();
	}

	void modified() {
		if (parent) {
			modifiedMapIterator it = parent->fieldModified.begin();
			while (it != parent->fieldModified.end()) {
				it->second = true;
				it++;
			}
		}
		else {
			modifiedMapIterator it = fieldModified.begin();
			while (it != fieldModified.end()) {
				it->second = true;
				it++;
			}
		}
	}

	// callback for changed connection state
	void itemConnectionChanged(connection_handler_args args) {
		try {
			if (!szName[0]) {
				CaLabDbgPrintf("Missing PV name in itemConnectionChanged");
				return;
			}
			int32 size;
			if (args.op == CA_OP_CONN_UP) {
				int32 err = lock();
				if (err) CaLabDbgPrintf("lock failed in connection up");
				isConnected = true;
				if (RefNum.size()) {
					modified();
					unlock();
					postEvent();
				}
				else {
					modified();
					unlock();
				}
			}
			else if (args.op == CA_OP_CONN_DOWN) {
				int32 err = lock();
				if (err) CaLabDbgPrintf("lock failed in connection down");
				isConnected = false;
				size = (int32)strlen(alarmStatusString[epicsAlarmComm]);
				if (!StatusString || (*StatusString)->cnt != size) {
					NumericArrayResize(uB, 1, (UHandle*)&StatusString, size);
					(*StatusString)->cnt = size;
				}
				memcpy((*StatusString)->str, alarmStatusString[epicsAlarmComm], size);
				StatusNumber = epicsAlarmComm;
				size = (int32)strlen(alarmSeverityString[epicsSevInvalid]);
				if (!SeverityString || (*SeverityString)->cnt != size) {
					NumericArrayResize(uB, 1, (UHandle*)&SeverityString, size);
					(*SeverityString)->cnt = size;
				}
				memcpy((*SeverityString)->str, alarmSeverityString[epicsSevInvalid], size);
				SeverityNumber = epicsSevInvalid;
				setError(ECA_DISCONN);
				if (RefNum.size()) {
					modified();
					unlock();
					postEvent();
				}
				else {
					modified();
					unlock();
				}
			}
		}
		catch (std::exception& ex) {
			DbgTime(); CaLabDbgPrintf("%s", ex.what());
		}
	}

	// callback for changed value (incl. field values)
	void itemValueChanged(evargs args) {
		try {
			bool bDbrTime = false;
			bool validDoubleArray = false;
			bool validStringArray = false;
			dbr_ctrl_enum* tmpEnum;
			int32 iSize;
			MgErr err = noErr;
			char szTmp[MAX_STRING_SIZE];
			if (!szName[0] || args.status != ECA_NORMAL)
				return;
			int32 lerr = lock();
			if (lerr) CaLabDbgPrintf("lock failed in itemValueChanged");
			if (args.type == DBR_CLASS_NAME) {
				const char* tmpClassName = (const char*)((dbr_string_t*)dbr_value_ptr(args.dbr, DBR_CLASS_NAME));
				if (!tmpClassName || strlen(tmpClassName) > MAX_STRING_SIZE) return;
				memcpy(className, tmpClassName, strlen(tmpClassName));
				className[strlen(tmpClassName)] = 0x0;
				unlock();
				return;
			}
			numberOfValues = args.count;
			if (!parent && !doubleValueArray) {
				err += NumericArrayResize(fD, 1, (UHandle*)&doubleValueArray, args.count);
				if (err == noErr) {
					(*doubleValueArray)->dimSize = args.count;
				}
			}
			if (!parent && doubleValueArray && (DSCheckHandle(doubleValueArray) == noErr)) {
				if (!doubleValueArray || (long)(*doubleValueArray)->dimSize < args.count) {
					err += NumericArrayResize(fD, 1, (UHandle*)&doubleValueArray, args.count);
					if (err == noErr) {
						(*doubleValueArray)->dimSize = args.count;
					}
				}
				validDoubleArray = true;
			}
			if (!parent && !stringValueArray) {
				stringValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t) + args.count * sizeof(LStrHandle[1]));
				(*stringValueArray)->dimSize = args.count;
			}
			if (!parent && stringValueArray && (err = (DSCheckHandle(stringValueArray)) == noErr)) {
				if (!stringValueArray || (long)(*stringValueArray)->dimSize < args.count) {
					if (stringValueArray && (long)(*stringValueArray)->dimSize != args.count) {
						err += DeleteStringArray(stringValueArray);
					}
					stringValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t) + args.count * sizeof(LStrHandle[1]));
					(*stringValueArray)->dimSize = args.count;
				}
				validStringArray = true;
			}
			switch (args.type) {
			case DBR_TIME_STRING:
				for (long lCount = 0; lCount < args.count; lCount++) {
					char* tmp;
					iSize = (int32)strlen(((dbr_string_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
					if (parent && parent->szName) {
						parent->properties[std::string(szName + strlen(parent->szName) + 1)] = std::string(((dbr_string_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
					}
					else {
						if (validDoubleArray) {
							(*doubleValueArray)->elt[lCount] = strtod(((dbr_string_t*)dbr_value_ptr(args.dbr, args.type))[lCount], &tmp);
						}
						if (validStringArray) {
							if (LHStrLen((*stringValueArray)->elt[lCount]) != iSize) {
								err += NumericArrayResize(uB, 1, (UHandle*)&(*stringValueArray)->elt[lCount], iSize);
								LStrLen(*(*stringValueArray)->elt[lCount]) = iSize;
							}
							if (iSize)
								memcpy(LStrBuf(*(*stringValueArray)->elt[lCount]), ((dbr_string_t*)dbr_value_ptr(args.dbr, args.type))[lCount], iSize);
						}
					}
				}
				bDbrTime = 1;
				break;
			case DBR_TIME_SHORT:
				short shortValue;
				for (long lCount = 0; lCount < args.count; lCount++) {
					shortValue = ((dbr_short_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
					iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%d", (uInt32)shortValue);
					if (parent && parent->szName) {
						parent->properties[std::string(szName + strlen(parent->szName) + 1)] = std::string(szTmp);
					}
					else {
						if (validDoubleArray) {
							(*doubleValueArray)->elt[lCount] = shortValue;
						}
						if (validStringArray) {
							if (LHStrLen((*stringValueArray)->elt[lCount]) != iSize) {
								err += NumericArrayResize(uB, 1, (UHandle*)&(*stringValueArray)->elt[lCount], iSize);
								LStrLen(*(*stringValueArray)->elt[lCount]) = iSize;
							}
							if (iSize)
								memcpy(LStrBuf(*(*stringValueArray)->elt[lCount]), szTmp, iSize);
						}
					}
				}
				bDbrTime = 1;
				break;
			case DBR_TIME_CHAR:
				for (long lCount = 0; lCount < args.count; lCount++) {
					iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%d", ((dbr_char_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
					if (parent && parent->szName) {
						parent->properties[std::string(szName + strlen(parent->szName) + 1)] = std::string(szTmp);
					}
					else {
						if (validDoubleArray) {
							(*doubleValueArray)->elt[lCount] = ((dbr_char_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
						}
						if (validStringArray) {
							if (LHStrLen((*stringValueArray)->elt[lCount]) != iSize) {
								err += NumericArrayResize(uB, 1, (UHandle*)&(*stringValueArray)->elt[lCount], iSize);
								LStrLen(*(*stringValueArray)->elt[lCount]) = iSize;
							}
							if (iSize)
								memcpy(LStrBuf(*(*stringValueArray)->elt[lCount]), szTmp, iSize);
						}
					}
				}
				bDbrTime = 1;
				break;
			case DBR_TIME_LONG:
				for (long lCount = 0; lCount < args.count; lCount++) {
					iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%d", ((dbr_long_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
					if (parent && parent->szName) {
						parent->properties[std::string(szName + strlen(parent->szName) + 1)] = std::string(szTmp);
					}
					else {
						if (validDoubleArray) {
							(*doubleValueArray)->elt[lCount] = ((dbr_long_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
						}
						if (validStringArray) {
							if (LHStrLen((*stringValueArray)->elt[lCount]) != iSize) {
								err += NumericArrayResize(uB, 1, (UHandle*)&(*stringValueArray)->elt[lCount], iSize);
								LStrLen(*(*stringValueArray)->elt[lCount]) = iSize;
							}
							if (iSize)
								memcpy(LStrBuf(*(*stringValueArray)->elt[lCount]), szTmp, iSize);
						}
					}
				}
				bDbrTime = 1;
				break;
			case DBR_TIME_FLOAT:
				if (properties.size() && prec == -1) {
					propertyMapIterator nameIterator = properties.find("PREC");
					if (nameIterator != properties.end()) {
						if (nameIterator->second.size()) {
							prec = std::stoi(nameIterator->second);
						}
					}
					else {
						prec = -2;
					}
				}
				for (long lCount = 0; lCount < args.count; lCount++) {					
					if(prec >= 0)
						if(abs(((dbr_float_t*)dbr_value_ptr(args.dbr, args.type))[lCount]) < 1e-3 || abs(((dbr_float_t*)dbr_value_ptr(args.dbr, args.type))[lCount]) >= 1e7)
							iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%.*e", prec, ((dbr_float_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
						else
							iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%.*f", prec, ((dbr_float_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
					else
						iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%g", ((dbr_float_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
					iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%g", ((dbr_float_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
					if (parent && parent->szName) {
						parent->properties[std::string(szName + strlen(parent->szName) + 1)] = std::string(szTmp);
					}
					else {
						if (validDoubleArray) {
							(*doubleValueArray)->elt[lCount] = ((dbr_float_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
						}
						if (validStringArray) {
							if (LHStrLen((*stringValueArray)->elt[lCount]) != iSize) {
								err += NumericArrayResize(uB, 1, (UHandle*)&(*stringValueArray)->elt[lCount], iSize);
								LStrLen(*(*stringValueArray)->elt[lCount]) = iSize;
							}
							if (iSize)
								memcpy(LStrBuf(*(*stringValueArray)->elt[lCount]), szTmp, iSize);
						}
					}
				}
				bDbrTime = 1;
				break;
			case DBR_TIME_DOUBLE:
				if (properties.size() && prec == -1) {
					propertyMapIterator nameIterator = properties.find("PREC");
					if (nameIterator != properties.end()) {
						if (nameIterator->second.size()) {
							prec = std::stoi(nameIterator->second);
						}
					}
					else {
						prec = -2;
					}
				}
				for (long lCount = 0; lCount < args.count; lCount++) {
					if(prec >= 0)
						if(abs(((dbr_double_t*)dbr_value_ptr(args.dbr, args.type))[lCount]) < 1e-3 || abs(((dbr_double_t*)dbr_value_ptr(args.dbr, args.type))[lCount]) >= 1e7)
							iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%.*e", prec, ((dbr_double_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
						else
							iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%.*f", prec, ((dbr_double_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
					else
						iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%g", ((dbr_double_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
					if (parent && parent->szName) {
						parent->properties[std::string(szName + strlen(parent->szName) + 1)] = std::string(szTmp);
					}
					else {
						if (validDoubleArray) {
							(*doubleValueArray)->elt[lCount] = ((dbr_double_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
						}
						if (validStringArray) {
							if (LHStrLen((*stringValueArray)->elt[lCount]) != iSize) {
								err += NumericArrayResize(uB, 1, (UHandle*)&(*stringValueArray)->elt[lCount], iSize);
								LStrLen(*(*stringValueArray)->elt[lCount]) = iSize;
							}
							if (iSize)
								memcpy(LStrBuf(*(*stringValueArray)->elt[lCount]), szTmp, iSize);
						}
					}
				}
				bDbrTime = 1;
				break;
			case DBR_TIME_ENUM:
				for (long lCount = 0; lCount < args.count; lCount++) {
					dbr_enum_t enumValue = ((dbr_enum_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
					if (sEnum.no_str > 0 && enumValue < sEnum.no_str)
						iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%s", sEnum.strs[((dbr_enum_t*)dbr_value_ptr(args.dbr, args.type))[lCount]]);
					else if (sEnum.no_str == 0)
						iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%d", ((dbr_enum_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
					else
						iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%s", "Illegal Value");
					if (parent && parent->szName) {
						std::string fieldValue = myItemsFindEnum(szName, enumValue);
						if (fieldValue.empty()) {
							iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%d", enumValue);
							fieldValue = szTmp;
						}
						parent->properties[std::string(szName + strlen(parent->szName) + 1)] = fieldValue;
					}
					else {
						if (validDoubleArray) {
							(*doubleValueArray)->elt[lCount] = ((dbr_enum_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
						}
						if (validStringArray) {
							if (LHStrLen((*stringValueArray)->elt[lCount]) != iSize) {
								err += NumericArrayResize(uB, 1, (UHandle*)&(*stringValueArray)->elt[lCount], iSize);
								LStrLen(*(*stringValueArray)->elt[lCount]) = iSize;
							}
							if (iSize)
								memcpy(LStrBuf(*(*stringValueArray)->elt[lCount]), szTmp, iSize);
						}
					}
				}
				bDbrTime = 1;
				break;
			case DBR_CTRL_ENUM:
				tmpEnum = (dbr_ctrl_enum*)args.dbr;
				if (!tmpEnum) break;
				sEnum.no_str = tmpEnum->no_str;
				for (uInt32 i = 0; i < (uInt32)tmpEnum->no_str; i++) {
					epicsSnprintf(sEnum.strs[i], MAX_ENUM_STRING_SIZE, "%s", tmpEnum->strs[i]);
				}
				for (long lCount = 0; lCount < args.count; lCount++) {
					iSize = 0;
					if (parent && parent->szName) {
						dbr_enum_t enumValue = ((dbr_enum_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
						std::string fieldValue = myItemsFindEnum(szName, enumValue);
						if (fieldValue.empty()) {
							iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%d", enumValue);
							fieldValue = szTmp;
						}
						parent->properties[std::string(szName + strlen(parent->szName) + 1)] = fieldValue;
					}
					else {
						dbr_enum_t enumValue = ((dbr_enum_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
						std::string enumStringValue = myItemsFindEnum(szName, enumValue);
						if (enumStringValue.empty()) {
							iSize = epicsSnprintf(szTmp, MAX_STRING_SIZE, "%d", enumValue);
							enumStringValue = "szTmp";
						}
						else {
							iSize = (int32)enumStringValue.size();
						}
						if (validDoubleArray) {
							(*doubleValueArray)->elt[lCount] = enumValue;
						}
						if (validStringArray) {
							if (LHStrLen((*stringValueArray)->elt[lCount]) != iSize) {
								err += NumericArrayResize(uB, 1, (UHandle*)&(*stringValueArray)->elt[lCount], iSize);
								LStrLen(*(*stringValueArray)->elt[lCount]) = iSize;
							}
							if (iSize)
								memcpy(LStrBuf(*(*stringValueArray)->elt[lCount]), enumStringValue.c_str(), iSize);
						}
					}
					modified();
				}
				bDbrTime = 0;
				break;
			default:
				setError(ECA_BADTYPE);
				hasValue = true;
				unlock();
				return;
			}
			if (args.status == ECA_NORMAL) {
				// Update time stamp, status, severity and error messages
				if (bDbrTime) {
					iSize = (int32)epicsTimeToStrftime(szTmp, MAX_STRING_SIZE, "%Y-%m-%d %H:%M:%S.%06f", &((struct dbr_time_short*)args.dbr)->stamp);
					if (!TimeStampString || (*TimeStampString)->cnt != iSize) {
						err += NumericArrayResize(uB, 1, (UHandle*)&TimeStampString, iSize);
						(*TimeStampString)->cnt = iSize;
					}
					memcpy((*TimeStampString)->str, szTmp, iSize);
					TimeStampNumber = ((struct dbr_time_short*)args.dbr)->stamp.secPastEpoch;

					iSize = (int32)strlen(alarmStatusString[((struct dbr_time_short*)args.dbr)->status]);
					if (!StatusString || (*StatusString)->cnt != iSize) {
						err += NumericArrayResize(uB, 1, (UHandle*)&StatusString, iSize);
						(*StatusString)->cnt = iSize;
					}
					memcpy((*StatusString)->str, alarmStatusString[((struct dbr_time_short*)args.dbr)->status], iSize);
					StatusNumber = ((struct dbr_time_short*)args.dbr)->status;

					iSize = (int32)strlen(alarmSeverityString[((struct dbr_time_short*)args.dbr)->severity]);
					if (!SeverityString || (*SeverityString)->cnt != iSize) {
						err += NumericArrayResize(uB, 1, (UHandle*)&SeverityString, iSize);
						(*SeverityString)->cnt = iSize;
					}
					memcpy((*SeverityString)->str, alarmSeverityString[((struct dbr_time_short*)args.dbr)->severity], iSize);
					SeverityNumber = ((struct dbr_time_short*)args.dbr)->severity;
					modified();
				}
			}
			setError(args.status);
			hasValue = true;
			if (bDbrTime && (RefNum.size() || (parent && parent->RefNum.size()))) {
				unlock();
				if (parent)
					parent->postEvent();
				else
					postEvent();
			}
			else {
				unlock();
			}
		}
		catch (std::exception& ex) {
			DbgTime(); CaLabDbgPrintf("%s", ex.what());
			if (locked.load())
				unlock();
		}
	}

	// write EPICS PV
	//    ValueArray2D:       data array (buffer)
	//    DataType:           data type
	//      # => LabVIEW => C++ => EPICS
	//	    0 = > String = > char[] = > dbr_string_t
	//		1 = > Single - precision, floating - point = > float = > dbr_float_t
	//		2 = > Double - precision, floating - point = > double = > dbr_double_t
	//		3 = > Byte signed integer = > char = > dbr_char_t
	//		4 = > Word signed integer = > short = > dbr_short_t
	//		5 = > Long signed integer = > int = > dbr_long_t
	//		6 = > Quad signed integer = > long = > dbr_long_t
	//    Row:                row in data array
	//    ValuesPerSet:       numbers of values in row
	//    Error:              resulting error
	//    Timeout:            EPICS event timeout in seconds
	//    Wait4Readback:      true = callback will be used (no interrupt of motor records)
	void put(void* ValueArray2D, uInt32 DataType, uInt32 row, uInt32 ValuesPerSet, sError* Error, double Timeout, bool Wait4Readback) {
		LStrHandle currentStringValue;
		char szTmp[MAX_ERROR_SIZE];
		uInt32 iResult = ECA_NORMAL;
		uInt32 iPos = 0;
		uInt32 valuesPerSetMax;
		int32 stringSize;
		uInt32 size = 0;
		try {
			if (stopped || !caID || !*(((bool*)caID) + currentlyConnectedPos) || !numberOfValues) {
				if (!stopped) {
					iResult = ECA_DISCONN;
					stringSize = (int32)strlen(ca_message(iResult));
					if (!Error->source || (*Error->source)->cnt != stringSize) {
						NumericArrayResize(uB, 1, (UHandle*)&Error->source, stringSize);
						(*Error->source)->cnt = stringSize;
					}
					memcpy((*Error->source)->str, ca_message(iResult), stringSize);
					if (iResult != ECA_NORMAL)
						Error->code = ERROR_OFFSET + iResult;
					else
						Error->code = 0;
					Error->status = 0;
				}
				return;
			}
			valuesPerSetMax = numberOfValues;
			tasks.fetch_add(1);
			putReadBack = false;
			if (ValuesPerSet > valuesPerSetMax)
				stringSize = valuesPerSetMax;
			else
				stringSize = ValuesPerSet;
			// Create new transfer object (writeValueArray)
			switch (DataType) {
			case 0:
				size = static_cast<unsigned long long>(stringSize) * MAX_STRING_SIZE * sizeof(char);
				if (writeValueArraySize != size) {
					void* tmp = realloc(writeValueArray, size);
					if (tmp) {
						writeValueArray = (char*)tmp;
						writeValueArraySize = size;
					}
				}
				memset(writeValueArray, 0, size);
				break;
			case 1:
				if (nativeType == DBF_STRING) {
					size = static_cast<unsigned long long>(stringSize) * MAX_STRING_SIZE * sizeof(char);
					if (writeValueArraySize != size) {
						void* tmp = realloc(writeValueArray, size);
						if (tmp) {
							writeValueArray = (char*)tmp;
							writeValueArraySize = size;
						}
					}
					memset(writeValueArray, 0, size);
				}
				else {
					size = stringSize * sizeof(float);
					if (writeValueArraySize != size) {
						void* tmp = realloc(writeValueArray, size);
						if (tmp) {
							writeValueArray = (float*)tmp;
							writeValueArraySize = size;
						}
					}
				}
				break;
			case 2:
				if (nativeType == DBF_STRING) {
					size = static_cast<unsigned long long>(stringSize) * MAX_STRING_SIZE * sizeof(char);
					if (writeValueArraySize != size) {
						void* tmp = realloc(writeValueArray, size);
						if (tmp) {
							writeValueArray = (char*)tmp;
							writeValueArraySize = size;
						}
					}
					memset(writeValueArray, 0, size);
				}
				else {
					size = stringSize * sizeof(double);
					if (writeValueArraySize != size) {
						void* tmp = realloc(writeValueArray, size);
						if (tmp) {
							writeValueArray = (double*)tmp;
							writeValueArraySize = size;
						}
					}
				}
				break;
			case 3:
				size = stringSize * sizeof(char);
				if (writeValueArraySize != size) {
					void* tmp = realloc(writeValueArray, size);
					if (tmp) {
						writeValueArray = (char*)tmp;
						writeValueArraySize = size;
					}
				}
				break;
			case 4:
				size = stringSize * sizeof(short);
				if (writeValueArraySize != size) {
					void* tmp = realloc(writeValueArray, size);
					if (tmp) {
						writeValueArray = (short*)tmp;
						writeValueArraySize = size;
					}
				}
				break;
			case 5:
			case 6:
				size = stringSize * sizeof(int);
				if (writeValueArraySize != size) {
					void* tmp = realloc(writeValueArray, size);
					if (tmp) {
						writeValueArray = (int*)tmp;
						writeValueArraySize = size;
					}
				}
				break;
			default:
				// Handled in previous switch-case statement
				break;
			}
			for (int32 col = 0; col < stringSize; col++) {
				switch (DataType) {
				case 0:
					iPos = row * ValuesPerSet + col;
					currentStringValue = (**(sStringArray2DHdl*)ValueArray2D)->elt[iPos];
					if (currentStringValue && (*currentStringValue)->cnt < MAX_STRING_SIZE) {
						memcpy(szTmp, (*currentStringValue)->str, (*currentStringValue)->cnt);
						szTmp[(*currentStringValue)->cnt] = 0x0;
					}
					else {
						szTmp[0] = 0x0;
					}
					if (nativeType != DBF_STRING && nativeType != DBF_ENUM) {
						char* found = 0x0;
						while ((found = strstr(szTmp, ",")) != 0x0)
							*found = '.';
					}
					memcpy((char*)writeValueArray + (int64)col * (int64)MAX_STRING_SIZE, szTmp, strlen(szTmp));
					break;
				case 1:
					iPos = row * ValuesPerSet + col;
					if (nativeType == DBF_STRING) {
						epicsSnprintf((char*)writeValueArray + (int64)col * (int64)MAX_STRING_SIZE, MAX_STRING_SIZE, "%f", (float)(**(sDoubleArray2DHdl*)ValueArray2D)->elt[iPos]);
					}
					else {
						((float*)writeValueArray)[col] = (float)(**(sDoubleArray2DHdl*)ValueArray2D)->elt[iPos];
					}
					break;
				case 2:
					iPos = row * ValuesPerSet + col;
					if (nativeType == DBF_STRING) {
						epicsSnprintf((char*)writeValueArray + (int64)col * (int64)MAX_STRING_SIZE, MAX_STRING_SIZE, "%f", (**(sDoubleArray2DHdl*)ValueArray2D)->elt[iPos]);
					}
					else {
						((double*)writeValueArray)[col] = (**(sDoubleArray2DHdl*)ValueArray2D)->elt[iPos];
					}
					break;
				case 3:
					iPos = row * ValuesPerSet + col;
					((char*)writeValueArray)[col] = (char)(**(sLongArray2DHdl*)ValueArray2D)->elt[iPos];
					break;
				case 4:
					iPos = row * ValuesPerSet + col;
					((short*)writeValueArray)[col] = (short)(**(sLongArray2DHdl*)ValueArray2D)->elt[iPos];
					break;
				case 5:
				case 6:
					iPos = row * ValuesPerSet + col;
					((int*)writeValueArray)[col] = (int)(**(sLongArray2DHdl*)ValueArray2D)->elt[iPos];
					break;
				default:
					;
				}
			}
			switch (DataType) {
			case 0:
				if (Wait4Readback)
					iResult = ca_array_put_callback(DBR_STRING, stringSize, caID, writeValueArray, putState, this);
				else
					iResult = ca_array_put(DBR_STRING, stringSize, caID, writeValueArray);
				break;
			case 1:
				if (nativeType == DBF_STRING) {
					if (Wait4Readback)
						iResult = ca_array_put_callback(DBR_STRING, stringSize, caID, writeValueArray, putState, this);
					else
						iResult = ca_array_put(DBR_STRING, stringSize, caID, writeValueArray);
				}
				else {
					if (Wait4Readback)
						iResult = ca_array_put_callback(DBR_FLOAT, stringSize, caID, writeValueArray, putState, this);
					else
						iResult = ca_array_put(DBR_FLOAT, stringSize, caID, writeValueArray);
				}
				break;
			case 2:
				if (nativeType == DBF_STRING) {
					if (Wait4Readback)
						iResult = ca_array_put_callback(DBR_STRING, stringSize, caID, writeValueArray, putState, this);
					else
						iResult = ca_array_put(DBR_STRING, stringSize, caID, writeValueArray);
				}
				else {
					if (Wait4Readback)
						iResult = ca_array_put_callback(DBR_DOUBLE, stringSize, caID, writeValueArray, putState, this);
					else
						iResult = ca_array_put(DBR_DOUBLE, stringSize, caID, writeValueArray);
				}
				break;
			case 3:
				if (Wait4Readback)
					iResult = ca_array_put_callback(DBR_CHAR, stringSize, caID, writeValueArray, putState, this);
				else
					iResult = ca_array_put(DBR_CHAR, stringSize, caID, writeValueArray);
				break;
			case 4:
				if (Wait4Readback)
					iResult = ca_array_put_callback(DBR_SHORT, stringSize, caID, writeValueArray, putState, this);
				else
					iResult = ca_array_put(DBR_SHORT, stringSize, caID, writeValueArray);
				break;
			case 5:
			case 6:
				if (Wait4Readback)
					iResult = ca_array_put_callback(DBR_LONG, stringSize, caID, writeValueArray, putState, this);
				else
					iResult = ca_array_put(DBR_LONG, stringSize, caID, writeValueArray);
				break;
			default:
				;
			}
			stringSize = (int32)strlen(ca_message(iResult));
			if (!Error->source || (*Error->source)->cnt != stringSize) {
				NumericArrayResize(uB, 1, (UHandle*)&Error->source, stringSize);
				(*Error->source)->cnt = stringSize;
			}
			memcpy((*Error->source)->str, ca_message(iResult), stringSize);
			if (iResult != ECA_NORMAL)
				Error->code = ERROR_OFFSET + iResult;
			else
				Error->code = 0;
			Error->status = 0;
		}
		catch (std::exception& ex) {
			DbgTime(); CaLabDbgPrintf("%s", ex.what());
			if (locked.load())
				unlock();
		}
		tasks.fetch_sub(1);
	}

	// post LV user event
	void postEvent() {
		if (!reservedCounter || !isConnected) return; // no VIs ready to run
#if defined _WIN32 || defined _WIN64
		if (!dllStatus) return;		// DLL_PROCESS_DETACH
#endif
		int32 err = lock();
		if (err) CaLabDbgPrintf("lock failed in postEvent");
		tasks.fetch_add(1);
		std::vector<LVUserEventRef>::iterator itRefNum;
		std::vector<sResult*>::iterator itEventResultCluster;
		try {
			MgErr err = noErr;
			itRefNum = RefNum.begin();
			itEventResultCluster = eventResultCluster.begin();
			while (itRefNum != RefNum.end() && itEventResultCluster != eventResultCluster.end()) {
				if (*itRefNum) {
					if (!*itEventResultCluster
						|| DSCheckPtr(*itEventResultCluster) != noErr
						|| !(*itEventResultCluster)->PVName) {
						itEventResultCluster = eventResultCluster.erase(itEventResultCluster);
						itRefNum = RefNum.erase(itRefNum);
						continue;
					}
					if (stringValueArray && *stringValueArray && (*stringValueArray)->dimSize && (*itEventResultCluster)->PVName) {
						sStringArrayHdl resultStringArrayHdl = (*itEventResultCluster)->StringValueArray;
						sDoubleArrayHdl resultNumberArrayHdl = (*itEventResultCluster)->ValueNumberArray;
						if (!resultStringArrayHdl) {
							(*itEventResultCluster)->StringValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t) + (*stringValueArray)->dimSize * sizeof(LStrHandle[1]));
							if ((*itEventResultCluster)->StringValueArray && *((*itEventResultCluster)->StringValueArray)) {
								(*((*itEventResultCluster)->StringValueArray))->dimSize = (*stringValueArray)->dimSize;
								resultStringArrayHdl = (*itEventResultCluster)->StringValueArray;
							}
						}
						if (!resultNumberArrayHdl) {
							(*itEventResultCluster)->ValueNumberArray = (sDoubleArrayHdl)DSNewHClr(sizeof(size_t) + (*stringValueArray)->dimSize * sizeof(double[1]));
							if ((*itEventResultCluster)->ValueNumberArray && *((*itEventResultCluster)->ValueNumberArray)) {
								(*((*itEventResultCluster)->ValueNumberArray))->dimSize = (*doubleValueArray)->dimSize;
								resultNumberArrayHdl = (*itEventResultCluster)->ValueNumberArray;
							}
						}
						if (resultStringArrayHdl) {
							if ((*resultStringArrayHdl)->dimSize != (*stringValueArray)->dimSize) {
								CaLabDbgPrintf("stringValueArray size mismatch %d vs. %d", (*resultStringArrayHdl)->dimSize, (*stringValueArray)->dimSize);
								itEventResultCluster = eventResultCluster.erase(itEventResultCluster);
								itRefNum = RefNum.erase(itRefNum);
								continue;
							}
							for (uInt32 j = 0; j < (*stringValueArray)->dimSize && j < (*resultStringArrayHdl)->dimSize; j++) {
								if (!(*resultStringArrayHdl)->elt[j] || ((*stringValueArray)->elt[j] && ((*(*resultStringArrayHdl)->elt[j])->cnt != (*(*stringValueArray)->elt[j])->cnt))) {
									err += NumericArrayResize(uB, 1, (UHandle*)&(*resultStringArrayHdl)->elt[j], (*stringValueArray)->elt[j] ? (*(*stringValueArray)->elt[j])->cnt : 1);
									(*(*resultStringArrayHdl)->elt[j])->cnt = (*stringValueArray)->elt[j] ? (*(*stringValueArray)->elt[j])->cnt : 1;
								}
								if ((*stringValueArray)->elt[j])
									memcpy((*(*resultStringArrayHdl)->elt[j])->str, (*(*stringValueArray)->elt[j])->str, (*(*stringValueArray)->elt[j])->cnt);
								else
									memcpy((*(*resultStringArrayHdl)->elt[j])->str, "\0", 1);
								if (resultNumberArrayHdl) {
									(*resultNumberArrayHdl)->elt[j] = (*doubleValueArray)->elt[j];
								}
							}
							(*itEventResultCluster)->valueArraySize = (uInt32)(*stringValueArray)->dimSize;
						}
						else if (resultNumberArrayHdl) {
							if ((*resultNumberArrayHdl)->dimSize != (*doubleValueArray)->dimSize) {
								CaLabDbgPrintf("numberValueArray size mismatch %d vs. %d", (*resultNumberArrayHdl)->dimSize, (*doubleValueArray)->dimSize);
								itEventResultCluster = eventResultCluster.erase(itEventResultCluster);
								itRefNum = RefNum.erase(itRefNum);
								continue;
							}
							for (uInt32 j = 0; j < (*resultNumberArrayHdl)->dimSize; j++) {
								(*resultNumberArrayHdl)->elt[j] = (*doubleValueArray)->elt[j];
							}
						}
						size_t propertiesSize = properties.size();
						if (propertiesSize && (*itEventResultCluster)->FieldNameArray && (*(*itEventResultCluster)->FieldNameArray)->dimSize && (*itEventResultCluster)->FieldValueArray && (*(*itEventResultCluster)->FieldValueArray)->dimSize && (*(*itEventResultCluster)->FieldNameArray)->dimSize == (*(*itEventResultCluster)->FieldValueArray)->dimSize) {
							for (uInt32 l = 0; l < (*(*itEventResultCluster)->FieldNameArray)->dimSize; l++) {
								propertyMapIterator nameIterator = properties.find(std::string((char*)(*(*(*itEventResultCluster)->FieldNameArray)->elt[l])->str, (size_t)(*(*(*itEventResultCluster)->FieldNameArray)->elt[l])->cnt));
								if (nameIterator != properties.end()) {
									std::string fieldValue = nameIterator->second;
									size_t fieldValueLength = fieldValue.size();
									if (fieldValueLength) {
										if (!(*(*itEventResultCluster)->FieldValueArray)->elt[l] || ((size_t)(*(*(*itEventResultCluster)->FieldValueArray)->elt[l])->cnt != fieldValueLength)) {
											err += NumericArrayResize(uB, 1, (UHandle*)&(*(*itEventResultCluster)->FieldValueArray)->elt[l], fieldValueLength);
											(*(*(*itEventResultCluster)->FieldValueArray)->elt[l])->cnt = (int32)fieldValueLength;
										}
										memcpy((*(*(*itEventResultCluster)->FieldValueArray)->elt[l])->str, fieldValue.c_str(), fieldValueLength);
									}
									else {
										err += NumericArrayResize(uB, 1, (UHandle*)&(*(*itEventResultCluster)->FieldValueArray)->elt[l], 0);
										(*(*(*itEventResultCluster)->FieldValueArray)->elt[l])->cnt = 0;
									}
								}
								else {
									err += NumericArrayResize(uB, 1, (UHandle*)&(*(*itEventResultCluster)->FieldValueArray)->elt[l], 0);
									(*(*(*itEventResultCluster)->FieldValueArray)->elt[l])->cnt = 0;
								}
							}
						}
						(*itEventResultCluster)->TimeStampNumber = TimeStampNumber;
						if (TimeStampString) {
							if (!(*itEventResultCluster)->TimeStampString || (*(*itEventResultCluster)->TimeStampString)->cnt != (*TimeStampString)->cnt) {
								NumericArrayResize(uB, 1, (UHandle*)&(*itEventResultCluster)->TimeStampString, (*TimeStampString)->cnt);
								(*(*itEventResultCluster)->TimeStampString)->cnt = (*TimeStampString)->cnt;
							}
							memcpy((*(*itEventResultCluster)->TimeStampString)->str, (*TimeStampString)->str, (*TimeStampString)->cnt);
						}
						if (StatusString) {
							if (!(*itEventResultCluster)->StatusString || (*(*itEventResultCluster)->StatusString)->cnt != (*StatusString)->cnt) {
								NumericArrayResize(uB, 1, (UHandle*)&(*itEventResultCluster)->StatusString, (*StatusString)->cnt);
								(*(*itEventResultCluster)->StatusString)->cnt = (*StatusString)->cnt;
							}
							memcpy((*(*itEventResultCluster)->StatusString)->str, (*StatusString)->str, (*StatusString)->cnt);
						}
						if (SeverityString) {
							if (!(*itEventResultCluster)->SeverityString || (*(*itEventResultCluster)->SeverityString)->cnt != (*SeverityString)->cnt) {
								NumericArrayResize(uB, 1, (UHandle*)&(*itEventResultCluster)->SeverityString, (*SeverityString)->cnt);
								(*(*itEventResultCluster)->SeverityString)->cnt = (*SeverityString)->cnt;
							}
							memcpy((*(*itEventResultCluster)->SeverityString)->str, (*SeverityString)->str, (*SeverityString)->cnt);
						}
						if (ErrorIO.source) {
							if (!(*itEventResultCluster)->ErrorIO.source || (*(*itEventResultCluster)->ErrorIO.source)->cnt != (*ErrorIO.source)->cnt) {
								NumericArrayResize(uB, 1, (UHandle*)&(*itEventResultCluster)->ErrorIO.source, (*ErrorIO.source)->cnt);
								(*(*itEventResultCluster)->ErrorIO.source)->cnt = (*ErrorIO.source)->cnt;
							}
							memcpy((*(*itEventResultCluster)->ErrorIO.source)->str, (*ErrorIO.source)->str, (*ErrorIO.source)->cnt);
						}
						(*itEventResultCluster)->StatusNumber = StatusNumber;
						(*itEventResultCluster)->SeverityNumber = SeverityNumber;
						(*itEventResultCluster)->ErrorIO.code = ErrorIO.code;
						(*itEventResultCluster)->ErrorIO.status = ErrorIO.status;
						// Post it!
						MgErr posterr = PostLVUserEvent(*itRefNum, *itEventResultCluster);
						if (posterr != mgNoErr) {
							itRefNum = RefNum.erase(itRefNum);
							CaLabDbgPrintf("PostLVUserEvent failed with error %d", posterr);
							itEventResultCluster = eventResultCluster.erase((itEventResultCluster));
							continue;
						}
					}
					else {
						int32 size = (int32)strlen(alarmStatusString[epicsAlarmComm]);
						if (!(*itEventResultCluster)->StatusString || (*(*itEventResultCluster)->StatusString)->cnt != size) {
							NumericArrayResize(uB, 1, (UHandle*)&(*itEventResultCluster)->StatusString, size);
							(*(*itEventResultCluster)->StatusString)->cnt = size;
						}
						memcpy((*(*itEventResultCluster)->StatusString)->str, alarmStatusString[epicsAlarmComm], size);
						size = (int32)strlen(alarmSeverityString[epicsSevInvalid]);
						if (!(*itEventResultCluster)->SeverityString || (*(*itEventResultCluster)->SeverityString)->cnt != size) {
							NumericArrayResize(uB, 1, (UHandle*)&(*itEventResultCluster)->SeverityString, size);
							(*(*itEventResultCluster)->SeverityString)->cnt = size;
						}
						memcpy((*(*itEventResultCluster)->SeverityString)->str, alarmSeverityString[epicsSevInvalid], size);
						if (!(*itEventResultCluster)->ErrorIO.source || (*(*itEventResultCluster)->ErrorIO.source)->cnt != (*ErrorIO.source)->cnt) {
							NumericArrayResize(uB, 1, (UHandle*)&(*itEventResultCluster)->ErrorIO.source, (*ErrorIO.source)->cnt);
							(*(*itEventResultCluster)->ErrorIO.source)->cnt = (*ErrorIO.source)->cnt;
						}
						if (!(*itEventResultCluster)->ErrorIO.source || (*(*itEventResultCluster)->ErrorIO.source)->cnt != (int32)strlen(ca_message(ECA_DISCONN))) {
							NumericArrayResize(uB, 1, (UHandle*)&(*itEventResultCluster)->ErrorIO.source, strlen(ca_message(ECA_DISCONN)));
							(*(*itEventResultCluster)->ErrorIO.source)->cnt = (int32)strlen(ca_message(ECA_DISCONN));
						}
						memcpy((*(*itEventResultCluster)->ErrorIO.source)->str, ca_message(ECA_DISCONN), strlen(ca_message(ECA_DISCONN)));
						(*itEventResultCluster)->StatusNumber = epicsAlarmComm;
						(*itEventResultCluster)->SeverityNumber = epicsSevInvalid;
						(*itEventResultCluster)->ErrorIO.code = ERROR_OFFSET + epicsSevInvalid;
						(*itEventResultCluster)->ErrorIO.status = 0;
						// Post it!
						MgErr posterr = PostLVUserEvent(*itRefNum, *itEventResultCluster);
						if (posterr != mgNoErr) {
							CaLabDbgPrintf("PostLVUserEvent failed with error %d", posterr);
							itRefNum = RefNum.erase(itRefNum);
							itEventResultCluster = eventResultCluster.erase((itEventResultCluster));
							continue;
						}
					}
					itRefNum++;
					itEventResultCluster++;
				}
				else {
					itRefNum++;
					itEventResultCluster++;
					CaLabDbgPrintf("post event of %s has no reference number", szName);
				}
			}
		}
		catch (std::exception& ex) {
			DbgTime(); CaLabDbgPrintf("%s", ex.what());
			itRefNum = RefNum.begin();
			while (itRefNum != RefNum.end()) {
				itRefNum = RefNum.erase(itRefNum);
			}
			itEventResultCluster = eventResultCluster.begin();
			while (itEventResultCluster != eventResultCluster.end()) {
				itEventResultCluster = eventResultCluster.erase(itEventResultCluster);
			}
		}
		tasks.fetch_sub(1);
		unlock();
		return;
	}
};

typedef std::unordered_map<std::string, calabItem*> pvMap;
typedef pvMap::iterator pvMapIterator;
pvMap myItems;

std::string myItemsFindEnum(std::string name, dbr_enum_t enumValue) {
	std::string enumString = "";
	pvMapIterator fieldItemIterator = myItems.find(name);
	if (fieldItemIterator != myItems.end()) {
		if (enumValue < fieldItemIterator->second->sEnum.no_str)
			enumString = fieldItemIterator->second->sEnum.strs[enumValue];
		else
			enumString = "Illegal Value";
	}
	return enumString;
}

// Class used to store globals and ensure library is initialized.
class globals {
public:
	mutable std::shared_mutex mapLock;	// map mutex

	globals() {
		// caLabLoad(); // since EPICS 7.0.7 we have to create THE EPICS context in a separate thread
		epicsThreadCreate("createCaContext",
			epicsThreadPriorityBaseMax,
			epicsThreadGetStackSize(epicsThreadStackBig),
			(EPICSTHREADFUNC)caLabLoad, 0x0);
		getLock = epicsMutexCreate();
		putLock = epicsMutexCreate();
	}

	~globals() {
#if defined _WIN32 || defined _WIN64
		if (!dllStatus) return; // DLL_PROCESS_DETACH
#endif
		uInt32 timeout = 1000;
		stopped = true;
		while (timeout > 0 && tasks.load() > 0) {
			epicsThreadSleep(.01);
			timeout--;
		}
		if (timeout <= 0)
			CaLabDbgPrintf("Error: Could not terminate all running tasks of CA Lab.");
		for (auto& iter : myItems)
			delete iter.second;
		epicsMutexDestroy(putLock);
		epicsMutexDestroy(getLock);
		ca_context_destroy();
		caLabUnload();
	}

	// Insert into global list of pv names
	calabItem* insert(std::string name, calabItem* item) {
		mapLock.lock();		// exclusive lock
		auto search = myItems.find(name);
		if (search != myItems.end()) {
#ifdef _DEBUG
			CaLabDbgPrintf("%s is already in the map myItems", name.c_str());
#endif
			delete item;
			item = search->second;
		}
		else {
			myItems.insert({ name, item });
		}
		mapLock.unlock();
		return item;
	}

	// add new data object if not exists
	//    name: EPICS variable name
	//    FieldNameArray: field names of interest of current EPICS variable
	//    return: pointer to added / 'found in list' data object
	calabItem* add(void* currentInstance, LStrHandle name, sStringArrayHdl FieldNameArray = 0x0, uInt32 filter = 0) {
		unsigned char cName[MAX_NAME_SIZE];
		std::string sName;
		calabItem* currentItem;

		_LToCStrN(*name, cName, sizeof(cName));
		sName = (char*)cName;
		mapLock.lock_shared();
		pvMapIterator itemIterator = myItems.find(sName);
		if (itemIterator != myItems.end()) {
			currentItem = itemIterator->second;
			if (currentItem && FieldNameArray && *FieldNameArray) {
				calabItem* fieldItem;
				std::string fieldName = "";
				LStrHandle fullFieldName = nullptr;
				propertyMapIterator nameIterator;
				int32 fullsize = 0;
				for (uInt32 i = 0; i < (*FieldNameArray)->dimSize; i++) {
					if ((*FieldNameArray)->elt[i] && *(*FieldNameArray)->elt[i] && (*(*FieldNameArray)->elt[i])->cnt) {
						fieldName = std::string((char*)(*(*FieldNameArray)->elt[i])->str, (size_t)(*(*FieldNameArray)->elt[i])->cnt);
						fieldName = trim(fieldName);
						nameIterator = currentItem->properties.find(fieldName);
						if (nameIterator == currentItem->properties.end()) {
							fullsize = (*currentItem->name)->cnt + 1 + (int32)fieldName.size();
							NumericArrayResize(uB, 1, (UHandle*)&fullFieldName, fullsize);
							memcpy(LStrBuf(*fullFieldName), LStrBuf(*currentItem->name), LStrLen(*currentItem->name));
							memcpy(LStrBuf(*fullFieldName) + LStrLen(*currentItem->name), ".", 1);
							memcpy(LStrBuf(*fullFieldName) + LStrLen(*currentItem->name) + 1, fieldName.c_str(), fieldName.size());
							LStrLen(*fullFieldName) = fullsize;
							fieldItem = new calabItem(currentInstance, fullFieldName, 0x0);
							fieldItem->parent = currentItem;
							mapLock.unlock_shared();
							fieldItem = insert(std::string((char*)(*fullFieldName)->str, (size_t)(*fullFieldName)->cnt).c_str(), fieldItem);
							mapLock.lock_shared();
						}
					}
				}
				if (fullFieldName) {
					DSDisposeHandle(fullFieldName);
				}
			}
			mapLock.unlock_shared();
		}
		else {
			mapLock.unlock_shared();
			currentItem = new calabItem(currentInstance, name, FieldNameArray);
			if (currentItem) {
				currentItem = insert(sName, currentItem);
				if (FieldNameArray && *FieldNameArray) {
					LStrHandle fullFieldName = nullptr;
					int32 fullsize = 0;
					for (uInt32 i = 0; i < (*FieldNameArray)->dimSize; i++) {
						if ((*FieldNameArray)->elt[i] && *(*FieldNameArray)->elt[i]) {
							std::string fieldName = std::string((char*)(*(*FieldNameArray)->elt[i])->str, (size_t)(*(*FieldNameArray)->elt[i])->cnt);
							fieldName = trim(fieldName);
							fullsize = LStrLen(*name) + 1 + LStrLen(*((*FieldNameArray)->elt[i]));
							NumericArrayResize(uB, 1, (UHandle*)&fullFieldName, fullsize);
							memcpy(LStrBuf(*fullFieldName), LStrBuf(*name), LStrLen(*name));
							memcpy(LStrBuf(*fullFieldName) + LStrLen(*name), ".", 1);
							memcpy(LStrBuf(*fullFieldName) + LStrLen(*name) + 1, LStrBuf(*((*FieldNameArray)->elt[i])), LStrLen(*((*FieldNameArray)->elt[i])));
							LStrLen(*fullFieldName) = fullsize;
							char* cFieldName = new char[fullsize + 1LL];
							_LToCStrN(*fullFieldName, (CStr)cFieldName, fullsize + 1);
							std::string sFieldName = (char*)cFieldName;
							calabItem* fieldItem;
							mapLock.lock_shared();
							pvMapIterator itemIterator = myItems.find(sFieldName);
							if (itemIterator == myItems.end()) {
								mapLock.unlock_shared();
								fieldItem = new calabItem(currentInstance, fullFieldName, 0x0);
								fieldItem->parent = currentItem;
								fieldItem = insert(sFieldName, fieldItem);
							}
							else {
								mapLock.unlock_shared();
							}
							delete[] cFieldName;
						}
					}
					if (fullFieldName) {
						DSDisposeHandle(fullFieldName);
					}
				}
			}
		}
		return currentItem;
	}

} globals;

// error handler for segfault
void signalHandler(int signum) {
	switch (signum) {
	case SIGABRT:
		DbgTime(); CaLabDbgPrintf("Abnormal termination of CA Lab");
		if (pCaLabDbgFile)
			epicsThreadSleep(5);
		signal(SIGABRT, 0x0);
		break;
	case SIGFPE:
		DbgTime(); CaLabDbgPrintf("An erroneous arithmetic operation, such as a divide by zero or an operation resulting in overflow.");
		if (pCaLabDbgFile)
			epicsThreadSleep(5);
		signal(SIGFPE, 0x0);
		break;
	case SIGILL:
		DbgTime(); CaLabDbgPrintf("Detection of an illegal instruction.");
		if (pCaLabDbgFile)
			epicsThreadSleep(5);
		signal(SIGILL, 0x0);
		break;
	case SIGINT:
		DbgTime(); CaLabDbgPrintf("Receipt of an interactive attention signal.");
		if (pCaLabDbgFile)
			epicsThreadSleep(5);
		signal(SIGINT, 0x0);
		break;
	case SIGSEGV:
		DbgTime(); CaLabDbgPrintf("An invalid access to storage.");
		if (pCaLabDbgFile)
			epicsThreadSleep(5);
		signal(SIGSEGV, 0x0);
		break;
	case SIGTERM:
		DbgTime(); CaLabDbgPrintf("A termination request sent to the program.");
		if (pCaLabDbgFile)
			epicsThreadSleep(5);
		signal(SIGTERM, 0x0);
		break;
	}
}

// callback for Channel Access warnings and errors
void exceptionCallback(struct exception_handler_args args) {
	// Don't enter if library terminates
	if (stopped)
		return;

	if (args.chid) {
		DbgTime(); CaLabDbgPrintf("%s: %s", ca_name(args.chid), ca_message(args.stat));
		return;
	}
	if (args.stat == 200) {
		if (!err200) {
			err200 = true;
			DbgTime(); CaLabDbgPrintf("%s (Please check your configuration of \"EPICS_CA_ADDR_LIST\" and \"EPICS_CA_AUTO_ADDR_LIST\")", ca_message(args.stat));
		}
		return;
	}
	if (args.stat != 192) {
		DbgTime(); CaLabDbgPrintf("Channel Access error: %s", ca_message(args.stat));
		CaLabDbgPrintf("%s", args.ctx);
		return;
	}
}

// clean up LV string array
MgErr DeleteStringArray(sStringArrayHdl array) {
	MgErr err = noErr;
	err = DSCheckHandle(array);
	if (err != noErr)
		return err;
	for (uInt32 i = 0; i < (*array)->dimSize; i++)
		err += DSDisposeHandle((*array)->elt[i]);
	err += DSDisposeHandle(array);
	return err;
}

// DbgPrintf wrapper
//    format: format specifier
//    ...: additional arguments
MgErr CaLabDbgPrintf(const char* format, ...) {
	int done = 0;
#if defined _WIN32 || defined _WIN64
	if (!dllStatus) return done; // DLL_PROCESS_DETACH
#endif
	va_list listPointer;
	va_start(listPointer, format);
	if (pCaLabDbgFile) {
		done = vfprintf(pCaLabDbgFile, format, listPointer);
		fprintf(pCaLabDbgFile, "\n");
		fflush(pCaLabDbgFile);
	}
	else {
		done = DbgPrintfv(format, listPointer);
	}
	va_end(listPointer);
	return done;
}

// write current time stamp into debug window
void DbgTime(void) {
#ifdef _DEBUG
	time_t ltime;
	ltime = time(NULL);
	CaLabDbgPrintf("------------------------------");
	CaLabDbgPrintf("%s", asctime(localtime(&ltime)));
#endif
}

// DbgPrintf wrapper for debug mode only
//    format: format specifier
//    ...: additional arguments
MgErr CaLabDbgPrintfD(const char* format, ...) {
	int done = 0;
#ifdef _DEBUG
	va_list listPointer;
	va_start(listPointer, format);
	if (pCaLabDbgFile) {
		done = vfprintf(pCaLabDbgFile, format, listPointer);
		fprintf(pCaLabDbgFile, "\n");
		fflush(pCaLabDbgFile);
	}
	else {
		done = DbgPrintfv(format, listPointer);
	}
	va_end(listPointer);
#endif
	return done;
}

// callback of LabVIEW when any caLab-VI is loaded
//    instanceState: undocumented pointer
extern "C" EXPORT MgErr reserved(InstanceDataPtr * instanceState) {
	reservedCounter++;
	return 0;
}

// callback of LV when any caLab-VI is unloaded
//    instanceState: undocumented pointer
extern "C" EXPORT MgErr unreserved(InstanceDataPtr * instanceState) {
	if (reservedCounter > 0) {
		//CaLabDbgPrintf("unreserved %d", (uInt32)*instanceState);
		reservedCounter--;
	}
	else
		CaLabDbgPrintf("\"unreserved()\" called too often!");
	return 0;
}

// callback of LV when any caLab-VI is aborted
//    instanceState: undocumented pointer
extern "C" EXPORT MgErr aborted(InstanceDataPtr * instanceState) {
	return 0;
}

// validate pointer
int valid(void* pointer) {
	if (pointer != NULL) {
		try {
			return ((calabItem*)pointer)->validAddress == (void*)pointer;
		}
		catch (std::exception& e) {
			DbgTime(); CaLabDbgPrintf("Error: %s", e.what());
		}
	}
	return false;
}

// callback of EPICS for changed connection state
//    args:   contains pointer to data object
void connectionChanged(connection_handler_args args) {
	// Don't enter if library terminates
	if (stopped)
		return;
	try {
		calabItem* item = (calabItem*)ca_puser(args.chid);
		if (item)
			item->itemConnectionChanged(args);
	}
	catch (std::exception& ex) {
		DbgTime(); CaLabDbgPrintf("%s", ex.what());
	}
}

// callback for changed EPICS values
//    args:   contains pointer to data object
void valueChanged(evargs args) {
	// Don't enter if library terminates
	if (stopped)
		return;
	try {
		calabItem* item = (calabItem*)ca_puser(args.chid);
		if (item)
			item->itemValueChanged(args);
	}
	catch (std::exception& ex) {
		DbgTime(); CaLabDbgPrintf("%s", ex.what());
	}
}

// callback for finished put task
//    args:   contains pointer to data object
void putState(evargs args) {
	// Don't enter if library terminates
	if (stopped)
		return;
	calabItem* item = (calabItem*)ca_puser(args.chid);
	item->setError(args.status);
	if (item)
		item->putReadBack = true;
	else
		CaLabDbgPrintf("putState got corrupt evargs");

}

// check whether all data objects got values
//    maxNumberOfValues: maximum number of values in single array across all read arrays
//    PvIndexArray: Pointer array of data objects
//    Timeout: time out for check values
//    all: ignore PvIndexArray and check full list of known data objects
void wait4value(uInt32& maxNumberOfValues, sLongArrayHdl* PvIndexArray, time_t Timeout, bool all = false) {
	time_t stop = time(nullptr) + Timeout;
	calabItem* currentItem;
	time_t timeout;
	uInt32 counter;
	bool isFirstRun = true;
	while ((timeout = time(nullptr)) < stop) {
		counter = 1;
		maxNumberOfValues = 0;
		if (all) {
			for (uInt32 i = 0; i < (**PvIndexArray)->dimSize; i++) {
				currentItem = (calabItem*)(**PvIndexArray)->elt[i];
				if (!valid(currentItem)) {
					DbgTime(); CaLabDbgPrintf("Error in wait4value all: Index array is corrupted.");
					continue;
				}
				if (isFirstRun) {
					currentItem->isPassive = false;
					propertyMapIterator nameIterator = currentItem->properties.begin();
					while (nameIterator != currentItem->properties.end()) {
						pvMapIterator myItemIterator = myItems.find(currentItem->szName + std::string(".") + nameIterator->first);
						if (myItemIterator != myItems.end()) {
							myItemIterator->second->isPassive = false;
						}
						nameIterator++;
					}
				}
				if (currentItem->hasValue) {
					if (!currentItem->parent) {
						counter++;
						if (currentItem->numberOfValues > maxNumberOfValues) {
							maxNumberOfValues = currentItem->numberOfValues;
						}
					}
				}
				else {
					continue;
				}
			}
		}
		else {
			for (uInt32 i = 0; i < (**PvIndexArray)->dimSize; i++) {
				currentItem = (calabItem*)(**PvIndexArray)->elt[i];
				if (!valid(currentItem)) {
					DbgTime(); CaLabDbgPrintf("Error in wait4value: Index array is corrupted.");
					continue;
				}
				if (isFirstRun) {
					currentItem->isPassive = false;
					propertyMapIterator nameIterator = currentItem->properties.begin();
					while (nameIterator != currentItem->properties.end()) {
						pvMapIterator myItemIterator = myItems.find(currentItem->szName + std::string(".") + nameIterator->first);
						if (myItemIterator != myItems.end()) {
							myItemIterator->second->isPassive = false;
						}
						nameIterator++;
					}
				}
				if (currentItem->hasValue) {
					if (!currentItem->parent) {
						counter++;
						int32 err = currentItem->lock();
						if (err) CaLabDbgPrintf("lock failed on currentItem in wait4value");
						if (currentItem->numberOfValues > maxNumberOfValues)
							maxNumberOfValues = currentItem->numberOfValues;
						currentItem->unlock();
					}
				}
			}
		}
		isFirstRun = false;
		if (counter > (**PvIndexArray)->dimSize)
			break;
		epicsThreadSleep(.001);
	}
	if (timeout >= stop) {
		//CaLabDbgPrintfD("timeout in wait4value");
		if (all) {
			globals.mapLock.lock_shared();
			for (auto& iter : myItems) {
				currentItem = iter.second;
				if (!valid(currentItem))
					continue;
				if (!currentItem->hasValue) {
					if (currentItem->parent && currentItem->parent->hasValue) {
						currentItem->hasValue = true;
						currentItem->isPassive = true;
					}
				}
			}
			globals.mapLock.unlock_shared();
		}
		else {
			for (uInt32 i = 0; i < (**PvIndexArray)->dimSize; i++) {
				currentItem = (calabItem*)(**PvIndexArray)->elt[i];
				if (!valid(currentItem))
					continue;
				if (!currentItem->hasValue) {
					if (currentItem->parent && currentItem->parent->hasValue) {
						currentItem->hasValue = true;
						currentItem->isPassive = true;
					}
				}
			}
		}
	}
}

// read EPICS PVs
//    PvNameArray:            handle of a array of PV names
//    FieldNameArray:         handle of a array of optional field names
//    PvIndexArray:           handle of a array of indexes
//    Timeout:                EPICS event timeout in seconds
//    ResultArray:            handle of a result-cluster (result object)
//    FirstStringValue:       handle of a array of first values converted to a string
//    FirstDoubleValue:       handle of a array of first values converted to a double value
//    DoubleValueArray:       handle of a 2d array of double values
//    DoubleValueArraySize:   array size of DoubleValueArray
//    CommunicationStatus:    status of Channel Access communication; 0 = no problem; 1 = any problem occurred
//    FirstCall:              indicator for first call
//    NoMDEL:                 indicator for ignoring monitor dead band (TRUE: use caget instead of camonitor)
extern "C" EXPORT void getValue(sStringArrayHdl * PvNameArray, sStringArrayHdl * FieldNameArray, sLongArrayHdl * PvIndexArray, double Timeout, sResultArrayHdl * ResultArray, sStringArrayHdl * FirstStringValue, sDoubleArrayHdl * FirstDoubleValue, sDoubleArray2DHdl * DoubleValueArray, LVBoolean * CommunicationStatus, LVBoolean * FirstCall, LVBoolean * NoMDEL = 0, LVBoolean * IsInitialized = 0, int filter = 0) {
	epicsMutexLock(getLock);
	if (filter <= 0) {
		filter = 0xffff;
	}
	try {
		if (stopped) {
			epicsMutexUnlock(getLock);
			return;
		}
		if (!*PvNameArray || (**PvNameArray)->dimSize == 0 || !(**PvNameArray)->elt[0]) {
			DbgTime(); CaLabDbgPrintf("Warning: caLabGet needs any PV name");
			epicsMutexUnlock(getLock);
			return;
		}
		sResult* currentResult;
		calabItem* currentItem;
		MgErr err = noErr;
		uInt32 maxNumberOfValues = 0;
		uInt32 doubleValueArrayIndex = 0;
		*CommunicationStatus = 0;
		if (bCaLabPolling)
			*NoMDEL = true;
		if (*NoMDEL)
			*FirstCall = true;
		if ((*PvIndexArray && **PvIndexArray && (**PvIndexArray)->dimSize != (**PvNameArray)->dimSize)) {
			*FirstCall = true;
			*IsInitialized = false;
		}
		if (!*IsInitialized) {
			if (*FirstCall) {
				// START: RESET LV VARIABLES
				if (*ResultArray && (**ResultArray)->dimSize != (**PvNameArray)->dimSize) {
					for (uInt32 i = 0; i < (**ResultArray)->dimSize; i++) {
						currentResult = &(**ResultArray)->result[i];
						if (currentResult->PVName)
							err += DSDisposeHandle(currentResult->PVName);
						currentResult->PVName = 0x0;
						if (currentResult->StringValueArray)
							err += DeleteStringArray(currentResult->StringValueArray);
						currentResult->StringValueArray = 0x0;
						if (currentResult->ValueNumberArray)
							err += DSDisposeHandle(currentResult->ValueNumberArray);
						currentResult->ValueNumberArray = 0x0;
						if (currentResult->StatusString)
							err += DSDisposeHandle(currentResult->StatusString);
						currentResult->StatusString = 0x0;
						if (currentResult->SeverityString)
							err += DSDisposeHandle(currentResult->SeverityString);
						currentResult->SeverityString = 0x0;
						if (currentResult->TimeStampString)
							err += DSDisposeHandle(currentResult->TimeStampString);
						currentResult->TimeStampString = 0x0;
						if (currentResult->FieldNameArray)
							err += DSDisposeHandle(currentResult->FieldNameArray);
						currentResult->FieldNameArray = 0x0;
						if (currentResult->FieldValueArray)
							err += DSDisposeHandle(currentResult->FieldValueArray);
						currentResult->FieldValueArray = 0x0;
						if (currentResult->ErrorIO.source)
							err += DSDisposeHandle(currentResult->ErrorIO.source);
						currentResult->ErrorIO.source = 0x0;
					}
					if (*ResultArray)
						err += DSDisposeHandle(*ResultArray);
					*ResultArray = 0x0;
					if (*FirstStringValue) {
						for (uInt32 j = 0; j < (**FirstStringValue)->dimSize; j++) {
							err += DSDisposeHandle((**FirstStringValue)->elt[j]);
							(**FirstStringValue)->elt[j] = 0x0;
						}
						err += DSDisposeHandle(*FirstStringValue);
						*FirstStringValue = 0x0;
					}
					if (*FirstDoubleValue) {
						err += DSDisposeHandle(*FirstDoubleValue);
						*FirstDoubleValue = 0x0;
					}
					if (*DoubleValueArray) {
						err += DSDisposeHandle(*DoubleValueArray);
						*DoubleValueArray = 0x0;
					}
				}
				if (!*ResultArray) {
					*ResultArray = (sResultArrayHdl)DSNewHClr(sizeof(size_t) + (**PvNameArray)->dimSize * sizeof(sResult[1]));
					(**ResultArray)->dimSize = (**PvNameArray)->dimSize;
				}
				if (!PvIndexArray || !*PvIndexArray || DSCheckHandle(PvIndexArray) != noErr || (**PvIndexArray)->dimSize != (**PvNameArray)->dimSize) {
					err += NumericArrayResize(iQ, 1, (UHandle*)PvIndexArray, (**PvNameArray)->dimSize);
					(**PvIndexArray)->dimSize = (**PvNameArray)->dimSize;
				}
				if (err != noErr) {
					DbgTime(); CaLabDbgPrintfD("Error: Bad memory clean up. (%d)", err);
				}
				for (uInt32 i = 0; i < (**PvNameArray)->dimSize; i++) {
					currentItem = globals.add(PvNameArray, (**PvNameArray)->elt[i], *FieldNameArray, filter);
					if (!currentItem) {
						CaLabDbgPrintf("Error in creating PV %.*s", (*(**PvNameArray)->elt[i])->cnt, (*(**PvNameArray)->elt[i])->str);
						epicsMutexUnlock(getLock);
						return;
					}
					currentItem->isPassive = false;
					(**PvIndexArray)->elt[i] = (uint64_t)currentItem;
					err += NumericArrayResize(uB, 1, (UHandle*)&(**ResultArray)->result[i].PVName, (*currentItem->name)->cnt);
					memcpy((*(**ResultArray)->result[i].PVName)->str, (*currentItem->name)->str, (*currentItem->name)->cnt);
					(*(**ResultArray)->result[i].PVName)->cnt = (*currentItem->name)->cnt;
				}
				wait4value(maxNumberOfValues, PvIndexArray, (time_t)Timeout, true);
				if (!maxNumberOfValues) {
					for (uInt32 i = 0; i < (**PvIndexArray)->dimSize; i++) {
						currentItem = (calabItem*)(**PvIndexArray)->elt[i];
						if (!valid(currentItem)) {
							DbgTime(); CaLabDbgPrintf("Error in getValue no max #: Index array is corrupted.");
							continue;
						}
						int32 err = currentItem->lock();
						if (err) CaLabDbgPrintf("lock failed on timout in getvalue");
						currentResult = &(**ResultArray)->result[i];
						if (filter & out_filter::pviError && currentItem->ErrorIO.source) {
							if (!currentResult->ErrorIO.source || (*currentResult->ErrorIO.source)->cnt != (*currentItem->ErrorIO.source)->cnt) {
								NumericArrayResize(uB, 1, (UHandle*)&currentResult->ErrorIO.source, (*currentItem->ErrorIO.source)->cnt);
								(*currentResult->ErrorIO.source)->cnt = (*currentItem->ErrorIO.source)->cnt;
							}
							memcpy((*currentResult->ErrorIO.source)->str, (*currentItem->ErrorIO.source)->str, (*currentItem->ErrorIO.source)->cnt);
							currentResult->ErrorIO.code = currentItem->ErrorIO.code;
							currentResult->ErrorIO.status = currentItem->ErrorIO.status;
						}
						if (currentItem->ErrorIO.code)
							*CommunicationStatus = 1;
						currentItem->unlock();
					}
					epicsMutexUnlock(getLock);
					return;
				}
				if (!*FirstStringValue || (**FirstStringValue)->dimSize != (**PvNameArray)->dimSize) {
					*FirstStringValue = (sStringArrayHdl)DSNewHClr(sizeof(size_t) + (**PvNameArray)->dimSize * sizeof(LStrHandle[1]));
					(**FirstStringValue)->dimSize = (**PvNameArray)->dimSize;
				}
				if (!*FirstDoubleValue || (**FirstDoubleValue)->dimSize != (**PvNameArray)->dimSize) {
					*FirstDoubleValue = (sDoubleArrayHdl)DSNewHClr(sizeof(size_t) + (**PvNameArray)->dimSize * sizeof(double[1]));
					(**FirstDoubleValue)->dimSize = (**PvNameArray)->dimSize;
				}
				if (!*DoubleValueArray || (**DoubleValueArray)->dimSizes[0] != (uInt32)(**PvNameArray)->dimSize || (**DoubleValueArray)->dimSizes[1] != maxNumberOfValues) {
					err += NumericArrayResize(fD, 2, (UHandle*)DoubleValueArray, (**PvNameArray)->dimSize * maxNumberOfValues);
					(**DoubleValueArray)->dimSizes[0] = (int32)(**PvNameArray)->dimSize;
					(**DoubleValueArray)->dimSizes[1] = maxNumberOfValues;
				}
			}   // END: RESET LV VARIABLES
			else {
				if (*NoMDEL) {
					wait4value(maxNumberOfValues, PvIndexArray, (time_t)Timeout);
				}
				if (*DoubleValueArray) {
					maxNumberOfValues = (**DoubleValueArray)->dimSizes[1];
				}
				else {
					*CommunicationStatus = 1;
					epicsMutexUnlock(getLock);
					return;
				}
			}
		}
		else {
			if (*DoubleValueArray) {
				maxNumberOfValues = (**DoubleValueArray)->dimSizes[1];
			}
		}
		if (*PvIndexArray && **PvIndexArray) {
			for (uInt32 i = 0; i < (**PvIndexArray)->dimSize; i++) {
				currentItem = (calabItem*)(**PvIndexArray)->elt[i];
				if (!*FirstCall && !currentItem->fieldModified[PvNameArray].load()) {
					if (currentItem->ErrorIO.code)
						*CommunicationStatus = 1;
					continue;
				}
				if (!valid(currentItem)) {
					*CommunicationStatus = 1;
					DbgTime(); CaLabDbgPrintf("Error in getValue: Index array is corrupted.");
					epicsMutexUnlock(getLock);
					return;
				}
				int32 err = currentItem->lock();
				if (err) CaLabDbgPrintf("lock failed on timout in getvalue 2");
				currentResult = &(**ResultArray)->result[i];
				if (filter & out_filter::pviStatusAsString && currentItem->StatusString) {
					if (!currentResult->StatusString || (*currentResult->StatusString)->cnt != (*currentItem->StatusString)->cnt) {
						NumericArrayResize(uB, 1, (UHandle*)&currentResult->StatusString, (*currentItem->StatusString)->cnt);
						(*currentResult->StatusString)->cnt = (*currentItem->StatusString)->cnt;
					}
					memcpy((*currentResult->StatusString)->str, (*currentItem->StatusString)->str, (*currentItem->StatusString)->cnt);
				}
				if (filter & out_filter::pviStatusAsNumber) {
					currentResult->StatusNumber = currentItem->StatusNumber;
				}
				if (filter & out_filter::pviSeverityAsString && currentItem->SeverityString) {
					if (!currentResult->SeverityString || (*currentResult->SeverityString)->cnt != (*currentItem->SeverityString)->cnt) {
						NumericArrayResize(uB, 1, (UHandle*)&currentResult->SeverityString, (*currentItem->SeverityString)->cnt);
						(*currentResult->SeverityString)->cnt = (*currentItem->SeverityString)->cnt;
					}
					memcpy((*currentResult->SeverityString)->str, (*currentItem->SeverityString)->str, (*currentItem->SeverityString)->cnt);
				}
				if (filter & out_filter::pviSeverityAsNumber) {
					currentResult->SeverityNumber = currentItem->SeverityNumber;
				}
				if (filter & out_filter::pviTimestampAsString && currentItem->TimeStampString) {
					if (!currentResult->TimeStampString || (*currentResult->TimeStampString)->cnt != (*currentItem->TimeStampString)->cnt) {
						NumericArrayResize(uB, 1, (UHandle*)&currentResult->TimeStampString, (*currentItem->TimeStampString)->cnt);
						(*currentResult->TimeStampString)->cnt = (*currentItem->TimeStampString)->cnt;
					}
					memcpy((*currentResult->TimeStampString)->str, (*currentItem->TimeStampString)->str, (*currentItem->TimeStampString)->cnt);
				}
				if (filter & out_filter::pviTimestampAsNumber) {
					currentResult->TimeStampNumber = currentItem->TimeStampNumber;
				}
				if (filter & out_filter::pviError && currentItem->ErrorIO.source) {
					if (!currentResult->ErrorIO.source || (*currentResult->ErrorIO.source)->cnt != (*currentItem->ErrorIO.source)->cnt) {
						NumericArrayResize(uB, 1, (UHandle*)&currentResult->ErrorIO.source, (*currentItem->ErrorIO.source)->cnt);
						(*currentResult->ErrorIO.source)->cnt = (*currentItem->ErrorIO.source)->cnt;
					}
					memcpy((*currentResult->ErrorIO.source)->str, (*currentItem->ErrorIO.source)->str, (*currentItem->ErrorIO.source)->cnt);
					currentResult->ErrorIO.code = currentItem->ErrorIO.code;
					currentResult->ErrorIO.status = currentItem->ErrorIO.status;
				}
				if (currentItem->ErrorIO.code)
					*CommunicationStatus = 1;
				if (currentItem->numberOfValues <= 0) {
					currentItem->unlock();
					continue;
				}
				if (filter & out_filter::pviValuesAsString && (!currentResult->StringValueArray || (*currentResult->StringValueArray)->dimSize != currentItem->numberOfValues)) {
					if (currentResult->StringValueArray) {
						DeleteStringArray(currentResult->StringValueArray);
					}
					currentResult->StringValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t) + currentItem->numberOfValues * sizeof(LStrHandle[1]));
					(*currentResult->StringValueArray)->dimSize = currentItem->numberOfValues;
				}
				if (filter & out_filter::pviValuesAsNumber && (!currentResult->ValueNumberArray || (*currentResult->ValueNumberArray)->dimSize != currentItem->numberOfValues)) {
					err += NumericArrayResize(fD, 1, (UHandle*)&currentResult->ValueNumberArray, currentItem->numberOfValues);
					(*currentResult->ValueNumberArray)->dimSize = currentItem->numberOfValues;
				}
				for (uInt32 j = 0; maxNumberOfValues > 0 && j < currentItem->numberOfValues; j++) {
					if (filter & out_filter::pviValuesAsString && !currentItem->stringValueArray) {
						// was connected (user event) and is waiting for reconnect
						epicsMutexUnlock(getLock);
						return;
					}
					else if (filter & out_filter::pviValuesAsNumber && !currentItem->doubleValueArray) {
						// was connected (user event) and is waiting for reconnect
						epicsMutexUnlock(getLock);
						return;
					}
					if (filter & out_filter::pviValuesAsNumber && !(*currentItem->stringValueArray)->elt[j]) {
						if (maxNumberOfValues > 0 && currentItem->numberOfValues != maxNumberOfValues)
							doubleValueArrayIndex += maxNumberOfValues - currentItem->numberOfValues;
						continue;
					}
					if (filter & out_filter::pviValuesAsString && (!currentResult->StringValueArray || !(*currentResult->StringValueArray)->elt[j] || !(*currentItem->stringValueArray)->elt[j] || (*(*currentItem->stringValueArray)->elt[j])->cnt != (*(*currentResult->StringValueArray)->elt[j])->cnt)) {
						if ((*currentItem->stringValueArray)->elt[j]) {
							err += NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->StringValueArray)->elt[j], (*(*currentItem->stringValueArray)->elt[j])->cnt);
							(*(*currentResult->StringValueArray)->elt[j])->cnt = (*(*currentItem->stringValueArray)->elt[j])->cnt;
						}
						else {
							err += NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->StringValueArray)->elt[j], 1);
							(*(*currentResult->StringValueArray)->elt[j])->cnt = 1;
						}
					}
					if (filter & out_filter::pviValuesAsString) {
						if ((*currentItem->stringValueArray)->elt[j]) {
							memcpy((*(*currentResult->StringValueArray)->elt[j])->str, (*(*currentItem->stringValueArray)->elt[j])->str, (*(*currentItem->stringValueArray)->elt[j])->cnt);
						}
						else {
							memcpy((*(*currentResult->StringValueArray)->elt[j])->str, "?", 1);
						}
					}
					if (filter & out_filter::pviValuesAsNumber) {
						(*currentResult->ValueNumberArray)->elt[j] = (*currentItem->doubleValueArray)->elt[j];
					}
					if (filter & out_filter::firstValueAsString && j == 0) {
						if (currentResult->StringValueArray) {
							err += DSCopyHandle(&(**FirstStringValue)->elt[i], (*currentResult->StringValueArray)->elt[j]);
						}
						else {
							err += NumericArrayResize(uB, 1, (UHandle*)&(**FirstStringValue)->elt[i], (*(*currentItem->stringValueArray)->elt[j])->cnt);
							if (err == noErr) {
								memcpy((*(**FirstStringValue)->elt[i])->str, (*(*currentItem->stringValueArray)->elt[j])->str, (*(*currentItem->stringValueArray)->elt[j])->cnt);
								(*(**FirstStringValue)->elt[i])->cnt = (*(*currentItem->stringValueArray)->elt[j])->cnt;
							}
						}
					}
					if (filter & out_filter::firstValueAsNumber && j == 0) {
						(**FirstDoubleValue)->elt[i] = (*currentItem->doubleValueArray)->elt[j];
					}
					if (filter & out_filter::valueArrayAsNumber) {
						if (doubleValueArrayIndex < (**DoubleValueArray)->dimSizes[0] * (**DoubleValueArray)->dimSizes[1]) {
							(**DoubleValueArray)->elt[doubleValueArrayIndex++] = (*currentItem->doubleValueArray)->elt[j];
						}
						else {
							// array with mixed data types must be padded
							doubleValueArrayIndex++;
						}
					}
				}
				if (filter & out_filter::pviElements) {
					currentResult->valueArraySize = currentItem->numberOfValues;
				}
				if (filter & out_filter::valueArrayAsNumber && maxNumberOfValues > 0 && currentItem->numberOfValues != maxNumberOfValues)
					doubleValueArrayIndex += maxNumberOfValues - currentItem->numberOfValues;
				if (filter & out_filter::pviFieldNames && !currentResult->FieldNameArray && FieldNameArray && *FieldNameArray) {
					if (currentResult->FieldNameArray)
						err += DeleteStringArray(currentResult->FieldNameArray);
					currentResult->FieldNameArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t) + (**FieldNameArray)->dimSize * sizeof(LStrHandle[1]));
					(*currentResult->FieldNameArray)->dimSize = (**FieldNameArray)->dimSize;
					for (uInt32 l = 0; l < (**FieldNameArray)->dimSize; l++) {
						NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->FieldNameArray)->elt[l], (*(**FieldNameArray)->elt[l])->cnt);
						(*(*currentResult->FieldNameArray)->elt[l])->cnt = (*(**FieldNameArray)->elt[l])->cnt;
						memcpy((*(*currentResult->FieldNameArray)->elt[l])->str, (*(**FieldNameArray)->elt[l])->str, (*(**FieldNameArray)->elt[l])->cnt);
					}
				}
				if (filter & out_filter::pviFieldValues && (*FirstCall || currentItem->fieldModified[PvNameArray].load()) && (FieldNameArray && *FieldNameArray && **FieldNameArray && (**FieldNameArray)->dimSize)) {
					if (currentResult->FieldValueArray)
						err += DeleteStringArray(currentResult->FieldValueArray);
					currentResult->FieldValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t) + (**FieldNameArray)->dimSize * sizeof(LStrHandle[1]));
					if (currentResult->FieldValueArray && *currentResult->FieldValueArray) {
						(*currentResult->FieldValueArray)->dimSize = (**FieldNameArray)->dimSize;
						for (uInt32 l = 0; l < (**FieldNameArray)->dimSize; l++) {
							propertyMapIterator nameIterator = currentItem->properties.find(std::string((char*)(*(*currentResult->FieldNameArray)->elt[l])->str, (size_t)(*(*currentResult->FieldNameArray)->elt[l])->cnt));
							if (nameIterator != currentItem->properties.end()) {
								std::string fieldValue = nameIterator->second;
								size_t fieldValueLength = fieldValue.size();
								if (fieldValueLength) {
									if (!(*currentResult->FieldValueArray)->elt[l] || ((size_t)(*(*currentResult->FieldValueArray)->elt[l])->cnt != fieldValueLength)) {
										err += NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->FieldValueArray)->elt[l], fieldValueLength);
										(*(*currentResult->FieldValueArray)->elt[l])->cnt = (int32)fieldValueLength;
									}
									memcpy((*(*currentResult->FieldValueArray)->elt[l])->str, fieldValue.c_str(), fieldValueLength);
								}
								else {
									err += NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->FieldValueArray)->elt[l], 0);
									(*(*currentResult->FieldValueArray)->elt[l])->cnt = 0;
								}
							}
							else {
								err += NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->FieldValueArray)->elt[l], 0);
								(*(*currentResult->FieldValueArray)->elt[l])->cnt = 0;
							}
						}
					}
				}
				currentItem->fieldModified[PvNameArray] = false;
				currentItem->unlock();
				if (*NoMDEL) {
					currentItem->disconnect();
				}
			}
		}
	}
	catch (std::exception& ex) {
		DbgTime(); CaLabDbgPrintf("%s", ex.what());
	}
	epicsMutexUnlock(getLock);
}

// creates new LV user event
//    RefNum:            reference number of event
//    ResultArrayHdl:    target item
extern "C" EXPORT void addEvent(LVUserEventRef * RefNum, sResult * ResultPtr) {
	// Don't enter if library terminates
	if (stopped)
		return;
	if (!ResultPtr
		|| DSCheckPtr(ResultPtr) != noErr
		|| !ResultPtr->PVName
		|| DSCheckHandle(ResultPtr->PVName) != noErr) {
		return;
	}
	calabItem* currentItem = 0x0;
	currentItem = globals.add(0x0, ResultPtr->PVName, 0x0, 0);
	int32 err = currentItem->lock();
	if (err) CaLabDbgPrintf("lock failed on timout in addEvent");
	currentItem->RefNum.push_back(*RefNum);
	currentItem->eventResultCluster.push_back(ResultPtr);
	currentItem->unlock();
	currentItem->postEvent();
}

// destroys all eventResultClusters in all PVs associated with an event
//    RefNum:            reference number of event
//    ResultArrayHdl:    target item
extern "C" EXPORT void destroyEvent(LVUserEventRef * RefNum) {
	globals.mapLock.lock_shared();
	for (auto& item : myItems) {
		calabItem* currentItem = item.second;
		if (!valid(currentItem)) {
			CaLabDbgPrintf("destroyevent currentitem invalid");
			continue;
		}
		currentItem->lock();
		std::vector<LVUserEventRef>::iterator ref = currentItem->RefNum.begin();
		std::vector<sResult*>::iterator it = currentItem->eventResultCluster.begin();
		while (ref != currentItem->RefNum.end()) {
			if (*ref == *RefNum) {
				ref = currentItem->RefNum.erase(ref);
				it = currentItem->eventResultCluster.erase(it);
			}
			else {
				ref++;
				it++;
			}
		}
		currentItem->unlock();
	}
	globals.mapLock.unlock_shared();
}

// Write EPICS PV
//    PvNameArray:        array of PV names
//    PvIndexArray:       handle of a array of indexes
//    StringValueArray2D: 2D-array of string values
//    DoubleValueArray2D: 2D-array of double values
//    LongValueArray2D:   2D-array of long values
//    dataType:           type of EPICS channel
//    Timeout:            EPICS event timeout in seconds
//    Synchronous:        true = callback will be used (no interrupt of motor records)
//    ValuesSetInColumns: true = read values set vertically in array
//    ErrorArray:         array of resulting errors
//    Status:             0 = no problem; 1 = any problem occurred
//    FirstCall:          indicator for first call
//    dataTypes
//   ===========
//        # => LabVIEW                          => C++       => EPICS
//        0 => String                           => char[]    => dbr_string_t
//        1 => Single-precision, floating-point => float     => dbr_float_t
//        2 => Double-precision, floating-point => double    => dbr_double_t
//        3 => Byte signed integer              => char      => dbr_char_t
//        4 => Word signed integer              => short     => dbr_short_t
//        5 => Long signed integer              => long      => dbr_long_t
//        6 => Quad signed integer              => long      => dbr_long_t
extern "C" EXPORT void putValue(sStringArrayHdl * PvNameArray, sLongArrayHdl * PvIndexArray, sStringArray2DHdl * StringValueArray2D, sDoubleArray2DHdl * DoubleValueArray2D, sLongArray2DHdl * LongValueArray2D, uInt32 DataType, double Timeout, LVBoolean * Synchronous, sErrorArrayHdl * ErrorArray, LVBoolean * Status, LVBoolean * FirstCall) {
	// Don't enter if library terminates
	if (stopped)
		return;
	epicsMutexLock(putLock);
	try {
		calabItem* currentItem = 0x0;
		uInt32 iNumberOfValueSets = 0;
		uInt32 iValuesPerSet = 0;
		uInt32 maxNumberOfValues = 0;

		// Check handles and pointers
		if (!*PvNameArray || !**PvNameArray || ((uInt32)(**PvNameArray)->dimSize) <= 0) {
			*Status = 1;
			DbgTime(); CaLabDbgPrintf("Missing or corrupt PV name array in caLabPut.");
			epicsMutexUnlock(putLock);
			return;
		}
		if ((*PvIndexArray && **PvIndexArray && (**PvIndexArray)->dimSize != (**PvNameArray)->dimSize))
			*FirstCall = true;
		// calculate data frame
		switch (DataType) {
		case 0:
			if (!*StringValueArray2D) {
				*Status = 1;
				DbgTime(); CaLabDbgPrintf("Missing values to write.");
				epicsMutexUnlock(putLock);
				return;
			}
			if (((*(*(*StringValueArray2D))).dimSizes)[0] == 1 && (**PvNameArray)->dimSize > 1) {
				iNumberOfValueSets = ((**StringValueArray2D)->dimSizes)[1];
				iValuesPerSet = ((**StringValueArray2D)->dimSizes)[0];
			}
			else {
				iNumberOfValueSets = ((**StringValueArray2D)->dimSizes)[0];
				iValuesPerSet = ((**StringValueArray2D)->dimSizes)[1];
			}
			break;
		case 1:
		case 2:
			if (!*DoubleValueArray2D) {
				*Status = 1;
				DbgTime(); CaLabDbgPrintf("Missing values to write.");
				epicsMutexUnlock(putLock);
				return;
			}
			if (((*(*(*DoubleValueArray2D))).dimSizes)[0] == 1 && (**PvNameArray)->dimSize > 1) {
				iNumberOfValueSets = ((**DoubleValueArray2D)->dimSizes)[1];
				iValuesPerSet = ((**DoubleValueArray2D)->dimSizes)[0];
			}
			else {
				iNumberOfValueSets = ((**DoubleValueArray2D)->dimSizes)[0];
				iValuesPerSet = ((**DoubleValueArray2D)->dimSizes)[1];
			}
			break;
		case 3:
		case 4:
		case 5:
		case 6:
			if (!*LongValueArray2D) {
				*Status = 1;
				DbgTime(); CaLabDbgPrintf("Missing values to write.");
				epicsMutexUnlock(putLock);
				return;
			}
			if (((*(*(*LongValueArray2D))).dimSizes)[0] == 1 && (**PvNameArray)->dimSize > 1) {
				iNumberOfValueSets = ((**LongValueArray2D)->dimSizes)[1];
				iValuesPerSet = ((**LongValueArray2D)->dimSizes)[0];
			}
			else {
				iNumberOfValueSets = ((**LongValueArray2D)->dimSizes)[0];
				iValuesPerSet = ((**LongValueArray2D)->dimSizes)[1];
			}
			break;
		default:
			*Status = 1;
			DbgTime(); CaLabDbgPrintf("Unknown data type in value array of caLabPut.");
			epicsMutexUnlock(putLock);
			return;
		}
		if (iNumberOfValueSets > 0) {
			if (!*ErrorArray || iNumberOfValueSets != (**ErrorArray)->dimSize) {
				if (!*ErrorArray) {
					*ErrorArray = (sErrorArrayHdl)DSNewHClr(sizeof(size_t) + iNumberOfValueSets * sizeof(sError[1]));
					(**ErrorArray)->dimSize = iNumberOfValueSets;
				}
				else {
					DSDisposeHandle(*ErrorArray);
					*ErrorArray = (sErrorArrayHdl)DSNewHClr(sizeof(size_t) + iNumberOfValueSets * sizeof(sError[1]));
					(**ErrorArray)->dimSize = iNumberOfValueSets;
				}
			}
		}
		else {
			*Status = 1;
			DbgTime(); CaLabDbgPrintf("Missing values to write.");
			epicsMutexUnlock(putLock);
			return;
		}
		*Status = 0;
		if (bCaLabPolling) *Synchronous = false;
		if (*FirstCall) {
			if (*PvIndexArray && (**PvIndexArray)->dimSize != (**PvNameArray)->dimSize) {
				DSDisposeHandle(*PvIndexArray);
				*PvIndexArray = (sLongArrayHdl)DSNewHClr(sizeof(size_t) + (**PvNameArray)->dimSize * sizeof(uint64_t[1]));
				(**PvIndexArray)->dimSize = (**PvNameArray)->dimSize;
			}
			else if (!*PvIndexArray) {
				*PvIndexArray = (sLongArrayHdl)DSNewHClr(sizeof(size_t) + (**PvNameArray)->dimSize * sizeof(uint64_t[1]));
				(**PvIndexArray)->dimSize = (**PvNameArray)->dimSize;
			}
			for (uInt32 i = 0; i < iNumberOfValueSets && i < (**PvNameArray)->dimSize; i++) {
				currentItem = globals.add(0x0, (**PvNameArray)->elt[i], 0x0, 1);
				(**PvIndexArray)->elt[i] = (uint64_t)currentItem;
				currentItem->isPassive = false;
			}
			wait4value(maxNumberOfValues, PvIndexArray, (time_t)Timeout);
		}
		for (uInt32 row = 0; *PvIndexArray && row < iNumberOfValueSets && row < (**PvNameArray)->dimSize; row++) {
			currentItem = (calabItem*)(**PvIndexArray)->elt[row];
			if (!valid(currentItem)) {
				*Status = 1;
				DbgTime(); CaLabDbgPrintf("Error in putValue: Index array is corrupted.");
				epicsMutexUnlock(putLock);
				return;
			}
			switch (DataType) {
			case 0:
				currentItem->put((void*)StringValueArray2D, DataType, row, iValuesPerSet, &(**ErrorArray)->result[row], Timeout, *Synchronous);
				break;
			case 1:
			case 2:
				currentItem->put((void*)DoubleValueArray2D, DataType, row, iValuesPerSet, &(**ErrorArray)->result[row], Timeout, *Synchronous);
				break;
			case 3:
			case 4:
			case 5:
			case 6:
				currentItem->put((void*)LongValueArray2D, DataType, row, iValuesPerSet, &(**ErrorArray)->result[row], Timeout, *Synchronous);
				break;
			default:
				// Handled in previous switch-case statement
				break;
			}
		}
		if (*Synchronous) {
			time_t stop = time(nullptr) + (time_t)Timeout;
			uInt32 row;
			double wait = .01;
			do {
				epicsThreadSleep(wait);
				for (row = 0; *PvIndexArray && row < iNumberOfValueSets && row < (**PvNameArray)->dimSize; row++) {
					currentItem = (calabItem*)(**PvIndexArray)->elt[row];
					if (!valid(currentItem)) {
						*Status = 1;
						DbgTime(); CaLabDbgPrintf("Error in putValue->sync: Index array is corrupted.");
						epicsMutexUnlock(putLock);
						return;
					}
					if (!currentItem->putReadBack) {						
						break;
					}
					else {
						(**ErrorArray)->result[row].code = currentItem->ErrorIO.code;
						(**ErrorArray)->result[row].status = currentItem->ErrorIO.status;
						size_t stringSize = (*(currentItem->ErrorIO.source))->cnt;
						if (!&(**ErrorArray)->result[row] || (*((**ErrorArray)->result[row].source))->cnt != stringSize) {
							NumericArrayResize(uB, 1, (UHandle*)&(**ErrorArray)->result[row].source, stringSize);
							(*(**ErrorArray)->result[row].source)->cnt = (int32)stringSize;
						}
						memcpy((*(**ErrorArray)->result[row].source)->str, (*currentItem->ErrorIO.source)->str, stringSize);
					}
				}
			} while (row < iNumberOfValueSets && row < (**PvNameArray)->dimSize && time(nullptr) < stop);
			if (time(nullptr) >= stop) {
				DbgTime(); CaLabDbgPrintf("Write values run into timeout.");
			}
		}
		for (uInt32 row = 0; row < iNumberOfValueSets && row < (**PvNameArray)->dimSize; row++) {
			if ((**ErrorArray)->result[row].code)
				*Status = 1;
		}
	}
	catch (std::exception& ex) {
		DbgTime(); CaLabDbgPrintf("%s", ex.what());
	}
	epicsMutexUnlock(putLock);
}

bool comp(std::pair<std::string, calabItem*> a, std::pair<std::string, calabItem*> b) {
	return a.first.compare(b.first);
}

// get context info for EPICS
//   InfoStringArray2D:     container for results
//   InfoStringArraySize:   elements in result container
//   FirstCall:             indicator for first call
extern "C" EXPORT void info(sStringArray2DHdl * InfoStringArray2D, sResultArrayHdl * ResultArray, LVBoolean * FirstCall) {
	try {
		// Don't enter if library terminates
		if (stopped)
			return;
		const ENV_PARAM** ppParam = env_param_list;	// Environment variables of EPICS context
		uInt32				lStringArraySets = 0;		// Number of result arrays
		char** pszNames = 0;				// Name array
		char** pszValues = 0;				// Value array
		uInt32				count = 0;					// Number of environment variables
		const char* pVal = 0;					// Pointer to environment variables of EPICS context
		uInt32				infoArrayDimensions = 2;	// Currently we are using two array as result
		MgErr				err = noErr;				// Error code for debugging
		sResult* currentResult;				// Current LabVIEW result cluster
		calabItem* currentItem;				// Current local item
		epicsMutexLock(getLock);
		while (*ppParam != NULL) {
			lStringArraySets++;
			ppParam++;
		}
		lStringArraySets += 5; // version of library + CALAB_POLLING + CALAB_NODBG + EPICS_VERSION_STRING + ca_version
		pszNames = (char**)malloc(lStringArraySets * sizeof(char*));
		pszValues = (char**)malloc(lStringArraySets * sizeof(char*));
		for (uInt32 i = 0; i < lStringArraySets; i++) {
			pszNames[i] = (char*)malloc(255 * sizeof(char));
			memset(pszNames[i], 0, 255);
			pszValues[i] = (char*)malloc(255 * sizeof(char));
			memset(pszValues[i], 0, 255);
		}
		ppParam = env_param_list;
		count = 0;
		memcpy(pszNames[count], "CA LAB VERSION", strlen("CA LAB VERSION"));
#if  IsOpSystem64Bit
#ifdef _DEBUG
		epicsSnprintf(pszValues[count], 255, "%s DEBUG 64-bit", CALAB_VERSION);
#else
		epicsSnprintf(pszValues[count], 255, "%s 64-bit", CALAB_VERSION);
#endif
#else
#ifdef _DEBUG
		epicsSnprintf(pszValues[count], 255, "%s DEBUG", CALAB_VERSION);
#else
		epicsSnprintf(pszValues[count], 255, "%s", CALAB_VERSION);
#endif
#endif
		count++;
		memcpy(pszNames[count], "COMPILED FOR EPICS BASE", strlen("COMPILED FOR EPICS BASE"));
		memcpy(pszValues[count], EPICS_VERSION_STRING, strlen(EPICS_VERSION_STRING));
		count++;
		memcpy(pszNames[count], "CA PROTOCOL VERSION", strlen("CA PROTOCOL VERSION"));
		memcpy(pszValues[count], ca_version(), strlen(ca_version()));
		count++;
		while (*ppParam != NULL) {
			pVal = envGetConfigParamPtr(*ppParam);
			memcpy(pszNames[count], (*ppParam)->name, strlen((*ppParam)->name));
			if (pVal)
				memcpy(pszValues[count], pVal, strlen(pVal));
			else
				memcpy(pszValues[count], "undefined", strlen("undefined"));
			count++;
			ppParam++;
		}
		memcpy(pszNames[count], "CALAB_POLLING", strlen("CALAB_POLLING"));
		if (getenv("CALAB_POLLING"))
			memcpy(pszValues[count], getenv("CALAB_POLLING"), strlen(getenv("CALAB_POLLING")));
		else
			memcpy(pszValues[count], "undefined", strlen("undefined"));
		count++;
		memcpy(pszNames[count], "CALAB_NODBG", strlen("CALAB_NODBG"));
		if (getenv("CALAB_NODBG"))
			memcpy(pszValues[count], getenv("CALAB_NODBG"), strlen(getenv("CALAB_NODBG")));
		else
			memcpy(pszValues[count], "undefined", strlen("undefined"));
		count++;
		// Create InfoStringArray2D or use previous one
		err += NumericArrayResize(uQ, infoArrayDimensions, (UHandle*)InfoStringArray2D, static_cast<size_t>(infoArrayDimensions) * lStringArraySets);
		(**InfoStringArray2D)->dimSizes[0] = lStringArraySets;
		(**InfoStringArray2D)->dimSizes[1] = infoArrayDimensions;
		uInt32 iNameCounter = 0;
		uInt32 iValueCounter = 0;
		size_t lSize = 0;
		for (uInt32 i = 0; i < (infoArrayDimensions * lStringArraySets); i++) {
			if (i % 2) {
				lSize = strlen(pszValues[iValueCounter]);
				err += NumericArrayResize(uB, 1, (UHandle*)&(**InfoStringArray2D)->elt[i], lSize);
				memcpy((*(**InfoStringArray2D)->elt[i])->str, pszValues[iValueCounter], lSize);
				(*(**InfoStringArray2D)->elt[i])->cnt = (int32)lSize;
				iValueCounter++;
			}
			else {
				lSize = strlen(pszNames[iNameCounter]);
				err += NumericArrayResize(uB, 1, (UHandle*)&(**InfoStringArray2D)->elt[i], lSize);
				memcpy((*(**InfoStringArray2D)->elt[i])->str, pszNames[iNameCounter], lSize);
				(*(**InfoStringArray2D)->elt[i])->cnt = (int32)lSize;
				iNameCounter++;
			}
		}
		if (*ResultArray) {
			for (uInt32 i = 0; i < (**ResultArray)->dimSize; i++) {
				currentResult = &(**ResultArray)->result[i];
				if (currentResult->PVName)
					err += DSDisposeHandle(currentResult->PVName);
				currentResult->PVName = 0x0;
				if (currentResult->StringValueArray)
					err += DeleteStringArray(currentResult->StringValueArray);
				currentResult->StringValueArray = 0x0;
				if (currentResult->ValueNumberArray)
					err += DSDisposeHandle(currentResult->ValueNumberArray);
				currentResult->ValueNumberArray = 0x0;
				if (currentResult->StatusString)
					err += DSDisposeHandle(currentResult->StatusString);
				currentResult->StatusString = 0x0;
				if (currentResult->SeverityString)
					err += DSDisposeHandle(currentResult->SeverityString);
				currentResult->SeverityString = 0x0;
				if (currentResult->TimeStampString)
					err += DSDisposeHandle(currentResult->TimeStampString);
				currentResult->TimeStampString = 0x0;
				if (currentResult->FieldNameArray)
					err += DSDisposeHandle(currentResult->FieldNameArray);
				currentResult->FieldNameArray = 0x0;
				if (currentResult->FieldValueArray)
					err += DSDisposeHandle(currentResult->FieldValueArray);
				currentResult->FieldValueArray = 0x0;
				if (currentResult->ErrorIO.source)
					err += DSDisposeHandle(currentResult->ErrorIO.source);
				currentResult->ErrorIO.source = 0x0;
			}
			if (*ResultArray)
				err += DSDisposeHandle(*ResultArray);
			*ResultArray = 0x0;
		}
		// Compute number of top level items, for sizing ResultArray
		uInt32 iMainPVs = 0;
		globals.mapLock.lock_shared();
		for (auto& iter : myItems) {
			currentItem = iter.second;
			if (currentItem->parent)
				continue;
			iMainPVs++;
		}
		*ResultArray = (sResultArrayHdl)DSNewHClr(sizeof(size_t) + iMainPVs * sizeof(sResult));
		(**ResultArray)->dimSize = iMainPVs;
		// sort map for better usability
		std::map<std::string, calabItem*> sortedMap(myItems.begin(), myItems.end());
		uInt32 iCount = 0;
		for (auto& iter : sortedMap) {
			if (iCount > iMainPVs) {
				break;
			}
			currentItem = iter.second;
			// hide children (field-PVs)
			if (currentItem->parent)
				continue;
			if (!valid(currentItem)) {
				CaLabDbgPrintf("Error in info: Index array is corrupted.");
				break;
			}
			int32 err = currentItem->lock();
			if (err) CaLabDbgPrintf("lock failed on timout in info");
			currentResult = &(**ResultArray)->result[iCount];
			if (currentItem->name) {
				if (!currentResult->PVName || (*currentResult->PVName)->cnt != (*currentItem->name)->cnt) {
					NumericArrayResize(uB, 1, (UHandle*)&currentResult->PVName, (*currentItem->name)->cnt);
					(*currentResult->PVName)->cnt = (*currentItem->name)->cnt;
				}
				memcpy((*currentResult->PVName)->str, (*currentItem->name)->str, (*currentItem->name)->cnt);
			}
			if (currentItem->StatusString) {
				if (!currentResult->StatusString || (*currentResult->StatusString)->cnt != (*currentItem->StatusString)->cnt) {
					NumericArrayResize(uB, 1, (UHandle*)&currentResult->StatusString, (*currentItem->StatusString)->cnt);
					(*currentResult->StatusString)->cnt = (*currentItem->StatusString)->cnt;
				}
				memcpy((*currentResult->StatusString)->str, (*currentItem->StatusString)->str, (*currentItem->StatusString)->cnt);
				currentResult->StatusNumber = currentItem->StatusNumber;
			}
			if (currentItem->SeverityString) {
				if (!currentResult->SeverityString || (*currentResult->SeverityString)->cnt != (*currentItem->SeverityString)->cnt) {
					NumericArrayResize(uB, 1, (UHandle*)&currentResult->SeverityString, (*currentItem->SeverityString)->cnt);
					(*currentResult->SeverityString)->cnt = (*currentItem->SeverityString)->cnt;
				}
				memcpy((*currentResult->SeverityString)->str, (*currentItem->SeverityString)->str, (*currentItem->SeverityString)->cnt);
				currentResult->SeverityNumber = currentItem->SeverityNumber;
			}
			if (currentItem->TimeStampString) {
				if (!currentResult->TimeStampString || (*currentResult->TimeStampString)->cnt != (*currentItem->TimeStampString)->cnt) {
					NumericArrayResize(uB, 1, (UHandle*)&currentResult->TimeStampString, (*currentItem->TimeStampString)->cnt);
					(*currentResult->TimeStampString)->cnt = (*currentItem->TimeStampString)->cnt;
				}
				memcpy((*currentResult->TimeStampString)->str, (*currentItem->TimeStampString)->str, (*currentItem->TimeStampString)->cnt);
				currentResult->TimeStampNumber = currentItem->TimeStampNumber;
			}
			if (currentItem->ErrorIO.source) {
				if (!currentResult->ErrorIO.source || (*currentResult->ErrorIO.source)->cnt != (*currentItem->ErrorIO.source)->cnt) {
					NumericArrayResize(uB, 1, (UHandle*)&currentResult->ErrorIO.source, (*currentItem->ErrorIO.source)->cnt);
					(*currentResult->ErrorIO.source)->cnt = (*currentItem->ErrorIO.source)->cnt;
				}
				memcpy((*currentResult->ErrorIO.source)->str, (*currentItem->ErrorIO.source)->str, (*currentItem->ErrorIO.source)->cnt);
				currentResult->ErrorIO.code = currentItem->ErrorIO.code;
				currentResult->ErrorIO.status = currentItem->ErrorIO.status;
			}
			if (currentItem->numberOfValues <= 0) {
				currentItem->unlock();
				iCount++;
				continue;
			}
			if (!currentResult->StringValueArray) {
				currentResult->StringValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t) + currentItem->numberOfValues * sizeof(LStrHandle[1]));
				(*currentResult->StringValueArray)->dimSize = currentItem->numberOfValues;
				err += NumericArrayResize(fD, 1, (UHandle*)&currentResult->ValueNumberArray, currentItem->numberOfValues);
				(*currentResult->ValueNumberArray)->dimSize = currentItem->numberOfValues;
			}
			for (uInt32 j = 0; j < currentItem->numberOfValues && currentItem->stringValueArray && j < (*currentItem->stringValueArray)->dimSize; j++) {
				int32 sz = LHStrLen((*currentItem->stringValueArray)->elt[j]);
				if (sz != LHStrLen((*currentResult->StringValueArray)->elt[j])) {
					err += NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->StringValueArray)->elt[j], sz);
					LStrLen(*(*currentResult->StringValueArray)->elt[j]) = sz;
				}
				if (sz)
					memcpy(LStrBuf(*(*currentResult->StringValueArray)->elt[j]), LStrBuf(*(*currentItem->stringValueArray)->elt[j]), sz);
				(*currentResult->ValueNumberArray)->elt[j] = (*currentItem->doubleValueArray)->elt[j];
				currentResult->valueArraySize = currentItem->numberOfValues;
			}
			size_t propertiesSize = currentItem->properties.size() + 2; // +1 for the class name and +1 for the native data type
			if (!currentResult->FieldValueArray && propertiesSize) {
				if (currentResult->FieldNameArray)
					err += DeleteStringArray(currentResult->FieldNameArray);
				currentResult->FieldNameArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t) + propertiesSize * sizeof(LStrHandle[1]));
				if (currentResult->FieldNameArray)
					(*currentResult->FieldNameArray)->dimSize = propertiesSize;
				if (currentResult->FieldValueArray)
					err += DeleteStringArray(currentResult->FieldValueArray);
				currentResult->FieldValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t) + propertiesSize * sizeof(LStrHandle[1]));
				if (currentResult->FieldValueArray)
					(*currentResult->FieldValueArray)->dimSize = propertiesSize;
				uInt32 l = 0;
				std::string fieldName;
				size_t fieldNameLength;
				std::string fieldValue;
				size_t fieldValueLength;
				for (const std::pair<const std::string, std::string>& property : currentItem->properties) {
					fieldName = property.first;
					fieldNameLength = fieldName.size();
					fieldValue = property.second;
					fieldValueLength = fieldValue.size();
					if (fieldNameLength && currentResult->FieldNameArray && *currentResult->FieldNameArray) {
						NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->FieldNameArray)->elt[l], fieldNameLength);
						if ((*currentResult->FieldNameArray)->elt[l] && *(*currentResult->FieldNameArray)->elt[l]) {
							(*(*currentResult->FieldNameArray)->elt[l])->cnt = (int32)fieldNameLength;
							if ((*(*currentResult->FieldNameArray)->elt[l]))
								memcpy((*(*currentResult->FieldNameArray)->elt[l])->str, fieldName.c_str(), fieldNameLength);
							else
								(*(*currentResult->FieldNameArray)->elt[l])->str[0] = 0x0;
						}
						if (fieldValueLength && currentResult->FieldValueArray && *currentResult->FieldValueArray) {
							NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->FieldValueArray)->elt[l], fieldValueLength);
							if ((*currentResult->FieldValueArray)->elt[l] && *(*currentResult->FieldValueArray)->elt[l]) {
								(*(*currentResult->FieldValueArray)->elt[l])->cnt = (int32)fieldValueLength;
								if ((*(*currentResult->FieldValueArray)->elt[l]))
									memcpy((*(*currentResult->FieldValueArray)->elt[l])->str, fieldValue.c_str(), fieldValueLength);
								else
									(*(*currentResult->FieldValueArray)->elt[l])->str[0] = 0x0;
							}
						}
					}
					l++;
				}
				fieldName = "Class Name";
				fieldNameLength = fieldName.size();
				fieldValue = currentItem->className;
				fieldValueLength = fieldValue.size();
				if (fieldNameLength && currentResult->FieldNameArray && *currentResult->FieldNameArray && &(*currentResult->FieldNameArray)->elt[l]) {
					NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->FieldNameArray)->elt[l], fieldNameLength);
					if ((*currentResult->FieldNameArray)->elt[l] && *(*currentResult->FieldNameArray)->elt[l]) {
						memcpy((*(*currentResult->FieldNameArray)->elt[l])->str, fieldName.c_str(), fieldNameLength);
						(*(*currentResult->FieldNameArray)->elt[l])->cnt = (int32)fieldNameLength;
					}
				}
				if (fieldValueLength && currentResult->FieldValueArray && *currentResult->FieldValueArray && &(*currentResult->FieldValueArray)->elt[l]) {
					NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->FieldValueArray)->elt[l], fieldValueLength);
					if ((*currentResult->FieldValueArray)->elt[l] && *(*currentResult->FieldValueArray)->elt[l]) {
						memcpy((*(*currentResult->FieldValueArray)->elt[l])->str, fieldValue.c_str(), fieldValueLength);
						(*(*currentResult->FieldValueArray)->elt[l])->cnt = (int32)fieldValueLength;
					}
				}
				l++;
				fieldName = "Native Data Type";
				fieldNameLength = fieldName.size();
				fieldValue = dbr_type_to_text(currentItem->nativeType);
				fieldValueLength = fieldValue.size();
				if (fieldNameLength && currentResult->FieldNameArray && *currentResult->FieldNameArray && &(*currentResult->FieldNameArray)->elt[l]) {
					NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->FieldNameArray)->elt[l], fieldNameLength);
					if ((*currentResult->FieldNameArray)->elt[l] && *(*currentResult->FieldNameArray)->elt[l]) {
						memcpy((*(*currentResult->FieldNameArray)->elt[l])->str, fieldName.c_str(), fieldNameLength);
						(*(*currentResult->FieldNameArray)->elt[l])->cnt = (int32)fieldNameLength;
					}
				}
				if (fieldValueLength && currentResult->FieldValueArray && *currentResult->FieldValueArray && &(*currentResult->FieldValueArray)->elt[l]) {
					NumericArrayResize(uB, 1, (UHandle*)&(*currentResult->FieldValueArray)->elt[l], fieldValueLength);
					if ((*currentResult->FieldValueArray)->elt[l] && *(*currentResult->FieldValueArray)->elt[l]) {
						memcpy((*(*currentResult->FieldValueArray)->elt[l])->str, fieldValue.c_str(), fieldValueLength);
						(*(*currentResult->FieldValueArray)->elt[l])->cnt = (int32)fieldValueLength;
					}
				}
				l++;
			}
			currentItem->unlock();
			iCount++;
		}
		globals.mapLock.unlock_shared();

		for (uInt32 i = 0; i < lStringArraySets; i++) {
			free(pszNames[i]);
			pszNames[i] = 0;
		}
		free(pszNames);
		pszNames = 0;
		for (uInt32 i = 0; i < lStringArraySets; i++) {
			free(pszValues[i]);
			pszValues[i] = 0;
		}
		free(pszValues);
		pszValues = 0;
		epicsMutexUnlock(getLock);
	}
	catch (std::exception& ex) {
		DbgTime(); CaLabDbgPrintf("%s", ex.what());
	}
}

// removes EPICS PVs from event service
//   PvNameArray:     list of PVs, to be removed
//   All:             ignore PvNameArray and disconnect all known data objects
extern "C" EXPORT void disconnectPVs(sStringArrayHdl * PvNameArray, bool All) {
	try {
		// Don't enter if library terminates
		if (stopped)
			return;
		calabItem* currentItem = 0x0;
		if (All) {
			globals.mapLock.lock_shared();
			for (auto& iter : myItems) {
				currentItem = iter.second;
				if (!valid(currentItem)) {
					CaLabDbgPrintf("Error in disconnect all: Index array is corrupted.");
					break;
				}
				currentItem->disconnect();
			}
			globals.mapLock.unlock_shared();
			return;
		}
		if (*PvNameArray && **PvNameArray && ((uInt32)(**PvNameArray)->dimSize) > 0) {
			for (uInt32 i = 0; i < (**PvNameArray)->dimSize; i++) {
				char cName[MAX_NAME_SIZE]{};
				_LToCStrN(*((**PvNameArray)->elt[i]), (CStr)cName, sizeof(cName));
				std::string sName = cName;
				globals.mapLock.lock_shared();
				auto search = myItems.find(sName);
				currentItem = (search == myItems.end()) ? NULL : search->second;
				globals.mapLock.unlock_shared();
				if (!currentItem || !valid(currentItem)) {
					CaLabDbgPrintf("Error in disconnect: Index array is corrupted.");
					break;
				}
				// disconnect field listeners
				if (currentItem->parent) {
					char* pIndicator;
					pIndicator = strchr(currentItem->szName, '.');
					if (pIndicator) {
						int pos = (int)(strlen(currentItem->szName) - (pIndicator - currentItem->szName));
						if (strncmp((const char*)(*currentItem->name)->str, (const char*)(*(**PvNameArray)->elt[i])->str, static_cast<size_t>((*(**PvNameArray)->elt[i])->cnt) - pos) == 0) {
							currentItem->disconnect();
						}
					}
					continue;
				}
				// disconnect value listeners
				if ((*(**PvNameArray)->elt[i])->cnt == (*currentItem->name)->cnt && strncmp((const char*)(*currentItem->name)->str, (const char*)(*(**PvNameArray)->elt[i])->str, (*(**PvNameArray)->elt[i])->cnt) == 0) {
					currentItem->disconnect();
				}
			}
		}
	}
	catch (std::exception& ex) {
		DbgTime(); CaLabDbgPrintf("%s", ex.what());
	}
}

// Channel Access task
// connects / reconnects / disconnects data objects to EPICS
static void caTask(void) {
	try {
		tasks.fetch_add(1);
		uInt32 iResult = ECA_NORMAL;
		calabItem* currentItem;
		uInt32 sizeOfCurrentList = 0;
		uInt32 connectCounter = 0;
		std::chrono::duration<double> diff;
		ca_attach_context(pcac);
		while (!stopped) {
			if (myItems.empty()) {
				epicsThreadSleep(.01);
				continue;
			}
			sizeOfCurrentList = 0;
			connectCounter = 0;
			globals.mapLock.lock_shared();
			for (auto& iter : myItems) {
				currentItem = iter.second;
				if (!valid(currentItem))
					break;
				sizeOfCurrentList++;
				// create channel identifier
				if (!currentItem->caID) {
					int32 err = currentItem->lock();
					if (err) CaLabDbgPrintf("lock failed on timout in caTask 3");
					iResult = ca_create_channel(currentItem->szName, connectionChanged, (void*)currentItem, 20, &currentItem->caID);
					currentItem->unlock();
				}
				else {
					// subscribe channel
					if (!currentItem->isPassive && currentItem->isConnected && currentItem->caID && !currentItem->caEventID) {
						currentItem->nativeType = ca_field_type(currentItem->caID);
						if (currentItem->nativeType >= 0 && currentItem->nativeType < LAST_BUFFER_TYPE) {
							int32 err = currentItem->lock();
							if (err) CaLabDbgPrintf("lock failed on timout in caTask 4");
							iResult = ca_create_subscription(dbf_type_to_DBR_TIME(currentItem->nativeType), UINT_MAX, currentItem->caID, DBE_VALUE | DBE_ALARM, valueChanged, (void*)currentItem, &currentItem->caEventID);
							if (currentItem->nativeType == DBF_ENUM && !currentItem->sEnum.no_str) {
								iResult = ca_create_subscription(DBR_CTRL_ENUM, 1, currentItem->caID, DBE_VALUE, valueChanged, (void*)currentItem, &currentItem->caEnumEventID);
							}
							ca_get_callback(DBR_CLASS_NAME, currentItem->caID, valueChanged, 0x0);
							currentItem->unlock();
						}
						else {
							CaLabDbgPrintfD("%s skip create subscription because invalid native data type (%d)", currentItem->szName, currentItem->nativeType);
						}
					}
					else {
						// reconnect channel
						if (!currentItem->isPassive && !currentItem->isConnected && !currentItem->caEventID) {
							if (currentItem->parent && currentItem->parent->isConnected) {
							}
							else {
								diff = std::chrono::high_resolution_clock::now() - currentItem->timer;
								if (diff.count() > 10) {
									currentItem->timer = std::chrono::high_resolution_clock::now();
									if (currentItem->caID) {
										iResult = ca_clear_channel(currentItem->caID);
										if (iResult == ECA_NORMAL)
											iResult = ca_pend_io(3);
										if (iResult == ECA_NORMAL)
											currentItem->caID = 0x0;
									}
								}
							}
						}
						else {
							connectCounter++;
						}
					}
				}
				// unsubscribe channel
				if (currentItem->isPassive && currentItem->caEventID) {
					iResult = ca_clear_subscription(currentItem->caEventID);
					if (iResult == ECA_NORMAL)
						iResult = ca_pend_io(3);
					if (iResult == ECA_NORMAL)
						currentItem->caEventID = 0x0;
					currentItem->hasValue = false;
				}
			}
			globals.mapLock.unlock_shared();
			ca_flush_io();
			if (iResult != ECA_NORMAL) {
				DbgTime(); CaLabDbgPrintfD("CA Task error (3): %s", ca_message(iResult));
			}
			epicsThreadSleep(.001);
		}
		ca_detach_context();
		tasks.fetch_sub(1);
	}
	catch (std::exception& ex) {
		DbgTime(); CaLabDbgPrintf("%s", ex.what());
	}
}

// Global counter for tests
//   returns count of calls
extern "C" EXPORT uInt32 getCounter() {
	return ++globalCounter;
}

#if defined _WIN32 || defined _WIN64
#else
void loadFunctions() {
	caLibHandle = dlopen("libca.so", RTLD_LAZY);
	comLibHandle = dlopen("libCom.so", RTLD_LAZY);
	ca_array_get = (ca_array_get_t)dlsym(caLibHandle, "ca_array_get");
	ca_add_exception_event = (ca_add_exception_event_t)dlsym(caLibHandle, "ca_add_exception_event");
	ca_array_put = (ca_array_put_t)dlsym(caLibHandle, "ca_array_put");
	ca_array_put_callback = (ca_array_put_callback_t)dlsym(caLibHandle, "ca_array_put_callback");
	ca_attach_context = (ca_attach_context_t)dlsym(caLibHandle, "ca_attach_context");
	ca_clear_channel = (ca_clear_channel_t)dlsym(caLibHandle, "ca_clear_channel");
	ca_clear_subscription = (ca_clear_subscription_t)dlsym(caLibHandle, "ca_clear_subscription");
	ca_context_create = (ca_context_create_t)dlsym(caLibHandle, "ca_context_create");
	ca_context_destroy = (ca_context_destroy_t)dlsym(caLibHandle, "ca_context_destroy");
	ca_create_channel = (ca_create_channel_t)dlsym(caLibHandle, "ca_create_channel");
	ca_create_subscription = (ca_create_subscription_t)dlsym(caLibHandle, "ca_create_subscription");
	ca_array_get_callback = (ca_array_get_callback_t)dlsym(caLibHandle, "ca_array_get_callback");
	ca_current_context = (ca_current_context_t)dlsym(caLibHandle, "ca_current_context");
	ca_detach_context = (ca_detach_context_t)dlsym(caLibHandle, "ca_detach_context");
	ca_element_count = (ca_element_count_t)dlsym(caLibHandle, "ca_element_count");
	ca_flush_io = (ca_flush_io_t)dlsym(caLibHandle, "ca_flush_io");
	ca_field_type = (ca_field_type_t)dlsym(caLibHandle, "ca_field_type");
	ca_message = (ca_message_t)dlsym(caLibHandle, "ca_message");
	ca_name = (ca_name_t)dlsym(caLibHandle, "ca_name");
	ca_pend_io = (ca_pend_io_t)dlsym(caLibHandle, "ca_pend_io");
	ca_puser = (ca_puser_t)dlsym(caLibHandle, "ca_puser");
	ca_state = (ca_state_t)dlsym(caLibHandle, "ca_state");
	ca_version = (ca_version_t)dlsym(caLibHandle, "ca_version");
	dbr_value_offset = (dbr_value_offset_t)dlsym(caLibHandle, "dbr_value_offset");
	envGetConfigParamPtr = (envGetConfigParamPtr_t)dlsym(comLibHandle, "envGetConfigParamPtr");
	env_param_list = (env_param_list_t)dlsym(comLibHandle, "env_param_list");
	epicsMutexDestroy = (epicsMutexDestroy_t)dlsym(comLibHandle, "epicsMutexDestroy");
	epicsMutexLock = (epicsMutexLock_t)dlsym(comLibHandle, "epicsMutexLock");
	epicsMutexOsiCreate = (epicsMutexOsiCreate_t)dlsym(comLibHandle, "epicsMutexOsiCreate");
	epicsMutexUnlock = (epicsMutexUnlock_t)dlsym(comLibHandle, "epicsMutexUnlock");
	epicsMutexTryLock = (epicsMutexTryLock_t)dlsym(comLibHandle, "epicsMutexTryLock");
	epicsSnprintf = (epicsSnprintf_t)dlsym(comLibHandle, "epicsSnprintf");
	epicsThreadCreate = (epicsThreadCreate_t)dlsym(comLibHandle, "epicsThreadCreate");
	epicsThreadGetStackSize = (epicsThreadGetStackSize_t)dlsym(comLibHandle, "epicsThreadGetStackSize");
	epicsThreadSleep = (epicsThreadSleep_t)dlsym(comLibHandle, "epicsThreadSleep");
	epicsTimeToStrftime = (epicsTimeToStrftime_t)dlsym(comLibHandle, "epicsTimeToStrftime");
	dbr_value_size = (dbr_value_size_t)dlsym(caLibHandle, "dbr_value_size");
	dbr_size = (dbr_size_t)dlsym(caLibHandle, "dbr_size");
}
#endif

// prepare the library before first using
void caLabLoad(void) {
	uInt32 iResult = 0;
	if (pcac)
		return;

	if (getenv("CALAB_POLLING")) {
		bCaLabPolling = true;
	}
	else {
		bCaLabPolling = false;
	}
	const char* tmp = getenv("CALAB_NODBG");
	if (tmp) {
		if (strlen(tmp) > 3) {
			pCaLabDbgFile = fopen(tmp, "w");
		}
	}
	signal(SIGABRT, signalHandler);
	signal(SIGFPE, signalHandler);
	signal(SIGILL, signalHandler);
	signal(SIGINT, signalHandler);
	signal(SIGSEGV, signalHandler);
	signal(SIGTERM, signalHandler);
#if defined _WIN32 || defined _WIN64
	iResult = GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, // GET_MODULE_HANDLE_EX_FLAG_PIN to prevent unload lib
		(LPCTSTR)DllMain,
		&libRef);
	if (!iResult) {
		DbgTime(); CaLabDbgPrintf("Error: Could not retrieves a module handle for the myCaLib library.");
		return;
	}
#else
	loadFunctions();
#endif
	stopped = false;
	iResult = ca_context_create(ca_enable_preemptive_callback);
	if (iResult != ECA_NORMAL) {
		DbgTime(); CaLabDbgPrintf("Error: Could not create any instance of Channel Access.");
		return;
	}
	pcac = ca_current_context();
	ca_add_exception_event(exceptionCallback, NULL);
	epicsThreadCreate("caTask",
		epicsThreadPriorityBaseMax,
		epicsThreadGetStackSize(epicsThreadStackBig),
		(EPICSTHREADFUNC)caTask, 0);
#ifdef _DEBUG
	DbgTime(); CaLabDbgPrintfD("load CA Lab OK");
#endif
}

// clean up library before unload
void caLabUnload(void) {
#if defined _WIN32 || defined _WIN64
#else
	//dlclose(caLibHandle); <-- caused memory exceptions in memory manager of LV
	//dlclose(comLibHandle);
#endif
#ifdef _DEBUG
	DbgTime(); CaLabDbgPrintfD("unload CA Lab OK");
#endif
}

// LToCStrN is not available in (very) old LabVIEW versions
uInt32 _LToCStrN(ConstLStrP source, unsigned char* dest, uInt32 destSize) {
	uInt32 resultingSize = (source) ? (source->cnt + 1) : 1;
	if (resultingSize > destSize)
		resultingSize = destSize;
	if (resultingSize > 0) {
		if (source)
			strncpy((char*)dest, (char*)source->str, static_cast<size_t>(resultingSize) - 1);
		dest[resultingSize - 1] = '\0';
	}
	return resultingSize;
}

#ifndef __GNUC__
#pragma warning(pop)
#endif
