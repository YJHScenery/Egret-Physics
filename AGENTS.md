# AGENTS.md

## General Guidelines

- **Environment**: This project runs on **Windows**. All commands must be written in **PowerShell** and executed only
  via PowerShell.
- **Language for Responses and Comments**: Use **Chinese** for all responses, explanations, and code comments.
- **Code Naming**: Use **English** for naming classes, functions, objects, and files.
- **Documentation**: Every newly created class, function, or file **must** include Doxygen comments. Write the comment
  content in Chinese as required above.

## Build Instructions (CMake with Visual Studio 2026)

### Build Environment Setup

Execute the following command in **cmd** (NOT PowerShell) to configure Visual Studio 2026 environment:

```
%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
```

After the environment is configured, ensure you are in the folder containing `CMakeLists.txt`.

### Initial CMake Configuration (First Time Only)

Use **cmd** instead of PowerShell in this step, based on the last step which configure the Visual Studio 2026
environment.

Run this command only if the `build` folder does not exist or is empty:

```
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
```

### Build the Main Target

```
cmake --build build --config Release --target Egret_Physics
```

### Run the Executable

```
./build/Release/Egret_Physics.exe
```

## Project Overview

The project directory structure is organized as follows:

### Code Folder Descriptions

| Folder                           | Purpose                                                   |
|----------------------------------|-----------------------------------------------------------|
| `src/model/physics/rigid/`       | Rigid body dynamics, mass, inertia, collision detection   |
| `src/model/physics/field/`       | Force fields (gravity, electromagnetic, custom fields)    |
| `src/model/physics/constraints/` | Joints, springs, constraints between bodies               |
| `src/model/solver/`              | Numerical integrators                                     |
| `src/model/scene/`               | Scene graph, object management, spatial partitioning      |
| `src/model/strategy/`            | Pluggable simulation strategies                           |
| `src/view_model/`                | Data models that bridge physics simulation and UI         |
| `src/view/`                      | QML views, UI components, visualizers                     |
| `src/utils/`                     | Logging, math helpers, memory utilities, platform helpers |
| `tests/`                         | Unit tests for all core components                        |
| `dependency/include/`            | External header-only libraries                            |

## Logging System

The project uses a **dual logging system** for debugging and analysis.

### Log File Locations

Log files are stored in the `logs/` directory located in the same folder as the executable:

- **If AI builds the project**: `build/Release/logs/`
- **If user builds the project**: `cmake-build-release/logs/`

### Log Files

| Log File                 | Output Source                             | Priority                      |
|--------------------------|-------------------------------------------|-------------------------------|
| `logs/qdebug_output.log` | `qDebug()` in C++, `console.log()` in QML | **Primary** - read this first |
| `logs/async_logger.log`  | Custom asynchronous logging system        | Secondary                     |

### Logging Usage Guidelines

1. **When debugging issues**, always check `logs/qdebug_output.log` first
2. If relevant information is not found there, then check `logs/async_logger.log`
3. Both log files are created automatically when the application runs
4. The `logs/` directory is created automatically if it does not exist

### Implementation Notes

- The custom async logger is non-blocking and suitable for performance-critical simulation code
- `qDebug()` output is automatically redirected to `qdebug_output.log` on Windows
- QML's `console.log()` output is also captured in the same `qdebug_output.log` file

## Code Formatting and Style Rules

1. **File names**

- C++ files (`.h`, `.cpp`, `.hpp`, etc.): `snake_case`
- QML files: `PascalCase` (e.g., `MainWindow.qml`)

2. **Type names** (class, struct, enum, enum class, union): `PascalCase`

3. **Function names and local variables**: `camelCase` (lowercase first letter, e.g., `getValue`)

4. **Non‑compile‑time static variables** (both constant and non‑constant, e.g., `static int s_counter`):  
   Prefix `s_` + `PascalCase` (e.g., `s_MyStaticVar`)

5. **Compile‑time constants or Macro Identifier** (`constexpr` or `#define` macros): `UPPER_SNAKE_CASE`

6. **Class member variables**: Prefix `m_` + `camelCase` (e.g., `m_myMember`)

7. **Class static member variables**: Prefix `s_` + `PascalCase` (e.g., `s_MyStaticVar`)

8. **Braces for class, struct, enum, union, and function definitions**

- Opening brace on a new line
- Closing brace on a new line
  ```
  class MyClass
  {
  // ...
  };
  ```

9. **Braces for control flow statements** (`if`, `while`, `do‑while`, `for`, `range‑for`, `switch‑case`)

- Opening brace after the construct, preceded by a space, same line
- Closing brace on its own line
  ```
  if (condition) {
  // ...
  }
  ```

10. **Always use braces** for control flow statements, even if the body contains only one line.

11. **Variable initialization**

- Use brace initialization (`{}`) instead of the assignment operator to avoid implicit conversions.
  ```
  int x{42};        // Good
  int y = 42;       // Not allowed
  ```

12. **Narrowing conversions**

- Must be performed explicitly using `static_cast`.

13. **Memory management – `new` and `delete`**

- **Prohibited**, including when constructing standard library smart pointers (`std::unique_ptr`, `std::shared_ptr`),
  when you should use `std::make_shared` or `std::make_unique` instead.
- **Exceptions** allowed only in the following cases:
    1. Creating `QSharedPointer` or `QScopedPointer` instances.
    2. When Qt's object tree mechanism guarantees safe release (e.g., parent‑child ownership).
    3. When a third‑party library explicitly requires a pointer created by `new`.
    4. When unavoidable in QML – in such cases, call `QQmlEngine::setObjectOwnership(..., QQmlEngine::CppOwnership)` to
       transfer ownership to C++.

14. **Compile‑time evaluation**

- Use `constexpr` or C++20 metaprogramming techniques for compile‑time constants and functions.
- **Do not use `#define` to define constants.**

15. **Preprocessor directives (`#define`, etc.)**

- May only be used for **compilation control** (e.g., header guards, platform‑specific code) and **platform management
  **.

---
