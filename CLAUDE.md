# Project Rules for Vulkan Fluid Sim Smoke Test

- ALWAYS keep modular structure: small focused components (VulkanInstance, VulkanDevice, VulkanQueue, VulkanSwapchain, VulkanPipeline, etc.).
- VulkanCore is ONLY the orchestrator: owns components via unique_ptr, handles lifetime and init order.
- NEVER delete or consolidate components into monolith unless I explicitly say "ok to simplify to single file".
- When stuck, propose smaller steps within existing components instead of deleting.
- Prefer explicit dependency injection (pass dependencies to component constructors) over global state.
