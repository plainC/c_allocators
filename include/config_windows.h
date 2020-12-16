#ifndef __CONFIG_WINDOWS_H
#define __CONFIG_WINDOWS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <Windows.h>

static inline bool
atomic_cas_ptr(void** object, void** expected, void* desired)
{
    bool success;

    if (sizeof(void*) == 4) {
        long* o = (long*)object;
        LONG d = (LONG)desired;
        long comp = (long)*expected;

        int32_t value = _InterlockedCompareExchange(o, d, comp);
        success = value == comp;
        if (!success)
            *expected = (void*)value;
    }
    else {
        int64_t* o = (int64_t*)object;
        int64_t d = (int64_t)desired;
        int64_t comp = (int64_t)*expected;

        int64_t value = _InterlockedCompareExchange64(o, d, comp);
        success = value == comp;
        if (!success)
            *expected = (void*)value;
    }

    return success;
}

static inline bool
atomic_cas_uint(uint32_t* object, uint32_t* expected, uint32_t desired)
{
    bool success;

    long* o = (long*)object;
    LONG d = (LONG)desired;
    long comp = (long)*expected;

    int32_t value = _InterlockedCompareExchange(o, d, comp);
    success = value == comp;
    if (!success)
        *expected = value;

    return success;
}

#define CAS(object, expected, desired)      \
    atomic_cas_ptr((void**) (object), (void**) (expected), (void*) (desired))
#define CAS_UINT(object, expected, desired) \
    atomic_cas_uint((uint32_t*) (object), (uint32_t*) (expected), (uint32_t) (desired))
#define BZERO(ptr,n)                       \
    SecureZeroMemory(ptr,n)

#endif