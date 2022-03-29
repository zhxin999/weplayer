#ifndef _OBJECT_DEF_H_
#define _OBJECT_DEF_H_

#include <stdint.h>
#include <time.h>

#define OBJ_ID_LEN          256


typedef enum _OBJ_TYPE_e
{
    OBJTYPE_Base = 0,

    OBJTYPE_max
}OBJ_TYPE_e;

typedef struct _OBJ_BASE
{
    //指向下一个
    struct _OBJ_BASE* pNext;
    //定义类型名字，比如car, factory...
    OBJ_TYPE_e Type;
    //唯一编号
    char szDescID[OBJ_ID_LEN];

    //Alias    
    char szDescAlias[OBJ_ID_LEN];

}OBJ_BASE_t;

#define OBJ_BASE_MEMBER         OBJ_BASE_t base






#endif
