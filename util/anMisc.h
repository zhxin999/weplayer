#ifndef __AN_MISC_H__
#define __AN_MISC_H__

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#include <stdint.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

typedef void* (*anOS_Task_t)(void* param);

typedef void*  ANOSMutex_t;
typedef void*   ANOSTask_Obj_t;

void anOS_Mutex_Create(ANOSMutex_t* handle);
void anOS_Mutex_Lock(ANOSMutex_t handle);
void anOS_Mutex_unLock(ANOSMutex_t handle);
void anOS_Mutex_Destory(ANOSMutex_t handle);

int  anOS_Task_Create(ANOSTask_Obj_t* handle, void* param, anOS_Task_t task, void* usrctx);
void anOS_Task_Destory(ANOSTask_Obj_t handle, int timeout);
int  anOS_Task_IsCreated(ANOSTask_Obj_t handle);


inline void anmisc_data_exchange_long(long* Target, long Value)
{
#ifdef _WIN32
   InterlockedExchange(Target, Value);
#else
    __sync_lock_test_and_set(Target, Value);
#endif
}

inline void anmisc_data_exchange_int64(int64_t* Target, int64_t Value)
{
#ifdef _WIN32
   InterlockedExchange((uint64_t *)Target, (uint64_t)Value);
#else
    __sync_lock_test_and_set(Target, Value);
#endif
}


inline void anmisc_data_exchange_add_long(long* Target, long Value)
{
#ifdef _WIN32
   InterlockedExchangeAdd(Target, Value);
#else
    __sync_fetch_and_add(Target, Value);
#endif
}


#endif

