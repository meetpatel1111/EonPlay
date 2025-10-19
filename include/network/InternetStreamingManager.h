#ifndef INTERNETSTREAMINGMANAGER_H
#define INTERNETSTREAMINGMANAGER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QVector>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <memory>

/**
 * @brief Internet streaming services management system
 * 
 * Provides integration with YouTube, Twitch, internet radio stations,
 * podcast feeds, and stream recording capabilities.
 */
class InternetStreamingManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Streaming service types
     */
    enum ServiceType {
        YouTube = 0,
        Twitch,
        InternetRadio,
        Podcast,
        SHOUTcast,
        Icecast,
        Custom
    };

    /**
     * @brief Stream quality options
     */
    enum StreamQuality {
        Audio_64k = 0,
        Audio_128k,
        Audio_320k,
        Video_360p,
        Video_480p,
        Video_720p,
        Video_1080p,
        Video_4K,
        Best_Available
    };

    /**
     * @brief Internet stream information
     */
    struct InternetStream {
        QString id;                 // Stream ID
        QString title;              // Stream title
        QString description;        // Stream description
        QString channel;            // Channel/station name
        QUrl url;                  // Stream URL
        QUrl thumbnailUrl;         // Thumbnail URL
        ServiceType serviceType;   // Service type
        StreamQuality quality;     // Stream quality
        QString category;          // Category/genre
        QString language;          // Language code
        int viewerCount;           // Current viewers (live streams)
        qint64 duration;          // Duration in milliseconds (-1 for live)
        bool isLive;              // Is live stream
        bool isRecordable;        // Can be recorded
        QDateTime publishDate;     // Publish date
        
        InternetStream() : serviceType(Custom), quality(Best_Available), 
                          viewerCount(0), duration(-1), isLive(false), isRecordable(true) {}
    };

    /**
     * @brief Podcast episode information
     */
    struct PodcastEpisode {
        QString id;                 // Episode ID
        QString title;              // Episode title
        QString description;        // Episode description
        QString podcastName;        // Podcast name
        QUrl audioUrl;             // Audio URL
        QUrl thumbnailUrl;         // Thumbnail URL
        qint64 duration;          // Duration in milliseconds
        QDateTime publishDate;     // Publish date
        QString category;          // Category
        bool isDownloaded;        // Is downloaded locally
        QString localPath;        // Local file path (if downloaded)
        
        PodcastEpisode() : duration(0), isDownloaded(false) {}
    };

    /**
     * @brief Radio station information
     */
    struct RadioStation {
        QString id;                 // Station ID
        QString name;               // Station name
        QString description;        // Station description
        QUrl streamUrl;            // Stream URL
        QString genre;             // Music genre
        QString country;           // Country
        QString language;          // Language
        int bitrate;               // Bitrate in kbps
        QString codec;             // Audio codec
        bool isOnline;            // Station status
        int listenerCount;        // Current listeners
        
        RadioStation() : bitrate(0), isOnline(false), listenerCount(0) {}
    };

    /**
     * @brief Recording options
     */
    struct RecordingOptions {
        QString outputPath;         // Output file path
        QString format;            // Recording format (mp4, mp3, flac)
        StreamQuality quality;     // Recording quality
        qint64 maxDuration;       // Maximum duration in milliseconds (0 = unlimited)
        qint64 maxFileSize;       // Maximum file size in bytes (0 = unlimited)
        bool splitByTime;         // Split recording by time intervals
        int splitInterval;        // Split interval in minutes
        
        RecordingOptions() : quality(Best_Available), maxDuration(0), maxFileSize(0),
                           splitByTime(false), splitInterval(60) {}
    };

    explicit InternetStreamingManager(QObject* parent = nullptr);
    ~InternetStreamingManager() override;

    /**
     * @brief Initialize the internet streaming manager
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Shutdown the manager
     */
    void shutdown();

    // YouTube integration
    /**
     * @brief Search YouTube videos
     * @param query Search query
     * @param maxResults Maximum results to return
     * @return true if search started successfully
     */
    bool searchYouTube(const QString& query, int maxResults = 25);

    /**
     * @brief Get YouTube video stream URL
     * @param videoId YouTube video ID
     * @param quality Preferred quality
     * @return Stream URL
     */
    QUrl getYouTubeStreamUrl(const QString& videoId, StreamQuality quality = Best_Available);

    // Twitch integration
    /**
     * @brief Search Twitch streams
     * @param query Search query
     * @param category Category filter
     * @return true if search started successfully
     */
    bool searchTwitch(const QString& query, const QString& category = QString());

    /**
     * @brief Get Twitch stream URL
     * @param channelName Twitch channel name
     * @param quality Preferred quality
     * @return Stream URL
     */
    QUrl getTwitchStreamUrl(const QString& channelName, StreamQuality quality = Best_Available);

    // Internet radio
    /**
     * @brief Search radio stations
     * @param query Search query
     * @param genre Genre filter
     * @param country Country filter
     * @return true if search started successfully
     */
    bool searchRadioStations(const QString& query, const QString& genre = QString(), const QString& country = QString());

    /**
     * @brief Get popular radio stations
     * @param genre Genre filter
     * @param limit Maximum results
     * @return List of popular stations
     */
    QVector<RadioStation> getPopularRadioStations(const QString& genre = QString(), int limit = 50);

    // Podcast support
    /**
     * @brief Subscribe to podcast feed
     * @param feedUrl RSS feed URL
     * @return true if subscription successful
     */
    bool subscribeToPodcast(const QUrl& feedUrl);

    /**
     * @brief Unsubscribe from podcast
     * @param podcastId Podcast ID
     * @return true if unsubscription successful
     */
    bool unsubscribeFromPodcast(const QString& podcastId);

    /**
     * @brief Get subscribed podcasts
     * @return List of subscribed podcasts
     */
    QStringList getSubscribedPodcasts() const;

    /**
     * @brief Get podcast episodes
     * @param podcastId Podcast ID
     * @param limit Maximum episodes to return
     * @return List of episodes
     */
    QVector<PodcastEpisode> getPodcastEpisodes(const QString& podcastId, int limit = 50);

    /**
     * @brief Download podcast episode
     * @param episode Episode to download
     * @return true if download started successfully
     */
    bool downloadPodcastEpisode(const PodcastEpisode& episode);

    // Stream recording
    /**
     * @brief Start recording current stream
     * @param options Recording options
     * @return true if recording started successfully
     */
    bool startRecording(const RecordingOptions& options);

    /**
     * @brief Stop current recording
     */
    void stopRecording();

    /**
     * @brief Check if currently recording
     * @return true if recording is active
     */
    bool isRecording() const;

    /**
     * @brief Get recording progress
     * @return Progress information
     */
    struct RecordingProgress {
        qint64 duration;           // Recorded duration in milliseconds
        qint64 fileSize;          // Current file size in bytes
        QString currentFile;      // Current recording file
        
        RecordingProgress() : duration(0), fileSize(0) {}
    };

    /**
     * @brief Get current recording progress
     * @return Recording progress
     */
    RecordingProgress getRecordingProgress() const;

signals:
    /**
     * @brief Emitted when YouTube search completes
     * @param results Search results
     */
    void youTubeSearchCompleted(const QVector<InternetStream>& results);

    /**
     * @brief Emitted when Twitch search completes
     * @param results Search results
     */
    void twitchSearchCompleted(const QVector<InternetStream>& results);

    /**
     * @brief Emitted when radio station search completes
     * @param results Search results
     */
    void radioSearchCompleted(const QVector<RadioStation>& results);

    /**
     * @brief Emitted when podcast feed is parsed
     * @param podcastId Podcast ID
     * @param episodes List of episodes
     */
    void podcastFeedParsed(const QString& podcastId, const QVector<PodcastEpisode>& episodes);

    /**
     * @brief Emitted when recording starts
     * @param options Recording options
     */
    void recordingStarted(const RecordingOptions& options);

    /**
     * @brief Emitted when recording stops
     * @param filePath Output file path
     */
    void recordingStopped(const QString& filePath);

    /**
     * @brief Emitted when recording progress updates
     * @param progress Recording progress
     */
    void recordingProgressUpdated(const RecordingProgress& progress);

private:
    // Service integration methods
    QVector<InternetStream> parseYouTubeResponse(const QByteArray& data);
    QVector<InternetStream> parseTwitchResponse(const QByteArray& data);
    QVector<RadioStation> parseRadioResponse(const QByteArray& data);
    QVector<PodcastEpisode> parseRSSFeed(const QByteArray& data);

    // Recording methods
    bool initializeRecording(const RecordingOptions& options);
    void processRecordingData(const QByteArray& data);
    void finalizeRecording();

    // Helper methods
    QString extractYouTubeVideoId(const QUrl& url);
    QUrl buildYouTubeApiUrl(const QString& query, int maxResults);
    QUrl buildTwitchApiUrl(const QString& query, const QString& category);
    QUrl buildRadioApiUrl(const QString& query, const QString& genre, const QString& country);

    // Recording state
    bool m_recording;
    RecordingOptions m_recordingOptions;
    RecordingProgress m_recordingProgress;
    QTimer* m_recordingTimer;

    // Service API keys (would be configured)
    QString m_youtubeApiKey;
    QString m_twitchClientId;

    // Thread safety
    mutable QMutex m_recordingMutex;

    // Constants
    static constexpr int DEFAULT_SEARCH_LIMIT = 25;
    static constexpr int RECORDING_UPDATE_INTERVAL = 1000;
};

#endif // INTERNETSTREAMINGMANAGER_H