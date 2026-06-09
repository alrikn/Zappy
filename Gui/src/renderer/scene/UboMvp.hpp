/**
 * @file renderer/scene/UboMvp.hpp
 * @brief Plain data struct for the MVP Uniform Buffer Object payload.
 * @details Defines the exact memory layout that the GPU reads from descriptor binding 0
 *          in triangle.vert. The struct is 128 bytes (two mat4), which is the guaranteed
 *          minimum for std140-aligned UBOs. No model matrix is included here — per-object
 *          transforms will be handled separately in a later feature.
 *
 *          Architecture: this header is included by UniformBuffer (to know the write size)
 *          and by Renderer (to assemble the UboMvp before writing it). It has no Vulkan
 *          dependency — only GLM — so it is safe to include anywhere in the codebase.
 */

#pragma once

#include <glm/glm.hpp>

/**
 * @brief GPU-side Uniform Buffer Object layout for the view–projection transform.
 * @details Contains view and projection matrices only. Model transforms are per-object
 *          and are not included here.
 *
 *          std140 layout rule: each mat4 is stored as 4 columns of vec4 (16 bytes each),
 *          giving 64 bytes per matrix. Total struct size = 128 bytes exactly.
 *          The field order here must match the GLSL declaration in triangle.vert:
 *
 *          @code
 *          layout(set=0, binding=0) uniform UboMvp {
 *              mat4 view;
 *              mat4 proj;
 *          } ubo;
 *          @endcode
 *
 *          Lifetime: value type — constructed on the stack in Renderer::uploadUbo(),
 *          immediately copied into the persistently-mapped VMA buffer via memcpy.
 *          Not stored long-term; recreated each frame.
 */
struct UboMvp {
    glm::mat4 view; ///< World-space → camera-space transform (from Camera::viewMatrix()).
    glm::mat4 proj; ///< Camera-space → clip-space transform (from Camera::projMatrix()).
};
