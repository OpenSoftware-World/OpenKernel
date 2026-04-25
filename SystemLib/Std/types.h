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

#define NULL ((void *)0)

#define ALIGN_UP(x, align)   (((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

#define PACKED __attribute__((packed))
#define INLINE __attribute__((always_inline)) inline
#define NORETURN __attribute__((noreturn))

#define BIT(n) (1U << (n))
#define SET_BIT(x, n) ((x) |= BIT(n))
#define CLEAR_BIT(x, n) ((x) &= ~BIT(n))
#define TOGGLE_BIT(x, n) ((x) ^= BIT(n))
#define CHECK_BIT(x, n) ((x) & BIT(n))

#define UINT8_MAX   ((uint8_t)0xFF)
#define UINT16_MAX  ((uint16_t)0xFFFF)
#define UINT32_MAX  ((uint32_t)0xFFFFFFFF)
#define UINT64_MAX  ((uint64_t)0xFFFFFFFFFFFFFFFFULL)

#define OFFSET_OF(type, member) ((size_t)&(((type *)0)->member))
#define CONTAINER_OF(ptr, type, member) ((type *)((char *)(ptr) - OFFSET_OF(type, member)))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, min, max) (MAX((min), MIN((x), (max))))

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#define UNUSED(x) (void)(x)
#define STATIC_ASSERT(cond, msg) typedef char static_assert_##msg[(cond)?1:-1]

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define MEMORY_BARRIER() __asm__ volatile("" ::: "memory")

#define SWAP(a, b) do { \
    typeof(a) _tmp = (a); \
    (a) = (b); \
    (b) = _tmp; \
} while(0)

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)

#define CONTAINER_OF_SAFE(ptr, type, member) \
    (ptr ? CONTAINER_OF(ptr, type, member) : NULL)

#define BIT_MASK(len) ((1U << (len)) - 1)
#define BIT_RANGE(x, start, len) (((x) >> (start)) & BIT_MASK(len))

#define READ_ONCE(x) (*(volatile typeof(x) *)&(x))
#define WRITE_ONCE(x, val) (*(volatile typeof(x) *)&(x) = (val))

#define BUG() __builtin_trap()

#define TYPE_CHECK(x, y) (!!(sizeof((x) == (y))))

#define ASSERT_ALIGNED(x, a) STATIC_ASSERT(((a) & ((a) - 1)) == 0, alignment_must_be_power_of_two)

#define PAGE_SIZE 4096
#define PAGE_ALIGN(x) ALIGN_UP(x, PAGE_SIZE)
#define PAGE_ALIGN_DOWN(x) ALIGN_DOWN(x, PAGE_SIZE)

#define KB(x) ((x) * 1024ULL)
#define MB(x) (KB(x) * 1024ULL)
#define GB(x) (MB(x) * 1024ULL)

#define PTR_ADD(ptr, val) ((void *)((uint8_t *)(ptr) + (val)))
#define PTR_SUB(ptr, val) ((void *)((uint8_t *)(ptr) - (val)))

#define IS_NULL(ptr) ((ptr) == NULL)
#define BUILD_BUG_ON(cond) ((void)sizeof(char[1 - 2*!!(cond)]))

#define PREFETCH(x) __builtin_prefetch(x)
#define CPU_RELAX() __asm__ volatile("pause" ::: "memory")

#define TYPES_MODULE_NAME "Types Library"
#define TYPES_MODULE_DESC "Types Library for OpenKernel"
#define TYPES_MODULE_VER "2.0"
#define TYPES_MODULE_AUTHOR "OpenSoftware-World"
#define TYPES_MODULE_FILE_NAME "types.lib"
#define TYPES_MODULE_KRNL_VER "2.0"

#endif