#pragma once

#include "IComponent.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QMimeData>
#include <QFileInfo>
#include <QTimer>
#include <QHash>
#include <memory>

class IMediaEngine;
class PlaybackController;

/**
 * @brief Media information structure containing detailed playback info
 */
struct MediaInfo
{
    QString filePath;
    QString title;
    QString format;
    QString videoCodec;
    QString audioCodec;
    qint64 duration = 0;
    qint64 fileSize = 0;
    int videoBitrate = 0;
    int audioBitrate = 0;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    int audioChannels = 0;
    int audioSampleRate = 0;
    bool hasVideo = false;
    bool hasAudio = false;
    bool isValid = false;
    
    /**
     * @brief Clear all media information
     */
    void clear();
    
    /**
     * @brief Check if media info is complete
     * @return true if all essential fields are populated
     */
    bool isComplete() const;
    
    /**
     * @brief Get formatted string representation
     * @return Human-readable media info string
     */
    QString toString() const;
};

/**
 * @brief File format validation result
 */
enum class ValidationResult
{
    Valid,              // File is valid and supported
    UnsupportedFormat,  // File format not supported
    FileNotFound,       // File does not exist
    FileTooLarge,       // File exceeds size limits
    AccessDenied,       // Cannot access file
    CorruptedFile,      // File appears corrupted
    SecurityRisk        // File poses security risk
};

/**
 * @brief EonPlay file and URL support component
 * 
 * Handles drag-and-drop file operations, URL/stream playback,
 * file format validation, auto subtitle detection, media info
 * extraction, and screenshot capture functionality.
 */
class FileUrlSupport : public QObject, public IComponent
{
    Q_OBJECT

public:
    explicit FileUrlSupport(QObject* parent = nullptr);
    ~FileUrlSupport() override;
    
    // IComponent interface
    bool initialize() override;
    void shutdown() override;
    QString componentName() const override { return "FileUrlSupport"; }
    bool isInitialized() const override { return m_initialized; }
    
    /**
     * @brief Set the media engine for playback operations
     * @param engine Shared pointer to media engine
     */
    void setMediaEngine(std::shared_ptr<IMediaEngine> engine);
    
    /**
     * @brief Set the playback controller for advanced operations
     * @param controller Shared pointer to playback controller
     */
    void setPlaybackController(std::shared_ptr<PlaybackController> controller);
    
    // File operations
    
    /**
     * @brief Load media file with validation and auto subtitle detection
     * @param filePath Path to media file
     * @return true if file was loaded successfully
     */
    bool loadMediaFile(const QString& filePath);
    
    /**
     * @brief Load media from URL or stream
     * @param url URL to load (HTTP, HTTPS, RTSP, etc.)
     * @return true if URL was loaded successfully
     */
    bool loadMediaUrl(const QUrl& url);
    
    /**
     * @brief Validate media file format and accessibility
     * @param filePath Path to file to validate
     * @return Validation result
     */
    ValidationResult validateMediaFile(const QString& filePath) const;
    
    /**
     * @brief Get detailed validation error message
     * @param result Validation result
     * @param filePath File path for context
     * @return Human-readable error message
     */
    QString getValidationErrorMessage(ValidationResult result, const QString& filePath) const;
    
    // Drag and drop support
    
    /**
     * @brief Check if mime data contains supported media files
     * @param mimeData Drag and drop mime data
     * @return true if mime data contains playable media
     */
    bool canHandleMimeData(const QMimeData* mimeData) const;
    
    /**
     * @brief Process dropped files and URLs
     * @param mimeData Drag and drop mime data
     * @return List of successfully processed media paths/URLs
     */
    QStringList processDroppedMedia(const QMimeData* mimeData);
    
    /**
     * @brief Get list of supported file extensions
     * @return List of supported extensions (without dots)
     */
    QStringList getSupportedExtensions() const;
    
    /**
     * @brief Get file filter string for file dialogs
     * @return File filter string for QFileDialog
     */
    QString getFileFilter() const;
    
    // Media information
    
    /**
     * @brief Extract detailed media information
     * @param filePath Path to media file
     * @return MediaInfo structure with detailed information
     */
    MediaInfo extractMediaInfo(const QString& filePath);
    
    /**
     * @brief Get current media information
     * @return Current media info or empty info if no media loaded
     */
    MediaInfo getCurrentMediaInfo() const { return m_currentMediaInfo; }
    
    /**
     * @brief Check if current media has video content
     * @return true if current media contains video
     */
    bool hasVideo() const { return m_currentMediaInfo.hasVideo; }
    
    /**
     * @brief Check if current media has audio content
     * @return true if current media contains audio
     */
    bool hasAudio() const { return m_currentMediaInfo.hasAudio; }
    
    // Subtitle support
    
    /**
     * @brief Auto-detect and load subtitle files for media
     * @param mediaPath Path to media file
     * @return List of found subtitle files
     */
    QStringList autoDetectSubtitles(const QString& mediaPath);
    
    /**
     * @brief Check if subtitle file matches media file
     * @param mediaPath Path to media file
     * @param subtitlePath Path to subtitle file
     * @return true if subtitle matches media
     */
    bool isSubtitleMatch(const QString& mediaPath, const QString& subtitlePath) const;
    
    /**
     * @brief Get supported subtitle extensions
     * @return List of supported subtitle extensions
     */
    QStringList getSupportedSubtitleExtensions() const;
    
    // Screenshot capture
    
    /**
     * @brief Capture screenshot of current video frame
     * @param filePath Path to save screenshot (optional, auto-generated if empty)
     * @return Path to saved screenshot or empty string on failure
     */
    QString captureScreenshot(const QString& filePath = QString());
    
    /**
     * @brief Capture screenshot at specific position
     * @param position Position in milliseconds
     * @param filePath Path to save screenshot (optional)
     * @return Path to saved screenshot or empty string on failure
     */
    QString captureScreenshotAtPosition(qint64 position, const QString& filePath = QString());
    
    /**
     * @brief Set screenshot save directory
     * @param directory Directory to save screenshots
     */
    void setScreenshotDirectory(const QString& directory);
    
    /**
     * @brief Get current screenshot directory
     * @return Current screenshot save directory
     */
    QString getScreenshotDirectory() const { return m_screenshotDirectory; }
    
    /**
     * @brief Set screenshot format (PNG, JPG, etc.)
     * @param format Image format for screenshots
     */
    void setScreenshotFormat(const QString& format);
    
    /**
     * @brief Get current screenshot format
     * @return Current screenshot format
     */
    QString getScreenshotFormat() const { return m_screenshotFormat; }
    
    // URL validation and processing
    
    /**
     * @brief Validate URL for streaming
     * @param url URL to validate
     * @return true if URL is valid for streaming
     */
    bool isValidStreamUrl(const QUrl& url) const;
    
    /**
     * @brief Get supported URL schemes
     * @return List of supported URL schemes
     */
    QStringList getSupportedUrlSchemes() const;
    
    /**
     * @brief Check if URL is a local file
     * @param url URL to check
     * @return true if URL points to local file
     */
    bool isLocalFileUrl(const QUrl& url) const;
    
    /**
     * @brief Get subtitle files for media file
     * @param mediaPath Path to media file
     * @return List of subtitle files
     */
    QStringList getSubtitleFiles(const QString& mediaPath) const;
    
    /**
     * @brief Check if file is a valid media file
     * @param filePath Path to file to check
     * @return true if file is valid media
     */
    bool isValidMediaFile(const QString& filePath) const;

public slots:
    /**
     * @brief Handle file open request
     * @param filePath Path to file to open
     */
    void openFile(const QString& filePath);
    
    /**
     * @brief Handle URL open request
     * @param url URL to open
     */
    void openUrl(const QUrl& url);
    
    /**
     * @brief Handle multiple files open request
     * @param filePaths List of file paths to open
     */
    void openFiles(const QStringList& filePaths);

signals:
    /**
     * @brief Emitted when media file is successfully loaded
     * @param filePath Path to loaded media file
     * @param mediaInfo Detailed media information
     */
    void mediaFileLoaded(const QString& filePath, const MediaInfo& mediaInfo);
    
    /**
     * @brief Emitted when media URL is successfully loaded
     * @param url Loaded media URL
     * @param mediaInfo Available media information
     */
    void mediaUrlLoaded(const QUrl& url, const MediaInfo& mediaInfo);
    
    /**
     * @brief Emitted when file validation fails
     * @param filePath Path to invalid file
     * @param result Validation result
     * @param errorMessage Human-readable error message
     */
    void fileValidationFailed(const QString& filePath, ValidationResult result, const QString& errorMessage);
    
    /**
     * @brief Emitted when URL validation fails
     * @param url Invalid URL
     * @param errorMessage Error description
     */
    void urlValidationFailed(const QUrl& url, const QString& errorMessage);
    
    /**
     * @brief Emitted when subtitles are auto-detected
     * @param mediaPath Path to media file
     * @param subtitlePaths List of detected subtitle files
     */
    void subtitlesDetected(const QString& mediaPath, const QStringList& subtitlePaths);
    
    /**
     * @brief Emitted when media information is extracted
     * @param filePath Path to media file
     * @param mediaInfo Extracted media information
     */
    void mediaInfoExtracted(const QString& filePath, const MediaInfo& mediaInfo);
    
    /**
     * @brief Emitted when screenshot is captured
     * @param screenshotPath Path to saved screenshot
     * @param position Position where screenshot was taken
     */
    void screenshotCaptured(const QString& screenshotPath, qint64 position);
    
    /**
     * @brief Emitted when screenshot capture fails
     * @param errorMessage Error description
     */
    void screenshotCaptureFailed(const QString& errorMessage);
    
    /**
     * @brief Emitted when drag and drop operation is processed
     * @param processedFiles List of successfully processed files
     * @param failedFiles List of files that failed to process
     */
    void dragDropProcessed(const QStringList& processedFiles, const QStringList& failedFiles);

private slots:
    /**
     * @brief Handle media info extraction completion
     */
    void onMediaInfoExtractionComplete();
    
    /**
     * @brief Handle subtitle detection completion
     */
    void onSubtitleDetectionComplete();

private:
    /**
     * @brief Initialize supported file extensions
     */
    void initializeSupportedExtensions();
    
    /**
     * @brief Initialize supported URL schemes
     */
    void initializeSupportedUrlSchemes();
    
    /**
     * @brief Generate unique screenshot filename
     * @param mediaPath Path to media file for context
     * @return Generated screenshot filename
     */
    QString generateScreenshotFilename(const QString& mediaPath = QString()) const;
    
    /**
     * @brief Extract media info using libVLC
     * @param filePath Path to media file
     * @return Extracted media information
     */
    MediaInfo extractMediaInfoVLC(const QString& filePath);
    
    /**
     * @brief Validate file header for security
     * @param filePath Path to file to validate
     * @return true if file header is valid
     */
    bool validateFileHeader(const QString& filePath) const;
    
    /**
     * @brief Check file size limits
     * @param filePath Path to file to check
     * @return true if file size is within limits
     */
    bool checkFileSizeLimit(const QString& filePath) const;
    
    /**
     * @brief Search for subtitle files in directory
     * @param mediaPath Path to media file
     * @param directory Directory to search in
     * @return List of found subtitle files
     */
    QStringList searchSubtitlesInDirectory(const QString& mediaPath, const QString& directory) const;
    
    std::shared_ptr<IMediaEngine> m_mediaEngine;
    std::shared_ptr<PlaybackController> m_playbackController;
    
    bool m_initialized;
    
    // Supported formats
    QStringList m_supportedVideoExtensions;
    QStringList m_supportedAudioExtensions;
    QStringList m_supportedPlaylistExtensions;
    QStringList m_supportedSubtitleExtensions;
    QStringList m_supportedUrlSchemes;
    
    // Current media state
    MediaInfo m_currentMediaInfo;
    QString m_currentMediaPath;
    
    // Screenshot settings
    QString m_screenshotDirectory;
    QString m_screenshotFormat;
    
    // File size limits
    qint64 m_maxFileSize;
    
    // Async operations
    QTimer* m_mediaInfoTimer;
    QTimer* m_subtitleTimer;
    
    // Cache for media info
    QHash<QString, MediaInfo> m_mediaInfoCache;
    
    // Constants
    static constexpr qint64 DEFAULT_MAX_FILE_SIZE = 50LL * 1024 * 1024 * 1024; // 50GB
    static constexpr int MEDIA_INFO_TIMEOUT_MS = 5000; // 5 seconds
    static constexpr int SUBTITLE_SEARCH_TIMEOUT_MS = 2000; // 2 seconds
};