
#pragma once

#include "device/shader/common.glsl.hpp"

constexpr auto color_higan_vert = common_vert;

constexpr auto color_higan_frag = R"(
  #version 330 core

  layout(location = 0) out vec4 frag_color;

  in vec2 v_uv;

  uniform sampler2D u_input_map;

  void main() {
    vec4 color = texture(u_input_map, v_uv);

    color.rgb = pow(color.rgb, vec3(4.0));

    frag_color = vec4(
      1.000 * color.r + 0.196 * color.g,
      0.039 * color.r + 0.901 * color.g + 0.117 * color.b,
      0.196 * color.r + 0.039 * color.g + 0.862 * color.b,
      1.0);
  }
)";