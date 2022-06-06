// renderer-app.cpp : Defines the entry point for the application.
//

//#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using i64 = INT64;
using i32 = INT32;
using i16 = INT16;
using i8 = INT8;

using u64 = UINT64;
using u32 = UINT32;
using u16 = UINT16;
using u8 = UINT8;

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

struct Allocator;

typedef void* AllocateFunction(Allocator* context, u64 size);
typedef void FreeFunction(Allocator* context, void* data, u64 size);

struct Allocator
{
    AllocateFunction* allocateFunction = nullptr;
    FreeFunction* freeFunction = nullptr;

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
};

Allocator createPageAllocator()
{
    Allocator allocator = {};
    allocator.allocateFunction = +[](Allocator* context, u64 size) -> void*
    {
        return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    };
    allocator.freeFunction = +[](Allocator* context, void* data, u64 size)
    {
        VirtualFree(data, 0, MEM_RELEASE);
    };
    return allocator;
}

struct ArenaAllocator : Allocator
{
    u8* data = nullptr;
    u64 size = 0;
    u64 used = 0;
};

ArenaAllocator createArenaAllocator(void* data, u64 size)
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
    allocator.freeFunction = +[](Allocator* context, void* data, u64 size)
    {
        ArenaAllocator* allocator = (ArenaAllocator*)context;
        // Do not do anything! The arena can be freed in bulk
    };
    return allocator;
}




struct ReadFileResult
{
    u8* data;
    i64 size;
    i64 error;
    wchar_t const* errorText;
};

ReadFileResult readEntireFile(wchar_t const* filename)
{
    ReadFileResult result = {};

    // Open file
    HANDLE file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        result.errorText = L"CreateFileW failed";
        result.error = GetLastError();
        return result;
    }
    defer{ CloseHandle(file); };

    // Get file size
    LARGE_INTEGER fileSize = {};
    if (!GetFileSizeEx(file, &fileSize))
    {
        result.errorText = L"GetFileSizeEx failed";
        result.error = GetLastError();
        return result;
    }

    // Read file contents
    i64 bufferSize = fileSize.QuadPart;
    u8* buffer = (u8*)VirtualAlloc(NULL, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    u8* writeCursor = buffer;
    i64 bytesRemaining = bufferSize;
    while (bytesRemaining > 0)
    {
        DWORD bytesToRead = bytesRemaining > MAXDWORD ? MAXDWORD : (DWORD)bytesRemaining;
        DWORD bytesRead = 0;
        if (!ReadFile(file, writeCursor, bytesToRead, &bytesRead, NULL))
        {
            result.errorText = L"ReadFile failed";
            result.error = GetLastError();
            return result;
        }
        writeCursor += bytesRead;
        bytesRemaining -= bytesRead;
    }

    // Successfully read entire file contents
    result.data = buffer;
    result.size = fileSize.QuadPart;
    return result;
}

void freeReadFileResult(ReadFileResult* result)
{
    if (result->data)
    {
        // Memory size must be 0 if MEM_RELEASE is specified
        // This frees the entire allocation made by VirtualAlloc
        VirtualFree(result->data, 0, MEM_RELEASE);
    }
}

struct Vertex3
{
    float x;
    float y;
    float z;
};

struct Face
{
    // Vertex indices
    i32 v[3];

    // Normal indices
    i32 n[3];

    // TextureCoord indices
    i32 t[3];
};

struct ObjModel
{
    i64 verticesCount;
    Vertex3* vertices;
    i64 normalsCount;
    Vertex3* normals;
    i64 textureCoordsCount;
    Vertex3* textureCoords;
    i64 facesCount;
    Face* faces;
};

u8* parseFloat(u8* cursor, float* out)
{
    // Sign
    bool isNegative = false;
    if (*cursor == '-')
    {
        isNegative = true;
        cursor += 1;
    }

    // Integer part
    i32 integerPart = 0;
    while (*cursor >= '0' && *cursor <= '9')
    {
        integerPart = integerPart * 10 + (*cursor - '0');
        cursor += 1;
    }

    // Fractional part
    i32 fractionalPart = 0;
    i32 fractionalDivisor = 1;
    if (*cursor == '.')
    {
        cursor += 1;
        while (*cursor >= '0' && *cursor <= '9')
        {
            fractionalPart = fractionalPart * 10 + (*cursor - '0');
            fractionalDivisor *= 10;
            cursor += 1;
        }
    }

    float result = (float)(integerPart + (double)fractionalPart / fractionalDivisor);
    if (isNegative)
    {
        result = -result;
    }

    *out = result;
    return cursor;
}

u8* parseInteger(u8* cursor, i32* out)
{
    bool isNegative = false;
    if (*cursor == '-')
    {
        isNegative = true;
        cursor += 1;
    }

    i32 result = 0;
    while (*cursor >= '0' && *cursor <= '9')
    {
        result = result * 10 + (*cursor - '0');
        cursor += 1;
    }

    if (isNegative)
    {
        result = -result;
    }

    *out = result;
    return cursor;
}


// TODO: Actually return a ParseObjModelResult that includes the Model 
//       and error information
// The error information should include line number and column
// Maybe even the complete line 
// As well as a description of what went wrong
ObjModel parseObjModel(u8* data, i64 size, Allocator* allocator)
{
    ObjModel result = {};

    u8* end = data + size;

    // Count elements first, so that we can allocate
    u8* cursor = data;
    while (cursor < end)
    {
        if (*cursor == 'v')
        {
            cursor += 1;
            if (cursor == end)
            {
                break;
            }
            if (*cursor == ' ')
            {
                // This is a vertex
                result.verticesCount += 1;
            }
            else if (*cursor == 't')
            {
                // This is a texture coordinate
                result.textureCoordsCount += 1;
            }
            else if (*cursor == 'n')
            {
                result.normalsCount += 1;
            }
            else
            {
                OutputDebugStringW(L"Unexpected symbol found after v: ");
                char buffer[2] = { (char)*cursor, '\0' };
                OutputDebugStringA(buffer);
                OutputDebugStringW(L"\n");
            }

            while (cursor < end && *cursor != '\n')
            {
                cursor += 1;
            }
        }
        else if (*cursor == 'f')
        {
            result.facesCount += 1;

            while (cursor < end && *cursor != '\n')
            {
                cursor += 1;
            }
        }
        else if (*cursor == '\n')
        {
            cursor += 1;
        }
        else if (*cursor == '#')
        {
            // # comment
            while (cursor < end && *cursor != '\n')
            {
                cursor += 1;
            }
        }
        else if (*cursor == 'g')
        {
            // g [group name]
            while (cursor < end && *cursor != '\n')
            {
                cursor += 1;
            }
        }
        else if (*cursor == 'u')
        {
            // usemtl [material]
            while (cursor < end && *cursor != '\n')
            {
                cursor += 1;
            }
        }
        else if (*cursor == 'o')
        {
            // o [object name]
        }
        else
        {
            char buffer[2] = { (char)*cursor, '\0' };
            OutputDebugStringA(buffer);

            cursor += 1;
        }
    }

    // Allocate the buffers
    result.vertices = allocator->allocateArray<Vertex3>(result.verticesCount);
    result.normals = allocator->allocateArray<Vertex3>(result.normalsCount);
    result.textureCoords = allocator->allocateArray<Vertex3>(result.textureCoordsCount);
    result.faces = allocator->allocateArray<Face>(result.facesCount);

    Vertex3* verticesCursor = result.vertices;
    Vertex3* normalsCursor = result.normals;
    Vertex3* textureCoordsCursor = result.textureCoords;
    Face* facesCursor = result.faces;


    cursor = data;
    while (cursor < end)
    {
        if (*cursor == 'v')
        {
            cursor += 1;
            if (cursor == end)
            {
                break;
            }
            if (*cursor == ' ')
            {
                cursor += 1;

                // This is a vertex
                u8* newCursor = parseFloat(cursor, &verticesCursor->x);
                if (*newCursor == ' ') newCursor += 1;
                newCursor = parseFloat(newCursor, &verticesCursor->y);
                if (*newCursor == ' ') newCursor += 1;
                newCursor = parseFloat(newCursor, &verticesCursor->z);
                if (*newCursor == ' ') newCursor += 1;

                if (newCursor == cursor)
                {
                    // Something went wrong parsing the floats!
                    OutputDebugStringW(L"Something went wrong parsing the floats in a vertex!\n");
                }

                cursor = newCursor;
                verticesCursor += 1;
            }
            else if (*cursor == 't')
            {
                cursor += 1;
                if (*cursor == ' ')
                {
                    cursor += 1;
                }

                // This is a texture coordinate

                u8* newCursor = parseFloat(cursor, &textureCoordsCursor->x);
                if (*newCursor == ' ') newCursor += 1;
                textureCoordsCursor->y = 0.0f;
                newCursor = parseFloat(newCursor, &textureCoordsCursor->y);
                if (*newCursor == ' ') newCursor += 1;
                textureCoordsCursor->z = 0.0f;
                newCursor = parseFloat(newCursor, &textureCoordsCursor->z);
                if (*newCursor == ' ') newCursor += 1;

                if (newCursor == cursor)
                {
                    // Something went wrong parsing the floats!
                    OutputDebugStringW(L"Something went wrong parsing the floats in a texture coords!\n");
                }

                cursor = newCursor;
                textureCoordsCursor += 1;
            }
            else if (*cursor == 'n')
            {
                cursor += 1;

                u8* newCursor = parseFloat(cursor, &normalsCursor->x);
                if (*newCursor == ' ') newCursor += 1;
                newCursor = parseFloat(newCursor, &normalsCursor->y);
                if (*newCursor == ' ') newCursor += 1;
                newCursor = parseFloat(newCursor, &normalsCursor->z);
                if (*newCursor == ' ') newCursor += 1;

                if (newCursor == cursor)
                {
                    // Something went wrong parsing the floats!
                    OutputDebugStringW(L"Something went wrong parsing the floats in a normal!\n");
                }

                cursor = newCursor;
                normalsCursor += 1;
            }
            else
            {
                OutputDebugStringW(L"Unexpected symbol found after v: ");
                char buffer[2] = { (char)*cursor, '\0' };
                OutputDebugStringA(buffer);
                OutputDebugStringW(L"\n");
            }

            while (cursor < end && *cursor != '\n')
            {
                cursor += 1;
            }
        }
        else if (*cursor == 'f')
        {
            cursor += 1;
            if (*cursor == ' ')
            {
                cursor += 1;
            }

            for (int i = 0; i < 3; ++i)
            {
                // full:    [vertex]/[texture coord]/[normal]
                // normal:  [vertex]/[texture coord]
                // texture: [vertex]//[normal]

                // [vertex]
                u8* newCursor = parseInteger(cursor, facesCursor->v + i);
                if (newCursor == cursor)
                {
                    OutputDebugStringW(L"Could not parse vertex index\n");
                }
                cursor = newCursor;

                // /
                if (*cursor != '/')
                {
                    OutputDebugStringW(L"Expected a '/' but got '");
                    char buffer[2] = { (char)*cursor, '\0' };
                    OutputDebugStringA(buffer);
                    OutputDebugStringW(L"'\n");
                }
                cursor += 1;

                // [texture coord] (optional)
                newCursor = parseInteger(cursor, facesCursor->t + i);
                if (newCursor == cursor)
                {
                    // No texture coord defined
                    facesCursor->t[i] = -1;
                }
                cursor = newCursor;

                // /
                if (*cursor == '/')
                {
                    cursor += 1;

                    // [normal]
                    newCursor = parseInteger(cursor, facesCursor->n + i);
                    if (newCursor != cursor)
                    {
                        OutputDebugStringW(L"Could not parse normal index\n");
                    }
                    cursor = newCursor;
                }
                else
                {
                    // No normal defined
                    facesCursor->n[i] = -1;
                }

                if (*cursor == ' ' || *cursor == '\n')
                {
                    cursor += 1;
                }
                else
                {
                    OutputDebugStringW(L"Expected whitespace after face indices but got '");
                    char buffer[2] = { (char)*cursor, '\0' };
                    OutputDebugStringA(buffer);
                    OutputDebugStringW(L"'\n");
                }
            }
            facesCursor += 1;
        }
        else if (*cursor == '\n')
        {
            cursor += 1;
        }
        else if (*cursor == '#')
        {
            // # comment
            while (cursor < end && *cursor != '\n')
            {
                cursor += 1;
            }
        }
        else if (*cursor == 'g')
        {
            // g [group name]
            while (cursor < end && *cursor != '\n')
            {
                cursor += 1;
            }
        }
        else if (*cursor == 'u')
        {
            // usemtl [material]
            while (cursor < end && *cursor != '\n')
            {
                cursor += 1;
            }
        }
        else if (*cursor == 'o')
        {
            // o [object name]
        }
        else
        {
            char buffer[2] = { (char)*cursor, '\0' };
            OutputDebugStringA(buffer);

            cursor += 1;
        }
    }


    return result;
}

constexpr const u64 KB = 1024;
constexpr const u64 MB = 1024 * KB;
constexpr const u64 GB = 1024 * MB;

// This variable is expected by the compiler/linker if floats/doubles are used
extern "C" int _fltused = 0;

extern "C" int WINAPI WinMainCRTStartup(void)
{
    Allocator pageAllocator = createPageAllocator();
    u64 arenaSize = 4 * MB;
    void* arenaData = pageAllocator.allocate(arenaSize);
    ArenaAllocator arenaAllocator = createArenaAllocator(arenaData, arenaSize);

    // TODO: Create a arena allocator with fallback to the page allocator if size is too big

    wchar_t const* filename = L"data/Deer.obj";

    ReadFileResult fileResult = readEntireFile(filename);
    defer{ freeReadFileResult(&fileResult); };

    if (fileResult.error)
    {
        OutputDebugStringW(L"Could not read from file due to the following error:\n");
        OutputDebugStringW(fileResult.errorText);
        OutputDebugStringW(L"\n");
        // TODO: Add error code and format the message according to the error code
        return 1;
    }

    OutputDebugStringW(L"Read file content successfully!\n");

    ObjModel model = parseObjModel(fileResult.data, fileResult.size, &arenaAllocator);


    return 0;
}