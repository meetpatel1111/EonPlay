#pragma once

#include "data/MediaFile.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QMutex>

namespace EonPlay {
namespace Data {

/**
 * @brief Extracts metadata from media files using various methods
 * 
 * Provides comprehensive metadata extraction from audio and video files
 * using local libraries and web APIs for enhanced information.
 */
class MetadataExtractor : public QObject
{
    Q_OBJECT

public:
    enum ExtractionMethod {
        LocalOnly,      // Use only local metadata extraction
        WebEnhanced,    // Use local + web APIs for enhanced metadata
        WebOnly         // Use only web APIs (for testing)
    };

    struct ExtractionOptions {
        ExtractionMethod method = WebEnhanced;
        bool extractCoverArt = true;
        bool fetchFromWeb = true;
        bool cacheResults = true;
        int webRequestTimeout = 10000; // 10 seconds
        QString coverArtDirectory;
    };

    struct MediaMetadata {
        QString title;
        QString artist;
        QString album;
        QString genre;
        QString albumArtist;
        QString composer;
        QString comment;
        int year = 0;
        int trackNumber = 0;
        int discNumber = 0;
        qint64 duration = 0;        // in milliseconds
        int bitrate = 0;            // in kbps
        int sampleRate = 0;         // in Hz
        int channels = 0;
        QString codec;
        QString coverArtPath;
        
        // Video-specific metadata
        int width = 0;
        int height = 0;
        double frameRate = 0.0;
        QString videoCodec;
        QString audioCodec;
        
        // Additional metadata
        QHash<QString, QString> customTags;
        bool isValid = false;
    };

    explicit MetadataExtractor(QObject* parent = nullptr);
    ~MetadataExtractor() = default;

    // Configuration
    void setExtractionOptions(const ExtractionOptions& options) { m_options = options; }
    ExtractionOptions extractionOptions() const { return m_options; }

    void setApiKeys(const QHash<QString, QString>& apiKeys) { m_apiKeys = apiKeys; }
    void setApiKey(const QString& service, const QString& apiKey) { m_apiKeys[service] = apiKey; }

    // Metadata extraction
    void extractMetadata(const QString& filePath);
    MediaMetadata extractMetadataSync(const QString& filePath);
    void extractMetadataForFiles(const QStringList& filePaths);

    // Cover art extraction
    void extractCoverArt(const QString& filePath, const QString& outputPath = QString());
    QString extractCoverArtSync(const QString& filePath, const QString& outputPath = QString());

    // Web metadata enhancement
    void enhanceMetadataFromWeb(const MediaMetadata& basicMetadata, const QString& filePath);

    // Cache management
    void clearCache();
    void setCacheDirectory(const QString& directory);
    QString cacheDirectory() const { return m_cacheDirectory; }

    // Supported formats
    static QStringList supportedAudioFormats();
    static QStringList supportedVideoFormats();
    static bool isFormatSupported(const QString& filePath);

signals:
    void metadataExtracted(const QString& filePath, const MediaMetadata& metadata);
    void metadataExtractionFailed(const QString& filePath, const QString& error);
    void coverArtExtracted(const QString& filePath, const QString& coverArtPath);
    void coverArtExtractionFailed(const QString& filePath, const QString& error);
    void webEnhancementCompleted(const QString& filePath, const MediaMetadata& enhancedMetadata);
    void webEnhancementFailed(const QString& filePath, const QString& error);

private slots:
    void handleNetworkReply();

private:
    // Local metadata extraction
    MediaMetadata extractLocalMetadata(const QString& filePath);
    MediaMetadata extractWithTagLib(const QString& filePath);
    MediaMetadata extractWithFFmpeg(const QString& filePath);
    MediaMetadata extractWithVLC(const QString& filePath);

    // Cover art extraction
    QString extractCoverArtWithTagLib(const QString& filePath, const QString& outputPath);
    QString extractCoverArtWithFFmpeg(const QString& filePath, const QString& outputPath);

    // Web enhancement
    void fetchFromMusicBrainz(const MediaMetadata& metadata, const QString& filePath);
    void fetchFromLastFm(const MediaMetadata& metadata, const QString& filePath);
    void fetchFromSpotify(const MediaMetadata& metadata, const QString& filePath);
    void fetchFromDiscogs(const MediaMetadata& metadata, const QString& filePath);

    // Utility methods
    QString generateCoverArtPath(const QString& filePath, const QString& outputDir = QString());
    QString calculateMetadataHash(const MediaMetadata& metadata);
    MediaMetadata loadFromCache(const QString& filePath);
    void saveToCache(const QString& filePath, const MediaMetadata& metadata);
    
    // Network helpers
    void setupNetworkManager();
    QNetworkRequest createApiRequest(const QString& url, const QString& service);
    void processWebResponse(const QByteArray& data, const QString& service, 
                           const QString& filePath, const MediaMetadata& originalMetadata);

    // Format detection
    QString detectAudioFormat(const QString& filePath);
    QString detectVideoFormat(const QString& filePath);
    bool isAudioFile(const QString& filePath);
    bool isVideoFile(const QString& filePath);

    // Member variables
    ExtractionOptions m_options;
    QHash<QString, QString> m_apiKeys;
    QString m_cacheDirectory;
    
    // Network management
    QNetworkAccessManager* m_networkManager;
    QHash<QNetworkReply*, QString> m_pendingRequests;
    QHash<QNetworkReply*, MediaMetadata> m_pendingMetadata;
    
    // Caching
    QHash<QString, MediaMetadata> m_metadataCache;
    QHash<QString, QDateTime> m_cacheTimestamps;
    mutable QMutex m_cacheMutex;
    
    // External tool paths
    QString m_ffmpegPath;
    QString m_ffprobePath;
};

} // namespace Data
} // namespace EonPlay