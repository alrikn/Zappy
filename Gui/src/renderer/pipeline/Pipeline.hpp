/**
 * @file renderer/pipeline/Pipeline.hpp
 * @brief Owns the render pass, pipeline layout, and graphics pipeline for the triangle.
 * @details Loads the compiled SPIR-V shaders from SHADERS_DIR, creates the render pass
 *          (describing how the colour attachment is treated), creates an empty pipeline
 *          layout (no descriptor sets for the triangle), then assembles the full
 *          VkPipeline with fixed-function state (viewport, rasteriser, blending, etc.).
 *
 *          Architecture: Pipeline is created before SwapchainContext because the render
 *          pass handle is needed when creating framebuffers. SwapchainContext borrows
 *          the render pass via renderPass() — it must not outlive this object.
 */

#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

/**
 * @brief Encapsulates the VkRenderPass, VkPipelineLayout, and VkPipeline.
 * @details Lifetime: created in Renderer after DeviceContext. Destroyed in Renderer
 *          destructor after SwapchainContext (because SwapchainContext framebuffers
 *          reference the render pass owned here).
 *          Non-copyable, non-movable.
 *
 *          Thread-safety: not thread-safe. Pipeline construction and destruction must
 *          happen on the main thread.
 */
class Pipeline {
public:
    /**
     * @brief Load shaders, create render pass, pipeline layout, and graphics pipeline.
     * @details The two SPIR-V files (triangle.vert.spv and triangle.frag.spv) are loaded
     *          from shadersDir, compiled into temporary VkShaderModule objects, used to
     *          build the pipeline, then immediately destroyed (they are not needed at
     *          draw time — the pipeline has absorbed the compiled code).
     * @param device       The logical device.
     * @param imageFormat  The swapchain surface format (needed for the render pass attachment).
     * @param shadersDir   Path to the directory containing compiled .spv files (SHADERS_DIR).
     * @throws RendererInitException if a shader file cannot be opened or read.
     * @throws RendererVkException on any Vulkan call failure.
     */
    Pipeline(VkDevice device, VkFormat imageFormat, const std::string& shadersDir);

    /**
     * @brief Destroy the pipeline, pipeline layout, and render pass.
     */
    ~Pipeline();

    Pipeline(const Pipeline&)            = delete; ///< Non-copyable.
    Pipeline& operator=(const Pipeline&) = delete; ///< Non-copyable.
    Pipeline(Pipeline&&)                 = delete; ///< Non-movable.
    Pipeline& operator=(Pipeline&&)      = delete; ///< Non-movable.

    /**
     * @brief Return the VkRenderPass handle.
     * @details Needed by SwapchainContext to create framebuffers compatible with this pass.
     * @return Render pass handle valid for the lifetime of this object.
     */
    [[nodiscard]] VkRenderPass     renderPass()     const noexcept;

    /**
     * @brief Return the VkPipelineLayout handle.
     * @details Needed when binding the pipeline and when issuing push constant updates.
     * @return Pipeline layout handle valid for the lifetime of this object.
     */
    [[nodiscard]] VkPipelineLayout pipelineLayout() const noexcept;

    /**
     * @brief Return the VkPipeline handle.
     * @details Bound via vkCmdBindPipeline during command buffer recording.
     * @return Pipeline handle valid for the lifetime of this object.
     */
    [[nodiscard]] VkPipeline       pipeline()       const noexcept;

private:
    /**
     * @brief Read a SPIR-V binary file from disk into a word vector.
     * @details SPIR-V binaries are 4-byte aligned. The file is read as raw bytes and
     *          reinterpreted as uint32_t words for the VkShaderModuleCreateInfo.
     * @param path Absolute path to the .spv file.
     * @return Vector of uint32_t words containing the SPIR-V binary.
     * @throws RendererInitException if the file cannot be opened or read.
     */
    static std::vector<uint32_t> loadSpirv(const std::string& path);

    /**
     * @brief Create a VkShaderModule from SPIR-V words.
     * @details A VkShaderModule is a compiled shader program uploaded into the driver.
     *          The driver may compile it further to native GPU ISA at this point or at
     *          pipeline-creation time (implementation-defined). After the pipeline is
     *          created, the module is no longer needed and should be destroyed.
     * @param device The logical device.
     * @param code   SPIR-V binary words.
     * @return The created VkShaderModule. Caller is responsible for destruction.
     * @throws RendererVkException if vkCreateShaderModule fails.
     */
    static VkShaderModule createShaderModule(VkDevice device,
                                              const std::vector<uint32_t>& code);

    VkDevice         _device{VK_NULL_HANDLE};         ///< Borrowed — not owned.
    VkRenderPass     _renderPass{VK_NULL_HANDLE};      ///< Owned here.
    VkPipelineLayout _pipelineLayout{VK_NULL_HANDLE};  ///< Owned here.
    VkPipeline       _pipeline{VK_NULL_HANDLE};        ///< Owned here.
};
