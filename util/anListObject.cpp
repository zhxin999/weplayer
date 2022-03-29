#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "anListObject.h"
#include "anLogs.h"


const char* GetObjTypeString(OBJ_TYPE_e ObjType)
{
   switch (ObjType)
   {
      case OBJTYPE_Base:
         return "Base";
      default:
         break;
   }

   MessageError("[%s:%d] unknown type %d\n", __FUNCTION__, __LINE__, (int)ObjType);
   return "Unknown";
}

void ObjBase_RemoveFromList(OBJ_BASE_t** pHead, OBJ_BASE_t* pItem)
{
   OBJ_BASE_t* pCurNode;
   OBJ_BASE_t* pNextNode;

   pCurNode = *pHead;
   //删除表头的情况
   if (pCurNode == pItem)
   {
      *pHead =  pCurNode->pNext;
      return;
   }
   
   while (pCurNode)
   {
      pNextNode = pCurNode->pNext;

      //如果下一个就是要删除的
      if (pNextNode == pItem)
      {
         //调整接点
         pCurNode->pNext = pNextNode->pNext;
         return ;
         
      }
      
      pCurNode = pNextNode;
   }

   return;
}

//添加新元素
int ObjBase_List_AppendEnd(OBJ_BASE_t** pHead, OBJ_BASE_t* pNewItem)
{
   OBJ_BASE_t* pCurItem = *pHead;

   if (pNewItem == NULL)
   {
      return -1;
   }

   if (pCurItem == NULL)
   {
      *pHead = pNewItem;
      pNewItem->pNext = NULL;
      return 0;
   }

   while (pCurItem->pNext)
   {
      pCurItem = pCurItem->pNext;
   }

   pCurItem->pNext = pNewItem;
   pNewItem->pNext = NULL;

   return 0;
}


//添加新元素
int ObjBase_List_InsertHead(OBJ_BASE_t** pHead, OBJ_BASE_t* pNewItem)
{
   OBJ_BASE_t* pCurItem = *pHead;

   if (pNewItem == NULL)
   {
      return -1;
   }

   if (pCurItem == NULL)
   {
      *pHead = pNewItem;
      pNewItem->pNext = NULL;
      return 0;
   }

 
   pNewItem->pNext = pCurItem->pNext;
   pCurItem->pNext = pNewItem;

   return 0;
}

int ObjBase_List_AppendAfter(OBJ_BASE_t* pComparedItem, OBJ_BASE_t* pNewItem)
{
   if (pNewItem == NULL)
   {
      return -1;
   }
   if (pComparedItem == NULL)
   {
      return -1;
   }
   
   pNewItem->pNext = pComparedItem->pNext;
   pComparedItem->pNext = pNewItem;
   
   return 0;
}


//获取表头
OBJ_BASE_t* ObjBase_List_GetHead(OBJ_BASE_t** pHead)
{
   OBJ_BASE_t* pCurItem = *pHead;

   if (pCurItem == NULL)
   {
      return NULL;
   }

   *pHead = pCurItem->pNext;

   return pCurItem;
}
OBJ_BASE_t* ObjBase_GetNext(OBJ_BASE_t* pCurNode)
{
   if (pCurNode)
     return pCurNode->pNext;

   return NULL;
}

void ObjBase_FreeList(OBJ_BASE_t* pListHead)
{
   OBJ_BASE_t* pCurNode;
   OBJ_BASE_t* pNextNode;

   pCurNode = pListHead;
   while (pCurNode)
   {
      pNextNode = pCurNode->pNext;

      free(pCurNode);
      
      pCurNode = pNextNode;
   }

   return;
}

void ObjBase_FreeList_UserData(OBJ_BASE_t* pListHead, OBJ_USRDATA_FREE pUsrDataFreeCb)
{
   OBJ_BASE_t* pCurNode;
   OBJ_BASE_t* pNextNode;

   pCurNode = pListHead;
   while (pCurNode)
   {
      pNextNode = pCurNode->pNext;

      if (pUsrDataFreeCb)
      {
         pUsrDataFreeCb(pNextNode);
      }

      free(pCurNode);
      
      pCurNode = pNextNode;
   }

   return;
}


OBJ_BASE_t* ObjBase_GetNItem(OBJ_BASE_t* pCurNode, int N)
{
   int Cnt = 0;
   OBJ_BASE_t* pObjBase = NULL;

   pObjBase = pCurNode;

   while(pObjBase)
   {
      if (Cnt == N)
         return pObjBase;
      
      Cnt++;
      pObjBase = pObjBase->pNext;
   }

   return NULL;
}

int ObjBase_GetCount(OBJ_BASE_t* pCurNode)
{
   int Cnt = 0;
   OBJ_BASE_t* pObjBase = NULL;

   pObjBase = pCurNode;

   while(pObjBase)
   {
      Cnt++;
      pObjBase = pObjBase->pNext;
   }

   return Cnt;
}



int ObjBase_SetType(OBJ_BASE_t* Obj, OBJ_TYPE_e Type)
{
   Obj->Type = Type;

   return 0;
}
OBJ_TYPE_e ObjBase_GetType(OBJ_BASE_t* Obj)
{
   return Obj->Type;
}


char* ObjBase_GetID(OBJ_BASE_t* Obj)
{
   return Obj->szDescID;
}

char* ObjBase_GetAlias(OBJ_BASE_t* Obj)
{
   return Obj->szDescAlias;
}

OBJ_BASE_t* ObjBase_FindByID(OBJ_BASE_t* pCurNode, const char* szID)
{
   OBJ_BASE_t* pObjBase = NULL;

   pObjBase = pCurNode;

   while(pObjBase)
   {
      if (strcmp(pObjBase->szDescID, szID) == 0)
      {
         //
         return pObjBase;
      }
      
      pObjBase = pObjBase->pNext;
   }

   return NULL;
}

int ObjBase_List_Sort_Ascend(OBJ_BASE_t** pHead, OBJ_SORT_COMPARE compare)
{
   OBJ_BASE_t* pHeadTmp = *pHead;
   *pHead = NULL;

   if (compare == NULL)
   {
      return -1;
   }

   while (pHeadTmp)
   {
      //找到队列里面最小的，拍到前面
      OBJ_BASE_t* pObjBase = NULL;
      OBJ_BASE_t* pObjMin = NULL;
      
      pObjBase = pHeadTmp;
      
      pObjMin = pObjBase;
      while(pObjBase)
      {
         if (compare(pObjBase, pObjMin) < 0)
         {
            pObjMin = pObjBase;
         }
         
         pObjBase = pObjBase->pNext;
      }

      //从原来队列里面删除最小的
      ObjBase_RemoveFromList(&pHeadTmp, pObjMin);
      
      ObjBase_List_AppendEnd(pHead, pObjMin);
   }


   return 0;
}


