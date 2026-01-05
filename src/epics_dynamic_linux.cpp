#include "epics_compat.h"
#include <cstring>
#include <cstdlib>

#if !defined(_WIN32) && !defined(_WIN64)

// -------------------- dlopen handles --------------------
void* caLibHandle  = nullptr;
void* comLibHandle = nullptr;

// -------------------- Alarm strings (fixed extents) --------------------
const char* epicsAlarmSeverityStrings[4] = {"NO_ALARM","MINOR","MAJOR","INVALID"};
const char* epicsAlarmConditionStrings[22] = {
    "NO_ALARM","READ","WRITE","HIHI","HIGH","LOLO","LOW","STATE","COS","COMM",
    "TIMEOUT","HWLIMIT","CALC","SCAN","LINK","SOFT","BAD_SUB","UDF","DISABLE",
    "SIMM","READ_ACCESS","WRITE_ACCESS"
};

// -------------------- Channel Access function pointers --------------------
ca_add_exception_event_t    ca_add_exception_event    = nullptr;
ca_attach_context_t         ca_attach_context         = nullptr;
ca_detach_context_t         ca_detach_context         = nullptr;
ca_array_get_t              ca_array_get              = nullptr;
ca_array_put_t              ca_array_put              = nullptr;
ca_array_put_callback_t     ca_array_put_callback     = nullptr;
ca_clear_channel_t          ca_clear_channel          = nullptr;
ca_clear_subscription_t     ca_clear_subscription     = nullptr;
ca_clear_event_t            ca_clear_event            = nullptr;
ca_context_create_t         ca_context_create         = nullptr;
ca_context_destroy_t        ca_context_destroy        = nullptr;
ca_create_channel_t         ca_create_channel         = nullptr;
ca_create_subscription_t    ca_create_subscription    = nullptr;
ca_array_get_callback_t     ca_array_get_callback     = nullptr;
ca_current_context_t        ca_current_context        = nullptr;
ca_pend_io_t                ca_pend_io                = nullptr;
ca_pend_event_t             ca_pend_event             = nullptr;
ca_flush_io_t               ca_flush_io               = nullptr;
ca_field_type_t             ca_field_type             = nullptr;
ca_element_count_t          ca_element_count          = nullptr;
ca_message_t                ca_message                = nullptr;
ca_name_t                   ca_name                   = nullptr;
ca_puser_t                  ca_puser                  = nullptr;
ca_state_t                  ca_state                  = nullptr;
ca_version_t                ca_version                = nullptr;
dbr_value_offset_t          dbr_value_offset          = nullptr;
dbr_size_t                  dbr_size                  = nullptr;
dbr_value_size_t            dbr_value_size            = nullptr;

// Optional CA text table
const char* const*          dbr_text                  = nullptr;
unsigned short              dbr_text_dim              = 0;

// -------------------- libCom symbols --------------------
envGetConfigParamPtr_t      envGetConfigParamPtr    = nullptr;
epicsMutexOsiCreate_t       epicsMutexOsiCreate     = nullptr;
epicsMutexDestroy_t         epicsMutexDestroy       = nullptr;
epicsMutexLock_t            epicsMutexLock          = nullptr;
epicsMutexTryLock_t         epicsMutexTryLock       = nullptr;
epicsMutexUnlock_t          epicsMutexUnlock        = nullptr;
epicsThreadCreate_t         epicsThreadCreate       = nullptr;
epicsThreadGetStackSize_t   epicsThreadGetStackSize = nullptr;
epicsThreadSleep_t          epicsThreadSleep        = nullptr;
epicsSnprintf_t             epicsSnprintf           = nullptr;
epicsTimeToStrftime_t       epicsTimeToStrftime     = nullptr;
env_param_list_t            env_param_list          = nullptr;

// Fallback dbr_text if the symbol is not exported by libca on this platform.
static const char* k_dbr_text_fallback[] = {
    "DBR_STRING","DBR_SHORT","DBR_FLOAT","DBR_ENUM","DBR_CHAR","DBR_LONG","DBR_DOUBLE",
    "DBR_TIME_STRING","DBR_TIME_SHORT","DBR_TIME_FLOAT","DBR_TIME_ENUM",
    "DBR_TIME_CHAR","DBR_TIME_LONG","DBR_TIME_DOUBLE"
};

void loadFunctions(void) {
    if (!caLibHandle)   caLibHandle  = dlopen("libca.so",  RTLD_LAZY);
    if (!comLibHandle)  comLibHandle = dlopen("libCom.so", RTLD_LAZY);

    // Channel Access
    ca_array_get            = (ca_array_get_t)dlsym(caLibHandle, "ca_array_get");
    ca_add_exception_event  = (ca_add_exception_event_t)dlsym(caLibHandle, "ca_add_exception_event");
    ca_array_put            = (ca_array_put_t)dlsym(caLibHandle, "ca_array_put");
    ca_array_put_callback   = (ca_array_put_callback_t)dlsym(caLibHandle, "ca_array_put_callback");
    ca_attach_context       = (ca_attach_context_t)dlsym(caLibHandle, "ca_attach_context");
    ca_detach_context       = (ca_detach_context_t)dlsym(caLibHandle, "ca_detach_context");
    ca_clear_channel        = (ca_clear_channel_t)dlsym(caLibHandle, "ca_clear_channel");
    ca_clear_subscription   = (ca_clear_subscription_t)dlsym(caLibHandle, "ca_clear_subscription");
    ca_clear_event          = (ca_clear_event_t)dlsym(caLibHandle, "ca_clear_event");
    ca_context_create       = (ca_context_create_t)dlsym(caLibHandle, "ca_context_create");
    ca_context_destroy      = (ca_context_destroy_t)dlsym(caLibHandle, "ca_context_destroy");
    ca_create_channel       = (ca_create_channel_t)dlsym(caLibHandle, "ca_create_channel");
    ca_create_subscription  = (ca_create_subscription_t)dlsym(caLibHandle, "ca_create_subscription");
    ca_array_get_callback   = (ca_array_get_callback_t)dlsym(caLibHandle, "ca_array_get_callback");
    ca_current_context      = (ca_current_context_t)dlsym(caLibHandle, "ca_current_context");
    ca_pend_io              = (ca_pend_io_t)dlsym(caLibHandle, "ca_pend_io");
    ca_pend_event           = (ca_pend_event_t)dlsym(caLibHandle, "ca_pend_event");
    ca_flush_io             = (ca_flush_io_t)dlsym(caLibHandle, "ca_flush_io");
    ca_field_type           = (ca_field_type_t)dlsym(caLibHandle, "ca_field_type");
    ca_element_count        = (ca_element_count_t)dlsym(caLibHandle, "ca_element_count");
    ca_message              = (ca_message_t)dlsym(caLibHandle, "ca_message");
    ca_name                 = (ca_name_t)dlsym(caLibHandle, "ca_name");
    ca_puser                = (ca_puser_t)dlsym(caLibHandle, "ca_puser");
    ca_state                = (ca_state_t)dlsym(caLibHandle, "ca_state");
    ca_version              = (ca_version_t)dlsym(caLibHandle, "ca_version");
    dbr_value_offset        = (dbr_value_offset_t)dlsym(caLibHandle, "dbr_value_offset");
    dbr_size                = (dbr_size_t)dlsym(caLibHandle, "dbr_size");
    dbr_value_size          = (dbr_value_size_t)dlsym(caLibHandle, "dbr_value_size");

    // Optional dbr_text table
    dbr_text = (const char* const*)dlsym(caLibHandle, "dbr_text");
    const unsigned short* dimPtr = (const unsigned short*)dlsym(caLibHandle, "dbr_text_dim");
    if (dbr_text && dimPtr) {
        dbr_text_dim = *dimPtr;
    } else {
        dbr_text = k_dbr_text_fallback;
        dbr_text_dim = (unsigned short)(sizeof(k_dbr_text_fallback)/sizeof(k_dbr_text_fallback[0]));
    }

    // libCom
    envGetConfigParamPtr  = (envGetConfigParamPtr_t)dlsym(comLibHandle, "envGetConfigParamPtr");
    env_param_list        = (env_param_list_t)dlsym(comLibHandle, "env_param_list");
    epicsMutexOsiCreate   = (epicsMutexOsiCreate_t)dlsym(comLibHandle, "epicsMutexOsiCreate");
    epicsMutexDestroy     = (epicsMutexDestroy_t)dlsym(comLibHandle, "epicsMutexDestroy");
    epicsMutexLock        = (epicsMutexLock_t)dlsym(comLibHandle, "epicsMutexLock");
    epicsMutexTryLock     = (epicsMutexTryLock_t)dlsym(comLibHandle, "epicsMutexTryLock");
    epicsMutexUnlock      = (epicsMutexUnlock_t)dlsym(comLibHandle, "epicsMutexUnlock");
    epicsThreadCreate     = (epicsThreadCreate_t)dlsym(comLibHandle, "epicsThreadCreate");
    epicsThreadGetStackSize= (epicsThreadGetStackSize_t)dlsym(comLibHandle, "epicsThreadGetStackSize");
    epicsThreadSleep      = (epicsThreadSleep_t)dlsym(comLibHandle, "epicsThreadSleep");
    epicsSnprintf         = (epicsSnprintf_t)dlsym(comLibHandle, "epicsSnprintf");
    epicsTimeToStrftime   = (epicsTimeToStrftime_t)dlsym(comLibHandle, "epicsTimeToStrftime");
}

static ca_client_context* pcac = nullptr;

void caLabLoad(void) {
    if (pcac) return;
    loadFunctions();
}

void caLabUnload(void) {
    //dlclose(caLibHandle); <-- caused memory exceptions in memory manager of LV
	//dlclose(comLibHandle);
}

#endif // Linux
