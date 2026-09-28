#pragma once
#include "RenderStats.h"
#include <vector>

// Ordered 2D submission queue. Never sorts by texture across transparent objects.
namespace RenderQueue
{
    struct VertexData
    {
        float x, y, u, v, r, g, b, a;
    };

    void Begin(GLenum topology);
    void End();
    void Vertex(float x, float y);
    void TexCoord(float u, float v);
    void Color(float r, float g, float b, float a);
    void EnableTexture(bool enabled);
    void Texture(GLuint texture);
    void Flush();
    void ResetState();
    void Shutdown();
    // These are ordering barriers: pending geometry must use the old GL state.
    void Program(GLuint program);
    void Blend(GLenum source, GLenum destination);
    void PushMatrix();
    void PopMatrix();
    void Translate(float x, float y, float z);

    class StaticMesh
    {
      public:
        ~StaticMesh();
        StaticMesh() = default;
        StaticMesh(const StaticMesh&) = delete;
        StaticMesh& operator=(const StaticMesh&) = delete;
        void Upload(const std::vector<VertexData>& vertices, const std::vector<GLuint>& indices);
        void Draw() const;

      private:
        GLuint vertices = 0, indices = 0;
        GLsizei count = 0;
    };
} // namespace RenderQueue
