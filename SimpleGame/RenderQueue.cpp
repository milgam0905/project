#include "stdafx.h"
#include "RenderQueue.h"
#include "Profiler.h"
#include <cassert>
#include <cstddef>

namespace
{
    using V = RenderQueue::VertexData;

    struct Command
    {
        GLenum topology;
        GLuint texture;
        bool textured;
        size_t offset, count;
    };

    std::vector<V> primitive, vertices;
    std::vector<GLuint> indices;
    std::vector<Command> commands;
    V current = {0, 0, 0, 0, 1, 1, 1, 1};
    GLuint texture = 0, vbo = 0, ibo = 0;
    bool textured = false, recording = false;
    GLenum topology = GL_TRIANGLES;
    constexpr size_t VertexLimit = 65536;

    void Arrays(GLuint vertexBuffer, GLuint indexBuffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glClientActiveTexture(GL_TEXTURE0);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(2, GL_FLOAT, sizeof(V), reinterpret_cast<const void*>(offsetof(V, x)));
        glTexCoordPointer(2, GL_FLOAT, sizeof(V), reinterpret_cast<const void*>(offsetof(V, u)));
        glColorPointer(4, GL_FLOAT, sizeof(V), reinterpret_cast<const void*>(offsetof(V, r)));
    }

    void Restore()
    {
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        if (textured)
        {
            glEnable(GL_TEXTURE_2D);
        }
        else
        {
            glDisable(GL_TEXTURE_2D);
        }
        glBindTexture(GL_TEXTURE_2D, texture);
        glColor4f(current.r, current.g, current.b, current.a);
        glTexCoord2f(current.u, current.v);
    }
} // namespace

void RenderQueue::Begin(GLenum mode)
{
    assert(!recording);
    recording = true;
    topology = mode;
    primitive.clear();
}

void RenderQueue::Vertex(float x, float y)
{
    assert(recording);
    current.x = x;
    current.y = y;
    primitive.push_back(current);
}

void RenderQueue::TexCoord(float u, float v)
{
    current.u = u;
    current.v = v;
}

void RenderQueue::Color(float r, float g, float b, float a)
{
    current.r = r;
    current.g = g;
    current.b = b;
    current.a = a;
}

void RenderQueue::EnableTexture(bool value)
{
    textured = value;
}

void RenderQueue::Texture(GLuint value)
{
    texture = value;
}

void RenderQueue::End()
{
    assert(recording);
    recording = false;
    RenderStats::SourcePrimitive();
    if (vertices.size() + primitive.size() > VertexLimit)
    {
        Profiler::Count("batch.flush_capacity");
        Flush();
    }
    const GLuint base = static_cast<GLuint>(vertices.size());
    const size_t offset = indices.size();
    const size_t count = primitive.size();
    const bool lines =
        topology == GL_LINES || topology == GL_LINE_LOOP || topology == GL_LINE_STRIP;
    const GLenum output = lines ? GL_LINES : GL_TRIANGLES;
    auto triangle = [&](size_t a, size_t b, size_t c)
    {
        indices.push_back(base + static_cast<GLuint>(a));
        indices.push_back(base + static_cast<GLuint>(b));
        indices.push_back(base + static_cast<GLuint>(c));
    };
    if (topology == GL_QUADS)
    {
        for (size_t i = 0; i + 3 < count; i += 4)
        {
            triangle(i, i + 1, i + 2);
            triangle(i, i + 2, i + 3);
        }
    }
    else if (topology == GL_TRIANGLE_FAN)
    {
        for (size_t i = 1; i + 1 < count; ++i)
        {
            triangle(0, i, i + 1);
        }
    }
    else if (topology == GL_TRIANGLE_STRIP)
    {
        for (size_t i = 0; i + 2 < count; ++i)
        {
            triangle(i + (i % 2), i + 1 - (i % 2), i + 2);
        }
    }
    else if (topology == GL_TRIANGLES)
    {
        for (size_t i = 0; i + 2 < count; i += 3)
        {
            triangle(i, i + 1, i + 2);
        }
    }
    else if (lines)
    {
        const size_t step = topology == GL_LINES ? 2 : 1;
        for (size_t i = 0; i + 1 < count; i += step)
        {
            indices.push_back(base + static_cast<GLuint>(i));
            indices.push_back(base + static_cast<GLuint>(i + 1));
        }
        if (topology == GL_LINE_LOOP && count > 1)
        {
            indices.push_back(base + static_cast<GLuint>(count - 1));
            indices.push_back(base);
        }
    }
    else
    {
        Profiler::Count("batch.unsupported_topology");
        assert(false);
    }
    if (indices.size() == offset)
    {
        return;
    }
    vertices.insert(vertices.end(), primitive.begin(), primitive.end());
    const GLuint material = textured ? texture : 0;
    if (!commands.empty() && commands.back().topology == output &&
        commands.back().texture == material && commands.back().textured == textured)
    {
        commands.back().count += indices.size() - offset;
        Profiler::Count("batch.merged_primitives");
    }
    else
    {
        if (!commands.empty())
        {
            Profiler::Count(commands.back().topology != output ? "batch.split_topology"
                                                               : "batch.split_texture");
        }
        commands.push_back({output, material, textured, offset, indices.size() - offset});
        Profiler::Count("batch.state_runs");
    }
}

void RenderQueue::Flush()
{
    assert(!recording);
    if (commands.empty())
    {
        return;
    }
    Profiler::Scope scope("render.queue_flush");
    if (!vbo)
    {
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);
    }
    {
        Profiler::Scope uploadProfile("render.dynamic_buffer_upload");
        glActiveTexture(GL_TEXTURE0);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(V), vertices.data(), GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(GLuint),
                     indices.data(),
                     GL_STREAM_DRAW);
    }
    Profiler::Count("upload.dynamic_buffer_bytes",
                    vertices.size() * sizeof(V) + indices.size() * sizeof(GLuint));
    Profiler::Count("batch.flushes");
    Profiler::Gauge("batch.retained_cpu_bytes",
                    static_cast<double>(
                        vertices.capacity() * sizeof(V) + indices.capacity() * sizeof(GLuint) +
                        commands.capacity() * sizeof(Command) + primitive.capacity() * sizeof(V)));
    {
        Profiler::Scope submitProfile("render.batch_submit");
        Arrays(vbo, ibo);
        for (const Command& command : commands)
        {
            if (command.textured)
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, command.texture);
                Profiler::Count("batch.texture_bind_calls");
            }
            else
            {
                glDisable(GL_TEXTURE_2D);
            }
            glDrawElements(command.topology,
                           static_cast<GLsizei>(command.count),
                           GL_UNSIGNED_INT,
                           reinterpret_cast<const void*>(command.offset * sizeof(GLuint)));
            RenderStats::DrawCall(command.count);
        }
        Restore();
    }
    commands.clear();
    vertices.clear();
    indices.clear();
}

void RenderQueue::ResetState()
{
    Flush();
    current = {0, 0, 0, 0, 1, 1, 1, 1};
    texture = 0;
    textured = false;
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderQueue::Program(GLuint value)
{
    Flush();
    glUseProgram(value);
    Profiler::Count("batch.program_barriers");
}

void RenderQueue::Blend(GLenum source, GLenum destination)
{
    Flush();
    glBlendFunc(source, destination);
    Profiler::Count("batch.blend_barriers");
}

void RenderQueue::PushMatrix()
{
    Flush();
    glPushMatrix();
}

void RenderQueue::PopMatrix()
{
    Flush();
    glPopMatrix();
}

void RenderQueue::Translate(float x, float y, float z)
{
    Flush();
    glTranslatef(x, y, z);
}

void RenderQueue::Shutdown()
{
    Flush();
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    vbo = ibo = 0;
}

RenderQueue::StaticMesh::~StaticMesh()
{
    glDeleteBuffers(1, &vertices);
    glDeleteBuffers(1, &indices);
}

void RenderQueue::StaticMesh::Upload(const std::vector<VertexData>& data,
                                     const std::vector<GLuint>& elements)
{
    Flush();
    Profiler::Scope scope("render.static_mesh_upload");
    if (!vertices)
    {
        glGenBuffers(1, &vertices);
        glGenBuffers(1, &indices);
    }
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(V), data.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(GLuint), elements.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    count = static_cast<GLsizei>(elements.size());
    Profiler::Count("upload.static_buffer_bytes",
                    data.size() * sizeof(V) + elements.size() * sizeof(GLuint));
}

void RenderQueue::StaticMesh::Draw() const
{
    Flush();
    if (!count)
    {
        return;
    }
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    Arrays(vertices, indices);
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
    RenderStats::SourcePrimitive();
    RenderStats::DrawCall(count);
    Restore();
}
