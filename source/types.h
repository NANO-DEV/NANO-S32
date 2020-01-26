// Data Types - Intel x86 architecture

#ifndef _TYPES_H
#define _TYPES_H

// Default data types
typedef unsigned int   uint;
typedef unsigned int   size_t;

// Size specific data types
typedef signed char    int8_t;
typedef signed short   int16_t;
typedef signed int     int32_t;

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

// Complex types
typedef struct time_t {
  uint  year;
  uint  month;
  uint  day;
  uint  hour;
  uint  minute;
  uint  second;
 } time_t;

// Null pointer
#define NULL (0)

typedef _Bool bool;
#define FALSE 0
#define TRUE  1

#endif // _TYPES_H
