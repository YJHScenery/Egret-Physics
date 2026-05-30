# Egret Physics

![Version](https://img.shields.io/badge/version-0.1.0-blue.svg)
![License](https://img.shields.io/badge/license-GPLv3-green.svg)

A modern physics simulation engine with rigid body dynamics, collision detection, and constraint solving.

![program_screenshot_v0.0.1](./README.assets/program_screenshot_v0.0.1.png)

## Features

- **Rigid Body Dynamics**: Support for various rigid body shapes including spheres, boxes, cylinders, rings, disks, rods, and shells
- **Collision Detection**: Comprehensive collision detection system with multiple shape combinations
- **Constraint Solving**: Support for constraints including support surfaces, sliding rails, connecting lines, and connecting rods
- **Physics Fields**: Gravity and gravitational field support
- **Simulation Strategy**: Configurable broad-phase strategy, contact resolver, and integrator strategy
- **Qt-based UI**: Modern QML-based user interface with 3D visualization support

## Dependencies

| Library | Version | License |
|---------|---------|---------|
| Boost | 1.90.0 | Boost Software License 1.0 |
| Eigen | 5.0.0 | Mozilla Public License 2.0 |
| magic_enum | 0.9.8 | MIT License |
| Qt | 6.9.3 | LGPLv3/GPLv3 |

## Quick Start

### Prerequisites

- CMake 3.30.1 or later
- Qt 6.9.3 (with Core, Gui, Widgets, Quick, Qml, Quick3D, Sql modules)
- MSVC 2022 (Windows) or compatible C++ compiler

### Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `BUILD_TESTS` | Build unit tests | ON |
| `DEBUG_SIMULATE_RELEASE` | Simulate Release behavior in Debug build | OFF |

### Running the Application

After building, run the executable from the build directory:

```bash
# Windows
./Release/Egret_Physics.exe

# Linux (if applicable)
./Egret_Physics
```

## Project Structure

```
Egret Physics/
├── src/                    # Source code
│   ├── main.cpp            # Application entry point
│   ├── model/              # Core physics model
│   │   ├── physics/        # Physics engine components
│   │   │   ├── rigid/      # Rigid body systems
│   │   │   ├── field/      # Physics fields
│   │   │   └── constraints/# Constraints
│   │   ├── solver/         # Simulation solver
│   │   ├── scene/          # Scene management
│   │   └── strategy/       # Simulation strategies
│   ├── view_model/         # View models for UI
│   ├── view/               # UI views
│   └── utils/              # Utility functions
├── dependency/             # Third-party dependencies
│   └── include/            # Header-only libraries
├── resources/              # Resources (icons, RC files)
├── tests/                  # Unit tests
└── CMakeLists.txt          # Build configuration
```

## Usage

### Creating a Physics Scene

```cpp
#include "model/scene/world_scene_manager.h"
#include "model/solver/solver.h"
#include "model/strategy/integrator_strategy/semi_implicit_euler_integrator.h"
#include "model/strategy/broad_phase_strategy/brute_force_broad_phase.h"
#include "model/strategy/contact_strategy/frictionless_contact_resolver.h"

// Create solver with default configuration
egret::SolverConfig config{};
auto integrator = std::make_unique<egret::SemiImplicitEulerIntegrator>();
auto broadPhase = std::make_unique<egret::BruteForceBroadPhase>();
auto contactResolver = std::make_unique<egret::FrictionlessContactResolver>();

auto solver = std::make_unique<egret::Solver>(
    config,
    std::move(integrator),
    std::move(broadPhase),
    std::move(contactResolver)
);

// Create scene manager
auto scene = std::make_unique<egret::WorldSceneManager>(std::move(solver));

// Add a gravity field
scene->addGravityField({0, -9.81, 0}, {0, 0, 0}, "Gravity");

// Spawn a sphere at position (0, 10, 0) with radius 1.0 and mass 1.0
std::uint64_t sphereId = scene->spawnSphere("Sphere", {0, 10, 0}, {0, 0, 0}, 1.0, 1.0);

// Spawn a ground box (mass = 0 means static)
std::uint64_t groundId = scene->spawnBox("Ground", {0, -1, 0}, {0, 0, 0}, {10, 1, 10}, 0.0);

// Run simulation (10 seconds at 60 FPS)
for (int i = 0; i < 600; ++i) {
    egret::SolverStepResult result = scene->tick(1.0 / 60.0);
    // Access simulation stats if needed
    // std::cout << "Bodies: " << result.stats.bodyCount << std::endl;
}
```

### Adding Constraints

```cpp
// Create a simple pendulum constraint
// Anchor position at (0, 5, 0), connecting to the sphere
std::uint64_t pendulumId = scene->createSimplePendulum("Pendulum", 3.0, {0, 5, 0}, sphereId);

// Create two spheres connected by a line
std::uint64_t bodyA = scene->spawnSphere("Ball A", {-2, 5, 0}, {0, 0, 0}, 0.5, 1.0);
std::uint64_t bodyB = scene->spawnSphere("Ball B", {2, 5, 0}, {0, 0, 0}, 0.5, 1.0);
std::uint64_t lineId = scene->createConnectingLine("Connector", 5.0, bodyA, bodyB);

// Enable/disable constraints
scene->setConstraintEnabled(pendulumId, false);
```

### Working with Entity Properties

```cpp
// Get and set body position
Eigen::Vector3d pos = scene->getBodyPosition(sphereId).value_or({0, 0, 0});
scene->setBodyPosition(sphereId, {0, 20, 0});

// Get simulation time and step count
double simTime = scene->getSimulationTime();
std::size_t stepCount = scene->getStepCount();

// Clear the scene
scene->clear();
```

## Running Tests

To run the unit tests:

```bash
cd build
ctest -C Release
```

## License

This project is licensed under the GNU General Public License v3.0. See [GPL_V3.0.md](GPL_V3.0.md) for details.

For third-party library licenses, see [NOTICE](NOTICE).

## Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Make your changes and add tests
4. Ensure all tests pass
5. Submit a pull request

## Code Style

This project follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with the following exceptions:
- Use 4 spaces for indentation (not tabs)
- Line length limit is 120 characters

## Contact

For questions or issues, please open an issue in the repository.
