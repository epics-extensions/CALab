#pragma once

// CALab EPICS compatibility:
// Windows: link against EPICS headers/libraries at build time.
// Linux:   bind at runtime via dlopen/dlsym. Do NOT include EPICS headers.

#if defined(_WIN32) || defined(_WIN64)

  #define NOMINMAX
  #include <windows.h>
  #include <cadef.h>
  #include <db_access.h>
  #include <alarmString.h>
  #include <envDefs.h>
  #include <epicsStdio.h>
  #include <epicsVersion.h>
  #ifdef __cplusplus
    inline const char* ca_message_safe(long status) {
      const char* msg = ca_message(status);
      return msg ? msg : "Unknown EPICS CA error";
    }
  #endif
  #define EXPORT __declspec(dllexport)

#else  // -------------------- Linux dynamic binding --------------------

  #include <stdint.h>
  #include <stddef.h>
  #include <dlfcn.h>
  #include <limits.h>
  #include <epicsVersion.h>

  #define EXPORT
  #define __stdcall
  typedef unsigned long DWORD;
  #ifndef DLL_PROCESS_DETACH
  #define DLL_PROCESS_DETACH 0
  #endif

  // dlopen handles
  extern void* caLibHandle;
  extern void* comLibHandle;

  // Auto init/teardown
  void __attribute__((constructor)) caLabLoad(void);
  void __attribute__((destructor))  caLabUnload(void);
  void loadFunctions(void);

  // -------------------- Alarms --------------------
  enum epicsAlarmSeverity { epicsSevNone=0, epicsSevMinor=1, epicsSevMajor=2, epicsSevInvalid=3 };
  enum epicsAlarmCondition {
      epicsAlarmNone=0, epicsAlarmRead=1, epicsAlarmWrite=2, epicsAlarmHiHi=3, epicsAlarmHigh=4, epicsAlarmLoLo=5,
      epicsAlarmLow=6, epicsAlarmState=7, epicsAlarmCOS=8, epicsAlarmComm=9, epicsAlarmTimeout=10, epicsAlarmHwLimit=11,
      epicsAlarmCalc=12, epicsAlarmScan=13, epicsAlarmLink=14, epicsAlarmSoft=15, epicsAlarmBadSub=16, epicsAlarmUDF=17,
      epicsAlarmDisable=18, epicsAlarmSimm=19, epicsAlarmReadAccess=20, epicsAlarmWriteAccess=21
  };

  // Provide fixed-size externs so sizeof(...) works in users of this header.
  extern const char* epicsAlarmSeverityStrings[4];
  extern const char* epicsAlarmConditionStrings[22];
  #define alarmSeverityString epicsAlarmSeverityStrings
  #define alarmStatusString   epicsAlarmConditionStrings

  // -------------------- Constants/Types --------------------
  #define MAX_STRING_SIZE            40
  #define MAX_ENUM_STATES            16
  #define MAX_ENUM_STRING_SIZE       26
  #define epicsThreadPriorityBaseMax 91
  #define NO_ALARM                   0

  #define CA_K_ERROR    2
  #define CA_K_SUCCESS  1
  #define CA_K_WARNING  0
  #define CA_V_MSG_NO   0x03
  #define CA_V_SEVERITY 0x00
  #define CA_M_MSG_NO   0x0000FFF8
  #define CA_M_SEVERITY 0x00000007
  #define CA_INSERT_MSG_NO(code)   (((code)<< CA_V_MSG_NO)&CA_M_MSG_NO)
  #define CA_INSERT_SEVERITY(code) (((code)<< CA_V_SEVERITY)& CA_M_SEVERITY)

  #define DEFMSG(SEVERITY,NUMBER)\
  (CA_INSERT_MSG_NO(NUMBER) | CA_INSERT_SEVERITY(SEVERITY))

  #define ECA_NORMAL          DEFMSG(CA_K_SUCCESS,    0) /* success */
  #define ECA_MAXIOC          DEFMSG(CA_K_ERROR,      1) /* defunct */
  #define ECA_UKNHOST         DEFMSG(CA_K_ERROR,      2) /* defunct */
  #define ECA_UKNSERV         DEFMSG(CA_K_ERROR,      3) /* defunct */
  #define ECA_SOCK            DEFMSG(CA_K_ERROR,      4) /* defunct */
  #define ECA_CONN            DEFMSG(CA_K_WARNING,    5) /* defunct */
  #define ECA_ALLOCMEM        DEFMSG(CA_K_WARNING,    6)
  #define ECA_UKNCHAN         DEFMSG(CA_K_WARNING,    7) /* defunct */
  #define ECA_UKNFIELD        DEFMSG(CA_K_WARNING,    8) /* defunct */
  #define ECA_TOLARGE         DEFMSG(CA_K_WARNING,    9)
  #define ECA_TIMEOUT         DEFMSG(CA_K_WARNING,   10)
  #define ECA_NOSUPPORT       DEFMSG(CA_K_WARNING,   11) /* defunct */
  #define ECA_STRTOBIG        DEFMSG(CA_K_WARNING,   12) /* defunct */
  #define ECA_DISCONNCHID     DEFMSG(CA_K_ERROR,     13) /* defunct */
  #define ECA_BADTYPE         DEFMSG(CA_K_ERROR,     14)
  #define ECA_CHIDNOTFND      DEFMSG(CA_K_INFO,      15) /* defunct */
  #define ECA_CHIDRETRY       DEFMSG(CA_K_INFO,      16) /* defunct */
  #define ECA_INTERNAL        DEFMSG(CA_K_FATAL,     17)
  #define ECA_DBLCLFAIL       DEFMSG(CA_K_WARNING,   18) /* defunct */
  #define ECA_GETFAIL         DEFMSG(CA_K_WARNING,   19)
  #define ECA_PUTFAIL         DEFMSG(CA_K_WARNING,   20)
  #define ECA_ADDFAIL         DEFMSG(CA_K_WARNING,   21) /* defunct */
  #define ECA_BADCOUNT        DEFMSG(CA_K_WARNING,   22)
  #define ECA_BADSTR          DEFMSG(CA_K_ERROR,     23)
  #define ECA_DISCONN         DEFMSG(CA_K_WARNING,   24)
  #define ECA_DBLCHNL         DEFMSG(CA_K_WARNING,   25)
  #define ECA_EVDISALLOW      DEFMSG(CA_K_ERROR,     26)
  #define ECA_BUILDGET        DEFMSG(CA_K_WARNING,   27) /* defunct */
  #define ECA_NEEDSFP         DEFMSG(CA_K_WARNING,   28) /* defunct */
  #define ECA_OVEVFAIL        DEFMSG(CA_K_WARNING,   29) /* defunct */
  #define ECA_BADMONID        DEFMSG(CA_K_ERROR,     30)
  #define ECA_NEWADDR         DEFMSG(CA_K_WARNING,   31) /* defunct */
  #define ECA_NEWCONN         DEFMSG(CA_K_INFO,      32) /* defunct */
  #define ECA_NOCACTX         DEFMSG(CA_K_WARNING,   33) /* defunct */
  #define ECA_DEFUNCT         DEFMSG(CA_K_FATAL,     34) /* defunct */
  #define ECA_EMPTYSTR        DEFMSG(CA_K_WARNING,   35) /* defunct */
  #define ECA_NOREPEATER      DEFMSG(CA_K_WARNING,   36) /* defunct */
  #define ECA_NOCHANMSG       DEFMSG(CA_K_WARNING,   37) /* defunct */
  #define ECA_DLCKREST        DEFMSG(CA_K_WARNING,   38) /* defunct */
  #define ECA_SERVBEHIND      DEFMSG(CA_K_WARNING,   39) /* defunct */
  #define ECA_NOCAST          DEFMSG(CA_K_WARNING,   40) /* defunct */
  #define ECA_BADMASK         DEFMSG(CA_K_ERROR,     41)
  #define ECA_IODONE          DEFMSG(CA_K_INFO,      42)
  #define ECA_IOINPROGRESS    DEFMSG(CA_K_INFO,      43)
  #define ECA_BADSYNCGRP      DEFMSG(CA_K_ERROR,     44)
  #define ECA_PUTCBINPROG     DEFMSG(CA_K_ERROR,     45)
  #define ECA_NORDACCESS      DEFMSG(CA_K_WARNING,   46)
  #define ECA_NOWTACCESS      DEFMSG(CA_K_WARNING,   47)
  #define ECA_ANACHRONISM     DEFMSG(CA_K_ERROR,     48)
  #define ECA_NOSEARCHADDR    DEFMSG(CA_K_WARNING,   49)
  #define ECA_NOCONVERT       DEFMSG(CA_K_WARNING,   50)
  #define ECA_BADCHID         DEFMSG(CA_K_ERROR,     51)
  #define ECA_BADFUNCPTR      DEFMSG(CA_K_ERROR,     52)
  #define ECA_ISATTACHED      DEFMSG(CA_K_WARNING,   53)
  #define ECA_UNAVAILINSERV   DEFMSG(CA_K_WARNING,   54)
  #define ECA_CHANDESTROY     DEFMSG(CA_K_WARNING,   55)
  #define ECA_BADPRIORITY     DEFMSG(CA_K_ERROR,     56)
  #define ECA_NOTTHREADED     DEFMSG(CA_K_ERROR,     57)
  #define ECA_16KARRAYCLIENT  DEFMSG(CA_K_WARNING,   58)
  #define ECA_CONNSEQTMO      DEFMSG(CA_K_WARNING,   59)
  #define ECA_UNRESPTMO       DEFMSG(CA_K_WARNING,   60)

  #define CA_OP_CONN_UP   6
  #define CA_OP_CONN_DOWN 7

  #define DBE_VALUE (1<<0)
  #define DBE_ALARM (1<<2)

  typedef struct ca_client_context ca_client_context;
  typedef double      ca_real;
  typedef long        chtype;
  typedef unsigned    capri;
  typedef struct oldChannelNotify* chid;
  typedef struct oldSubscription*  evid;
  typedef chid        chanId;

  typedef uint8_t  epicsUInt8;
  typedef int16_t  epicsInt16;
  typedef uint16_t epicsUInt16;
  typedef int32_t  epicsInt32;
  typedef uint32_t epicsUInt32;
  typedef float    epicsFloat32;
  typedef double   epicsFloat64;

  typedef epicsUInt16 dbr_enum_t;
  typedef epicsInt16  dbr_short_t;
  typedef epicsUInt8  dbr_char_t;
  typedef epicsInt32  dbr_long_t;
  typedef epicsFloat32 dbr_float_t;
  typedef epicsFloat64 dbr_double_t;
  typedef char        epicsOldString[MAX_STRING_SIZE];
  typedef epicsOldString dbr_string_t;

  typedef enum { ca_disable_preemptive_callback, ca_enable_preemptive_callback } ca_preemptive_callback_select;
  typedef enum { epicsThreadStackSmall, epicsThreadStackMedium, epicsThreadStackBig } epicsThreadStackSizeClass;
  typedef enum { cs_never_conn, cs_prev_conn, cs_conn, cs_closed } channel_state;

  typedef struct epicsTimeStamp { epicsUInt32 secPastEpoch; epicsUInt32 nsec; } epicsTimeStamp;

  // Do not define ENV_PARAM body to avoid conflicts with envDefs.h if included elsewhere.
  struct envParam {
    const char* name;
    const char* pdflt;
  };
  typedef struct envParam ENV_PARAM;

  // EPICS status base type
  typedef long epicsStatus;

  struct connection_handler_args { chanId chid; long op; };
  typedef struct event_handler_args {
      void* usr; chanId chid; long type; long count; const void* dbr; int status;
  } evargs;
  typedef void caCh(struct connection_handler_args args);
  typedef void caEventCallBackFunc(struct event_handler_args);

  // DBF/DBR base
  #define DBF_STRING 0
  #define DBF_SHORT  1
  #define DBF_FLOAT  2
  #define DBF_ENUM   3
  #define DBF_CHAR   4
  #define DBF_LONG   5
  #define DBF_DOUBLE 6

  #define DBR_STRING DBF_STRING
  #define DBR_SHORT  DBF_SHORT
  #define DBR_FLOAT  DBF_FLOAT
  #define DBR_ENUM   DBF_ENUM
  #define DBR_CHAR   DBF_CHAR
  #define DBR_LONG   DBF_LONG
  #define DBR_DOUBLE DBF_DOUBLE

  #define DBR_TIME_STRING 14
  #define DBR_TIME_SHORT  15
  #define DBR_TIME_FLOAT  16
  #define DBR_TIME_ENUM   17
  #define DBR_TIME_CHAR   18
  #define DBR_TIME_LONG   19
  #define DBR_TIME_DOUBLE 20
  #define DBR_CTRL_ENUM   31

  #define ca_poll() ca_pend_event(1e-12)

  struct dbr_gr_enum { dbr_short_t status; dbr_short_t severity; dbr_short_t no_str;
      char strs[MAX_ENUM_STATES][MAX_ENUM_STRING_SIZE]; dbr_enum_t value; };
  struct dbr_ctrl_enum { dbr_short_t status; dbr_short_t severity; dbr_short_t no_str;
      char strs[MAX_ENUM_STATES][MAX_ENUM_STRING_SIZE]; dbr_enum_t value; };

  struct dbr_time_string { dbr_short_t status; dbr_short_t severity; epicsTimeStamp stamp; dbr_short_t pad; dbr_string_t value; };
  struct dbr_time_short  { dbr_short_t status; dbr_short_t severity; epicsTimeStamp stamp; dbr_short_t pad; dbr_short_t value; };
  struct dbr_time_float  { dbr_short_t status; dbr_short_t severity; epicsTimeStamp stamp; dbr_long_t  pad; dbr_float_t value; };
  struct dbr_time_enum   { dbr_short_t status; dbr_short_t severity; epicsTimeStamp stamp; dbr_short_t pad; dbr_enum_t  value; };
  struct dbr_time_char   { dbr_short_t status; dbr_short_t severity; epicsTimeStamp stamp; dbr_long_t  pad; dbr_char_t  value; };
  struct dbr_time_long   { dbr_short_t status; dbr_short_t severity; epicsTimeStamp stamp; dbr_long_t  pad; dbr_long_t  value; };
  struct dbr_time_double { dbr_short_t status; dbr_short_t severity; epicsTimeStamp stamp; dbr_long_t  pad; dbr_double_t value; };

  static inline int dbf_type_to_DBR_TIME(int dbf) {
      switch (dbf) {
          case DBF_STRING: return DBR_TIME_STRING;
          case DBF_SHORT:  return DBR_TIME_SHORT;
          case DBF_FLOAT:  return DBR_TIME_FLOAT;
          case DBF_ENUM:   return DBR_TIME_ENUM;
          case DBF_CHAR:   return DBR_TIME_CHAR;
          case DBF_LONG:   return DBR_TIME_LONG;
          case DBF_DOUBLE: return DBR_TIME_DOUBLE;
          default:         return -1;
      }
  }

  #define CA_PRIORITY_DEFAULT 0u

  // -------------------- Function pointer types --------------------
  typedef struct ca_client_context* (*ca_current_context_t)();
  typedef int   (*ca_add_exception_event_t)(void*, void*);
  typedef int   (*ca_array_get_t)(chtype, unsigned long, chid, void*);
  typedef int   (*ca_array_put_t)(chtype, unsigned long, chid, const void*);
  typedef int   (*ca_array_put_callback_t)(chtype, unsigned long, chid, const void*, caEventCallBackFunc*, void*);
  typedef int   (*ca_attach_context_t)(struct ca_client_context*);
  typedef int   (*ca_detach_context_t)();
  typedef int   (*ca_clear_channel_t)(chid);
  typedef int   (*ca_clear_subscription_t)(evid);
  typedef int   (*ca_clear_event_t)(evid);
  typedef int   (*ca_context_create_t)(ca_preemptive_callback_select);
  typedef void  (*ca_context_destroy_t)(void);
  typedef int   (*ca_create_channel_t)(const char*, caCh*, void*, capri, chid*);
  typedef int   (*ca_create_subscription_t)(chtype, unsigned long, chid, long, caEventCallBackFunc*, void*, evid*);
  typedef int   (*ca_array_get_callback_t)(chtype, unsigned long, chid, caEventCallBackFunc*, void*);
  typedef int   (*ca_pend_io_t)(ca_real);
  typedef int   (*ca_pend_event_t)(ca_real);
  typedef int   (*ca_flush_io_t)();
  typedef void  (*ca_poll_t)(void);
  typedef short (*ca_field_type_t)(chid);
  typedef unsigned long (*ca_element_count_t)(chid);
  typedef void* (*ca_puser_t)(chid);
  typedef const char* (*ca_message_t)(long);
  typedef const char* (*ca_name_t)(chid);
  typedef const char* (*ca_version_t)(void);
  typedef channel_state (*ca_state_t)(chid);

  typedef const unsigned short* dbr_value_offset_t;
  typedef const unsigned short* dbr_size_t;
  typedef const unsigned short* dbr_value_size_t;

  // Optional CA text table (if exported by libca). Fallback provided in linux .cpp.
  extern const char* const* dbr_text;
  extern unsigned short      dbr_text_dim;

  typedef void* epicsMutexId;
  typedef void* epicsThreadId;
  typedef enum { epicsMutexLockOK, epicsMutexLockTimeout, epicsMutexLockError } epicsMutexLockStatus;

  typedef epicsMutexId (*epicsMutexOsiCreate_t)(const char*, int);
  typedef void (*epicsMutexDestroy_t)(epicsMutexId);
  typedef epicsMutexLockStatus (*epicsMutexLock_t)(epicsMutexId);
  typedef epicsMutexLockStatus (*epicsMutexTryLock_t)(epicsMutexId);
  typedef void (*epicsMutexUnlock_t)(epicsMutexId);
  typedef epicsThreadId (*epicsThreadCreate_t)(const char*, unsigned int, unsigned int, void (*)(void*), void*);
  typedef unsigned int (*epicsThreadGetStackSize_t)(epicsThreadStackSizeClass);
  typedef void (*epicsThreadSleep_t)(double);
  #define EPICS_PRINTF_STYLE(f,a) __attribute__((format(__printf__,f,a)))
  typedef int(*epicsSnprintf_t)(char* str, size_t size, const char* format, ...) __attribute__((format(__printf__, 3, 4)));
  typedef size_t (*epicsTimeToStrftime_t)(char*, size_t, const char*, const epicsTimeStamp*);
  typedef const char* (*envGetConfigParamPtr_t)(const ENV_PARAM*);
  typedef const ENV_PARAM* (*env_param_list_t);

  // -------------------- Extern function pointers --------------------
  extern ca_add_exception_event_t    ca_add_exception_event;
  extern ca_attach_context_t         ca_attach_context;
  extern ca_detach_context_t         ca_detach_context;
  extern ca_array_get_t              ca_array_get;
  extern ca_array_put_t              ca_array_put;
  extern ca_array_put_callback_t     ca_array_put_callback;
  extern ca_clear_channel_t          ca_clear_channel;
  extern ca_clear_subscription_t     ca_clear_subscription;
  extern ca_clear_event_t            ca_clear_event;
  extern ca_context_create_t         ca_context_create;
  extern ca_context_destroy_t        ca_context_destroy;
  extern ca_create_channel_t         ca_create_channel;
  extern ca_create_subscription_t    ca_create_subscription;
  extern ca_array_get_callback_t     ca_array_get_callback;
  extern ca_current_context_t        ca_current_context;
  extern ca_pend_io_t                ca_pend_io;
  extern ca_pend_event_t             ca_pend_event;
  extern ca_flush_io_t               ca_flush_io;
  extern ca_field_type_t             ca_field_type;
  extern ca_element_count_t          ca_element_count;
  extern ca_message_t                ca_message;
  extern ca_name_t                   ca_name;
  extern ca_puser_t                  ca_puser;
  extern ca_state_t                  ca_state;
  extern ca_version_t                ca_version;
  extern dbr_value_offset_t          dbr_value_offset;
  extern dbr_size_t                  dbr_size;
  extern dbr_value_size_t            dbr_value_size;

  // libCom function pointers (prefixed to avoid header collisions)
  extern envGetConfigParamPtr_t      envGetConfigParamPtr;
  extern epicsMutexOsiCreate_t       epicsMutexOsiCreate;
  extern epicsMutexDestroy_t         epicsMutexDestroy;
  extern epicsMutexLock_t            epicsMutexLock;
  extern epicsMutexTryLock_t         epicsMutexTryLock;
  extern epicsMutexUnlock_t          epicsMutexUnlock;
  extern epicsThreadCreate_t         epicsThreadCreate;
  extern epicsThreadGetStackSize_t   epicsThreadGetStackSize;
  extern epicsThreadSleep_t          epicsThreadSleep;
  extern epicsSnprintf_t             epicsSnprintf;
  extern epicsTimeToStrftime_t       epicsTimeToStrftime;
  extern env_param_list_t            env_param_list;
  #ifdef __cplusplus
    inline const char* ca_message_safe(long status) {
      if (ca_message) {
          const char* msg = ca_message(status);
          return msg ? msg : "Unknown EPICS CA error";
      }
      return "Unknown EPICS CA error (ca_message not available)";
    }
  #endif
  // Convenience
  #define ca_get(type, chan, p)   ca_array_get((type), 1u, (chan), (p))
  #define ca_put(type, chan, p)   ca_array_put((type), 1u, (chan), (p))
  #define ca_get_callback(type, chan, pFunc, pArg) ca_array_get_callback((type), 1u, (chan), (pFunc), (pArg))
  #define dbr_value_ptr(PDBR, DBR_TYPE) ((void*)(((char*)(PDBR))+dbr_value_offset[DBR_TYPE]))

#endif  // Linux
