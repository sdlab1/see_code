// src/gui/renderer/gl_primitives.c
#include "see_code/gui/renderer/gl_primitives.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>

int gl_primitives_draw_solid_quad(GLuint program_id, float x, float y, float width, float height,
                                 float r, float g, float b, float a,
                                 const float mvp[16], GLuint vbo_id) {
    if (program_id == 0) {
        log_error("Invalid program ID for solid quad draw");
        return 0;
    }

    glUseProgram(program_id);

    GLint pos_attrib = glGetAttribLocation(program_id, "position");
    GLint color_attrib = glGetAttribLocation(program_id, "color");
    GLint mvp_uniform = glGetUniformLocation(program_id, "mvp");

    if (pos_attrib == -1 || color_attrib == -1 || mvp_uniform == -1) {
        log_error("Failed to get solid quad shader attribute/uniform locations");
        glUseProgram(0);
        return 0;
    }

    glUniformMatrix4fv(mvp_uniform, 1, GL_FALSE, mvp);

    GLfloat vertices[] = {
        x, y,
        x + width, y,
        x, y + height,
        x + width, y + height
    };
    GLfloat colors[] = {
        r, g, b, a,
        r, g, b, a,
        r, g, b, a,
        r, g, b, a
    };

    GLuint local_vbo = 0;
    if (vbo_id == 0) {
        glGenBuffers(1, &local_vbo);
        vbo_id = local_vbo;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) + sizeof(colors), NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(vertices), sizeof(colors), colors);

    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(color_attrib, 4, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid*)sizeof(vertices));
    glEnableVertexAttribArray(color_attrib);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(pos_attrib);
    glDisableVertexAttribArray(color_attrib);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    if (local_vbo != 0) {
        glDeleteBuffers(1, &local_vbo);
    }

    return 1;
}

int gl_primitives_draw_textured_quad(GLuint program_id, GLuint texture_id,
                                    float x, float y, float width, float height,
                                    float u0, float v0, float u1, float v1,
                                    unsigned int color_rgba,
                                    const float mvp[16], GLuint vbo_id) {
    if (program_id == 0 || texture_id == 0) {
        log_error("Invalid program or texture ID for textured quad draw");
        return 0;
    }

    glUseProgram(program_id);

    GLint pos_attrib = glGetAttribLocation(program_id, "position");
    GLint texcoord_attrib = glGetAttribLocation(program_id, "texcoord");
    GLint mvp_uniform = glGetUniformLocation(program_id, "mvp");
    GLint tex_uniform = glGetUniformLocation(program_id, "tex");
    GLint color_uniform = glGetUniformLocation(program_id, "text_color"); // Предполагаем, что шейдер использует это

    if (pos_attrib == -1 || texcoord_attrib == -1 || mvp_uniform == -1 || tex_uniform == -1) {
        log_error("Failed to get textured quad shader attribute/uniform locations");
        glUseProgram(0);
        return 0;
    }

    glUniformMatrix4fv(mvp_uniform, 1, GL_FALSE, mvp);
    glUniform1i(tex_uniform, 0); // Текстура на юните 0

    // Установка цвета текста
    if (color_uniform != -1) {
        float cr = ((color_rgba >> 16) & 0xFF) / 255.0f;
        float cg = ((color_rgba >> 8) & 0xFF) / 255.0f;
        float cb = (color_rgba & 0xFF) / 255.0f;
        float ca = ((color_rgba >> 24) & 0xFF) / 255.0f;
        glUniform4f(color_uniform, cr, cg, cb, ca);
    }

    GLfloat vertices[] = {
        x, y,
        x + width, y,
        x, y + height,
        x + width, y + height
    };
    GLfloat texcoords[] = {
        u0, v0,
        u1, v0,
        u0, v1,
        u1, v1
    };

    GLuint local_vbo = 0;
    if (vbo_id == 0) {
        glGenBuffers(1, &local_vbo);
        vbo_id = local_vbo;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) + sizeof(texcoords), NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(vertices), sizeof(texcoords), texcoords);

    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(texcoord_attrib, 2, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid*)sizeof(vertices));
    glEnableVertexAttribArray(texcoord_attrib);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDisableVertexAttribArray(pos_attrib);
    glDisableVertexAttribArray(texcoord_attrib);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    if (local_vbo != 0) {
        glDeleteBuffers(1, &local_vbo);
    }

    return 1;
}
