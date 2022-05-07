
#include "anLogs.h"
#include "anMisc.h"
#ifdef _WIN32
#include <Windows.h>
#include <stdint.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

void anmisc_data_exchange_long(long* Target, long Value)
{
#ifdef _WIN32
   InterlockedExchange(Target, Value);
#else
    __sync_lock_test_and_set(Target, Value);
#endif
}

void anmisc_data_exchange_int64(int64_t* Target, int64_t Value)
{
#ifdef _WIN32
   InterlockedExchange((uint64_t *)Target, (uint64_t)Value);
#else
    __sync_lock_test_and_set(Target, Value);
#endif
}


void anmisc_data_exchange_add_long(long* Target, long Value)
{
#ifdef _WIN32
   InterlockedExchangeAdd(Target, Value);
#else
    __sync_fetch_and_add(Target, Value);
#endif
}


