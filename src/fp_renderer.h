#pragma once

#include "fp_allocator.h"

struct Color {
    float color[4];
};

static const Color RED = { 1.0f, 0.0f, 0.0f, 1.0f };
static const Color GREEN = { 0.0f, 1.0f, 0.0f, 1.0f };
static const Color BLUE = { 0.0f, 0.0f, 1.0f, 1.0f };

enum RenderCommandType {
	Render_Rectangle,
};

struct RenderCommand {
	RenderCommandType type;
};

struct RenderCommandRectangle : RenderCommand {
	float x;
	float y;
	float width;
	float height;

	Color color;
};

struct RenderCommandBuffer {
	ArenaAllocator allocator;
    int rectCount;

	RenderCommand* first() {
		return (RenderCommand*)allocator.data;
	}

    RenderCommand* onePastLast() {
        return (RenderCommand*)(allocator.data + allocator.used);
    }

	void reset() {
		allocator.reset();
        rectCount = 0;
	}

	void push(RenderCommandRectangle* rect) {
		RenderCommandRectangle* target = allocator.allocateSingle<RenderCommandRectangle>();
		*target = *rect;
        rectCount += 1;
	}
};

static const char* VERTEX_SHADER_SIMPLE_COLOR =
"#version 330 core\n"
"#line " STR(__LINE__) "\n"
R"(
layout (location = 0) in vec3 pos;
layout (location = 1) in vec4 color;

uniform mat4 projection;

out vec4 vertexColor;

void main()
{
    vec4 pos = projection * vec4(pos.x, pos.y, pos.z, 1.0);
    gl_Position = pos;
    vertexColor = color;
}
)";

static const char* FRAGMENT_SHADER_VERTEX_COLOR =
"#version 330 core\n"
"#line " STR(__LINE__) "\n"
R"(
out vec4 FragColor;

in vec4 vertexColor;

void main()
{
    FragColor = vertexColor;
} 
)";

#pragma pack(1)
struct ColoredVertex {
    float pos[3];
    Color color;
};

struct Renderer {
    RenderCommandBuffer commands;
    // Used to to store temporary data during rendering
    ArenaAllocator temporaryRenderBuffer;

    // TODO: Get rid of the windows specific code here
    HDC deviceContext;

    unsigned int vertexArray;
    unsigned int vertexBuffer;

    unsigned int vertexShader;
    unsigned int fragmentShader;
    unsigned int shaderProgram;

    int projectionLocation;

    void setup(HDC dc, void* renderMemory, int renderMemorySize) {
        int commandSize = renderMemorySize / 2;
        commands.allocator = createArenaAllocator(renderMemory, commandSize);
        int tempSize = renderMemorySize - commandSize;
        temporaryRenderBuffer = createArenaAllocator((u8*)renderMemory + commandSize, tempSize);

        deviceContext = dc;

        glGenVertexArrays(1, &vertexArray);
        glBindVertexArray(vertexArray);

        glGenBuffers(1, &vertexBuffer);

        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        gl_compileShader(vertexShader, VERTEX_SHADER_SIMPLE_COLOR);

        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        gl_compileShader(fragmentShader, FRAGMENT_SHADER_VERTEX_COLOR);

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        gl_linkProgram(shaderProgram);

        projectionLocation = glGetUniformLocation(shaderProgram, "projection");
    }

    void render() {
        // Be careful if we use the same arenas for the command buffer and rendering memory
        // TODO: We probably want to separate them
        RenderCommand* command = commands.first();
        RenderCommand* onePastLast = commands.onePastLast();

        ColoredVertex* rectVertices = temporaryRenderBuffer.allocateArray<ColoredVertex>(commands.rectCount * 6ULL);
        ColoredVertex* rectVertex = rectVertices;

        while (command < onePastLast) {
            if (command->type == Render_Rectangle) {
                RenderCommandRectangle* rect = (RenderCommandRectangle*)command;

                ColoredVertex* vertex = rectVertex;

                float xPos = rect->x;
                float yPos = rect->y;
                Color color = rect->color;

                // First triangle
                rectVertex[0].pos[0] = xPos;
                rectVertex[0].pos[1] = yPos;
                rectVertex[0].pos[2] = 0.0f;
                rectVertex[0].color = color;
                rectVertex[1].pos[0] = xPos + rect->width;
                rectVertex[1].pos[1] = yPos;
                rectVertex[1].pos[2] = 0.0f;
                rectVertex[1].color = color;
                rectVertex[2].pos[0] = xPos;
                rectVertex[2].pos[1] = yPos + rect->height;
                rectVertex[2].pos[2] = 0.0f;
                rectVertex[2].color = color;

                // Second triangle
                rectVertex[3].pos[0] = xPos + rect->width;
                rectVertex[3].pos[1] = yPos;
                rectVertex[3].pos[2] = 0.0f;
                rectVertex[3].color = color;
                rectVertex[4].pos[0] = xPos + rect->width;
                rectVertex[4].pos[1] = yPos + rect->height;
                rectVertex[4].pos[2] = 0.0f;
                rectVertex[4].color = color;
                rectVertex[5].pos[0] = xPos;
                rectVertex[5].pos[1] = yPos + rect->height;
                rectVertex[5].pos[2] = 0.0f;
                rectVertex[5].color = color;

                rectVertex += 6;

                command = (RenderCommand*)((u8*)command + sizeof(RenderCommandRectangle));
            }
            else {
                OutputDebugStringW(L"Unknown command type\n");
                DebugBreak();
                continue;
            }
        }

        if (commands.rectCount > 0) {
            // TODO: Replace with named buffer calls
            glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
            int vertexCount = rectVertex - rectVertices;
            int bufferSize = vertexCount * sizeof(ColoredVertex);
            glBufferData(GL_ARRAY_BUFFER, bufferSize, rectVertices, GL_STREAM_DRAW);

            glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        }

        temporaryRenderBuffer.reset();
    }

    void beginFrame() {
        commands.reset();
    }

    void endFrame() {
        glFinish();
    }
};

