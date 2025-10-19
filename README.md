# Cross-Platform Media Player

A high-quality, cross-platform media player built with Qt 6 and libVLC, supporting comprehensive audio and video formats with modern UI and advanced features.

## Project Structure

```
├── CMakeLists.txt          # Main CMake configuration
├── src/                    # Source code
│   ├── core/              # Core application framework
│   ├── ui/                # User interface components
│   ├── media/             # Media playback and processing
│   ├── data/              # Data management and storage
│   ├── network/           # Network and streaming
│   └── ai/                # AI and smart features
├── include/               # Header files
├── resources/             # Application resources
│   ├── icons/            # Application icons
│   ├── themes/           # UI themes and stylesheets
│   └── linux/            # Linux-specific resources
├── tests/                 # Unit and integration tests
└── plugins/               # Plugin system
```

## Build Requirements

### Dependencies
- Qt 6.2 or later (Core, Widgets, Multimedia, MultimediaWidgets, Network)
- libVLC 3.0 or later
- CMake 3.20 or later
- C++17 compatible compiler

### Windows
- Visual Studio 2019 or later
- Qt 6 for Windows
- VLC SDK for Windows

### Linux
- GCC 9+ or Clang 10+
- Qt 6 development packages
- libVLC development packages

## Building

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Installation

### Windows
```bash
cmake --build . --target install
cpack -G WIX  # For MSI installer
cpack -G NSIS # For executable installer
```

### Linux
```bash
cmake --build . --target install
cpack -G DEB  # For .deb package
cpack -G RPM  # For .rpm package
```

## Features

- Comprehensive format support (MP4, AVI, MKV, MOV, WebM, MP3, FLAC, WAV, AAC, OGG)
- Hardware-accelerated decoding (DXVA on Windows, VA-API/VDPAU on Linux)
- Advanced subtitle support (SRT, ASS/SSA)
- Media library management with metadata extraction
- Cross-platform native UI with dark/light themes
- Plugin system for extensibility
- Network streaming capabilities
- AI-powered features for content analysis

## License

GPL-3.0 License - see LICENSE file for details.