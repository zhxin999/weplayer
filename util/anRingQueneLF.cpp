#include <stdlib.h>
#include <string.h>

#include "anLogs.h"
#include "anMisc.h"
#include "anRingQueneLF.h"

#define ANRQ_ITEM_WRITE_AVAIL     0x00
#define ANRQ_ITEM_READ_AVAIL      0x01
#define ANRQ_ITEM_READ_ING        0x02
#define ANRQ_ITEM_WRITE_ING       0x03

typedef struct _ANRQItem
{
   unsigned char flag;
}ANRQItem_t;

typedef struct _ANRingQueneLF
{
   int MemberCount;
   int UserDataSize;
   
   volatile int ReadPos;
   volatile int WritePos;

   //搞一个原子操作，计算数字,提高效率
   long dataCount;
   
   //放到后面，只分配一遍内存
   unsigned char* ptrQArray;
} ANRingQueneLF_t;


void *anRingQGetData(ANRingQueneLF_t* pRQLF, int Pos)
{
   void *ptrRQItem = NULL;
   
   if (pRQLF->ptrQArray && Pos < pRQLF->MemberCount)
   {
      ptrRQItem = pRQLF->ptrQArray + Pos * (sizeof(ANRQItem_t) + pRQLF->UserDataSize);
   }
   
   return ptrRQItem;
}


ANRingQueneLF_h ANRingQLF_Create(int nMemberCount, int UserDataSize)
{
   int QArraySize;
   ANRingQueneLF_t* pNewRQLF;

   QArraySize = nMemberCount * (UserDataSize + sizeof(ANRQItem_t));

   pNewRQLF = (ANRingQueneLF_t *)calloc(1, sizeof(ANRingQueneLF_t) + QArraySize);
   if (pNewRQLF == NULL)
   {
      MessageError("[%s:%d] Failed to alloc ANRingQueneLF_t\n", __FUNCTION__, __LINE__);
      goto exit;
   }

   pNewRQLF->UserDataSize = UserDataSize;
   pNewRQLF->MemberCount = nMemberCount;
   //初始化各个地方的值
   pNewRQLF->ptrQArray = ((unsigned char* )pNewRQLF) + sizeof(ANRingQueneLF_t);

exit:   

   return (ANRingQueneLF_h)pNewRQLF;
}

int ANRingQLF_Destory(ANRingQueneLF_h* handle)
{
   ANRingQueneLF_t** pRQLF = (ANRingQueneLF_t**)handle;

   if (pRQLF)
   {
      free(*pRQLF);
      *pRQLF = NULL;      
      return 0;
   }
   
   return -1;
}

void* ANRingQLF_Header_Get(ANRingQueneLF_h handle)
{
   void *ptrUsrData = NULL;
   ANRQItem_t * itemData = 0;
   ANRingQueneLF_t* pRQLF = (ANRingQueneLF_t*)handle;

   if (pRQLF == NULL)
   {
      MessageError("[%s:%d] NULL Object\n", __FUNCTION__, __LINE__);
      return NULL;
   }
   
   itemData = (ANRQItem_t *)anRingQGetData(pRQLF, pRQLF->ReadPos);
   if (itemData->flag == ANRQ_ITEM_READ_AVAIL)
   {
       ptrUsrData = (unsigned char *)itemData + sizeof(ANRQItem_t);
       itemData->flag = ANRQ_ITEM_READ_ING;
   }
   
   return ptrUsrData;
   
}

void* ANRingQLF_Header_Get_Readonly(ANRingQueneLF_h handle)
{
   void *ptrUsrData = NULL;
   ANRQItem_t * itemData = 0;
   ANRingQueneLF_t* pRQLF = (ANRingQueneLF_t*)handle;

   if (pRQLF == NULL)
   {
      MessageError("[%s:%d] NULL Object\n", __FUNCTION__, __LINE__);
      return NULL;
   }
   
   itemData = (ANRQItem_t *)anRingQGetData(pRQLF, pRQLF->ReadPos);
   if (itemData->flag == ANRQ_ITEM_READ_AVAIL)
   {
       ptrUsrData = (unsigned char *)itemData + sizeof(ANRQItem_t);
   }
   
   return ptrUsrData;
   
}


int ANRingQLF_Header_Pop(ANRingQueneLF_h handle)
{
   int ret = -1;
   ANRQItem_t * itemData = 0;
   ANRingQueneLF_t* pRQLF = (ANRingQueneLF_t*)handle;

   if (pRQLF == NULL)
   {
      MessageError("[%s:%d] NULL Object\n", __FUNCTION__, __LINE__);
      return -1;
   }
   
   itemData = (ANRQItem_t *)anRingQGetData(pRQLF, pRQLF->ReadPos);
   if (itemData->flag == ANRQ_ITEM_READ_ING)
   {
       itemData->flag = ANRQ_ITEM_WRITE_AVAIL;
       pRQLF->ReadPos = (pRQLF->ReadPos + 1)% pRQLF->MemberCount;
       anmisc_data_exchange_add_long(&(pRQLF->dataCount), -1);
       ret = 0;
   }
   else
   {
      MessageError("[%s:%d] not in reading\n", __FUNCTION__, __LINE__);
   }
   
   return ret;
}

void* ANRingQLF_Back_Get(ANRingQueneLF_h handle)
{
   void *ptrUsrData = NULL;
   ANRQItem_t * itemData = 0;
   ANRingQueneLF_t* pRQLF = (ANRingQueneLF_t*)handle;

   if (pRQLF == NULL)
   {
      MessageError("[%s:%d] NULL Object\n", __FUNCTION__, __LINE__);
      return NULL;
   }
   
   itemData = (ANRQItem_t *)anRingQGetData(pRQLF, pRQLF->WritePos);
   if (itemData->flag == ANRQ_ITEM_WRITE_AVAIL)
   {
       ptrUsrData = (unsigned char *)itemData + sizeof(ANRQItem_t);
       itemData->flag = ANRQ_ITEM_WRITE_ING;
   }
   
   return ptrUsrData;
}

int ANRingQLF_Back_Push(ANRingQueneLF_h handle)
{
   int ret = -1;
   ANRQItem_t * itemData = 0;
   ANRingQueneLF_t* pRQLF = (ANRingQueneLF_t*)handle;

   if (pRQLF == NULL)
   {
      MessageError("[%s:%d] NULL Object\n", __FUNCTION__, __LINE__);
      return -1;
   }
   
   itemData = (ANRQItem_t *)anRingQGetData(pRQLF, pRQLF->WritePos);
   if (itemData->flag == ANRQ_ITEM_WRITE_ING)
   {
       itemData->flag = ANRQ_ITEM_READ_AVAIL;
       pRQLF->WritePos = (pRQLF->WritePos + 1)% pRQLF->MemberCount;
       anmisc_data_exchange_add_long(&(pRQLF->dataCount), 1);
       ret = 0;
   }
   else
   {
      MessageError("[%s:%d] not in reading\n", __FUNCTION__, __LINE__);
   }
   
   return ret;
}

int ANRingQLF_Get_Count(ANRingQueneLF_h handle)
{
   long count = 0 ;
   ANRingQueneLF_t* pRQLF = (ANRingQueneLF_t*)handle;

   anmisc_data_exchange_long(&count, pRQLF->dataCount);

   return (int)count;
}


