#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

/**
 * @brief Manages internet streaming services integration
 * 
 * Provides access to YouTube/Twitch streams, internet radio (SHOUTcast/Icecast),
 * podcast RSS feeds, and stream recording functionality for EonPlay.
 */
class InternetStreamingService : public QObject
{
    Q_OBJECT

public:
    enum ServiceType {
        YOUTUBE_SERVICE,
        TWITCH_SERVICE,
        INTERNET_RADIO,
        PODCAST_SERVICE,
        UNKNOWN_SERVICE
    };

    enum StreamQuality {
        QUALITY_AUTO,
        QUALITY_144P,
        QUALITY_240P,
        QUALITY_360P,
        QUALITY_480P,
        QUALITY_720P,
        QUALITY_1080P,
        QUALITY_1440P,
        QUALITY_2160P,
        AUDIO_ONLY
    };

    struct StreamInfo {
        QString id;
        QString title;
        QString description;
        QString thumbnailUrl;
        QString streamUrl;
        ServiceType serviceType;
        StreamQuality quality;
        qint64 duration = -1; // -1 for live streams
        bool isLive = false;
        QString uploader;
        QDateTime publishDate;
        int viewCount = 0;
    };

    struct RadioStation {
        QString name;
        QString url;
        QString genre;
        QString description;
        QString website;
        int bitrate = 0;
        QString format; // MP3, AAC, OGG
        bool isOnline = false;
        int listeners = 0;
    };

    struct PodcastEpisode {
        QString title;
        QString description;
        QString audioUrl;
        QDateTime publishDate;
        qint64 duration = 0;
        QString guid;
        qint64 fileSize = 0;
        QString mimeType;
    };

    struct PodcastFeed {
        QString title;
        QString description;
        QString website;
        QString feedUrl;
        QString imageUrl;
        QString author;
        QString category;
        QList<PodcastEpisode> episodes;
        QDateTime lastUpdated;
    };

    explicit InternetStreamingService(QObject *parent = nullptr);
    ~InternetStreamingService();

    // YouTube integration
    bool searchYouTube(const QString& query, int maxResults = 20);
    bool getYouTubeVideoInfo(const QString& videoId);
    QList<StreamInfo> getYouTubeSearchResults() const;
    QString extractYouTubeVideoId(const QString& url) const;
    QStringList getAvailableYouTubeQualities(const QString& videoId) const;

    // Twitch integration
    bool searchTwitchStreams(const QString& query, int maxResults = 20);
    bool getTwitchStreamInfo(const QString& channelName);
    QList<StreamInfo> getTwitchSearchResults() const;
    bool isTwitchLive(const QString& channelName) const;

    // Internet Radio (SHOUTcast/Icecast)
    bool searchRadioStations(const QString& genre = QString(), const QString& query = QString());
    QList<RadioStation> getRadioStations() const;
    bool getRadioStationInfo(const QString& stationUrl);
    QStringList getRadioGenres() const;
    bool isRadioStationOnline(const QString& stationUrl) const;

    // Podcast support
    bool loadPodcastFeed(const QString& feedUrl);
    PodcastFeed getCurrentPodcastFeed() const;
    bool searchPodcasts(const QString& query);
    QList<PodcastFeed> getPodcastSearchResults() const;
    bool downloadPodcastEpisode(const PodcastEpisode& episode, const QString& downloadPath);

    // Stream recording
    bool startRecording(const QString& streamUrl, const QString& outputPath);
    void stopRecording();
    bool isRecording() const;
    qint64 getRecordingDuration() const;
    qint64 getRecordingFileSize() const;

    // General stream utilities
    ServiceType detectServiceType(const QUrl& url) const;
    bool isValidStreamUrl(const QUrl& url) const;
    QString getStreamTitle(const QUrl& url) const;
    
    // Configuration
    void setYouTubeApiKey(const QString& apiKey);
    void setTwitchClientId(const QString& clientId);
    void setUserAgent(const QString& userAgent);
    void setMaxConcurrentDownloads(int maxDownloads);

signals:
    void youtubeSearchCompleted(const QList<StreamInfo>& results);
    void youtubeVideoInfoReceived(const StreamInfo& info);
    void twitchSearchCompleted(const QList<StreamInfo>& results);
    void twitchStreamInfoReceived(const StreamInfo& info);
    void radioStationsLoaded(const QList<RadioStation>& stations);
    void radioStationInfoReceived(const RadioStation& station);
    void podcastFeedLoaded(const PodcastFeed& feed);
    void podcastSearchCompleted(const QList<PodcastFeed>& results);
    void podcastDownloadProgress(const QString& episodeId, int percentage);
    void podcastDownloadCompleted(const QString& episodeId, const QString& filePath);
    void recordingStarted(const QString& outputPath);
    void recordingStopped(qint64 duration, qint64 fileSize);
    void recordingProgress(qint64 duration, qint64 fileSize);
    void serviceError(const QString& error, ServiceType serviceType);

private slots:
    void onNetworkReplyFinished();
    void onNetworkReplyError(QNetworkReply::NetworkError error);
    void onRecordingTimerTimeout();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    // YouTube API helpers
    void processYouTubeSearchResponse(const QJsonObject& response);
    void processYouTubeVideoResponse(const QJsonObject& response);
    QString buildYouTubeSearchUrl(const QString& query, int maxResults) const;
    QString buildYouTubeVideoUrl(const QString& videoId) const;

    // Twitch API helpers
    void processTwitchSearchResponse(const QJsonObject& response);
    void processTwitchStreamResponse(const QJsonObject& response);
    QString buildTwitchSearchUrl(const QString& query, int maxResults) const;
    QString buildTwitchStreamUrl(const QString& channelName) const;

    // Radio station helpers
    void processRadioStationsResponse(const QString& response);
    void processRadioStationInfo(const QString& response);
    QString buildRadioSearchUrl(const QString& genre, const QString& query) const;
    QList<RadioStation> parseSHOUTcastDirectory(const QString& xml) const;
    QList<RadioStation> parseIcecastDirectory(const QString& json) const;

    // Podcast helpers
    void processPodcastFeedResponse(const QString& response);
    void processPodcastSearchResponse(const QJsonObject& response);
    PodcastFeed parseRSSFeed(const QString& xml) const;
    QList<PodcastEpisode> parseRSSEpisodes(const QString& xml) const;

    // Recording helpers
    void setupRecording(const QString& streamUrl, const QString& outputPath);
    void updateRecordingStats();

    // Network utilities
    void makeApiRequest(const QUrl& url, const QString& requestId = QString());
    void setupNetworkManager();
    QNetworkRequest createApiRequest(const QUrl& url) const;

    // Member variables
    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    QMap<QNetworkReply*, QString> m_pendingRequests;
    
    // Service data
    QList<StreamInfo> m_youtubeResults;
    QList<StreamInfo> m_twitchResults;
    QList<RadioStation> m_radioStations;
    QList<PodcastFeed> m_podcastResults;
    PodcastFeed m_currentPodcastFeed;
    
    // API configuration
    QString m_youtubeApiKey;
    QString m_twitchClientId;
    QString m_userAgent;
    int m_maxConcurrentDownloads;
    
    // Recording state
    bool m_isRecording;
    QString m_recordingOutputPath;
    QTimer* m_recordingTimer;
    qint64 m_recordingStartTime;
    QNetworkReply* m_recordingReply;
    
    // Download management
    QMap<QString, QNetworkReply*> m_activeDownloads;
    
    // Constants
    static const QString YOUTUBE_API_BASE_URL;
    static const QString TWITCH_API_BASE_URL;
    static const QString SHOUTCAST_API_BASE_URL;
    static const QString ICECAST_API_BASE_URL;
    static const int DEFAULT_MAX_DOWNLOADS = 3;
    static const int RECORDING_UPDATE_INTERVAL_MS = 1000;
};