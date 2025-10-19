# Hardware Acceleration

The Cross-Platform Media Player includes comprehensive hardware acceleration support to provide optimal playback performance while maintaining compatibility across different systems and graphics hardware.

## Overview

Hardware acceleration uses the GPU to decode video content, significantly reducing CPU usage and enabling smooth playback of high-resolution content. The media player automatically detects available acceleration methods and provides fallback to software decoding when needed.

## Supported Acceleration Methods

### Windows - DXVA (DirectX Video Acceleration)
- **Technology**: DirectX Video Acceleration 2.0
- **Requirements**: Windows Vista or later, compatible graphics driver
- **Supported Codecs**: H.264, MPEG-2, VC-1, HEVC (depending on hardware)
- **Graphics Cards**: Intel, AMD, NVIDIA with DXVA support

### Linux - VA-API (Video Acceleration API)
- **Technology**: Video Acceleration API
- **Requirements**: Linux with X11, compatible graphics driver
- **Supported Codecs**: H.264, MPEG-2, HEVC (depending on hardware)
- **Graphics Cards**: Primarily Intel and AMD graphics

### Linux - VDPAU (Video Decode and Presentation API for Unix)
- **Technology**: NVIDIA's Video Decode and Presentation API
- **Requirements**: Linux with X11, NVIDIA graphics driver
- **Supported Codecs**: H.264, MPEG-2, HEVC (depending on hardware)
- **Graphics Cards**: NVIDIA graphics cards

## Architecture

### HardwareAcceleration Class
The `HardwareAcceleration` class provides:
- Automatic detection of available acceleration methods
- Configuration and preference management
- Integration with libVLC for hardware-accelerated decoding
- Fallback mechanisms for unsupported configurations

### Integration with VLCBackend
The hardware acceleration system is tightly integrated with the VLC backend:
- Automatic configuration of libVLC arguments based on detected capabilities
- Dynamic switching between acceleration methods
- Graceful fallback to software decoding on errors

## Configuration

### User Settings
Hardware acceleration can be configured through user preferences:

```cpp
UserPreferences prefs;
prefs.hardwareAcceleration = true;              // Enable/disable hardware acceleration
prefs.preferredAccelerationType = "auto";       // Preferred acceleration method
```

### Acceleration Types
- `"auto"` - Automatically detect and use the best available method
- `"dxva"` - Force DXVA (Windows only)
- `"vaapi"` - Force VA-API (Linux only)
- `"vdpau"` - Force VDPAU (Linux only)
- `"none"` - Force software decoding

### Settings Integration
The settings are automatically persisted and loaded:

```cpp
// Settings are automatically saved to:
// Windows: %APPDATA%/CrossPlatformMediaPlayer/settings.ini
// Linux: ~/.config/CrossPlatformMediaPlayer/settings.ini

[hardware]
acceleration=true
preferredAccelerationType=auto
```

## Usage Examples

### Basic Detection
```cpp
#include "media/HardwareAcceleration.h"

HardwareAcceleration hwAccel;
hwAccel.initialize();

// Get available acceleration methods
QList<HardwareAccelerationInfo> accelerations = hwAccel.getAvailableAccelerations();
for (const auto& accel : accelerations) {
    qDebug() << "Available:" << accel.name << accel.available;
}

// Get best available method
HardwareAccelerationType best = hwAccel.getBestAvailableAcceleration();
qDebug() << "Best acceleration:" << HardwareAcceleration::getAccelerationTypeName(best);
```

### VLC Integration
```cpp
#include "media/VLCBackend.h"

VLCBackend vlcBackend;
vlcBackend.initialize();

// Configure hardware acceleration
vlcBackend.setHardwareAccelerationEnabled(true);
vlcBackend.setPreferredAccelerationType(HardwareAccelerationType::Auto);

// Or use user preferences
UserPreferences prefs;
prefs.hardwareAcceleration = true;
prefs.preferredAccelerationType = "auto";
vlcBackend.updateHardwareAccelerationFromPreferences(prefs);
```

### Testing Acceleration
```cpp
// Test if hardware acceleration is working
bool testPassed = hwAccel.testHardwareAcceleration("/path/to/test/video.mp4");
if (!testPassed) {
    qDebug() << "Hardware acceleration test failed, falling back to software";
    hwAccel.setPreferredAcceleration(HardwareAccelerationType::None);
}
```

## Error Handling and Fallback

### Automatic Fallback
The system provides multiple levels of fallback:

1. **Primary**: Use preferred acceleration method
2. **Secondary**: Fall back to best available alternative
3. **Tertiary**: Use software decoding

### Error Detection
- Invalid or corrupted drivers
- Unsupported video formats
- Hardware limitations
- System resource constraints

### Graceful Degradation
When hardware acceleration fails:
- Automatic switch to software decoding
- User notification (optional)
- Performance monitoring and adjustment
- Retry mechanisms for temporary failures

## Performance Considerations

### CPU Usage Reduction
Hardware acceleration typically reduces CPU usage by:
- **H.264 1080p**: 60-80% reduction
- **H.264 4K**: 70-90% reduction
- **HEVC 4K**: 80-95% reduction

### Memory Usage
- GPU memory usage for decoded frames
- Reduced system memory pressure
- Improved overall system responsiveness

### Power Consumption
- Lower CPU power consumption
- Potentially higher GPU power usage
- Overall system power efficiency improvement

## Troubleshooting

### Common Issues

#### Windows DXVA Issues
- **Problem**: DXVA not detected
- **Solution**: Update graphics drivers, check Windows Media Feature Pack

#### Linux VA-API Issues
- **Problem**: VA-API initialization fails
- **Solution**: Install `libva-utils`, check driver installation
- **Command**: `vainfo` to test VA-API

#### Linux VDPAU Issues
- **Problem**: VDPAU not available
- **Solution**: Install NVIDIA drivers, check `vdpauinfo`
- **Command**: `vdpauinfo` to test VDPAU

### Debug Information
Enable debug logging to troubleshoot issues:

```cpp
// Enable hardware acceleration logging
QLoggingCategory::setFilterRules("mediaplayer.hwaccel.debug=true");
```

### System Requirements

#### Minimum Requirements
- **Windows**: Windows 7 with Platform Update
- **Linux**: X11 display server, compatible graphics driver
- **Graphics**: DirectX 9.0c compatible (Windows) or OpenGL 2.0 (Linux)

#### Recommended Requirements
- **Windows**: Windows 10 with latest graphics drivers
- **Linux**: Recent distribution with up-to-date graphics drivers
- **Graphics**: Dedicated graphics card with hardware decode support

## API Reference

### HardwareAcceleration Class

#### Public Methods
```cpp
bool initialize();
QList<HardwareAccelerationInfo> detectAvailableAcceleration();
HardwareAccelerationType getBestAvailableAcceleration() const;
bool setPreferredAcceleration(HardwareAccelerationType type);
void setHardwareAccelerationEnabled(bool enabled);
QStringList getVLCArguments() const;
bool testHardwareAcceleration(const QString& testVideoPath = QString());
```

#### Static Methods
```cpp
static QString getAccelerationTypeName(HardwareAccelerationType type);
static QString getAccelerationTypeDescription(HardwareAccelerationType type);
static bool isPlatformSupported();
static QString accelerationTypeToString(HardwareAccelerationType type);
static HardwareAccelerationType stringToAccelerationType(const QString& typeString);
```

#### Signals
```cpp
void accelerationAvailabilityChanged(bool available);
void accelerationEnabledChanged(bool enabled);
void preferredAccelerationChanged(HardwareAccelerationType type);
```

### HardwareAccelerationInfo Structure
```cpp
struct HardwareAccelerationInfo {
    HardwareAccelerationType type;
    QString name;
    QString description;
    bool available;
    bool enabled;
    QStringList supportedCodecs;
    QString driverVersion;
    QString deviceName;
};
```

## Future Enhancements

### Planned Features
- **macOS Support**: VideoToolbox hardware acceleration
- **Advanced Profiles**: Codec-specific acceleration preferences
- **Performance Monitoring**: Real-time performance metrics
- **Adaptive Quality**: Dynamic quality adjustment based on performance
- **Multi-GPU Support**: Utilization of multiple graphics cards

### Experimental Features
- **AI Upscaling**: GPU-accelerated video enhancement
- **HDR Processing**: Hardware-accelerated HDR tone mapping
- **Real-time Filters**: GPU-accelerated video filters and effects

## Contributing

When contributing to hardware acceleration features:

1. **Test on Multiple Platforms**: Ensure compatibility across Windows and Linux
2. **Handle Edge Cases**: Account for driver bugs and hardware limitations
3. **Provide Fallbacks**: Always ensure software decoding works as backup
4. **Document Changes**: Update this documentation for new features
5. **Performance Testing**: Verify performance improvements and resource usage

## License and Credits

The hardware acceleration implementation uses:
- **libVLC**: VideoLAN's multimedia framework
- **DXVA**: Microsoft's DirectX Video Acceleration
- **VA-API**: Intel's Video Acceleration API
- **VDPAU**: NVIDIA's Video Decode and Presentation API

All implementations respect the respective licenses and terms of use.