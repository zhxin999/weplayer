#ifndef _OBJECT_BASIC_H_
#define _OBJECT_BASIC_H_

#include <stdint.h>
#include "anListObject_def.h"

typedef int(*OBJ_SORT_COMPARE)(OBJ_BASE_t* pNode1, OBJ_BASE_t* pNode2);

typedef void (*OBJ_USRDATA_FREE)(OBJ_BASE_t* pNode);


const char* GetObjTypeString(OBJ_TYPE_e ObjType);

char* ObjBase_GetID(OBJ_BASE_t* Obj);
char* ObjBase_GetAlias(OBJ_BASE_t* Obj);
int ObjBase_SetType(OBJ_BASE_t* Obj, OBJ_TYPE_e Type);
OBJ_TYPE_e ObjBase_GetType(OBJ_BASE_t* Obj);


int ObjBase_List_AppendEnd(OBJ_BASE_t** pHead, OBJ_BASE_t* pNewItem);
int ObjBase_List_AppendAfter(OBJ_BASE_t* pComparedItem, OBJ_BASE_t* pNewItem);
int ObjBase_List_InsertHead(OBJ_BASE_t** pHead, OBJ_BASE_t* pNewItem);

//获取表头
OBJ_BASE_t* ObjBase_List_GetHead(OBJ_BASE_t** pHead);
void ObjBase_FreeList(OBJ_BASE_t* pListHead);
void ObjBase_FreeList_UserData(OBJ_BASE_t* pListHead, OBJ_USRDATA_FREE pUsrDataFreeCb);

void ObjBase_RemoveFromList(OBJ_BASE_t** pHead, OBJ_BASE_t* pItem);


OBJ_BASE_t* ObjBase_GetNext(OBJ_BASE_t* pCurNode);
int ObjBase_GetCount(OBJ_BASE_t* pCurNode);
OBJ_BASE_t* ObjBase_GetNItem(OBJ_BASE_t* pCurNode, int N);
OBJ_BASE_t* ObjBase_FindByID(OBJ_BASE_t* pCurNode, const char* szID);
int ObjBase_List_Sort_Ascend(OBJ_BASE_t** pHead, OBJ_SORT_COMPARE compare);

#endif
