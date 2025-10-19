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
}Validatio
nResult FileUrlSupport::validateMediaFile(const QString& filePath) const
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

QStringList FileUrlSupport}les;
Fibtitleurn su 
    ret
       }       }
ePath;
 s << subtitlileeF subtitl        ath)) {
   , subtitlePdiaPathMatch(meitleSubtif (is     e
   media filhes the itle matc if subtCheck/       /
  ;
        h()PatuteFilefo.absolh = fileInitlePattring subt   QSs) {
     eInfo : fileInfo& fililer (const QF 
    fo;
   e)r::ReadablDiiles | Q QDir::FeFilters,t(namInfoLis.entryiles = dirt fQFileInfoLis   
 rys in directoubtitle fileGet all s//        
   }
xt);
  rg(e"*.%1").a QString(ters <<  nameFil  {
    s) tensionleExedSubtitupportm_st : tring& exQSconst 
    for (ers;st nameFilt   QStringLisions
 btitle extenrs for sute filterea/ C
    /}
        
s;eFileurn subtitlet
        r) {s().exist   if (!dirtory);
 r(direcQDir di   
    
 tleFiles;ubtit s QStringLis
{
   constctory) String& direconst QmediaPath, ing& Stronst Qy(cnDirectoresIhSubtitl:searclSupport:FileUrgList 

QStrineSize;
}_maxFilze() <= msinfo.rn fileIretuh);
    filePatInfo(Info file
    QFilet
{Path) consing& filetrnst QSizeLimit(cokFileSt::checileUrlSuppor
bool F
}
rn true;retu
    validationprehensive  more comuld have shon codectioch - produpproabasic ais is a d
    // Th supporteon isensi if extidme val, assur formatsor othe F    // }
    
n true;
     retur   VE") {
   "WAid(8, 4) == r.m && heade"RIFF")rtsWith(staader.    if (he
/ WAV files
    
    /
    }rn true;  retu      {
 S"))ith("OggstartsW(header. if iles
   GG f
    // O }
    e;
   urn truret
        ")) {LaCrtsWith("fsta(header.    if C files
   // FLA  
 ;
    }
  urn true       ret
  {xE0))) == 00xE0] & header[1 (F' &&xF== '\r[0] ) || (headeWith("ID3"r.starts if (heade
   P3 files // M
    
   
    }rn true;     retu3")) {
   \xA\xDF\x45\x1AWith(".startsif (header     files
    // MKV 
   }
    true;
 eturn      r") {
  VI  "Aid(8, 4) ==er.m) && headith("RIFF"r.startsWheade    if (iles
    // AVI f

    };
    turn true   re   ") {
  "ftypid(4, 4) == der.m   if (heaes
 4V/MOV fil MP4/M   //
 es
    gnatur sirehensivecompe  would havonementatimplfull iheck - a implified c is a s/ Thistures
    /le signaedia fimon m for comckon - cheatider valid/ Basic hea
    /    }
    se;
al  return f     y()) {
 isEmptr.   if (heade 
 
   ();.close file16);
   le.read(der = fieArray hea   QBytation
 der validytes for heast 16 bRead fir  
    // 
  
    }turn false;    re    )) {
ce::ReadOnlyIODevile.open(Qf (!fi);
    iathfilePFile file(
    Qconst
{h) ePatfiltring& der(const QSHeadateFile::valiSupportUrlilebool F;
}

return info    
    
toString(); << info.dia info:"Extracted me< "rt) <ppog(fileUrlSu
    qCDebu   > 0;
  ion& info.duratudio) &info.hasAhasVideo || fo. (ind =o.isVali   
    infstance);
 e(vlcInasele libvlc_rdia);
   a_release(melibvlc_medi up
    an Cle //
      }
    ee(mrl);
 c_fr     libvl);
   r(().toUppe.suffix fileInfoformat = info.     8(mrl));
  mUtfng::frorl(QStrirl u
        QUf (mrl) {a);
    i(medi_get_mrllibvlc_media =  char* mrlt info
    Get forma
    //   );
 kCountks, tracrelease(traccks_a_trac_medi libvl   ase tracks
/ Rele
    /
       }    }
 
          break;   t:
         defaul   
                  reak;
              b           }
            
>psz_codec);ck-ratf8(t:fromUc = QString:dioCodeau   info.          {
       psz_codec) ack->    if (tr       }
           
          ps to kbertConv000; // trate / 1->i_biackte = traudioBitra    info.         {
        0) _bitrate >k->irac      if (t    
      io->i_rate;>audck-ate = trampleRaudioSa      info.       
   nnels;udio->i_charack->a t =udioChannelsinfo.a           ;
     io = trueudnfo.hasA          i      _audio:
ibvlc_track  case l                
        ;
      break         }
                  
 z_codec);track->psf8(ng::fromUtdec = QStrieoCoid.vfo      in          
    _codec) {track->psz if (                    }

            to kbps; // Convert / 1000_bitrate track->iitrate =fo.videoB         in       > 0) {
    rate i_bitf (track-> i              _den;
 _frame_rateeo->iack->vidnum / trrate_e_deo->i_frame)track->vidoublRate = (rame info.f          ht;
     eo->i_heigtrack->vid= height fo. in               ;
thideo->i_widrack->v= th nfo.widt       i
         rue; tasVideo =nfo.h  i        :
      ideock_vbvlc_tra case li        {
    ack->i_type)  switch (tr    
      
    ;ks[i]track = tracrack_t* ia_t_medlc     libv i++) {
   ackCount;tr = 0; i < ned int ifor (unsig
    
    );cksdia, &tracks_get(meedia_tralc_munt = libvint trackCoed 
    unsign;cksra tck_t**ia_tra libvlc_medo
   cks infet tra
    // G
        }econds
o millisrt t; // Conve/ 1000 = duration duration  info.{
      ion > 0) (durata);
    if on(mediget_duratiia_bvlc_medon = li duratic_time_tlibvln
     Get duratio  
    //
  
    }imeout++;   t00);
     sleep(1:m QThread:
       0) {t < 5eoued && timus_skipptatia_parsed_slc_meda) == libvmeditatus(d_st_parsea_gec_medihile (libvl 0;
    weout =t tim)
    inutwith timeote (leing to comppars a bit for  // Wait     
 dia);
 parse(memedia_vlc_ibinfo
    l to get mediarse // Pa
        }
    n info;
      returnce);
  taease(vlcIns libvlc_rel
       tion";trac for info exte VLC mediacreailed to  "Fat) <<or(fileUrlSuppngarniqCW        {
dia)  (!me
    if());stData.con.toUtf8()Pathance, filestInvlcw_path(lc_media_ne= libvmedia t* c_media_   libvl
 te media   // Crea
    
 ;
    }n infotur    renfo";
     media iance for instate VLCreled to c< "Faiport) <g(fileUrlSupCWarnin{
        q) tance(!vlcInsif ;
    s)), vlcArgvlcArgs[0]/sizeof(s)rgvlcAf(_new(sizeovlcstance = libt* vlcInce_anlibvlc_inst
    
    "
    };  "--no-osd     pu",
 "--no-s        io",
  "--no-aud",
      "--no-video     ",
   mmyf=du "--int{
       lcArgs[] = onst char* vtion
    c extraca infomedie for ancinstLC mporary libVteate a 
    // Cre();
    eNameBaseteo.compl fileInf.title =  info;
  ize()Info.s = fileleSize  info.fi  ePath);
(filleInfoQFileInfo fi   
    
 ilePath;Path = f  info.file  Info info;
ia
    MedfilePath)
{t QString& LC(consMediaInfoVrt::extractFileUrlSuppodiaInfo 

Mee);
}(filenamthPabsoluteFile).atorynshotDireccreem_s QDir(rn
    retu
    er());rmat.toLowhotFop, m_screensmestamaseName, ti%3").arg(b%2.g("%1_ame = QStrinng filentri;
    QShmmss")_h"yyyyMMddg(e().toStrintDateTimTime::currenmp = QDateg timestaQStrin
       
    }
 reenshot";"_Sc() + aseNameeBo.completInfdiaaseName = me;
        bo(mediaPath)fo mediaInf  QFileIn     
 y()) {Path.isEmpt(!media    if ";
    
nshotnPlay_Screeme = "EoaseNaing b
{
    QStr) constmediaPathng& QStrit (conshotFilenameateScreensport::generup FileUrlS
QStringcast";
}
outt" << "sh< "icecas" <ash" << "d "hls         <<           "
     " << "tcp" << "rtp<< "udpe"  "fil" <<<< "mmstsh" " << "mm< "mms        <                mps"
 "rt" << "rtmp<< "rtsp" tps" << " << "f" << "ftpttpstp" << "h << "htesemrlSchortedU_supp  m
{
  UrlSchemes()portedtializeSupt::iniupporrlS
void FileUrc";
}
sf" << "lf" << "s "usup" << "s  <<                    
           s"<< "mk" < "mpl2"pjs" < << < "psb"" <jssaqt" << " << "                                "txt"
 t" <<" << "rmisai" << "p" << "sm< "dfx" <tml  << "t                               "vtt"
dx" << sub" << "i" << "a << "ss" << "ass" << "srtnssioExtenSubtitlerted    m_supponsions
le exte Subtit    
    //"rmx";
" << << "rms << "rmp"  << "rv" << "ra""rm" "ram" <<       <<                         l"
  "zp" << smi" << ""smil<<  << "wpl" x"" << "wmvx     << "w                          x"
  < "wa"asx" << f" <"xsp"pls" << " <<  "m3u8m3u" <<s << "siontenlaylistExportedP
    m_supextensionsist // Playl    
    < "mpp";
mp+" <     << "                      "mpc"
   2" << vr" << "sdds" << "a" << "s< "htkxi" <<< " "pvf"    <<                        mat"
   f" << " << "s"ircam"<  "voc" <" <<"rf64"w64" << f" << ca     << "                       "aifc"
  "aiff" << gsm" << spx" << "< "<< "3ga" < << "ra" "au"      <<                "
         "amr"dts" <<  "ac3" << "tta" <<"wv" <<  "ape" <<  <<pus"     << "o                         a"
ma" << "m4" << "w"ogg" << aacav" << "" << "w << "flacmp3" << "ExtensionsortedAudiouppm_snsions
    dio exte/ Au   
    /4b";
 " << "f"f4ap" << v" << "f4"f4nsv" <<  << "                     
         "roq"<<< "mxf" " <2ts" << "m << "mts"<< "ts" "dvv" << "og  <<                           "vob"
  mvb" << " << "r" << "rm "asf"3g2" <<"3gp" << " << "m4v        <<                     "
  " << "webmlv "f< "wmv" <<"mov" << << "mkv" < "avi" mp4" << << "ionsideoExtensm_supportedVns
    ioxtenseo eid
    // V()
{onsrtedExtensizeSuppot::initialilSupporUroid File

v
}meout";tion tietectitle d "Subupport) <<g(fileUrlSebuqCD  {
  )
onComplete(Detectititlert::onSubppod FileUrlSu

voimeout";
}action titro ex"Media infort) << ileUrlSuppbug(f
    qCDeplete()
{tractionComnMediaInfoExSupport::oUrlvoid File

}

    };ist"playled to ld be addng files woule, remainit fiLoaded firspport) << "UrlSuqCDebug(file     mented
   impleem is laylist systn phelist wles to playremaining fiO: Add  TOD
        //.first())) {Pathsile(fileediaF   if (loadMrst file
  Load the fi
    //  }
     ;
      return {
   s.isEmpty())ePathf (fil
{
    iPaths)List& filengt QStri(const::openFilespporrlSuid FileU
}

vo);MediaUrl(url load)
{
   st QUrl& url(conrlport::openUileUrlSup
void F;
}
h)atile(fileP loadMediaF  ath)
{
 tring& filePt QSle(consort::openFieUrlSuppoid Fil

v;
}= "file"() =wer).toLoheme(| url.sce() |il.isLocalFn urlreturnst
{
     co& url)st QUrl(conleUrl:isLocalFiort:UrlSupp
bool File
}
UrlSchemes;pportedreturn m_su
    st
{ conSchemes()edUrlSupport::get