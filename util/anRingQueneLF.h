#ifndef __AN_RINGQUENE_LOCKFREE_H__
#define __AN_RINGQUENE_LOCKFREE_H__

typedef void* ANRingQueneLF_h;

ANRingQueneLF_h ANRingQLF_Create(int nMemberCount, int UserDataSize);

int ANRingQLF_Destory(ANRingQueneLF_h* handle);
void* ANRingQLF_Header_Get(ANRingQueneLF_h handle);
int ANRingQLF_Header_Pop(ANRingQueneLF_h handle);
void* ANRingQLF_Back_Get(ANRingQueneLF_h handle);
int ANRingQLF_Back_Push(ANRingQueneLF_h handle);
int ANRingQLF_Get_Count(ANRingQueneLF_h handle);

void* ANRingQLF_Header_Get_Readonly(ANRingQueneLF_h handle);


#endif

