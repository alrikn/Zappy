/**
 * @file renderer/pipeline/Pipeline.cpp
 * @brief Implementation of Pipeline: render pass, pipeline layout, graphics pipeline.
 * @details The graphics pipeline is Vulkan's compiled description of the full rendering
 *          state. Once created, it is immutable — changing any state requires creating a
 *          new pipeline. This makes it expensive to create but cheap to use: the GPU
 *          optimises its shader compilation and fixed-function state for exactly this
 *          configuration.
 *
 *          Steps performed in the constructor:
 *            1. Load SPIR-V binaries from disk.
 *            2. Create VkShaderModule objects from the binaries.
 *            3. Create the VkRenderPass with two attachments: colour (index 0) and
 *               depth/stencil (index 1). The depth attachment uses the format probed
 *               by DepthResources. The subpass dependency is extended to cover the
 *               EARLY_FRAGMENT_TESTS stage so depth reads/writes are properly synchronised.
 *            4. Create the VkPipelineLayout (empty — no descriptor sets for this demo).
 *            5. Assemble VkGraphicsPipelineCreateInfo with a populated
 *               VkPipelineDepthStencilStateCreateInfo (depth test ON, depth write ON,
 *               compare op LESS) and call vkCreateGraphicsPipelines.
 *            6. Destroy the VkShaderModule objects (pipeline has absorbed the code).
 *
 *          Architecture: no other file in the project calls vkCreateGraphicsPipelines
 *          or vkCreateRenderPass. This encapsulation allows future features to add
 *          additional pipelines (e.g. for tiles, ImGui) without touching this file.
 */

#include "renderer/pipeline/Pipeline.hpp"
#include "renderer/device/VkCheck.hpp"
#include "exceptions.hpp"

#include <array>
#include <spdlog/spdlog.h>
#include <fstream>

// ─── static helpers ───────────────────────────────────────────────────────────

/**
 * @brief Read a SPIR-V binary file from disk into a uint32_t word vector.
 * @param path Absolute path to the .spv file.
 * @return Vector of uint32_t words (SPIR-V is 4-byte aligned).
 */
std::vector<uint32_t> Pipeline::loadSpirv(const std::string& path)
{
    // std::ios::ate: open at the end so tellg() gives us the file size immediately.
    // std::ios::binary: read raw bytes, no newline translation.
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw RendererInitException("Failed to open shader file: " + path);
    }

    const std::size_t fileSize = static_cast<std::size_t>(file.tellg());
    if (fileSize == 0) {
        throw RendererInitException("Shader file is empty: " + path);
    }

    // SPIR-V format: 32-bit words. The file size must be a multiple of 4.
    // We allocate in uint32_t units and read the raw bytes into the underlying storage.
    std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()),
              static_cast<std::streamsize>(fileSize));

    if (!file) {
        throw RendererInitException("Failed to read shader file: " + path);
    }

    spdlog::debug("Pipeline::loadSpirv: loaded '{}' ({} bytes)", path, fileSize);
    return code;
}

/**
 * @brief Create a VkShaderModule from SPIR-V words.
 * @param device The logical device.
 * @param code   SPIR-V binary words.
 * @return The created VkShaderModule.
 */
VkShaderModule Pipeline::createShaderModule(VkDevice device,
                                             const std::vector<uint32_t>& code)
{
    // VkShaderModule: a compiled shader program object.
    // Physically: the driver copies the SPIR-V binary into GPU-accessible memory.
    // Some drivers (e.g. AMD RDNA) do JIT compilation to native ISA here;
    // others (e.g. NVIDIA) defer compilation until pipeline creation.
    // After the pipeline is created, the module is no longer referenced by the
    // driver — it is safe to destroy.
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode    = code.data();

    VkShaderModule module{VK_NULL_HANDLE};
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &module));
    return module;
}

// ─── constructor ──────────────────────────────────────────────────────────────

/**
 * @brief Load shaders, create render pass, pipeline layout, and graphics pipeline.
 * @param device       The logical device.
 * @param colorFormat  The swapchain surface format.
 * @param depthFormat  The depth image format selected by DepthResources.
 * @param shadersDir   Path to the directory containing compiled .spv files.
 */
Pipeline::Pipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat,
                   const std::string& shadersDir)
    : _device(device)
{
    // ── Load shader modules ───────────────────────────────────────────────────
    const std::string vertPath = shadersDir + "/triangle.vert.spv";
    const std::string fragPath = shadersDir + "/triangle.frag.spv";

    const auto vertCode = loadSpirv(vertPath);
    const auto fragCode = loadSpirv(fragPath);

    // VkShaderModule: temporary containers for the SPIR-V bytecode.
    // We create them, reference them during pipeline creation, then immediately destroy them.
    VkShaderModule vertModule = createShaderModule(_device, vertCode);
    VkShaderModule fragModule = createShaderModule(_device, fragCode);

    // ── Render pass creation ──────────────────────────────────────────────────
    //
    // VkRenderPass: describes the attachments (colour, depth, stencil) that a group
    // of draw commands will read from and write to, and the layout transitions they
    // undergo. It does not allocate any memory — it is a blueprint.
    //
    // Physically: the GPU uses the render pass to schedule memory bandwidth optimisations
    // (e.g. tile-based deferred rendering on mobile GPUs). On desktop GPUs it primarily
    // informs the driver about image layout transitions and allows it to elide unnecessary
    // loads and stores (e.g. DONT_CARE avoids reading old depth data back from DRAM).

    // ── Attachment 0: swapchain colour buffer ─────────────────────────────────
    //
    // VkAttachmentDescription: declares one attachment slot in the render pass.
    // The slot index (0 here) must match the pAttachments[] index in the
    // VkFramebufferCreateInfo and the attachment index in the VkAttachmentReference.
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = colorFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT; // no MSAA
    // CLEAR: discard old contents and fill with the clear colour at render pass begin.
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // STORE: keep the rendered pixels in memory so the swapchain can present them.
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    // Stencil: not used for the colour attachment. DONT_CARE avoids stencil bandwidth.
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // UNDEFINED: we don't care about the previous image layout — CLEAR overwrites anyway.
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    // PRESENT_SRC_KHR: the layout the swapchain expects before vkQueuePresentKHR.
    // The driver inserts a layout transition at render pass end to achieve this.
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // ── Attachment 1: depth/stencil buffer ────────────────────────────────────
    //
    // A second VkAttachmentDescription for the depth image. The GPU writes one
    // depth value per pixel here during the EARLY_FRAGMENT_TESTS stage and reads it
    // back for the compare operation on each subsequent fragment.
    //
    // loadOp = CLEAR: reset all depth values to the far-plane value (1.0) at the
    //   start of every frame. Without a clear, old depth data from the previous
    //   frame would survive, causing geometry from frame N to incorrectly occlude
    //   geometry from frame N+1.
    // storeOp = DONT_CARE: the depth buffer is not sampled after the render pass
    //   ends. Telling the driver we don't care lets tile-based GPUs skip writing the
    //   depth tile back to main memory, saving bandwidth.
    // initialLayout = UNDEFINED: same reasoning as the colour attachment.
    // finalLayout = DEPTH_STENCIL_ATTACHMENT_OPTIMAL: the depth image stays in
    //   its optimal layout for depth attachment use. It does not need to transition
    //   to PRESENT_SRC_KHR because we never present it directly.
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = depthFormat;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // VkAttachmentReference: the colour subpass references attachment slot 0.
    // COLOR_ATTACHMENT_OPTIMAL: the layout the image is in during rendering — the
    // GPU can read/write it with optimal cache behaviour in the colour write stage.
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // VkAttachmentReference for the depth attachment: slot 1.
    // DEPTH_STENCIL_ATTACHMENT_OPTIMAL: the layout the depth image uses during
    // the early fragment test. The GPU needs the image in this layout to perform
    // hardware depth testing (compare + conditional write).
    // Skipping this reference: the subpass would have no depth attachment, the
    // GPU would not perform depth testing, and closer geometry would NOT occlude
    // farther geometry — the whole purpose of this feature would be broken.
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // VkSubpassDescription: one subpass — all draw commands write to both attachments.
    // GRAPHICS: this is a graphics subpass (as opposed to COMPUTE or RAYTRACE).
    // pDepthStencilAttachment: pointer to exactly one depth/stencil reference (not an array).
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // VkSubpassDependency: synchronisation between the implicit "before render pass"
    // pseudo-subpass (VK_SUBPASS_EXTERNAL) and subpass 0.
    //
    // srcStageMask covers both COLOR_ATTACHMENT_OUTPUT and EARLY_FRAGMENT_TESTS.
    //   COLOR_ATTACHMENT_OUTPUT: wait for the swapchain image to be writable
    //     (the presentation engine may still be reading it from the last frame).
    //   EARLY_FRAGMENT_TESTS: wait for any previous depth read/write on this image
    //     to complete before our subpass starts accessing it. Without this, a GPU
    //     that re-orders rendering work could start writing depth data before a
    //     previous frame's depth read is finished — silent depth-value corruption.
    //
    // dstAccessMask covers DEPTH_STENCIL_ATTACHMENT_WRITE_BIT in addition to
    //   COLOR_ATTACHMENT_WRITE_BIT. This tells the GPU that our subpass will write
    //   to the depth attachment, completing the dependency declaration.
    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                             | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Gather both attachment descriptions into an array ordered by slot index.
    // The array index = the attachment slot index referenced in attachment references
    // and in the VkFramebufferCreateInfo::pAttachments array.
    const std::array<VkAttachmentDescription, 2> attachments = {
        colorAttachment,
        depthAttachment,
    };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    VK_CHECK(vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass));
    spdlog::debug("Pipeline: VkRenderPass created (colour + depth attachments).");

    // ── Pipeline layout creation ──────────────────────────────────────────────
    //
    // VkPipelineLayout: describes the interface between the CPU and the shaders —
    // which descriptor sets and push constants the shaders expect.
    // For this triangle there are none: empty layout, zero bindings.
    // This layout must match the shader's layout qualifiers (the triangle shaders
    // have no uniforms, so an empty layout is correct).
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 0;
    pipelineLayoutInfo.pSetLayouts            = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = nullptr;

    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout));
    spdlog::debug("Pipeline: VkPipelineLayout created.");

    // ── Shader stage setup ────────────────────────────────────────────────────
    //
    // VkPipelineShaderStageCreateInfo: tells the pipeline which shader module to
    // use at each programmable stage and which entry point function to call.
    // "main" is the conventional entry point name in GLSL.
    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertModule;
    vertStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragModule;
    fragStageInfo.pName  = "main";

    const VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};

    // ── Vertex input state ────────────────────────────────────────────────────
    //
    // VkPipelineVertexInputStateCreateInfo: describes the format of vertex data coming
    // from vertex buffers. For this triangle, all vertices are hardcoded in the vertex
    // shader via gl_VertexIndex — there is no vertex buffer. Both binding and attribute
    // descriptions are empty.
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 0;
    vertexInputInfo.pVertexBindingDescriptions      = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

    // ── Input assembly state ──────────────────────────────────────────────────
    //
    // VkPipelineInputAssemblyStateCreateInfo: how vertices are assembled into primitives.
    // TRIANGLE_LIST: every 3 consecutive vertices form one triangle (no index sharing).
    // primitiveRestartEnable=VK_FALSE: not using strip/fan primitives with restart indices.
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ── Viewport and scissor (dynamic) ────────────────────────────────────────
    //
    // We use dynamic viewport and scissor so the pipeline does not bake the window
    // dimensions into its state. This is required for swapchain resize support in a
    // future feature. The actual values are set via vkCmdSetViewport/vkCmdSetScissor
    // during command buffer recording.
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // ── Rasterisation state ───────────────────────────────────────────────────
    //
    // VkPipelineRasterizationStateCreateInfo: controls how triangles are filled.
    // FILL: fill the triangle with fragments (as opposed to WIREFRAME or POINT).
    // BACK: cull back-facing triangles. The triangle winding in triangle.vert
    //       is counter-clockwise in clip space → front-facing.
    // No depth bias — not needed for a flat 2D triangle.
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode  = VK_CULL_MODE_NONE;    // 2D triangle has no back face; culling disabled
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    // ── Multisampling state ───────────────────────────────────────────────────
    //
    // MSAA disabled: 1 sample per pixel. Anti-aliasing is a future concern.
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ── Colour blend state ────────────────────────────────────────────────────
    //
    // VkPipelineColorBlendAttachmentState: how the fragment shader output colour is
    // combined with the existing framebuffer colour.
    // blendEnable=VK_FALSE: no blending — the fragment output is written directly.
    // colorWriteMask: all four channels (R, G, B, A) are written.
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                                        | VK_COLOR_COMPONENT_G_BIT
                                        | VK_COLOR_COMPONENT_B_BIT
                                        | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable     = VK_FALSE;
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &colorBlendAttachment;

    // ── Dynamic state ─────────────────────────────────────────────────────────
    //
    // VkPipelineDynamicStateCreateInfo: which pipeline states can be changed at
    // draw time without recreating the pipeline. Viewport and scissor are dynamic
    // so we can handle window resize in a future feature without rebuilding the pipeline.
    const VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    // ── Depth/stencil state ───────────────────────────────────────────────────
    //
    // VkPipelineDepthStencilStateCreateInfo: controls the fixed-function depth test
    // and stencil test stages that run per fragment.
    //
    // depthTestEnable = VK_TRUE: before writing a fragment's colour, the GPU compares
    //   the fragment's depth value against the current value in the depth buffer at
    //   that pixel. If the fragment loses the comparison it is discarded — its colour
    //   is never written, and the depth buffer is not updated for that pixel.
    //   Physically: the EARLY_FRAGMENT_TESTS stage reads the depth buffer, compares,
    //   and either kills the fragment or allows it to proceed to the fragment shader.
    //   On most GPUs this happens in dedicated fixed-function hardware, not a shader.
    //   Skipping (leaving nullptr): depth testing is disabled — fragments are written
    //   in draw-submission order, causing farther geometry to overwrite nearer geometry
    //   whenever it happens to be submitted last.
    //
    // depthWriteEnable = VK_TRUE: when a fragment passes the depth test, write its
    //   depth value back to the depth buffer. This "claims" that pixel for future
    //   fragments to test against. Without writes, the depth buffer would stay at its
    //   cleared value (1.0) forever and every fragment would pass the test — effectively
    //   disabling depth testing for all but the first fragment at each pixel.
    //
    // depthCompareOp = VK_COMPARE_OP_LESS: the comparison rule. A fragment passes if
    //   its depth is LESS than the stored depth value, i.e. it is CLOSER to the camera.
    //   The depth buffer is cleared to 1.0 at the start of every frame (the farthest
    //   possible depth value), so the very first fragment at any pixel always passes.
    //   Using GREATER instead would give a "reversed depth" buffer (nearer = higher
    //   value after perspective divide) — useful for precision but not our convention.
    //
    // Stencil operations: disabled. Stencil testing is a separate feature (masking
    //   regions of the screen). Setting all stencil ops to KEEP and compareOp to ALWAYS
    //   makes the stencil test a no-op.
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_TRUE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;  // no depth-bounds clamping
    depthStencil.stencilTestEnable     = VK_FALSE;  // stencil not used in this feature
    // front/back stencil ops: zero-initialised (all KEEP, compareOp NEVER) — harmless
    // because stencilTestEnable = VK_FALSE makes them irrelevant.

    // ── Graphics pipeline creation ────────────────────────────────────────────
    //
    // vkCreateGraphicsPipelines: the most expensive Vulkan call in this feature.
    // Physically: the driver links the shader modules (compiling to native GPU ISA
    // if it hasn't already), bakes all the fixed-function state into GPU command
    // sequences, and stores the result as an opaque pipeline object. All future draw
    // calls with this pipeline skip the compilation step — it is done once here.
    //
    // Skipping this step: there is nothing to bind during rendering; vkCmdBindPipeline
    // would have no valid object and subsequent draw calls would produce undefined results.
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = _pipelineLayout;
    pipelineInfo.renderPass          = _renderPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;  // not deriving from another pipeline

    VK_CHECK(vkCreateGraphicsPipelines(
        _device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline));

    spdlog::info("Pipeline: VkPipeline created.");

    // ── Destroy shader modules ────────────────────────────────────────────────
    //
    // Shader modules are only needed during pipeline creation. Once the pipeline
    // has been compiled, the SPIR-V bytecode is no longer referenced by the driver.
    // Destroying the modules here frees the memory used to hold the bytecode.
    // The pipeline itself continues to work — it has already compiled the shaders.
    vkDestroyShaderModule(_device, fragModule, nullptr);
    vkDestroyShaderModule(_device, vertModule, nullptr);
    spdlog::debug("Pipeline: shader modules destroyed (no longer needed).");
}

// ─── destructor ───────────────────────────────────────────────────────────────

/**
 * @brief Destroy the pipeline, pipeline layout, and render pass.
 */
Pipeline::~Pipeline()
{
    // Destroy in reverse creation order.
    if (_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(_device, _pipeline, nullptr);
        spdlog::debug("Pipeline: VkPipeline destroyed.");
    }
    if (_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
        spdlog::debug("Pipeline: VkPipelineLayout destroyed.");
    }
    if (_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(_device, _renderPass, nullptr);
        spdlog::debug("Pipeline: VkRenderPass destroyed.");
    }
}

// ─── accessors ────────────────────────────────────────────────────────────────

/**
 * @brief Return the VkRenderPass handle.
 * @return Render pass handle valid for the lifetime of this object.
 */
VkRenderPass Pipeline::renderPass() const noexcept
{
    return _renderPass;
}

/**
 * @brief Return the VkPipelineLayout handle.
 * @return Pipeline layout handle valid for the lifetime of this object.
 */
VkPipelineLayout Pipeline::pipelineLayout() const noexcept
{
    return _pipelineLayout;
}

/**
 * @brief Return the VkPipeline handle.
 * @return Pipeline handle valid for the lifetime of this object.
 */
VkPipeline Pipeline::pipeline() const noexcept
{
    return _pipeline;
}
