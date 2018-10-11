#ifndef _SDMBNLocal_H_
#define _SDMBNLocal_H_

#include <SDMBN.h>

///// DEBUGGING MACROS ///////////////////////////////////////////////////////
#define SDMBN_SERIALIZE

#define SDMBN_INFO
#define SDMBN_ERROR

#ifdef SDMBN_SERIALIZE
    #define SERIALIZE_PRINT(...) printf(__VA_ARGS__); printf("\n");
#else
    #define SERIALIZE_PRINT(...)
#endif

#ifdef SDMBN_INFO
    #define INFO_PRINT(...) printf(__VA_ARGS__); printf("\n");
    #ifndef SDMBN_ERROR
        #define SDMBN_ERROR
    #endif
#else
    #define INFO_PRINT(...)
#endif

#ifdef SDMBN_ERROR
    #define ERROR_PRINT(...) printf(__VA_ARGS__); printf("\n");
#else
    #define ERROR_PRINT(...)
#endif

///// FUNCTION PROTOTYPES ////////////////////////////////////////////////////
int local_get_perflow(PerflowKey *key, int id, int raiseEvents);
int local_put_perflow(int hashkey, PerflowKey *key, char *state);
int local_get_shared(int id);
int local_put_shared(int hashkey, char *state);
int local_get_multiflow(PerflowKey *key, int id);
int local_put_multiflow(int hashkey, PerflowKey *key, char *state);
int local_del_multiflow(PerflowKey *key, int id, int force);
int local_del_perflow(PerflowKey *key, int id);

//LFR --
int local_premove_state(PerflowKey *key, int id, int raiseEvents);
#endif
