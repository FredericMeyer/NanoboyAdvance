/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <array>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <nba/log.hpp>
#include <platform/device/ogl_video_device.hpp>
#include <memory>

#include "device/shader/color_higan.glsl.hpp"
#include "device/shader/color_agb.glsl.hpp"
#include "device/shader/lcd_ghosting.glsl.hpp"
#include "device/shader/lcd1x.glsl.hpp"
#include "device/shader/output.glsl.hpp"
#include "device/shader/sharp_bilinear.glsl.hpp"
#include "device/shader/xbrz.glsl.hpp"

using Video = nba::PlatformConfig::Video;

namespace nba {

static constexpr int gba_screen_width  = 240;
static constexpr int gba_screen_height = 160;

static constexpr std::array<GLfloat, 4 * 4> kQuadVertices = {
// position | UV coord
  -1, -1,     0, 0,
   1, -1,     1, 0,
  -1,  1,     0, 1,
   1,  1,     1, 1
};

OGLVideoDevice::OGLVideoDevice(std::shared_ptr<PlatformConfig> config) : config(config) {
}

OGLVideoDevice::~OGLVideoDevice() {
  ReleaseShaderPrograms();
  glDeleteVertexArrays(1, &quad_vao);
  glDeleteBuffers(1, &quad_vbo);
  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(textures.size(), textures.data());
}

void OGLVideoDevice::Initialize() {
  // Create a fullscreen quad to render to the viewport.
  glGenVertexArrays(1, &quad_vao);
  glGenBuffers(1, &quad_vbo);
  glBindVertexArray(quad_vao);
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  // Create three textures and a framebuffer for postprocessing.
  glGenFramebuffers(1, &fbo);
  glGenTextures(textures.size(), textures.data());

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glEnable(GL_FRAMEBUFFER_SRGB);

  ReloadConfig();
}

void OGLVideoDevice::ReloadConfig() {
  if(config->video.filter == Video::Filter::Linear ||
     config->video.filter == Video::Filter::Sharp) {
    texture_filter = GL_LINEAR;
  } else {
    texture_filter = GL_NEAREST;
  }

  UpdateTextures();
  CreateShaderPrograms();
}

void OGLVideoDevice::UpdateTextures() {
  constexpr auto set_texture_params = [=](GLint tex_filter) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex_filter);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  };

  const bool color_correction_active = config->video.color != Video::Color::No;
  const auto texture_format = color_correction_active ? GL_RGBA8 : GL_SRGB8_ALPHA8;

  // Alternating input sampler and output framebuffer attachment.
  for(const auto texture : {textures[input_index], textures[output_index]}) {
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, texture_format, gba_screen_width,
                 gba_screen_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    set_texture_params(texture_filter);
  }

  /* Depending on the order of shaders, xBRZ scaler may need an intermediate
   * output-sized texture, to be sampled in the LCD ghosting effect shader.
   * This improves the look of the latter.
   */
  const bool xbrz_ghosting =
    config->video.filter == Video::Filter::xBRZ && config->video.lcd_ghosting;
  if(config->video.lcd_ghosting) {
    const GLint history_texture_format = (!color_correction_active || xbrz_ghosting) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    const GLsizei texture_width  = xbrz_ghosting ? view_width : gba_screen_width;
    const GLsizei texture_height = xbrz_ghosting ? view_height : gba_screen_height;

    glBindTexture(GL_TEXTURE_2D, textures[history_index]);

    glTexImage2D(GL_TEXTURE_2D, 0, history_texture_format, texture_width, texture_height, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    set_texture_params(GL_NEAREST);
  }

  if(xbrz_ghosting) {
    glBindTexture(GL_TEXTURE_2D, textures[xbrz_output_index]);

    glTexImage2D(GL_TEXTURE_2D, 0, texture_format, view_width, view_height, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    set_texture_params(GL_NEAREST);
  }
}

void OGLVideoDevice::CreateShaderPrograms() {
  auto const& video = config->video;

  ReleaseShaderPrograms();

  // TODO: Switch to designated initializers after moving to C++20.
  // Color correction pass
  switch(video.color) {
    case Video::Color::No: break;
    case Video::Color::higan: {
      auto [success, program] = CompileProgram(color_higan_vert, color_higan_frag);
      if(success) {
        shader_passes.push_back({program});
      }
      break;
    }
    case Video::Color::AGB: {
      auto [success, program] = CompileProgram(color_agb_vert, color_agb_frag);
      if(success) {
        shader_passes.push_back({program});
      }
      break;
    }
  }

  // LCD ghosting (interframe blending) pass
  if(video.lcd_ghosting) {
    auto [success, program] = CompileProgram(lcd_ghosting_vert, lcd_ghosting_frag);
    if(success) {
      const GLuint input_texture = video.filter == Video::Filter::xBRZ ? xbrz_output_index : input_index;
      shader_passes.push_back({program, {{input_texture, history_index}}});
    }
  }

  // Output pass (final)
  switch(video.filter) {
    // xBRZ freescale upsampling filter (two passes)
    case Video::Filter::xBRZ: {
      auto [success0, program0] = CompileProgram(xbrz0_vert, xbrz0_frag);
      auto [success1, program1] = CompileProgram(xbrz1_vert, xbrz1_frag);

      if(success0 && success1) {
        if(video.lcd_ghosting) {
          shader_passes.insert(std::prev(shader_passes.end()), {program0});
          shader_passes.insert(
            std::prev(shader_passes.end()),
            {program1, {{output_index, input_index}, xbrz_output_index}});
        } else {
          shader_passes.push_back({program0});
          shader_passes.push_back({program1, {{output_index, input_index}}});
        }
      } else {
        if(success0) glDeleteProgram(program0);
        if(success1) glDeleteProgram(program1);
      }
      break;
    }
    // Sharp bilinear.
    case Video::Filter::Sharp: {
      auto [success, program] = CompileProgram(sharp_bilinear_vert, sharp_bilinear_frag);
      if(success) {
        shader_passes.push_back({program});
      }
      break;
    }
    // Lcd1x filter
    case Video::Filter::Lcd1x: {
      auto [success, program] = CompileProgram(lcd1x_vert, lcd1x_frag);
      if(success) {
        shader_passes.push_back({program});
      }
      break;
    }
    // Plain linear/nearest-interpolated output.
    case Video::Filter::Nearest:
    case Video::Filter::Linear: {
      auto [success, program] = CompileProgram(output_vert, output_frag);
      if(success) {
        shader_passes.push_back({program});
      }
      break;
    }
  }

  UpdateShaderUniforms();
}

void OGLVideoDevice::UpdateShaderUniforms() {
  // Set constant shader uniforms.
  for(const auto& shader_pass : shader_passes) {
    glUseProgram(shader_pass.program);

    const auto input_map = glGetUniformLocation(shader_pass.program, "u_input_map");
    if(input_map != -1) {
      glUniform1i(input_map, 0);
    }

    const auto history_map = glGetUniformLocation(shader_pass.program, "u_history_map");
    if(history_map != -1) {
      glUniform1i(history_map, 1);
    }

    const auto info_map = glGetUniformLocation(shader_pass.program, "u_info_map");
    if(info_map != -1) {
      glUniform1i(info_map, 1);
    }

    const auto output_size = glGetUniformLocation(shader_pass.program, "u_output_size");
    if(output_size != -1) {
      glUniform2f(output_size, view_width, view_height);
    }
  }
}

void OGLVideoDevice::ReleaseShaderPrograms() {
  for(const auto& shader_pass : shader_passes) {
    glDeleteProgram(shader_pass.program);
  }
  shader_passes.clear();
}

auto OGLVideoDevice::CompileShader(
  GLenum type,
  char const* source
) -> std::pair<bool, GLuint> {
  char const* source_array[] = { source };
  
  auto shader = glCreateShader(type);

  glShaderSource(shader, 1, source_array, nullptr);
  glCompileShader(shader);
  
  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if(compiled == GL_FALSE) {
    GLint max_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_length);

    auto error_log = std::make_unique<GLchar[]>(max_length);
    glGetShaderInfoLog(shader, max_length, &max_length, error_log.get());
    Log<Error>("OGLVideoDevice: failed to compile shader:\n{0}", error_log.get());
    return std::make_pair(false, shader);
  }

  return std::make_pair(true, shader);
}

auto OGLVideoDevice::CompileProgram(
  char const* vertex_src,
  char const* fragment_src
) -> std::pair<bool, GLuint> {
  auto [vert_success, vert_id] = CompileShader(GL_VERTEX_SHADER, vertex_src);
  auto [frag_success, frag_id] = CompileShader(GL_FRAGMENT_SHADER, fragment_src);
  
  if(!vert_success || !frag_success) {
    return std::make_pair<bool, GLuint>(false, 0);
  } else {
    auto prog_id = glCreateProgram();

    glAttachShader(prog_id, vert_id);
    glAttachShader(prog_id, frag_id);
    glLinkProgram(prog_id);
    glDeleteShader(vert_id);
    glDeleteShader(frag_id);

    return std::make_pair(true, prog_id);
  }
}

void OGLVideoDevice::SetViewport(int x, int y, int width, int height) {
  view_x = x;
  view_y = y;
  view_width  = width;
  view_height = height;

  UpdateTextures();
  UpdateShaderUniforms();
}

void OGLVideoDevice::SetDefaultFBO(GLuint fbo) {
  default_fbo = fbo;
}

void OGLVideoDevice::Draw(u32* buffer) {
  // Update and bind LCD screen texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textures[input_index]);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gba_screen_width, gba_screen_height,
                  GL_BGRA, GL_UNSIGNED_BYTE, buffer);

  glBindVertexArray(quad_vao);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

  for(auto shader_pass = shader_passes.begin(); shader_pass != shader_passes.end(); ++shader_pass) {
    glUseProgram(shader_pass->program);

    if(shader_pass == std::prev(shader_passes.end())) {
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, default_fbo);
      glViewport(view_x, view_y, view_width, view_height);
    } else {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, textures[shader_pass->textures.output]);

      GLint output_width  = 0;
      GLint output_height = 0;
      glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &output_width);
      glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &output_height);

      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[shader_pass->textures.output], 0);

      glViewport(0, 0, output_width, output_height);
    }

    for(size_t i = 0; i < shader_pass->textures.inputs.size(); ++i) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, textures[shader_pass->textures.inputs[i]]);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    std::swap(textures[input_index], textures[output_index]);
  }

  if(config->video.lcd_ghosting) {
    GLint copy_area_x        = 0;
    GLint copy_area_y        = 0;
    GLsizei copy_area_width  = gba_screen_width;
    GLsizei copy_area_height = gba_screen_height;
    if(config->video.filter == Video::Filter::xBRZ) {
      copy_area_x      = view_x;
      copy_area_y      = view_y;
      copy_area_width  = view_width;
      copy_area_height = view_height;
      glBindFramebuffer(GL_READ_FRAMEBUFFER, default_fbo);
      glReadBuffer(GL_BACK);
    } else {
      glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
      glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[output_index], 0);
      glReadBuffer(GL_COLOR_ATTACHMENT0);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[history_index]);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, copy_area_x, copy_area_y, copy_area_width, copy_area_height);
  }
}

} // namespace nba
