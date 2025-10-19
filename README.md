# EonPlay - Timeless Media Player

> **Play for eons** - A futuristic, cross-platform media player built for the ages.

[![CI/CD Pipeline](https://github.com/yourusername/eonplay/actions/workflows/ci.yml/badge.svg)](https://github.com/yourusername/eonplay/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

## 🚀 Overview

EonPlay is a timeless, futuristic media player designed to provide an exceptional media experience across Windows and Linux platforms. Built with Qt 6 and powered by libVLC, EonPlay combines cutting-edge technology with intuitive design to create a media player that will truly "play for eons."

## ✨ Features

### 🎵 Advanced Playback
- **Fast Forward/Rewind** with variable speed control (2x, 5x, 10x, 20x)
- **Playback Modes** - Normal, Repeat One, Repeat All, Shuffle
- **Frame-by-Frame Stepping** for precise video navigation
- **Resume Playback** from last position automatically
- **Crossfade & Gapless** playback for seamless audio transitions
- **Seek Thumbnails** for visual navigation

### 🎬 Media Support
- **Video Formats**: MP4, AVI, MKV, MOV, WebM
- **Audio Formats**: MP3, FLAC, WAV, AAC, OGG
- **Subtitle Support**: SRT, ASS/SSA with advanced styling
- **Hardware Acceleration**: DXVA (Windows), VA-API/VDPAU (Linux)

### 🎨 Modern Interface
- **Futuristic Design** with dark/light themes
- **Cross-Platform** native look and feel
- **System Integration** - media keys, notifications, tray
- **Accessibility** support with screen reader compatibility

## 🛠️ Technology Stack

- **Framework**: Qt 6.5.3 with C++17
- **Media Engine**: libVLC for comprehensive codec support
- **Build System**: CMake with cross-platform configuration
- **Database**: SQLite for media library management
- **CI/CD**: GitHub Actions with automated testing and deployment

## 📦 Installation

### Windows
```bash
# Download the latest release
# Run the .msi installer or extract the portable .zip
```

### Linux
```bash
# Ubuntu/Debian
sudo dpkg -i eonplay_1.0.0_amd64.deb

# Red Hat/Fedora
sudo rpm -i eonplay-1.0.0.x86_64.rpm

# AppImage (Universal)
chmod +x EonPlay-1.0.0-x86_64.AppImage
./EonPlay-1.0.0-x86_64.AppImage
```

## 🚀 Quick Start

1. **Launch EonPlay** from your applications menu
2. **Open Media** via File → Open or drag & drop
3. **Enjoy** the timeless media experience!

### Keyboard Shortcuts
- `Space` - Play/Pause
- `←/→` - Seek backward/forward
- `↑/↓` - Volume up/down
- `F` - Toggle fullscreen
- `S` - Take screenshot

## 🏗️ Building from Source

### Prerequisites
- Qt 6.5.3 or later
- CMake 3.20 or later
- libVLC 3.0 or later
- C++17 compatible compiler

### Build Steps
```bash
# Clone the repository
git clone https://github.com/yourusername/eonplay.git
cd eonplay

# Configure and build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run
./build/EonPlay
```

## 🧪 Testing

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run specific test
./build/tests/test_playback_controller
```

## 📚 Documentation

- **API Documentation**: [Generated with Doxygen](https://yourusername.github.io/eonplay/api/)
- **User Guide**: [docs/UserGuide.md](docs/UserGuide.md)
- **Advanced Features**: [docs/AdvancedPlaybackFeatures.md](docs/AdvancedPlaybackFeatures.md)
- **Development**: [docs/Development.md](docs/Development.md)

## 🤝 Contributing

We welcome contributions to EonPlay! Please see our [Contributing Guide](CONTRIBUTING.md) for details.

### Development Workflow
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

EonPlay is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for details.

## 🌟 Acknowledgments

- **Qt Framework** - Cross-platform application framework
- **libVLC** - Multimedia framework and media player
- **Contributors** - Everyone who helps make EonPlay better

## 🔮 Roadmap

- [ ] AI-powered content analysis and recommendations
- [ ] Cloud synchronization across devices  
- [ ] Plugin system for extensibility
- [ ] Mobile companion app
- [ ] Streaming service integrations

---

**EonPlay** - *Timeless, futuristic; play for eons* 🎵✨

Made with ❤️ by the EonPlay Team