/******************************************************************************
* Core types and definitions
*
* Author: Fabian Paus
*
******************************************************************************/#pragma once

#pragma once 

#include <stdint.h>

// Signed and unsigned integer types
using i64 = int64_t;
using i32 = int32_t;
using i16 = int16_t;
using i8 = int8_t;

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

constexpr const u64 KB = 1024;
constexpr const u64 MB = 1024 * KB;
constexpr const u64 GB = 1024 * MB;

/**
* Defer functionality allows you to execute code at scope exit.
* 
* Example:
* {
*     FILE* f = fopen(...)
* 
*     // Ensures fclose(f) is called at scope exit.
*     defer { fclose(f); };
*     ...
* }
*/ 
template <typename FunctionT>
struct DeferHelper
{
    FunctionT function;

    DeferHelper(FunctionT function)
        : function(function)
    { }

    ~DeferHelper()
    {
        function();
    }
};

#define CONCAT_1(x, y) x##y
#define CONCAT_2(x, y) CONCAT_1(x, y)
#define CONCAT_COUNTER(x) CONCAT_2(x, __COUNTER__)
#define defer DeferHelper CONCAT_COUNTER(defer) = [&]()

#define Assert(expression) if(!(expression)) {*(volatile int *)0 = 0;}
