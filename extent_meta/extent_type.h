#ifndef _EXTENT_TYPE_H_
#define _EXTENT_TYPE_H_

typedef unsigned long  uint64_t;
typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;

// start from 1
// 0 == nullptr
#define Ext_Nat_Entry_ID_Type   uint16_t
#define Ext_Node_ID_Type        Ext_Nat_Entry_ID_Type

#endif
