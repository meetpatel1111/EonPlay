# EonPlay - Advanced Playback Features

This document describes the advanced playback features implemented in EonPlay - the timeless, futuristic media player. Play for eons.

## Overview

EonPlay's advanced playback features extend the basic play/pause/stop functionality with sophisticated controls for enhanced user experience. These timeless features are implemented in the `PlaybackController` class and provide:

- Fast forward/rewind with variable speed control
- Loop/repeat and shuffle playback modes
- Resume from last position functionality
- Frame-by-frame stepping for video content
- Crossfade between tracks for smooth transitions
- Gapless playback support
- Thumbnail seek preview functionality

## Features

### 1. Fast Forward/Rewind with Speed Control

Allows users to quickly navigate through media content at various speeds.

**API:**
```cpp
// Start fast forward at specified speed
void startFastForward(SeekSpeed speed = SeekSpeed::Normal);

// Start rewind at specified speed  
void startRewind(SeekSpeed speed = SeekSpeed::Normal);

// Stop fast seeking and return to normal playback
void stopFastSeek();

// Check if currently fast seeking
bool isFastSeeking() const;
```

**Speed Options:**
- `SeekSpeed::Slow` - 2x speed
- `SeekSpeed::Normal` - 5x speed (default)
- `SeekSpeed::Fast` - 10x speed
- `SeekSpeed::VeryFast` - 20x speed

**Usage Example:**
```cpp
// Start fast forward at 10x speed
controller->startFastForward(SeekSpeed::Fast);

// Check if fast seeking is active
if (controller->isFastSeeking()) {
    // Update UI to show fast seek indicator
}

// Stop fast seeking
controller->stopFastSeek();
```

### 2. Playback Modes

Provides different playback behaviors for playlists and individual tracks.

**API:**
```cpp
// Set playback mode
void setPlaybackMode(PlaybackMode mode);

// Get current playback mode
PlaybackMode playbackMode() const;
```

**Available Modes:**
- `PlaybackMode::Normal` - Play once and stop
- `PlaybackMode::RepeatOne` - Repeat current track
- `PlaybackMode::RepeatAll` - Repeat entire playlist
- `PlaybackMode::Shuffle` - Shuffle playlist order

**Usage Example:**
```cpp
// Enable repeat all mode
controller->setPlaybackMode(PlaybackMode::RepeatAll);

// Enable shuffle mode
controller->setPlaybackMode(PlaybackMode::Shuffle);
```

### 3. Frame-by-Frame Stepping

Allows precise navigation through video content one frame at a time.

**API:**
```cpp
// Step forward one frame
void stepForward();

// Step backward one frame
void stepBackward();
```

**Usage Example:**
```cpp
// Step through video frame by frame
controller->stepForward();  // Next frame
controller->stepBackward(); // Previous frame
```

**Note:** Frame stepping automatically pauses playback for precise control.

### 4. Resume from Last Position

Automatically saves and restores playback positions for media files.

**API:**
```cpp
// Save current position for a file
void saveResumePosition(const QString& filePath);

// Load and seek to saved position
bool loadResumePosition(const QString& filePath);

// Clear saved position
void clearResumePosition(const QString& filePath);
```

**Behavior:**
- Positions are automatically saved when loading new media
- Resume positions are only saved if playback is more than 5 seconds from start and 10 seconds from end
- Positions are automatically loaded when media is opened (if resume is enabled in settings)

**Usage Example:**
```cpp
// Manual position management
controller->saveResumePosition("/path/to/video.mp4");

// Load resume position
if (controller->loadResumePosition("/path/to/video.mp4")) {
    qDebug() << "Resumed from saved position";
}
```

### 5. Crossfade Between Tracks

Provides smooth transitions between audio tracks by fading out the current track while fading in the next.

**API:**
```cpp
// Enable/disable crossfade with duration
void setCrossfadeEnabled(bool enabled, int duration = 3000);

// Check if crossfade is enabled
bool isCrossfadeEnabled() const;

// Get crossfade duration
int crossfadeDuration() const;
```

**Configuration:**
- Duration can be set between 500ms and 10 seconds
- Crossfade works with audio content
- Requires playlist management integration

**Usage Example:**
```cpp
// Enable 5-second crossfade
controller->setCrossfadeEnabled(true, 5000);

// Check crossfade status
if (controller->isCrossfadeEnabled()) {
    int duration = controller->crossfadeDuration();
    qDebug() << "Crossfade enabled with" << duration << "ms duration";
}
```

### 6. Gapless Playback

Eliminates silence between tracks for continuous audio playback.

**API:**
```cpp
// Enable/disable gapless playback
void setGaplessPlayback(bool enabled);

// Check if gapless playback is enabled
bool isGaplessPlayback() const;
```

**Usage Example:**
```cpp
// Enable gapless playback for albums
controller->setGaplessPlayback(true);
```

### 7. Seek Thumbnail Preview

Generates thumbnail images at specific positions for visual seek preview.

**API:**
```cpp
// Generate thumbnail for position
QString generateSeekThumbnail(qint64 position);

// Enable/disable thumbnail generation
void setSeekThumbnailEnabled(bool enabled);

// Check if thumbnails are enabled
bool isSeekThumbnailEnabled() const;
```

**Features:**
- Thumbnails are cached for performance
- Only works with video content
- Returns base64-encoded image data
- Cache is automatically managed to prevent memory issues

**Usage Example:**
```cpp
// Generate thumbnail for 30-second mark
QString thumbnail = controller->generateSeekThumbnail(30000);
if (!thumbnail.isEmpty()) {
    // Display thumbnail in seek preview
    displayThumbnail(thumbnail);
}
```

## Signals

The PlaybackController emits signals for all advanced features:

```cpp
// Playback mode changes
void playbackModeChanged(PlaybackMode mode);

// Fast seek state changes
void fastSeekChanged(bool active, int speed);

// Crossfade settings changes
void crossfadeChanged(bool enabled, int duration);

// Gapless playback changes
void gaplessPlaybackChanged(bool enabled);

// Thumbnail generation
void seekThumbnailGenerated(qint64 position, const QString& thumbnail);
```

## Settings Integration

Advanced playback features are integrated with the settings system:

```cpp
struct UserPreferences {
    // Advanced playback settings
    bool crossfadeEnabled = false;      // Enable crossfade between tracks
    int crossfadeDuration = 3000;       // Crossfade duration in milliseconds
    bool gaplessPlayback = false;       // Enable gapless playback
    bool seekThumbnailsEnabled = true;  // Enable seek preview thumbnails
    int fastSeekSpeed = 5;              // Default fast seek speed multiplier
};
```

## Requirements Satisfied

This implementation satisfies the following requirements from the specification:

- **Requirement 3.1**: Advanced playback controls including fast forward/rewind
- **Requirement 3.2**: Playback modes (repeat, shuffle) and speed control
- **Requirement 3.4**: Frame-by-frame stepping and advanced seek operations
- **Requirement 3.5**: Resume functionality and user preference persistence

## Example Usage

See `examples/advanced_playback_example.cpp` for a complete demonstration of all advanced playback features.

## Integration Notes

- Features work with any media engine implementing the `IMediaEngine` interface
- VLC backend integration provides optimal support for all features
- UI components can connect to signals for real-time updates
- Settings are automatically persisted and restored between sessions