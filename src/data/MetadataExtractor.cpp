#include "data/MetadataExtractor.h"
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QDebug>
#include <QMutexLocker>
#include <QMimeDatabase>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(metadataExtractor, "eonplay.data.metadata")

namespace EonPlay {
namespace Data {

MetadataExtractor::MetadataExtractor(QObject* parent)
    : QObject(parent)
    , m_networkManager(nullptr)
{
    // Set default cache directory
    m_cacheDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/metadata";
    QDir().mkpath(m_cacheDirectory);
    
    // Set default cover art directory
    m_options.coverArtDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/coverart";
    QDir().mkpath(m_options.coverArtDirectory);
    
    setupNetworkManager();
    
    // Find external tools
    m_ffmpegPath = findExecutable("ffmpeg");
    m_ffprobePath = findExecutable("ffprobe");
    
    qCInfo(metadataExtractor) << "MetadataExtractor initialized";
    qCDebug(metadataExtractor) << "FFmpeg path:" << m_ffmpegPath;
    qCDebug(metadataExtractor) << "FFprobe path:" << m_ffprobePath;
}

void MetadataExtractor::extractMetadata(const QString& filePath)
{
    if (!QFile::exists(filePath)) {
        emit metadataExtractionFailed(filePath, "File does not exist");
        return;
    }
    
    if (!isFormatSupported(filePath)) {
        emit metadataExtractionFailed(filePath, "Unsupported file format");
        return;
    }
    
    // Check cache first
    if (m_options.cacheResults) {
        MediaMetadata cachedMetadata = loadFromCache(filePath);
        if (cachedMetadata.isValid) {
            emit metadataExtracted(filePath, cachedMetadata);
            return;
        }
    }
    
    // Extract metadata
    MediaMetadata metadata = extractLocalMetadata(filePath);
    
    if (metadata.isValid) {
        // Cache the result
        if (m_options.cacheResults) {
            saveToCache(filePath, metadata);
        }
        
        emit metadataExtracted(filePath, metadata);
        
        // Enhance with web data if enabled
        if (m_options.fetchFromWeb && m_options.method != LocalOnly) {
            enhanceMetadataFromWeb(metadata, filePath);
        }
    } else {
        emit metadataExtractionFailed(filePath, "Failed to extract metadata");
    }
}

MetadataExtractor::MediaMetadata MetadataExtractor::extractMetadataSync(const QString& filePath)
{
    MediaMetadata metadata;
    
    if (!QFile::exists(filePath)) {
        return metadata;
    }
    
    if (!isFormatSupported(filePath)) {
        return metadata;
    }
    
    // Check cache first
    if (m_options.cacheResults) {
        metadata = loadFromCache(filePath);
        if (metadata.isValid) {
            return metadata;
        }
    }
    
    // Extract metadata
    metadata = extractLocalMetadata(filePath);
    
    // Cache the result
    if (metadata.isValid && m_options.cacheResults) {
        saveToCache(filePath, metadata);
    }
    
    return metadata;
}

void MetadataExtractor::extractMetadataForFiles(const QStringList& filePaths)
{
    for (const QString& filePath : filePaths) {
        extractMetadata(filePath);
    }
}

void MetadataExtractor::extractCoverArt(const QString& filePath, const QString& outputPath)
{
    if (!QFile::exists(filePath)) {
        emit coverArtExtractionFailed(filePath, "File does not exist");
        return;
    }
    
    QString coverArtPath = extractCoverArtSync(filePath, outputPath);
    
    if (!coverArtPath.isEmpty()) {
        emit coverArtExtracted(filePath, coverArtPath);
    } else {
        emit coverArtExtractionFailed(filePath, "No cover art found or extraction failed");
    }
}

QString MetadataExtractor::extractCoverArtSync(const QString& filePath, const QString& outputPath)
{
    QString finalOutputPath = outputPath;
    if (finalOutputPath.isEmpty()) {
        finalOutputPath = generateCoverArtPath(filePath);
    }
    
    // Try TagLib first (for audio files)
    if (isAudioFile(filePath)) {
        QString result = extractCoverArtWithTagLib(filePath, finalOutputPath);
        if (!result.isEmpty()) {
            return result;
        }
    }
    
    // Try FFmpeg as fallback
    return extractCoverArtWithFFmpeg(filePath, finalOutputPath);
}

void MetadataExtractor::enhanceMetadataFromWeb(const MediaMetadata& basicMetadata, const QString& filePath)
{
    if (m_options.method == LocalOnly) {
        return;
    }
    
    // Try different web services
    if (m_apiKeys.contains("musicbrainz") || true) { // MusicBrainz doesn't require API key
        fetchFromMusicBrainz(basicMetadata, filePath);
    }
    
    if (m_apiKeys.contains("lastfm")) {
        fetchFromLastFm(basicMetadata, filePath);
    }
    
    if (m_apiKeys.contains("spotify")) {
        fetchFromSpotify(basicMetadata, filePath);
    }
}

void MetadataExtractor::clearCache()
{
    QMutexLocker locker(&m_cacheMutex);
    
    m_metadataCache.clear();
    m_cacheTimestamps.clear();
    
    // Clear cache directory
    QDir cacheDir(m_cacheDirectory);
    cacheDir.removeRecursively();
    cacheDir.mkpath(".");
    
    qCInfo(metadataExtractor) << "Metadata cache cleared";
}

void MetadataExtractor::setCacheDirectory(const QString& directory)
{
    m_cacheDirectory = directory;
    QDir().mkpath(m_cacheDirectory);
}

QStringList MetadataExtractor::supportedAudioFormats()
{
    return QStringList() << "mp3" << "flac" << "wav" << "aac" << "ogg" << "wma" << "m4a" << "opus" << "ape" << "wv";
}

QStringList MetadataExtractor::supportedVideoFormats()
{
    return QStringList() << "mp4" << "avi" << "mkv" << "mov" << "wmv" << "flv" << "webm" << "m4v" << "3gp" << "ogv";
}

bool MetadataExtractor::isFormatSupported(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    QStringList allFormats = supportedAudioFormats();
    allFormats.append(supportedVideoFormats());
    
    return allFormats.contains(extension);
}

MetadataExtractor::MediaMetadata MetadataExtractor::extractLocalMetadata(const QString& filePath)
{
    MediaMetadata metadata;
    
    // Try different extraction methods
    if (isAudioFile(filePath)) {
        // For audio files, try TagLib first
        metadata = extractWithTagLib(filePath);
        
        if (!metadata.isValid && !m_ffprobePath.isEmpty()) {
            // Fallback to FFmpeg
            metadata = extractWithFFmpeg(filePath);
        }
    } else if (isVideoFile(filePath)) {
        // For video files, use FFmpeg
        if (!m_ffprobePath.isEmpty()) {
            metadata = extractWithFFmpeg(filePath);
        }
        
        if (!metadata.isValid) {
            // Fallback to VLC
            metadata = extractWithVLC(filePath);
        }
    }
    
    // Set basic file information
    if (metadata.isValid) {
        QFileInfo fileInfo(filePath);
        if (metadata.title.isEmpty()) {
            metadata.title = fileInfo.completeBaseName();
        }
    }
    
    return metadata;
}

MetadataExtractor::MediaMetadata MetadataExtractor::extractWithTagLib(const QString& filePath)
{
    MediaMetadata metadata;
    
    // This is a placeholder implementation
    // In a real implementation, you would use TagLib C++ library
    // For now, we'll simulate basic metadata extraction
    
    QFileInfo fileInfo(filePath);
    metadata.title = fileInfo.completeBaseName();
    metadata.isValid = true;
    
    qCDebug(metadataExtractor) << "TagLib extraction (placeholder) for:" << filePath;
    
    return metadata;
}

MetadataExtractor::MediaMetadata MetadataExtractor::extractWithFFmpeg(const QString& filePath)
{
    MediaMetadata metadata;
    
    if (m_ffprobePath.isEmpty()) {
        return metadata;
    }
    
    QProcess ffprobe;
    QStringList arguments;
    arguments << "-v" << "quiet"
              << "-print_format" << "json"
              << "-show_format"
              << "-show_streams"
              << filePath;
    
    ffprobe.start(m_ffprobePath, arguments);
    
    if (!ffprobe.waitForFinished(10000)) { // 10 second timeout
        qCWarning(metadataExtractor) << "FFprobe timeout for:" << filePath;
        return metadata;
    }
    
    if (ffprobe.exitCode() != 0) {
        qCWarning(metadataExtractor) << "FFprobe failed for:" << filePath;
        return metadata;
    }
    
    QByteArray output = ffprobe.readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(output);
    
    if (doc.isNull()) {
        qCWarning(metadataExtractor) << "Invalid JSON from FFprobe for:" << filePath;
        return metadata;
    }
    
    QJsonObject root = doc.object();
    
    // Extract format information
    if (root.contains("format")) {
        QJsonObject format = root["format"].toObject();
        
        if (format.contains("duration")) {
            metadata.duration = static_cast<qint64>(format["duration"].toString().toDouble() * 1000);
        }
        
        if (format.contains("bit_rate")) {
            metadata.bitrate = format["bit_rate"].toString().toInt() / 1000; // Convert to kbps
        }
        
        if (format.contains("tags")) {
            QJsonObject tags = format["tags"].toObject();
            
            metadata.title = tags["title"].toString();
            metadata.artist = tags["artist"].toString();
            metadata.album = tags["album"].toString();
            metadata.genre = tags["genre"].toString();
            metadata.albumArtist = tags["album_artist"].toString();
            metadata.composer = tags["composer"].toString();
            metadata.comment = tags["comment"].toString();
            
            if (tags.contains("date")) {
                metadata.year = tags["date"].toString().left(4).toInt();
            }
            
            if (tags.contains("track")) {
                QString track = tags["track"].toString();
                metadata.trackNumber = track.split('/').first().toInt();
            }
            
            if (tags.contains("disc")) {
                QString disc = tags["disc"].toString();
                metadata.discNumber = disc.split('/').first().toInt();
            }
        }
    }
    
    // Extract stream information
    if (root.contains("streams")) {
        QJsonArray streams = root["streams"].toArray();
        
        for (const QJsonValue& streamValue : streams) {
            QJsonObject stream = streamValue.toObject();
            QString codecType = stream["codec_type"].toString();
            
            if (codecType == "video") {
                metadata.width = stream["width"].toInt();
                metadata.height = stream["height"].toInt();
                metadata.videoCodec = stream["codec_name"].toString();
                
                if (stream.contains("r_frame_rate")) {
                    QString frameRate = stream["r_frame_rate"].toString();
                    QStringList parts = frameRate.split('/');
                    if (parts.size() == 2) {
                        double num = parts[0].toDouble();
                        double den = parts[1].toDouble();
                        if (den > 0) {
                            metadata.frameRate = num / den;
                        }
                    }
                }
            } else if (codecType == "audio") {
                metadata.sampleRate = stream["sample_rate"].toString().toInt();
                metadata.channels = stream["channels"].toInt();
                metadata.audioCodec = stream["codec_name"].toString();
                
                if (metadata.codec.isEmpty()) {
                    metadata.codec = metadata.audioCodec;
                }
            }
        }
    }
    
    metadata.isValid = !metadata.title.isEmpty() || metadata.duration > 0;
    
    qCDebug(metadataExtractor) << "FFmpeg extraction completed for:" << filePath;
    
    return metadata;
}

MetadataExtractor::MediaMetadata MetadataExtractor::extractWithVLC(const QString& filePath)
{
    MediaMetadata metadata;
    
    // This is a placeholder for VLC-based metadata extraction
    // In a real implementation, you would use libVLC to extract metadata
    
    QFileInfo fileInfo(filePath);
    metadata.title = fileInfo.completeBaseName();
    metadata.isValid = true;
    
    qCDebug(metadataExtractor) << "VLC extraction (placeholder) for:" << filePath;
    
    return metadata;
}

QString MetadataExtractor::extractCoverArtWithTagLib(const QString& filePath, const QString& outputPath)
{
    // Placeholder for TagLib cover art extraction
    // In a real implementation, you would use TagLib to extract embedded cover art
    
    Q_UNUSED(filePath)
    Q_UNUSED(outputPath)
    
    return QString(); // No cover art extracted in placeholder
}

QString MetadataExtractor::extractCoverArtWithFFmpeg(const QString& filePath, const QString& outputPath)
{
    if (m_ffmpegPath.isEmpty()) {
        return QString();
    }
    
    QProcess ffmpeg;
    QStringList arguments;
    arguments << "-i" << filePath
              << "-an" << "-vcodec" << "copy"
              << "-f" << "image2"
              << "-y" // Overwrite output
              << outputPath;
    
    ffmpeg.start(m_ffmpegPath, arguments);
    
    if (!ffmpeg.waitForFinished(10000)) { // 10 second timeout
        return QString();
    }
    
    if (ffmpeg.exitCode() == 0 && QFile::exists(outputPath)) {
        return outputPath;
    }
    
    return QString();
}

void MetadataExtractor::fetchFromMusicBrainz(const MediaMetadata& metadata, const QString& filePath)
{
    if (metadata.artist.isEmpty() || metadata.title.isEmpty()) {
        return;
    }
    
    QString url = "https://musicbrainz.org/ws/2/recording/";
    QUrlQuery query;
    query.addQueryItem("query", QString("artist:\"%1\" AND recording:\"%2\"").arg(metadata.artist, metadata.title));
    query.addQueryItem("fmt", "json");
    query.addQueryItem("limit", "1");
    
    QUrl requestUrl(url);
    requestUrl.setQuery(query);
    
    QNetworkRequest request = createApiRequest(requestUrl.toString(), "musicbrainz");
    QNetworkReply* reply = m_networkManager->get(request);
    
    m_pendingRequests[reply] = filePath;
    m_pendingMetadata[reply] = metadata;
}

void MetadataExtractor::fetchFromLastFm(const MediaMetadata& metadata, const QString& filePath)
{
    if (!m_apiKeys.contains("lastfm") || metadata.artist.isEmpty() || metadata.title.isEmpty()) {
        return;
    }
    
    QString apiKey = m_apiKeys["lastfm"];
    QString url = "https://ws.audioscrobbler.com/2.0/";
    
    QUrlQuery query;
    query.addQueryItem("method", "track.getInfo");
    query.addQueryItem("api_key", apiKey);
    query.addQueryItem("artist", metadata.artist);
    query.addQueryItem("track", metadata.title);
    query.addQueryItem("format", "json");
    
    QUrl requestUrl(url);
    requestUrl.setQuery(query);
    
    QNetworkRequest request = createApiRequest(requestUrl.toString(), "lastfm");
    QNetworkReply* reply = m_networkManager->get(request);
    
    m_pendingRequests[reply] = filePath;
    m_pendingMetadata[reply] = metadata;
}

void MetadataExtractor::fetchFromSpotify(const MediaMetadata& metadata, const QString& filePath)
{
    // Placeholder for Spotify Web API integration
    Q_UNUSED(metadata)
    Q_UNUSED(filePath)
}

void MetadataExtractor::fetchFromDiscogs(const MediaMetadata& metadata, const QString& filePath)
{
    // Placeholder for Discogs API integration
    Q_UNUSED(metadata)
    Q_UNUSED(filePath)
}

QString MetadataExtractor::generateCoverArtPath(const QString& filePath, const QString& outputDir)
{
    QString dir = outputDir.isEmpty() ? m_options.coverArtDirectory : outputDir;
    QFileInfo fileInfo(filePath);
    
    QString baseName = fileInfo.completeBaseName();
    QString coverArtPath = QDir(dir).absoluteFilePath(baseName + "_cover.jpg");
    
    return coverArtPath;
}

QString MetadataExtractor::calculateMetadataHash(const MediaMetadata& metadata)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(metadata.title.toUtf8());
    hash.addData(metadata.artist.toUtf8());
    hash.addData(metadata.album.toUtf8());
    hash.addData(QByteArray::number(metadata.duration));
    
    return hash.result().toHex();
}

MetadataExtractor::MediaMetadata MetadataExtractor::loadFromCache(const QString& filePath)
{
    QMutexLocker locker(&m_cacheMutex);
    
    MediaMetadata metadata;
    
    if (m_metadataCache.contains(filePath)) {
        QDateTime cacheTime = m_cacheTimestamps.value(filePath);
        QFileInfo fileInfo(filePath);
        
        // Check if cache is still valid (file not modified since cache)
        if (cacheTime >= fileInfo.lastModified()) {
            metadata = m_metadataCache[filePath];
        }
    }
    
    return metadata;
}

void MetadataExtractor::saveToCache(const QString& filePath, const MediaMetadata& metadata)
{
    QMutexLocker locker(&m_cacheMutex);
    
    m_metadataCache[filePath] = metadata;
    m_cacheTimestamps[filePath] = QDateTime::currentDateTime();
}

void MetadataExtractor::setupNetworkManager()
{
    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
        connect(m_networkManager, &QNetworkAccessManager::finished,
                this, &MetadataExtractor::handleNetworkReply);
    }
}

QNetworkRequest MetadataExtractor::createApiRequest(const QString& url, const QString& service)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EonPlay/1.0");
    request.setRawHeader("Accept", "application/json");
    
    if (service == "musicbrainz") {
        // MusicBrainz requires a proper User-Agent
        request.setHeader(QNetworkRequest::UserAgentHeader, "EonPlay/1.0 (contact@eonplay.com)");
    }
    
    return request;
}

void MetadataExtractor::handleNetworkReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    QString filePath = m_pendingRequests.take(reply);
    MediaMetadata originalMetadata = m_pendingMetadata.take(reply);
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        
        // Determine service from URL
        QString service;
        if (reply->url().host().contains("musicbrainz")) {
            service = "musicbrainz";
        } else if (reply->url().host().contains("audioscrobbler")) {
            service = "lastfm";
        }
        
        processWebResponse(data, service, filePath, originalMetadata);
    } else {
        emit webEnhancementFailed(filePath, reply->errorString());
    }
    
    reply->deleteLater();
}

void MetadataExtractor::processWebResponse(const QByteArray& data, const QString& service,
                                          const QString& filePath, const MediaMetadata& originalMetadata)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        emit webEnhancementFailed(filePath, "Invalid JSON response");
        return;
    }
    
    MediaMetadata enhancedMetadata = originalMetadata;
    QJsonObject root = doc.object();
    
    if (service == "musicbrainz") {
        if (root.contains("recordings")) {
            QJsonArray recordings = root["recordings"].toArray();
            if (!recordings.isEmpty()) {
                QJsonObject recording = recordings[0].toObject();
                
                // Enhance metadata with MusicBrainz data
                if (enhancedMetadata.title.isEmpty()) {
                    enhancedMetadata.title = recording["title"].toString();
                }
                
                if (recording.contains("length")) {
                    enhancedMetadata.duration = recording["length"].toInt();
                }
                
                // Extract additional information from releases
                if (recording.contains("releases")) {
                    QJsonArray releases = recording["releases"].toArray();
                    if (!releases.isEmpty()) {
                        QJsonObject release = releases[0].toObject();
                        if (enhancedMetadata.album.isEmpty()) {
                            enhancedMetadata.album = release["title"].toString();
                        }
                        
                        if (release.contains("date")) {
                            QString date = release["date"].toString();
                            enhancedMetadata.year = date.left(4).toInt();
                        }
                    }
                }
            }
        }
    } else if (service == "lastfm") {
        if (root.contains("track")) {
            QJsonObject track = root["track"].toObject();
            
            // Enhance with Last.fm data
            if (track.contains("album")) {
                QJsonObject album = track["album"].toObject();
                if (enhancedMetadata.album.isEmpty()) {
                    enhancedMetadata.album = album["title"].toString();
                }
            }
            
            if (track.contains("toptags")) {
                QJsonObject topTags = track["toptags"].toObject();
                if (topTags.contains("tag")) {
                    QJsonArray tags = topTags["tag"].toArray();
                    if (!tags.isEmpty() && enhancedMetadata.genre.isEmpty()) {
                        enhancedMetadata.genre = tags[0].toObject()["name"].toString();
                    }
                }
            }
        }
    }
    
    // Cache enhanced metadata
    if (m_options.cacheResults) {
        saveToCache(filePath, enhancedMetadata);
    }
    
    emit webEnhancementCompleted(filePath, enhancedMetadata);
}

QString MetadataExtractor::findExecutable(const QString& name)
{
    // Try to find executable in PATH
    QProcess which;
    which.start("where", QStringList() << name); // Windows
    if (!which.waitForFinished(3000) || which.exitCode() != 0) {
        which.start("which", QStringList() << name); // Unix/Linux
        if (!which.waitForFinished(3000) || which.exitCode() != 0) {
            return QString();
        }
    }
    
    QString path = which.readAllStandardOutput().trimmed();
    return path.split('\n').first(); // Take first result if multiple
}

bool MetadataExtractor::isAudioFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    return supportedAudioFormats().contains(extension);
}

bool MetadataExtractor::isVideoFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    return supportedVideoFormats().contains(extension);
}

} // namespace Data
} // namespace EonPlay