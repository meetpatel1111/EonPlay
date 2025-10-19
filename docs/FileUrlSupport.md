# EonPlay File & URL Support

This document describes the file and URL support functionality implemented in EonPlay as part of task 2.5.

## Overview

The FileUrlSupport component provides comprehensive support for loading media files and URLs, handling drag-and-drop operations, validating file formats, auto-detecting subtitles, extracting media information, and capturing screenshots.

## Features Implemented

### 1. Drag-and-Drop File Support

- **DragDropWidget**: A reusable widget that handles drag-and-drop operations
- Supports dropping multiple files and URLs
- Visual feedback during drag operations
- Automatic file validation and processing
- Error handling for unsupported formats

**Usage:**
```cpp
auto dragDropWidget = new DragDropWidget();
dragDropWidget->setFileUrlSupport(fileUrlSupport);
connect(dragDropWidget, &DragDropWidget::filesDropped, 
        this, &MyClass::onFilesDropped);
```

### 2. URL/Stream Playback Capability

- Support for multiple URL schemes: HTTP, HTTPS, RTSP, RTMP, FTP, etc.
- Stream validation and error reporting
- Local file URL handling (file://)
- Network stream compatibility

**Supported URL Schemes:**
- `http://` and `https://` - Web streams
- `rtsp://` - Real-Time Streaming Protocol
- `rtmp://` - Real-Time Messaging Protocol
- `ftp://` - File Transfer Protocol
- `file://` - Local file URLs
- `mms://` - Microsoft Media Server
- And more...

### 3. File Format Validation and Error Reporting

- Comprehensive file extension validation
- File header validation for security
- File size limit checking
- Access permission verification
- Detailed error messages for validation failures

**Validation Results:**
- `Valid` - File is supported and accessible
- `UnsupportedFormat` - File format not supported
- `FileNotFound` - File does not exist
- `FileTooLarge` - File exceeds size limits
- `AccessDenied` - Cannot access file
- `CorruptedFile` - File appears corrupted
- `SecurityRisk` - File poses security risk

### 4. Auto Subtitle Loading Detection

- Automatic detection of subtitle files in the same directory
- Support for common subtitle formats: SRT, ASS, SSA, VTT, etc.
- Intelligent matching based on filename patterns
- Search in common subtitle subdirectories

**Subtitle Matching Patterns:**
- Exact match: `movie.mp4` → `movie.srt`
- Language suffix: `movie.mp4` → `movie.en.srt`
- Common patterns: `movie.mp4` → `movie_english.srt`

### 5. Playback Info Display (Bitrate, Codec, FPS)

- **MediaInfoWidget**: Comprehensive media information display
- Real-time extraction of media metadata using libVLC
- Detailed technical information including:
  - Video: Resolution, frame rate, codec, bitrate
  - Audio: Codec, channels, sample rate, bitrate
  - General: Duration, file size, format

**MediaInfo Structure:**
```cpp
struct MediaInfo {
    QString filePath;
    QString title;
    QString format;
    QString videoCodec;
    QString audioCodec;
    qint64 duration;
    qint64 fileSize;
    int videoBitrate;
    int audioBitrate;
    int width, height;
    double frameRate;
    int audioChannels;
    int audioSampleRate;
    bool hasVideo;
    bool hasAudio;
    bool isValid;
};
```

### 6. Screenshot Capture Functionality

- Capture screenshots of current video frame
- Capture screenshots at specific positions
- Configurable screenshot format (PNG, JPG, etc.)
- Automatic filename generation with timestamps
- Custom save directory support

**Screenshot Features:**
- Current frame capture
- Position-specific capture
- Multiple image formats
- Automatic directory creation
- Timestamp-based naming

## Supported File Formats

### Video Formats
- **Container formats**: MP4, AVI, MKV, MOV, WMV, FLV, WebM, M4V, 3GP, ASF, RM, VOB, OGV, TS, MTS, M2TS
- **Codecs**: H.264, H.265, VP8, VP9, MPEG-2, MPEG-4, DivX, Xvid, and more

### Audio Formats
- **Lossless**: FLAC, WAV, AIFF, APE, WV, TTA
- **Lossy**: MP3, AAC, OGG, WMA, M4A, Opus
- **Professional**: AC3, DTS, AMR, AU, RA

### Playlist Formats
- M3U, M3U8, PLS, XSPF, ASX, WPL, SMIL

### Subtitle Formats
- SRT, ASS, SSA, SUB, IDX, VTT, TTML, DFXP, SMI, RT, TXT

## API Reference

### FileUrlSupport Class

#### Core Methods
```cpp
// File operations
bool loadMediaFile(const QString& filePath);
bool loadMediaUrl(const QUrl& url);
ValidationResult validateMediaFile(const QString& filePath) const;

// Drag and drop
bool canHandleMimeData(const QMimeData* mimeData) const;
QStringList processDroppedMedia(const QMimeData* mimeData);

// Media information
MediaInfo extractMediaInfo(const QString& filePath);
MediaInfo getCurrentMediaInfo() const;

// Subtitle detection
QStringList autoDetectSubtitles(const QString& mediaPath);
bool isSubtitleMatch(const QString& mediaPath, const QString& subtitlePath) const;

// Screenshot capture
QString captureScreenshot(const QString& filePath = QString());
QString captureScreenshotAtPosition(qint64 position, const QString& filePath = QString());

// Configuration
QStringList getSupportedExtensions() const;
QStringList getSupportedUrlSchemes() const;
QString getFileFilter() const;
```

#### Signals
```cpp
// Media loading
void mediaFileLoaded(const QString& filePath, const MediaInfo& mediaInfo);
void mediaUrlLoaded(const QUrl& url, const MediaInfo& mediaInfo);

// Validation errors
void fileValidationFailed(const QString& filePath, ValidationResult result, const QString& errorMessage);
void urlValidationFailed(const QUrl& url, const QString& errorMessage);

// Subtitle detection
void subtitlesDetected(const QString& mediaPath, const QStringList& subtitlePaths);

// Media information
void mediaInfoExtracted(const QString& filePath, const MediaInfo& mediaInfo);

// Screenshot capture
void screenshotCaptured(const QString& screenshotPath, qint64 position);
void screenshotCaptureFailed(const QString& errorMessage);

// Drag and drop
void dragDropProcessed(const QStringList& processedFiles, const QStringList& failedFiles);
```

## Example Usage

### Basic File Loading
```cpp
auto fileUrlSupport = std::make_shared<FileUrlSupport>();
fileUrlSupport->setMediaEngine(mediaEngine);
fileUrlSupport->initialize();

// Load a media file
if (fileUrlSupport->loadMediaFile("/path/to/video.mp4")) {
    qDebug() << "File loaded successfully";
}

// Load a URL
QUrl streamUrl("http://example.com/stream.m3u8");
if (fileUrlSupport->loadMediaUrl(streamUrl)) {
    qDebug() << "Stream loaded successfully";
}
```

### Drag and Drop Integration
```cpp
auto dragDropWidget = new DragDropWidget(parentWidget);
dragDropWidget->setFileUrlSupport(fileUrlSupport);

connect(dragDropWidget, &DragDropWidget::filesDropped,
        [](const QStringList& processed, const QStringList& failed) {
    qDebug() << "Processed:" << processed.size() << "Failed:" << failed.size();
});
```

### Media Information Display
```cpp
auto mediaInfoWidget = new MediaInfoWidget();
mediaInfoWidget->setFileUrlSupport(fileUrlSupport);
mediaInfoWidget->setShowTechnicalDetails(true);

// The widget will automatically update when media is loaded
```

### Screenshot Capture
```cpp
// Capture current frame
QString screenshotPath = fileUrlSupport->captureScreenshot();

// Capture at specific position
QString screenshotPath = fileUrlSupport->captureScreenshotAtPosition(60000); // 1 minute

// Configure screenshot settings
fileUrlSupport->setScreenshotDirectory("/path/to/screenshots");
fileUrlSupport->setScreenshotFormat("PNG");
```

## Security Considerations

- File header validation prevents malicious file exploitation
- File size limits prevent resource exhaustion
- Path validation prevents directory traversal attacks
- URL scheme validation prevents unauthorized protocol access
- Sandboxed media decoding through libVLC

## Performance Considerations

- Media information extraction is performed asynchronously
- Thumbnail/screenshot generation is cached
- File validation is optimized for common cases
- Large file handling with appropriate size limits
- Memory-efficient subtitle detection

## Testing

The implementation includes comprehensive unit tests covering:
- File format validation
- URL validation
- Subtitle detection and matching
- Media information extraction
- Screenshot functionality
- Drag and drop operations

Run tests with:
```bash
cd build
ctest -R test_file_url_support
```

## Integration with EonPlay

This component integrates seamlessly with other EonPlay components:
- **VLCBackend**: Uses libVLC for media loading and information extraction
- **PlaybackController**: Provides media loading interface
- **UI Components**: DragDropWidget and MediaInfoWidget for user interaction
- **Settings System**: Configurable screenshot and validation settings

## Future Enhancements

Potential future improvements:
- Cloud storage integration (Google Drive, Dropbox)
- Advanced metadata extraction (album art, lyrics)
- Batch file processing
- Custom subtitle format support
- Video thumbnail generation
- Network stream recording
- Playlist import/export