/******************************************************************************
* Wavefront OBJ loader
*
* This file contains data structures and functions to work with OBJ files.
*
* Author: Fabian Paus
*
******************************************************************************/

#pragma once

#include "fp_core.h"
#include "fp_allocator.h"

// TODO: Replace OutputDebugString with OS independent functions
//       Maybe even better to return an error string that can be printed outside
//#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


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

    void free(Allocator* allocator)
    {
        allocator->freeArray(vertices, verticesCount);
        allocator->freeArray(normals, normalsCount);
        allocator->freeArray(textureCoords, textureCoordsCount);
        allocator->freeArray(faces, facesCount);
    }
};

static u8* parseFloat(u8* cursor, float* out)
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

static u8* parseInteger(u8* cursor, i32* out)
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
static ObjModel parseObjModel(u8* data, i64 size, Allocator* allocator)
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

