/******************************************************************************
* Memory allocators
* 
* This file contains different memory allocator implementations.
* 
* TODO: List the individual allocators
* 
* Author: Fabian Paus
* 
******************************************************************************/

#include "fp_core.h"

/**
* Allocator interface
* 
* An allocator needs to provide an AllocateFunction and FreeFunction with the
* signatures defined below. You can subclass Allocator to add additional data
* and create stateful allocator.
* 
*/
struct Allocator;

typedef void* AllocateFunction(Allocator* context, u64 size);
typedef void FreeFunction(Allocator* context, void* data, u64 size);

struct Allocator
{
    AllocateFunction* allocateFunction;
    FreeFunction* freeFunction;

    void* allocate(u64 size)
    {
        return allocateFunction(this, size);
    }

    void free(void* allocatedData, u64 size)
    {
        freeFunction(this, allocatedData, size);
    }

    template <typename T>
    T* allocateArray(u64 count)
    {
        return (T*)allocate(count * sizeof(T));
    }

    template <typename T>
    void freeArray(T* allocatedData, u64 size)
    {
        free(allocatedData, size * sizeof(T));
    }
};

typedef bool OwnsFunction(Allocator* context, void* data, u64 size);

struct OwningAllocator : Allocator
{
    OwnsFunction* ownsFunction;

    bool owns(void* data, u64 size)
    {
        return ownsFunction(this, data, size);
    }
};

/**
 * Create a page allocator.
 * 
 * This allocator directly returns allocated memory in multiples of the 
 * OS page size. It is stateless (at least from the users perspective).
 * This allocator needs to be implemented by each OS separately.
 * 
 * Windows: Implemented via VirtualAlloc
 */ 
Allocator createPageAllocator();


struct ArenaAllocator : OwningAllocator
{
    u8* data;
    u64 size;
    u64 used;

    void reset()
    {
        used = 0;
    }
};

/**
 * Create an arena allocator.
 * 
 * An arena allocator uses a fixed sized buffer from which allocations are made.
 * The allocations work like a stack by simply increasing the used size by the
 * required allocation size. Therefore, allocations are very efficient.
 * 
 * If the buffer size is exceeded, allocations will return nullptr.
 * 
 * Memory can only be freed all at once by calling reset(). This frees memory 
 * from all allocations made to the allocator.
 */
static ArenaAllocator createArenaAllocator(void* data, u64 size)
{
    ArenaAllocator allocator = {};
    allocator.data = (u8*)data;
    allocator.size = size;
    allocator.used = 0;

    allocator.allocateFunction = +[](Allocator* context, u64 size) -> void*
    {
        ArenaAllocator* allocator = (ArenaAllocator*)context;

        u64 used = allocator->used;
        if (used + size > allocator->size)
        {
            return nullptr;
        }

        u8* current = allocator->data + used;
        allocator->used += size;

        return current;
    };
    allocator.freeFunction = +[](Allocator* context, void* data, u64 size) -> void
    {
        ArenaAllocator* allocator = (ArenaAllocator*)context;
        // Do not do anything! The arena can be freed in bulk
    };
    allocator.ownsFunction = +[](Allocator* context, void* data, u64 size) -> bool
    {
        ArenaAllocator* allocator = (ArenaAllocator*)context;

        u8* begin = allocator->data;
        u8* end = begin + allocator->size;
        bool beginInside = begin <= data && data < end;

        u8* dataEnd = (u8*)data + size;
        bool endInside = begin <= dataEnd && dataEnd <= end;

        return beginInside && endInside;
    };
    return allocator;
}



struct LinkedArenaAllocator : ArenaAllocator
{
    LinkedArenaAllocator* next;
};

struct DynamicArenaAllocator : OwningAllocator
{
    LinkedArenaAllocator* current;
    u64 arenaSize;
    Allocator* base;

    void pushNextArena()
    {
        u8* arenaMemory = (u8*)base->allocate(arenaSize);

        // Create the LinkedArenaAllocator in place in its own memory
        LinkedArenaAllocator* newArena = (LinkedArenaAllocator*)(arenaMemory);
        *(ArenaAllocator*)newArena = createArenaAllocator(arenaMemory, arenaSize);
        newArena->used = sizeof(LinkedArenaAllocator);

        newArena->next = current;
        current = newArena;
    }

    void reset()
    {
        while (current->next)
        {
            // Save the next pointer, because it is stored as part of the allocation
            LinkedArenaAllocator* next = current->next;

            base->free(current->data, current->size);

            current = next;
        }

        // Now, only the a single arena is still allocated.
        current->used = sizeof(LinkedArenaAllocator);
    }
};

/**
 * DynamicArenaAllocator
 *
 * This allocator uses a dynamic list of arena allocators.
 * If the current arena allocator cannot fulfill the allocation request,
 * a new arena will be allocated using a base allocator.
 * 
 * Freeing individual allocations is not implemented. However, through reset()
 * the memory of all arenas except one will be freed and the remaining arena
 * will be reset.
 * 
 * This allocator can only fulfill allocations below the requested arenaSize
 * minus sizeof(LinkedArenaAllocator) since the data structure is embedded
 * in the allocation.
 */
static DynamicArenaAllocator createDynamicArenaAllocator(Allocator* base, u64 arenaSize = 4 * KB)
{
    DynamicArenaAllocator result = {};

    // Make sure that arenaSize > sizeof (LinkedArenaAllocator)
    if (arenaSize <= sizeof(LinkedArenaAllocator))
    {
        // TODO: How to report the error?
        return result;
    }

    result.arenaSize = arenaSize;
    result.base = base;

    result.pushNextArena();

    result.allocateFunction = +[](Allocator* context, u64 size) -> void*
    {
        DynamicArenaAllocator* allocator = (DynamicArenaAllocator*)context;

        // Check whether allocation fits into an empty arena
        if (size > allocator->arenaSize - sizeof(LinkedArenaAllocator))
        {
            return nullptr;
        }

        // Try allocation from current arena first.
        // This might fail if size exceeds the remaining space in the arena.
        void* arenaMemory = allocator->current->allocate(size);
        if (arenaMemory)
        {
            return arenaMemory;
        }

        // Allocation did not fit in current arena, so push a new, empty arena.
        allocator->pushNextArena();

        // Now, the allocation cannot fail on the new arena since the size check 
        // at the start of this function ensures that size fits into a fresh arena.
        return allocator->current->allocate(size);
    };
    result.freeFunction = +[](Allocator* context, void* data, u64 size)
    {
        DynamicArenaAllocator* allocator = (DynamicArenaAllocator*)context;
        // Do not do anything! The arena can be freed in bulk, see reset()
    };
    result.ownsFunction = +[](Allocator* context, void* data, u64 size) -> bool
    {
        DynamicArenaAllocator* allocator = (DynamicArenaAllocator*)context;

        LinkedArenaAllocator* arena = allocator->current;
        while (arena)
        {
            if (arena->owns(data, size))
            {
                return true;
            }
            arena = arena->next;
        }

        return false;
    };

    return result;
}


struct ArenaWithFallbackAllocator : Allocator
{
    DynamicArenaAllocator arena;

    void reset()
    {
        arena.reset();
    }
};

/**
 * ArenaWithFallbackAllocator
 * 
 * Uses a dynamically growing list of arena allocators and additonally
 * falls back to the base allocator if the requested size is too big for
 * an empty arena.
 * 
 * Only the big allocations are freed individually. Small allocations made
 * to the arenas can only be freed in bulk using reset().
 */
ArenaWithFallbackAllocator createArenaWithFallbackAllocator(Allocator* baseAndFallback, u64 arenaSize = 4 * KB)
{
    ArenaWithFallbackAllocator result = {};

    result.arena = createDynamicArenaAllocator(baseAndFallback, arenaSize);

    result.allocateFunction = +[](Allocator* context, u64 size) -> void*
    {
        ArenaWithFallbackAllocator* allocator = (ArenaWithFallbackAllocator*)context;

        void* memory = allocator->arena.allocate(size);
        if (memory == nullptr)
        {
            // Use the base allocator as fallback if allocation does not fit in arena
            memory = allocator->arena.base->allocate(size);
        }

        return memory;
    };
    result.freeFunction = +[](Allocator* context, void* data, u64 size)
    {
        ArenaWithFallbackAllocator* allocator = (ArenaWithFallbackAllocator*)context;
        
        if (allocator->arena.owns(data, size))
        {
            // Do nothing, must be freed in bulk!
        }
        else
        {
            allocator->arena.base->free(data, size);
        }
    };

    return result;
}

