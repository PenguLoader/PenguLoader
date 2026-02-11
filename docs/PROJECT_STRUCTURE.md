# Project Structure

This document describes the reorganized project structure of Pengu Loader.

## Directory Layout

```
PenguLoader/
├── core/                  # C++ core module (injected into League Client)
│   ├── src/              # Source files (.cc, .mm)
│   ├── cef/              # CEF headers (submodule)
│   ├── res/              # Resources
│   └── CMakeLists.txt    # CMake configuration for core
├── loader/               # Tauri-based loader application
│   ├── src/              # TypeScript/SolidJS source
│   ├── src-tauri/        # Rust backend
│   └── package.json      # Loader-specific dependencies
├── plugins/              # Preload plugins (JavaScript)
│   ├── src/              # Plugin source
│   └── package.json      # Plugins-specific dependencies
├── bin/                  # Build output directory
├── CMakeLists.txt        # Root CMake configuration
├── package.json          # Root package.json with workspaces
├── pnpm-workspace.yaml   # pnpm workspace configuration
└── README.md             # Main project README
```

## Build System

### JavaScript/TypeScript (Workspaces)

The project uses **pnpm workspaces** to manage the loader and plugins packages:

- **Root package.json**: Defines the workspace and common scripts
- **loader/package.json**: Dependencies for the Tauri loader app
- **plugins/package.json**: Dependencies for the preload plugins

#### Available Scripts

```bash
# Install all dependencies
pnpm install

# Development
pnpm dev:loader    # Run loader in dev mode
pnpm dev:plugins   # Run plugins in dev mode

# Build
pnpm build:loader  # Build loader
pnpm build:plugins # Build plugins
pnpm build         # Build both plugins and loader

# Tauri commands
pnpm tauri dev     # Run Tauri in dev mode
pnpm tauri build   # Build Tauri app
```

### C++ Core Module (CMake)

The core module can be built using **CMake** or the existing platform-specific build tools:

#### Using CMake (Recommended for cross-platform)

```bash
# Configure
cmake -B build -S .

# Build
cmake --build build --config Release

# Install (optional)
cmake --install build
```

#### Platform-Specific Options

**Windows**: The Visual Studio solution (`pengu.sln`) is still available:
```bash
msbuild.exe pengu.sln -t:build -p:Configuration=Release -p:Platform=x64
```

**macOS**: The Makefile is still available:
```bash
make release
```

## CMakeLists.txt Placement

### Why Root-Level CMakeLists.txt?

The main `CMakeLists.txt` is placed at the **root** level for the following reasons:

1. **Flexibility**: Allows future expansion with additional C++ modules
2. **Standard Practice**: Most multi-module projects use root-level CMake
3. **IDE Support**: Better integration with IDEs like CLion, VS Code
4. **Unified Build**: Can build all components from a single entry point
5. **CI/CD**: Easier to configure continuous integration

The `core/CMakeLists.txt` is a subdirectory project that contains the actual build logic for the core module.

### Benefits of This Structure

- **Separation of Concerns**: Each module (core, loader, plugins) is self-contained
- **Parallel Development**: Teams can work on different modules independently
- **Scalability**: Easy to add new modules or components
- **Build Flexibility**: Choose between CMake, MSBuild, or Make based on needs

## Dependency Management

### JavaScript Dependencies

All JavaScript dependencies are managed through the workspace:

- Common dev dependencies (TypeScript, Node types) are in the root
- Module-specific dependencies stay in their respective package.json files
- pnpm efficiently handles shared dependencies across workspaces

### C++ Dependencies

- **CEF Headers**: Managed as a git submodule in `core/cef`
- **Platform Libraries**: Linked through CMake or platform build tools

## Migration Notes

### From Old Structure

The previous structure had:
- Separate `loader/package.json` and `plugins/package.json`
- No CMake support (only Makefile for macOS, MSBuild for Windows)
- No unified root package.json

### After Refactoring

- **Single root package.json** with workspace configuration
- **CMake support** for cross-platform C++ builds
- Existing build methods (MSBuild, Makefile) still work
- Better IDE support and development experience

## Development Workflow

### Initial Setup

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/PenguLoader/PenguLoader.git

# Install JavaScript dependencies
pnpm install

# Build everything
pnpm build
```

### Making Changes

1. **JavaScript/TypeScript changes**: Edit files in `loader/` or `plugins/`
2. **C++ changes**: Edit files in `core/src/`
3. **Build**: Use the appropriate build command for what you changed

### CI/CD

The GitHub Actions workflow (`.github/workflows/build.yml`) has been updated to:
- Install dependencies from the root using `pnpm install`
- Build plugins using `pnpm build:plugins`
- Build the loader using the Tauri action
- Support both Windows and macOS builds

## Future Improvements

Potential enhancements to consider:

1. **Unified Build Script**: Create a master build script that orchestrates all builds
2. **Testing**: Add test infrastructure at workspace level
3. **Linting**: Unified linting configuration across workspaces
4. **Documentation**: Auto-generated API docs from source
5. **CMake Presets**: Add CMakePresets.json for common configurations
