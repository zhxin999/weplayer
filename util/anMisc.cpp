
#include "anLogs.h"
#include "anMisc.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif


struct _anOSMutex
{
#ifdef _WIN32
   CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
};

struct _anOSTask
{
#ifdef _WIN32
   HANDLE task_handle;
   anOS_Task_t task;

#else
   pthread_t task_handle;
#endif
   void* usrctx;
   int is_created;
};


void anOS_Mutex_Create(ANOSMutex_t* handle)
{
   struct _anOSMutex * mutex = (struct _anOSMutex *)calloc(1, sizeof(struct _anOSMutex));
   if (mutex == NULL)
      return;

#ifdef _WIN32
   InitializeCriticalSection(&(mutex->lock));
#else
   pthread_mutex_init(&(mutex->lock), NULL);
#endif   
   *handle = mutex;
}
void anOS_Mutex_Lock(ANOSMutex_t handle)
{
   struct _anOSMutex * mutex = (struct _anOSMutex *)handle;
   if (mutex == NULL)
      return;

#ifdef _WIN32
   EnterCriticalSection(&(mutex->lock));
#else
   pthread_mutex_lock(&(mutex->lock));
#endif   
}
void anOS_Mutex_unLock(ANOSMutex_t handle)
{
   struct _anOSMutex * mutex = (struct _anOSMutex *)handle;
   if (mutex == NULL)
      return;

#ifdef _WIN32
   LeaveCriticalSection(&(mutex->lock));
#else
   pthread_mutex_unlock(&(mutex->lock));
#endif   
}
void anOS_Mutex_Destory(ANOSMutex_t handle)
{
   struct _anOSMutex * mutex = (struct _anOSMutex *)handle;

   if (mutex == NULL)
      return;

#if _WIN32
      DeleteCriticalSection(&(mutex->lock));
#else
      pthread_mutex_destroy(&(mutex->lock));
#endif

   free(mutex);
}
#if _WIN32
DWORD WINAPI anOS_Task_Adaptor(LPVOID param)
{
   struct _anOSTask* task_obj = (struct _anOSTask *)param;

   if (task_obj)
   {
      task_obj->task(task_obj->usrctx);
   }

   return 0;
}
#endif

int anOS_Task_Create(ANOSTask_Obj_t* handle, void* param, anOS_Task_t task, void* usrctx)
{
   struct _anOSTask * task_obj;

   if (handle == NULL)
   {
      return -1;
   }
   
   task_obj = (struct _anOSTask *)calloc(1, sizeof(struct _anOSTask));

   task_obj->usrctx = usrctx;
 
#ifdef _WIN32
   task_obj->task = task;
   task_obj->task_handle = CreateThread((LPSECURITY_ATTRIBUTES )param, 0, anOS_Task_Adaptor, task_obj, 0, NULL);
#else
   pthread_create(&(task_obj->task_handle), (const pthread_attr_t *)param, task, usrctx);
#endif
   if (task_obj->task_handle == NULL)
   {
      MessageError("[%s:%d] can't create thread:\n", __FUNCTION__, __LINE__);
      return -1;
   }

   task_obj->is_created = 1;

   *handle = task_obj;

   return 0;
}

void anOS_Task_Destory(ANOSTask_Obj_t handle, int timeout)
{
   struct _anOSTask * task_obj = (struct _anOSTask *)handle;

   if (task_obj == NULL)
   {
      return;
   }

   if (task_obj->is_created)
   {
#if _WIN32
         WaitForSingleObject(task_obj->task_handle, INFINITE);
         CloseHandle(task_obj->task_handle);
#else
         pthread_join(task_obj->task_handle, NULL);
#endif
   }

   task_obj->is_created = 0;

   free(task_obj);
   
}
int anOS_Task_IsCreated(ANOSTask_Obj_t handle)
{
   struct _anOSTask * task_obj = (struct _anOSTask *)handle;
   if (task_obj == NULL)
   {
      return 0;
   }

   return task_obj->is_created;
}





