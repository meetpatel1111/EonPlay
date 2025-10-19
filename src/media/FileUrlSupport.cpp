#include "media/FileUrlSupport.h"
#include "media/IMediaEngine.h"
#include "media/PlaybackController.h"
#include <QLoggingCategory>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QMimeData>
#include <QStandardPaths>
#include <QDateTime>
#include <QImageWriter>
#include <QBuffer>
#include <QPixmap>
#include <QRegularExpression>
#include <QDirIterator>
#include <QApplication>
#include <QThread>

// libVLC includes for media info extraction
#include <vlc/vlc.h>

Q_LOGGING_CATEGORY(fileUrlSupport, "eonplay.fileurlsupport")

// MediaInfo implementation
void MediaInfo::clear()
{
    filePath.clear();
    title.clear();
    format.clear();
    videoCodec.clear();
    audioCodec.clear();
    duration = 0;
    fileSize = 0;
    videoBitrate = 0;
    audioBitrate = 0;
    width = 0;
    height = 0;
    frameRate = 0.0;
    audioChannels = 0;
    audioSampleRate = 0;
    hasVideo = false;
    hasAudio = false;
    isValid = false;
}

bool MediaInfo::isComplete() const
{
    return isValid && !filePath.isEmpty() && duration > 0 && (hasVideo || hasAudio);
}

QString MediaInfo::toString() const
{
    if (!isValid) {
        return "Invalid media";
    }
    
    QStringList info;
    
    if (!title.isEmpty()) {
        info << QString("Title: %1").arg(title);
    }
    
    if (!format.isEmpty()) {
        info << QString("Format: %1").arg(format);
    }
    
    if (duration > 0) {
        int hours = duration / (1000 * 60 * 60);
        int minutes = (duration % (1000 * 60 * 60)) / (1000 * 60);
        int seconds = (duration % (1000 * 60)) / 1000;
        info << QString("Duration: %1:%2:%3").arg(hours, 2, 10, QChar('0'))
                                            .arg(minutes, 2, 10, QChar('0'))
                                            .arg(seconds, 2, 10, QChar('0'));
    }
    
    if (hasVideo) {
        info << QString("Video: %1x%2").arg(width).arg(height);
        if (frameRate > 0) {
            info << QString("FPS: %1").arg(frameRate, 0, 'f', 2);
        }
        if (!videoCodec.isEmpty()) {
            info << QString("Video Codec: %1").arg(videoCodec);
        }
        if (videoBitrate > 0) {
            info << QString("Video Bitrate: %1 kbps").arg(videoBitrate);
        }
    }
    
    if (hasAudio) {
        if (!audioCodec.isEmpty()) {
            info << QString("Audio Codec: %1").arg(audioCodec);
        }
        if (audioChannels > 0) {
            info << QString("Channels: %1").arg(audioChannels);
        }
        if (audioSampleRate > 0) {
            info << QString("Sample Rate: %1 Hz").arg(audioSampleRate);
        }
        if (audioBitrate > 0) {
            info << QString("Audio Bitrate: %1 kbps").arg(audioBitrate);
        }
    }
    
    if (fileSize > 0) {
        double sizeMB = fileSize / (1024.0 * 1024.0);
        if (sizeMB >= 1024) {
            info << QString("Size: %1 GB").arg(sizeMB / 1024.0, 0, 'f', 2);
        } else {
            info << QString("Size: %1 MB").arg(sizeMB, 0, 'f', 1);
        }
    }
    
    return info.join(", ");
}

// FileUrlSupport implementation
FileUrlSupport::FileUrlSupport(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_screenshotFormat("PNG")
    , m_maxFileSize(DEFAULT_MAX_FILE_SIZE)
    , m_mediaInfoTimer(new QTimer(this))
    , m_subtitleTimer(new QTimer(this))
{
    // Set up screenshot directory
    m_screenshotDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (m_screenshotDirectory.isEmpty()) {
        m_screenshotDirectory = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    m_screenshotDirectory += "/EonPlay Screenshots";
    
    // Set up timers
    m_mediaInfoTimer->setSingleShot(true);
    m_mediaInfoTimer->setInterval(MEDIA_INFO_TIMEOUT_MS);
    connect(m_mediaInfoTimer, &QTimer::timeout, this, &FileUrlSupport::onMediaInfoExtractionComplete);
    
    m_subtitleTimer->setSingleShot(true);
    m_subtitleTimer->setInterval(SUBTITLE_SEARCH_TIMEOUT_MS);
    connect(m_subtitleTimer, &QTimer::timeout, this, &FileUrlSupport::onSubtitleDetectionComplete);
    
    qCDebug(fileUrlSupport) << "FileUrlSupport created";
}

FileUrlSupport::~FileUrlSupport()
{
    shutdown();
    qCDebug(fileUrlSupport) << "FileUrlSupport destroyed";
}

bool FileUrlSupport::initialize()
{
    if (m_initialized) {
        qCWarning(fileUrlSupport) << "FileUrlSupport already initialized";
        return true;
    }
    
    qCDebug(fileUrlSupport) << "Initializing FileUrlSupport";
    
    // Initialize supported file extensions
    initializeSupportedExtensions();
    
    // Initialize supported URL schemes
    initializeSupportedUrlSchemes();
    
    // Ensure screenshot directory exists
    QDir screenshotDir(m_screenshotDirectory);
    if (!screenshotDir.exists()) {
        if (!screenshotDir.mkpath(".")) {
            qCWarning(fileUrlSupport) << "Failed to create screenshot directory:" << m_screenshotDirectory;
            // Fall back to temp directory
            m_screenshotDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        }
    }
    
    m_initialized = true;
    qCDebug(fileUrlSupport) << "FileUrlSupport initialized successfully";
    qCDebug(fileUrlSupport) << "Screenshot directory:" << m_screenshotDirectory;
    qCDebug(fileUrlSupport) << "Supported video extensions:" << m_supportedVideoExtensions.size();
    qCDebug(fileUrlSupport) << "Supported audio extensions:" << m_supportedAudioExtensions.size();
    qCDebug(fileUrlSupport) << "Supported URL schemes:" << m_supportedUrlSchemes;
    
    return true;
}

void FileUrlSupport::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    qCDebug(fileUrlSupport) << "Shutting down FileUrlSupport";
    
    // Stop timers
    m_mediaInfoTimer->stop();
    m_subtitleTimer->stop();
    
    // Clear cache
    m_mediaInfoCache.clear();
    
    // Clear current media info
    m_currentMediaInfo.clear();
    m_currentMediaPath.clear();
    
    // Disconnect from engines
    m_mediaEngine.reset();
    m_playbackController.reset();
    
    m_initialized = false;
    qCDebug(fileUrlSupport) << "FileUrlSupport shut down";
}

void FileUrlSupport::setMediaEngine(std::shared_ptr<IMediaEngine> engine)
{
    m_mediaEngine = engine;
    qCDebug(fileUrlSupport) << "Media engine" << (engine ? "connected" : "disconnected");
}

void FileUrlSupport::setPlaybackController(std::shared_ptr<PlaybackController> controller)
{
    m_playbackController = controller;
    qCDebug(fileUrlSupport) << "Playback controller" << (controller ? "connected" : "disconnected");
}

bool FileUrlSupport::loadMediaFile(const QString& filePath)
{
    if (!m_initialized) {
        qCWarning(fileUrlSupport) << "FileUrlSupport not initialized";
        return false;
    }
    
    if (filePath.isEmpty()) {
        qCWarning(fileUrlSupport) << "Empty file path provided";
        return false;
    }
    
    qCDebug(fileUrlSupport) << "Loading media file:" << filePath;
    
    // Validate file
    ValidationResult validation = validateMediaFile(filePath);
    if (validation != ValidationResult::Valid) {
        QString errorMessage = getValidationErrorMessage(validation, filePath);
        qCWarning(fileUrlSupport) << "File validation failed:" << errorMessage;
        emit fileValidationFailed(filePath, validation, errorMessage);
        return false;
    }
    
    // Load media using media engine
    if (!m_mediaEngine) {
        qCWarning(fileUrlSupport) << "No media engine available";
        emit fileValidationFailed(filePath, ValidationResult::SecurityRisk, "No media engine available");
        return false;
    }
    
    bool success = m_mediaEngine->loadMedia(filePath);
    if (!success) {
        qCWarning(fileUrlSupport) << "Media engine failed to load file:" << filePath;
        emit fileValidationFailed(filePath, ValidationResult::CorruptedFile, "Failed to load media file");
        return false;
    }
    
    // Update current media info
    m_currentMediaPath = filePath;
    
    // Extract media information asynchronously
    QTimer::singleShot(100, this, [this, filePath]() {
        MediaInfo info = extractMediaInfo(filePath);
        m_currentMediaInfo = info;
        
        if (info.isValid) {
            emit mediaInfoExtracted(filePath, info);
            emit mediaFileLoaded(filePath, info);
            
            // Auto-detect subtitles
            QStringList subtitles = autoDetectSubtitles(filePath);
            if (!subtitles.isEmpty()) {
                emit subtitlesDetected(filePath, subtitles);
            }
        }
    });
    
    qCDebug(fileUrlSupport) << "Media file loaded successfully:" << filePath;
    return true;
}

bool FileUrlSupport::loadMediaUrl(const QUrl& url)
{
    if (!m_initialized) {
        qCWarning(fileUrlSupport) << "FileUrlSupport not initialized";
        return false;
    }
    
    if (!url.isValid()) {
        qCWarning(fileUrlSupport) << "Invalid URL provided:" << url.toString();
        emit urlValidationFailed(url, "Invalid URL format");
        return false;
    }
    
    qCDebug(fileUrlSupport) << "Loading media URL:" << url.toString();
    
    // Validate URL
    if (!isValidStreamUrl(url)) {
        QString errorMessage = QString("Unsupported URL scheme: %1").arg(url.scheme());
        qCWarning(fileUrlSupport) << errorMessage;
        emit urlValidationFailed(url, errorMessage);
        return false;
    }
    
    // Handle local file URLs
    if (isLocalFileUrl(url)) {
        QString localPath = url.toLocalFile();
        return loadMediaFile(localPath);
    }
    
    // Load URL using media engine
    if (!m_mediaEngine) {
        qCWarning(fileUrlSupport) << "No media engine available";
        emit urlValidationFailed(url, "No media engine available");
        return false;
    }
    
    bool success = m_mediaEngine->loadMedia(url.toString());
    if (!success) {
        qCWarning(fileUrlSupport) << "Media engine failed to load URL:" << url.toString();
        emit urlValidationFailed(url, "Failed to load media URL");
        return false;
    }
    
    // Update current media info
    m_currentMediaPath = url.toString();
    
    // Create basic media info for URLs
    MediaInfo info;
    info.filePath = url.toString();
    info.title = url.fileName();
    if (info.title.isEmpty()) {
        info.title = url.host();
    }
    info.isValid = true;
    
    m_currentMediaInfo = info;
    
    qCDebug(fileUrlSupport) << "Media URL loaded successfully:" << url.toString();
    emit mediaUrlLoaded(url, info);
    
    return true;
}

ValidationResult FileUrlSupport::validateMediaFile(const QString& filePath) const
{
    if (filePath.isEmpty()) {
        return ValidationResult::FileNotFound;
    }
    
    QFileInfo fileInfo(filePath);
    
    // Check if file exists
    if (!fileInfo.exists()) {
        return ValidationResult::FileNotFound;
    }
    
    // Check if file is readable
    if (!fileInfo.isReadable()) {
        return ValidationResult::AccessDenied;
    }
    
    // Check file size
    if (!checkFileSizeLimit(filePath)) {
        return ValidationResult::FileTooLarge;
    }
    
    // Check file extension
    QString extension = fileInfo.suffix().toLower();
    QStringList allSupported = m_supportedVideoExtensions + m_supportedAudioExtensions + m_supportedPlaylistExtensions;
    
    if (!allSupported.contains(extension)) {
        return ValidationResult::UnsupportedFormat;
    }
    
    // Validate file header for security
    if (!validateFileHeader(filePath)) {
        return ValidationResult::SecurityRisk;
    }
    
    return ValidationResult::Valid;
}

QString FileUrlSupport::getValidationErrorMessage(ValidationResult result, const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    
    switch (result) {
        case ValidationResult::Valid:
            return "File is valid";
            
        case ValidationResult::UnsupportedFormat:
            return QString("Unsupported file format: %1").arg(fileInfo.suffix().toUpper());
            
        case ValidationResult::FileNotFound:
            return QString("File not found: %1").arg(fileInfo.fileName());
            
        case ValidationResult::FileTooLarge:
            return QString("File too large: %1 (max %2 GB)")
                   .arg(fileInfo.fileName())
                   .arg(m_maxFileSize / (1024LL * 1024 * 1024));
            
        case ValidationResult::AccessDenied:
            return QString("Cannot access file: %1").arg(fileInfo.fileName());
            
        case ValidationResult::CorruptedFile:
            return QString("File appears to be corrupted: %1").arg(fileInfo.fileName());
            
        case ValidationResult::SecurityRisk:
            return QString("File poses security risk: %1").arg(fileInfo.fileName());
            
        default:
            return "Unknown validation error";
    }
}

bool FileUrlSupport::canHandleMimeData(const QMimeData* mimeData) const
{
    if (!mimeData) {
        return false;
    }
    
    // Check for URLs
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        for (const QUrl& url : urls) {
            if (isValidStreamUrl(url) || isLocalFileUrl(url)) {
                return true;
            }
        }
    }
    
    // Check for text URLs
    if (mimeData->hasText()) {
        QString text = mimeData->text().trimmed();
        QUrl url(text);
        if (url.isValid() && isValidStreamUrl(url)) {
            return true;
        }
    }
    
    return false;
}

QStringList FileUrlSupport::processDroppedMedia(const QMimeData* mimeData)
{
    QStringList processedFiles;
    
    if (!mimeData || !m_initialized) {
        return processedFiles;
    }
    
    qCDebug(fileUrlSupport) << "Processing dropped media";
    
    QStringList failedFiles;
    
    // Process URLs
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        for (const QUrl& url : urls) {
            if (isLocalFileUrl(url)) {
                QString filePath = url.toLocalFile();
                if (loadMediaFile(filePath)) {
                    processedFiles << filePath;
                } else {
                    failedFiles << filePath;
                }
            } else if (isValidStreamUrl(url)) {
                if (loadMediaUrl(url)) {
                    processedFiles << url.toString();
                } else {
                    failedFiles << url.toString();
                }
            }
        }
    }
    
    // Process text URLs
    if (mimeData->hasText() && processedFiles.isEmpty()) {
        QString text = mimeData->text().trimmed();
        QUrl url(text);
        if (url.isValid() && isValidStreamUrl(url)) {
            if (loadMediaUrl(url)) {
                processedFiles << url.toString();
            } else {
                failedFiles << url.toString();
            }
        }
    }
    
    qCDebug(fileUrlSupport) << "Processed" << processedFiles.size() << "files successfully";
    qCDebug(fileUrlSupport) << "Failed to process" << failedFiles.size() << "files";
    
    emit dragDropProcessed(processedFiles, failedFiles);
    
    return processedFiles;
}

QStringList FileUrlSupport::getSupportedExtensions() const
{
    return m_supportedVideoExtensions + m_supportedAudioExtensions + m_supportedPlaylistExtensions;
}

QString FileUrlSupport::getFileFilter() const
{
    QStringList videoFilters;
    for (const QString& ext : m_supportedVideoExtensions) {
        videoFilters << QString("*.%1").arg(ext);
    }
    
    QStringList audioFilters;
    for (const QString& ext : m_supportedAudioExtensions) {
        audioFilters << QString("*.%1").arg(ext);
    }
    
    QStringList playlistFilters;
    for (const QString& ext : m_supportedPlaylistExtensions) {
        playlistFilters << QString("*.%1").arg(ext);
    }
    
    QStringList allFilters = videoFilters + audioFilters + playlistFilters;
    
    QString filter = "All Supported Media (";
    filter += allFilters.join(" ");
    filter += ");;";
    
    if (!videoFilters.isEmpty()) {
        filter += "Video Files (";
        filter += videoFilters.join(" ");
        filter += ");;";
    }
    
    if (!audioFilters.isEmpty()) {
        filter += "Audio Files (";
        filter += audioFilters.join(" ");
        filter += ");;";
    }
    
    if (!playlistFilters.isEmpty()) {
        filter += "Playlist Files (";
        filter += playlistFilters.join(" ");
        filter += ");;";
    }
    
    filter += "All Files (*.*)";
    
    return filter;
}

MediaInfo FileUrlSupport::extractMediaInfo(const QString& filePath)
{
    // Check cache first
    if (m_mediaInfoCache.contains(filePath)) {
        QFileInfo fileInfo(filePath);
        // Check if file was modified since cache
        MediaInfo cached = m_mediaInfoCache[filePath];
        if (fileInfo.lastModified().toMSecsSinceEpoch() <= cached.fileSize) { // Using fileSize as timestamp
            return cached;
        }
    }
    
    qCDebug(fileUrlSupport) << "Extracting media info for:" << filePath;
    
    MediaInfo info = extractMediaInfoVLC(filePath);
    
    // Cache the result
    if (info.isValid) {
        m_mediaInfoCache[filePath] = info;
    }
    
    return info;
}

MediaInfo FileUrlSupport::extractMediaInfoVLC(const QString& filePath)
{
    MediaInfo info;
    info.clear();
    
    if (filePath.isEmpty()) {
        return info;
    }
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return info;
    }
    
    info.filePath = filePath;
    info.fileSize = fileInfo.size();
    
    // For now, return basic info without VLC dependency
    info.isValid = true;
    
    return info;
}

QStringList FileUrlSupport::autoDetectSubtitles(const QString& mediaPath)
{
    QStringList subtitleFiles;
    
    if (mediaPath.isEmpty()) {
        return subtitleFiles;
    }
    
    qCDebug(fileUrlSupport) << "Auto-detecting subtitles for:" << mediaPath;
    
    QFileInfo mediaInfo(mediaPath);
    QString mediaDir = mediaInfo.absolutePath();
    QString baseName = mediaInfo.completeBaseName();
    
    // Search in the same directory as the media file
    subtitleFiles = searchSubtitlesInDirectory(mediaPath, mediaDir);
    
    // Also search in common subtitle subdirectories
    QStringList subDirs = {"Subtitles", "Subs", "subtitles", "subs"};
    for (const QString& subDir : subDirs) {
        QString subDirPath = mediaDir + "/" + subDir;
        if (QDir(subDirPath).exists()) {
            QStringList subDirSubs = searchSubtitlesInDirectory(mediaPath, subDirPath);
            subtitleFiles.append(subDirSubs);
        }
    }
    
    // Remove duplicates
    subtitleFiles.removeDuplicates();
    
    qCDebug(fileUrlSupport) << "Found" << subtitleFiles.size() << "subtitle files";
    
    return subtitleFiles;
}

bool FileUrlSupport::isSubtitleMatch(const QString& mediaPath, const QString& subtitlePath) const
{
    QFileInfo mediaInfo(mediaPath);
    QFileInfo subtitleInfo(subtitlePath);
    
    QString mediaBaseName = mediaInfo.completeBaseName().toLower();
    QString subtitleBaseName = subtitleInfo.completeBaseName().toLower();
    
    // Exact match
    if (mediaBaseName == subtitleBaseName) {
        return true;
    }
    
    // Check if subtitle name starts with media name
    if (subtitleBaseName.startsWith(mediaBaseName)) {
        return true;
    }
    
    // Check for common patterns like movie.en.srt, movie.english.srt
    QRegularExpression pattern(QString("^%1[._-]").arg(QRegularExpression::escape(mediaBaseName)));
    if (pattern.match(subtitleBaseName).hasMatch()) {
        return true;
    }
    
    return false;
}

QStringList FileUrlSupport::getSupportedSubtitleExtensions() const
{
    return m_supportedSubtitleExtensions;
}

QString FileUrlSupport::captureScreenshot(const QString& filePath)
{
    if (!m_mediaEngine || !m_mediaEngine->hasVideo()) {
        qCWarning(fileUrlSupport) << "Cannot capture screenshot: No video content";
        emit screenshotCaptureFailed("No video content available");
        return QString();
    }
    
    qint64 currentPosition = m_mediaEngine->position();
    return captureScreenshotAtPosition(currentPosition, filePath);
}

QString FileUrlSupport::captureScreenshotAtPosition(qint64 position, const QString& filePath)
{
    if (!m_mediaEngine || !m_mediaEngine->hasVideo()) {
        qCWarning(fileUrlSupport) << "Cannot capture screenshot: No video content";
        emit screenshotCaptureFailed("No video content available");
        return QString();
    }
    
    qCDebug(fileUrlSupport) << "Capturing screenshot at position:" << position;
    
    // Get video frame from media engine
    QString frameData = m_mediaEngine->getVideoFrame(position);
    if (frameData.isEmpty()) {
        qCWarning(fileUrlSupport) << "Failed to get video frame";
        emit screenshotCaptureFailed("Failed to capture video frame");
        return QString();
    }
    
    // Decode base64 frame data
    QByteArray imageData = QByteArray::fromBase64(frameData.toUtf8());
    QPixmap pixmap;
    if (!pixmap.loadFromData(imageData)) {
        qCWarning(fileUrlSupport) << "Failed to decode video frame data";
        emit screenshotCaptureFailed("Failed to decode video frame");
        return QString();
    }
    
    // Generate filename if not provided
    QString screenshotPath = filePath;
    if (screenshotPath.isEmpty()) {
        screenshotPath = generateScreenshotFilename(m_currentMediaPath);
    }
    
    // Ensure directory exists
    QFileInfo screenshotInfo(screenshotPath);
    QDir screenshotDir = screenshotInfo.absoluteDir();
    if (!screenshotDir.exists()) {
        if (!screenshotDir.mkpath(".")) {
            qCWarning(fileUrlSupport) << "Failed to create screenshot directory:" << screenshotDir.path();
            emit screenshotCaptureFailed("Failed to create screenshot directory");
            return QString();
        }
    }
    
    // Save screenshot
    if (!pixmap.save(screenshotPath, m_screenshotFormat.toUtf8().constData())) {
        qCWarning(fileUrlSupport) << "Failed to save screenshot:" << screenshotPath;
        emit screenshotCaptureFailed("Failed to save screenshot file");
        return QString();
    }
    
    qCDebug(fileUrlSupport) << "Screenshot saved:" << screenshotPath;
    emit screenshotCaptured(screenshotPath, position);
    
    return screenshotPath;
}

void FileUrlSupport::setScreenshotDirectory(const QString& directory)
{
    if (directory != m_screenshotDirectory) {
        m_screenshotDirectory = directory;
        
        // Ensure directory exists
        QDir dir(directory);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        
        qCDebug(fileUrlSupport) << "Screenshot directory set to:" << directory;
    }
}

void FileUrlSupport::setScreenshotFormat(const QString& format)
{
    // Validate format
    QList<QByteArray> supportedFormats = QImageWriter::supportedImageFormats();
    QByteArray formatBytes = format.toUpper().toUtf8();
    
    if (supportedFormats.contains(formatBytes)) {
        m_screenshotFormat = format.toUpper();
        qCDebug(fileUrlSupport) << "Screenshot format set to:" << m_screenshotFormat;
    } else {
        qCWarning(fileUrlSupport) << "Unsupported screenshot format:" << format;
    }
}

bool FileUrlSupport::isValidStreamUrl(const QUrl& url) const
{
    if (!url.isValid()) {
        return false;
    }
    
    QString scheme = url.scheme().toLower();
    return m_supportedUrlSchemes.contains(scheme);
}

QStringList FileUrlSupport::getSubtitleFiles(const QString& mediaPath) const
{
    QStringList subtitleFiles;
    
    if (mediaPath.isEmpty()) {
        return subtitleFiles;
    }
    
    QFileInfo mediaInfo(mediaPath);
    QString baseName = mediaInfo.completeBaseName();
    QString directory = mediaInfo.absolutePath();
    
    // Check if subtitle matches the media file
    QDir dir(directory);
    QStringList subtitleExtensions = {"srt", "ass", "ssa", "vtt", "sub", "idx"};
    
    for (const QString& ext : subtitleExtensions) {
        QString subtitlePath = dir.absoluteFilePath(baseName + "." + ext);
        if (QFile::exists(subtitlePath)) {
            subtitleFiles << subtitlePath;
        }
    }
    
    return subtitleFiles;
}

QStringList FileUrlSupport::searchSubtitlesInDirectory(const QString& mediaPath, const QString& directory) const
{
    QStringList subtitleFiles;
    
    QDir dir(directory);
    if (!dir.exists()) {
        return subtitleFiles;
    }
    
    // Create filters for subtitle extensions
    QStringList nameFilters;
    for (const QString& extension : m_supportedSubtitleExtensions) {
        nameFilters << QString("*.%1").arg(extension);
    }
    
    // Get all subtitle files in directory
    QFileInfoList files = dir.entryInfoList(nameFilters, QDir::Files | QDir::Readable);
    
    for (const QFileInfo& fileInfo : files) {
        QString subtitlePath = fileInfo.absoluteFilePath();
        subtitleFiles << subtitlePath;
    }
    
    return subtitleFiles;
} 

bool FileUrlSupport::checkFileSizeLimit(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    qint64 fileSize = fileInfo.size();
    qint64 maxFileSize = m_maxFileSize;
    return fileSize <= maxFileSize;
}

bool FileUrlSupport::isValidMediaFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray header = file.read(16);
    file.close();
    
    if (header.size() < 4) {
        return false;
    }
    
    // Check for common media file signatures
    // WAV files
    if (header.startsWith("RIFF") && header.mid(8, 4) == "WAVE") {
        return true;
    }
    
    // OGG files
    if (header.startsWith("OggS")) {
        return true;
    }
    
    // FLAC files
    if (header.startsWith("fLaC")) {
        return true;
    }
    
    // MP3 files
    if (header.startsWith("ID3") || (header[0] == '\xFF' && (header[1] & 0xE0) == 0xE0)) {
        return true;
    }
    
    // MKV files
    if (header.startsWith("\x1A\x45\xDF\xA3")) {
        return true;
    }
    
    // AVI files
    if (header.startsWith("RIFF") && header.mid(8, 4) == "AVI ") {
        return true;

    }
    
    // MP4/MOV files
    if (header.mid(4, 4) == "ftyp") {
        return true;
    }
    
    // This is a simplified check - a full implementation would have
    // more comprehensive validation for common media file signatures
    return false;
}



QString FileUrlSupport::generateScreenshotFilename(const QString& mediaPath) const
{
    if (mediaPath.isEmpty()) {
        return QString();
    }
    
    QFileInfo mediaInfo(mediaPath);
    QString baseName = mediaInfo.completeBaseName();
    
    QString screenshotDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).absoluteFilePath("EonPlay_Screenshots");
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filename = QString("%1_%2_%3.png").arg(baseName, timestamp, "screenshot");
    
    return QDir(screenshotDirectory).absoluteFilePath(filename);
}

QStringList FileUrlSupport::getSupportedUrlSchemes() const
{
    return QStringList() << "http" << "https" << "ftp" << "ftps" << "rtsp" << "rtmp" << "hls" << "dash" << "icecast" << "shout"
                         << "tcp" << "rtp" << "udp" << "file" << "mms" << "mmsh" << "mmst";
}

void FileUrlSupport::initializeSupportedUrlSchemes()
{
    m_supportedUrlSchemes = getSupportedUrlSchemes();
}

QStringList FileUrlSupport::getSupportedSubtitleExtensions() const
{
    return QStringList() << "srt" << "vtt" << "ass" << "ssa" << "sub" << "idx" << "smi" << "rt" << "txt"
                         << "usf" << "jss" << "psb" << "pjs" << "mpl2" << "mks" << "dfxp" << "ttml";
}

QStringList FileUrlSupport::getSupportedPlaylistExtensions() const
{
    return QStringList() << "m3u" << "m3u8" << "pls" << "xspf" << "asx" << "wax" << "wvx" << "wmx"
                         << "wpl" << "smil" << "smi" << "zpl" << "ram" << "rm" << "ra" << "rv" << "rmp" << "rms";
}

QStringList FileUrlSupport::getSupportedAudioExtensions() const
{
    return QStringList() << "mp3" << "flac" << "wav" << "ogg" << "m4a" << "aac" << "wma" 
                         << "opus" << "ape" << "wv" << "tta" << "ac3" << "dts" << "amr"
                         << "au" << "ra" << "3ga" << "spx" << "gsm" << "aiff" << "aifc"
                         << "caf" << "w64" << "rf64" << "voc" << "ircam" << "sf" << "mat"
                         << "pvf" << "htk" << "sds" << "avr" << "sd2" << "flac";
}

QStringList FileUrlSupport::getSupportedVideoExtensions() const
{
    return QStringList() << "mp4" << "avi" << "mkv" << "mov" << "wmv" << "flv" << "webm"
                         << "m4v" << "3gp" << "3g2" << "asf" << "rm" << "rmvb" << "vob"
                         << "ogv" << "dv" << "ts" << "mts" << "m2ts" << "mxf" << "roq"
                         << "nsv" << "f4v" << "f4p" << "f4a" << "f4b";
}



void FileUrlSupport::initializeSupportedExtensions()
{
    m_supportedAudioExtensions = getSupportedAudioExtensions();
    m_supportedVideoExtensions = getSupportedVideoExtensions();
    m_supportedPlaylistExtensions = getSupportedPlaylistExtensions();
}

void FileUrlSupport::onSubtitleDetectionComplete()
{
    qCDebug(fileUrlSupport) << "Subtitle detection timeout";
}

void FileUrlSupport::onMediaInfoExtractionComplete()
{
    qCDebug(fileUrlSupport) << "Media info extraction timeout";
}

void FileUrlSupport::openFiles(const QStringList& filePaths)
{
    if (filePaths.isEmpty()) {
        return;
    }
    
    qCDebug(fileUrlSupport) << "Loading first file, remaining files would be added to playlist system";
    // TODO: Add remaining files to playlist when playlist system is implemented
    
    // Load the first file
    if (loadMediaFile(filePaths.first())) {
        // Successfully loaded
    }
}

void FileUrlSupport::openUrl(const QUrl& url)
{
    loadMediaUrl(url);
}

void FileUrlSupport::openFile(const QString& filePath)
{
    loadMediaFile(filePath);
}

bool FileUrlSupport::isLocalFileUrl(const QUrl& url) const
{
    return url.isLocalFile() || url.scheme().toLower() == "file";
}

