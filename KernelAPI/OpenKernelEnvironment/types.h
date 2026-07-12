#ifndef TYPES_H
#define TYPES_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef unsigned int size_t;
typedef uint8_t bool_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

typedef float float32_t;
typedef double float64_t;

typedef unsigned long uintptr_t;
typedef signed long intptr_t;

typedef volatile uint32_t atomic32_t;
typedef volatile uint64_t atomic64_t;

#define true 1
#define false 0

#define NULL 0

#endif