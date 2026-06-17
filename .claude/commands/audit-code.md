Perform a thorough C++ code quality and bug-hunting audit on specified files, or on all `.cpp` / `.hpp` files in `src/` and `libs/`.

## What to Check

### Memory & Resource Safety

- Uninitialised variables
- Use-after-free or dangling pointers
- Missing `nullptr` / `VK_NULL_HANDLE` checks before use
- Resource leaks (Vulkan handles, native window handles, memory allocations)
- Missing cleanup on error paths (an early `return false` that leaks a handle created earlier in the same function)
- Double-free or double-destroy of Vulkan objects
- Handles destroyed in reverse construction order (verify each `destroy()` frees the handles it owns, and that teardown unwinds in the reverse order of construction)
- `m_dying`-style guards respected so a worker or callback cannot touch a half-torn-down object

This project uses raw volk handles with manual `destroy()` and `VK_NULL_HANDLE`. Do NOT suggest
converting to `vk::raii` or vulkan-hpp; instead verify the existing manual ownership is correct.

### Vulkan-Specific

- Every `VkResult` from `vkCreate*` / `vkAllocate*` / `vkEnumerate*` / etc. MUST be checked. On failure
  the function must fill `out_error_message` and `return false` (no exceptions in production code).
- Incorrect struct `sType` fields
- Uninitialised or non-zeroed Vulkan create-info structs
- Incorrect queue family indices (graphics vs present)
- Swapchain image count assumptions
- Missing synchronisation (fences, semaphores, pipeline barriers) once rendering exists
- Using device features/extensions without first checking they are supported
- Validation-layer / debug-messenger output routed to the logger only in debug builds

### Logic Errors

- Off-by-one errors in loops and array indexing
- Integer overflow/underflow
- Incorrect operator precedence
- Unreachable code or dead branches
- Missing `break` in switch cases
- Implicit narrowing conversions

### C++ Best Practices

- Raw `new`/`delete` instead of smart pointers or RAII (factories return `std::unique_ptr`)
- C-style casts instead of `static_cast`/`reinterpret_cast`
- Missing `const` where appropriate
- Missing `[[nodiscard]]` on functions returning a status / error indicator
- Using `NULL` instead of `nullptr`
- Use of exceptions in production code (allowed only in test code under `libs/testing`)
- Cross-component callbacks must use C-style function pointers plus `void* user_data`, not `std::function`

### Platform-Specific

- Windows (Win32) and Linux (XCB) code paths both present in `#ifdef` blocks (one platform's branch missing is a bug)
- Platform-specific types used without guards
- Native handles leaking through public headers (consumers should only see `void* nativeHandle()` / `void* nativeDisplay()`)
- Hardcoded paths or assumptions about OS behaviour

## Output Format

For each issue found, report:

1. **File and line number**
2. **Severity** (critical / warning / suggestion)
3. **Description** of the issue
4. **Suggested fix**

If no files are specified via $ARGUMENTS, audit all `.cpp` and `.hpp` files in `src/` and `libs/`. Summarise with a count of issues by severity at the end.
