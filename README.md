# Eternal Night

A 2D sandbox adventure game with multiplayer support, featuring procedurally generated worlds, dynamic day/night cycles, caves, and combat mechanics.

## Features

- **Procedurally Generated Worlds**: Infinite procedurally generated terrain with customizable parameters (water amount, stone amount, cave density)
- **Dynamic Day/Night Cycle**: Real-time day/night transitions with atmospheric fog effects
- **Multiplayer Support**: Host or join multiplayer sessions to play with other players
- **Cave Systems**: Procedurally generated caves with unique exploration opportunities
- **Combat System**: Attack mechanics with stamina management and weapons
- **Inventory System**: Collect and manage items, blocks, and weapons
- **Block Placement**: Build and modify the terrain by placing/breaking blocks
- **Mob System**: Dynamic mob spawning and AI
- **Camera Control**: Smooth camera movement with zoom capabilities
- **Debug Mode**: Press F3 to toggle debug information overlay

## System Requirements

- **OS**: Windows (primary support)
- **C++ Standard**: C++17 or later
- **Build System**: CMake 3.21+
- **Dependencies**: Managed via vcpkg

## Building

### Prerequisites

1. Install [CMake](https://cmake.org/download/) (3.21 or later)
2. Install [Visual Studio](https://visualstudio.microsoft.com/) with C++ development tools or another C++ compiler
3. Install [vcpkg](https://github.com/Microsoft/vcpkg)

### Build Instructions

```bash
# Clone or navigate to the project directory
cd EternalNight

# Configure the project
cmake --preset default

# Build the project
cmake --build build

# Run the game
./build/EternalNight.exe
```

## How to Play

### Single Player

1. Launch the game
2. Click "New Game" from the main menu
3. Configure world settings (seed, water amount, stone amount, cave density)
4. Explore the generated world

### Multiplayer

#### Hosting a Server

1. Launch the game
2. Open the Multiplayer UI (bottom-right corner)
3. Enter a port number (default: 7777)
4. Click "Host" to start a server
5. Other players can connect to your IP address

#### Joining a Server

1. Launch the game
2. Open the Multiplayer UI
3. Enter the server's IP address and port
4. Click "Join" to connect

## Controls

- **WASD**: Move character
- **Mouse**: Aim and attack
- **Left Click**: Attack/Mine
- **Right Click**: Place block
- **Number Keys**: Switch inventory slots
- **Scroll Wheel**: Zoom camera in/out
- **F3**: Toggle debug information
- **ESC**: Open pause menu

## Game Mechanics

### Inventory

- Up to 9 item slots
- Different item types: weapons, blocks, miscellaneous items
- Select items to use them for building or combat

### Block Types

- **Stone**: Hard building material
- **Dirt**: Soft building material
- **Grass**: Natural terrain surface
- **Sand**: Desert biome material

### Combat

- Attack with equipped weapons
- Manage stamina for limited attack frequency
- Health system with damage taken from mobs and hazards
- HP and stamina indicators on-screen

### Exploration

- Discover caves for deeper exploration
- Mine resources and gather materials
- Encounter and defeat mobs
- Survive the night with environmental dangers

## Project Structure

```
src/
├── main.cpp                 # Main game loop and initialization
├── game_input.cpp/h         # Input handling
├── game_logic.cpp/h         # Game state and logic
├── game_render.cpp/h        # Rendering logic
├── player.cpp/h             # Player character class
├── world.cpp/h              # World management and chunk system
├── inventory.cpp/h          # Inventory system
├── multiplayer.cpp/h        # Multiplayer game logic
├── multiplayer_types.h      # Network data structures
├── engine/                  # Custom Forge game engine
│   ├── forge.c/h           # Engine core
│   ├── renderer.c/h        # Rendering system
│   ├── window.c/h          # Window management
│   ├── camera.c/h          # Camera system
│   ├── input.c/h           # Input processing
│   ├── worldgen.c/h        # World generation
│   └── ...                 # Other engine components
├── net/                    # Networking
│   ├── client.c/h         # Network client
│   ├── server.c/h         # Network server
│   ├── net.c/h            # Network utilities
│   └── protocol.h         # Network protocol definitions
└── ui/                     # User interface
    ├── imgui_lite.cpp/h   # Lightweight GUI library
    ├── main_menu.cpp/h    # Main menu interface
    ├── multiplayer_ui.cpp/h # Multiplayer controls UI
    └── ...                # Other UI components
```

## Configuration

Edit world parameters in the New Game menu:
- **Seed**: Random seed for world generation
- **Water Amount**: Percentage of water in the world
- **Stone Amount**: Percentage of stone deposits
- **Cave Density**: How frequently caves appear

## Networking

The game uses a client-server architecture for multiplayer:
- Server runs as a separate process
- Clients connect via IP and port
- Real-time state synchronization for players and mobs
- Input prediction and network snapshots

## Troubleshooting

### Game won't start
- Ensure all dependencies from vcpkg are installed
- Check that the build completed successfully
- Verify graphics drivers are up to date

### Multiplayer connection fails
- Check firewall settings allow the port
- Verify correct IP address and port
- Ensure both machines are on the same network
- Try default port 7777 if custom ports don't work

### Performance issues
- Enable debug mode (F3) to check frame rate
- Reduce world size or increase chunk loading distance
- Adjust camera zoom level

## Development

### Building in Debug Mode

```bash
cmake --preset debug
cmake --build build --config Debug
```

### Running with Debug Symbols

```bash
./build/EternalNight.exe
```

Toggle debug overlay with F3 to see current FPS.

## License

This project is provided as-is for educational and personal use.

## Credits

- Built with the custom **Forge Game Engine**
- Network implementation for multiplayer support
- Uses C++ standard library and vcpkg dependencies

## Contributing

For bug reports and feature requests, please document the issue clearly with reproduction steps.
