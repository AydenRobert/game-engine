# 🎮 Game Engine

A custom-built game engine written in C, originally inspired by
[Travis Vroman’s _Kohi Engine_](https://github.com/travisvroman/kohi) series.
This project will eventually aim to provide a sandbox for me to experiment,
nothing in particular is planned at the moment.

---

## 🚧 Current Status

The engine is under active development and currently implements:

- **Core Systems**
    - Logging, memory management, event handling, input, and general utilities
- **Platform Layer**
    - Linux (X11 + Wayland), Win32 in-progress
- **Renderer**
    - Vulkan backend and abstraction layers
    - Command buffers, framebuffers, fences, swapchains, render passes
- **Math Library**
    - Vectors, matrices, and math utilities
- **Container System**
    - Dynamic arrays (with custom allocators)

The engine successfully builds a shared library (`libengine.so`) and a small
**testbed** executable for interactive experiments.

---

## 🗂️ Project Structure

```
.
├── engine/               # Core engine source
│   ├── core/             # Logging, memory, input, events, etc.
│   ├── platform/         # OS abstraction (Win32, Linux X11, Wayland)
│   ├── renderer/         # Renderer API + Vulkan backend
│   ├── math/             # Math primitives (vec, mat, etc.)
│   ├── memory/           # Custom allocators
│   └── containers/       # Data structures (darray, etc.)
│
├── testbed/              # Example client game/application
│   └── src/
│       ├── entry.c
│       ├── game.c
│       └── game.h
│
├── tests/                # Unit tests with lightweight test runner
│   └── src/
│       ├── memory/
│       ├── test_manager.c
│       └── main.c
│
├── assets/               # Shaders, textures, etc.
├── bin/                  # Compiled binaries and runtime output
├── obj/                  # Object files
├── makefile              # Build system
└── README.md
```

---

## 🔧 Building

You’ll need:

- **GCC or Clang**
- **Make**
- **Vulkan SDK**
- **Wayland/X11 development libraries** (on Linux)

### Build Commands

```bash
# Build both the engine and the testbed
make

# Build only the engine
make engine

# Build only the testbed
make testbed

# Build only the tests
make tests

# Run the tests
make run-tests
```

Outputs:

- Shared library: `bin/libengine.so`
- Testbed executable: `bin/testbed`
- Unit tests: `bin/tests`

Run the testbed:

```bash
cd bin
./testbed
```

---

## 💡 Design Goals

- **Learn-by-doing architecture** - staying close to metal.
- **Explicit memory management** - predictable allocations via linear allocators
  and custom memory zones.
- **Platform-abstraction-first** - engine doesn’t assume OS-level details at
  higher layers.
- **Renderer modularity** - backend/frontend separation allows future
  experimentation with other APIs.
- **C-first philosophy** - minimal dependencies, maximum clarity.

---

## 🧭 Roadmap

- [x] Engine core + logger
- [x] Dynamic array container
- [x] Event system
- [x] Vulkan swapchain / renderpass setup
- [ ] Basic rendering abstraction layer
- [ ] Input system improvements
- [ ] Scene management
- [ ] Asset pipeline
- [ ] ECS integration
- [ ] Editor (far future)

---

## 🧱 License

This engine is an educational and experimental project.

---

## 🙏 Acknowledgements

Thanks to **Travis Vroman** and the _Kohi Engine_ series for the inspiration
(definitely not a strong enough word right now) and the clean architectural
foundation.
