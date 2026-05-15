# Core

## Allocator Design

Allocator design is a critical component of Farfocel's memory management. The engine uses a custom allocator interface (`fr::Alloc`) to manage memory efficiently. Allocators are polymorphic and may be stateful. Using polymorphic allocators simplifies memory management across DLL boundaries and enables specialized memory arenas without templating container types.

All core owning containers (where "owning" means the container manages its own raw memory) must adhere to the following contract:

### 1. Interface & Storage Contract

- The allocator is stored as a private/protected member variable of type `Alloc* m_alloc`.
- The allocator is initialized to `get_ambient_ctx().alloc` by default.
- Every owning container provides an explicit constructor that accepts an `Alloc*`.
- Every owning container provides a static factory method `with_alloc(Alloc*)`.
- Every owning container provides a `const Alloc* alloc() const noexcept` getter to access the allocator.

### 2. Propagation Rules

Compiler-generated copy and move operations are **fatal** for objects managing raw memory (they lead to double-frees and memory leaks). All owning containers **MUST** explicitly implement or `= delete` their copy/move constructors and assignment operators according to these propagation rules:

- **Copy Construction:** Allocators **DO NOT** propagate.
  - The newly constructed container uses `get_ambient_ctx().alloc` (unless an allocator is explicitly passed to a specific constructor). It then deep-copies the elements.
- **Move Construction:** Allocators **DO** propagate.
  - The newly constructed container steals both the memory pointer and the `m_alloc` pointer from the source. The source is left in a valid, empty state.
- **Copy Assignment:** Allocators **DO NOT** propagate.
  - The destination container keeps its existing allocator. It allocates new memory (if necessary) using its own allocator and deep-copies the elements.
- **Move Assignment:** Allocators **DO NOT** propagate.
  - **Fast-Path:** If `this->m_alloc == other.m_alloc`, the destination safely steals the memory pointer from the source.
  - **Slow-Path:** If `this->m_alloc != other.m_alloc`, the destination _cannot_ steal the memory. It must keep its own allocator, clear its current contents, allocate new memory, and perform an element-wise move from the source.
