#include "network/InternetStreamingService.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QTimer>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(internetStreaming, "eonplay.network.internetstreaming")

// API Base URLs
const QString InternetStreamingService::YOUTUBE_API_BASE_URL = "https://www.googleapis.com/youtube/v3";
const QString InternetStreamingService::TWITCH_API_BASE_URL = "https://api.twitch.tv/helix";
const QString InternetStreamingService::SHOUTCAST_API_BASE_URL = "https://directory.shoutcast.com";
const QString InternetStreamingService::ICECAST_API_BASE_URL = "https://dir.xiph.org";

InternetStreamingService::InternetStreamingService(QObject *parent)
    : QObject(parent)
    , m_networkManager(std::make_unique<QNetworkAccessManager>(this))
    , m_maxConcurrentDownloads(DEFAULT_MAX_DOWNLOADS)
    , m_isRecording(false)
    , m_recordingTimer(new QTimer(this))
    , m_recordingStartTime(0)
    , m_recordingReply(nullptr)
{
    setupNetworkManager();
    
    // Setup recording timer
    m_recordingTimer->setInterval(RECORDING_UPDATE_INTERVAL_MS);
    connect(m_recordingTimer, &QTimer::timeout, this, &InternetStreamingService::onRecordingTimerTimeout);
    
    // Set default user agent
    m_userAgent = "EonPlay/1.0 (Cross-Platform Media Player)";
    
    qCDebug(internetStreaming) << "InternetStreamingService initialized";
}

InternetStreamingService::~InternetStreamingService()
{
    stopRecording();
    
    // Cancel all active downloads
    for (auto reply : m_activeDownloads.values()) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
    }
    m_activeDownloads.clear();
}

void InternetStreamingService::setupNetworkManager()
{
    connect(m_networkManager.get(), &QNetworkAccessManager::finished,
            this, &InternetStreamingService::onNetworkReplyFinished);
}

// YouTube Integration
bool InternetStreamingService::searchYouTube(const QString& query, int maxResults)
{
    if (m_youtubeApiKey.isEmpty()) {
        emit serviceError("YouTube API key not configured", YOUTUBE_SERVICE);
        return false;
    }
    
    QUrl url(buildYouTubeSearchUrl(query, maxResults));
    makeApiRequest(url, "youtube_search");
    
    qCDebug(internetStreaming) << "YouTube search initiated:" << query;
    return true;
}

QString InternetStreamingService::buildYouTubeSearchUrl(const QString& query, int maxResults) const
{
    QUrl url(YOUTUBE_API_BASE_URL + "/search");
    QUrlQuery urlQuery;
    
    urlQuery.addQueryItem("part", "snippet");
    urlQuery.addQueryItem("q", query);
    urlQuery.addQueryItem("type", "video");
    urlQuery.addQueryItem("maxResults", QString::number(maxResults));
    urlQuery.addQueryItem("key", m_youtubeApiKey);
    
    url.setQuery(urlQuery);
    return url.toString();
}

bool InternetStreamingService::getYouTubeVideoInfo(const QString& videoId)
{
    if (m_youtubeApiKey.isEmpty()) {
        emit serviceError("YouTube API key not configured", YOUTUBE_SERVICE);
        return false;
    }
    
    QUrl url(buildYouTubeVideoUrl(videoId));
    makeApiRequest(url, "youtube_video_info");
    
    qCDebug(internetStreaming) << "YouTube video info requested:" << videoId;
    return true;
}

QString InternetStreamingService::buildYouTubeVideoUrl(const QString& videoId) const
{
    QUrl url(YOUTUBE_API_BASE_URL + "/videos");
    QUrlQuery urlQuery;
    
    urlQuery.addQueryItem("part", "snippet,contentDetails,statistics");
    urlQuery.addQueryItem("id", videoId);
    urlQuery.addQueryItem("key", m_youtubeApiKey);
    
    url.setQuery(urlQuery);
    return url.toString();
}

QString InternetStreamingService::extractYouTubeVideoId(const QString& url) const
{
    QRegularExpression regex(R"((?:youtube\.com\/watch\?v=|youtu\.be\/|youtube\.com\/embed\/)([a-zA-Z0-9_-]{11}))");
    QRegularExpressionMatch match = regex.match(url);
    
    if (match.hasMatch()) {
        return match.captured(1);
    }
    
    return QString();
}

void InternetStreamingService::processYouTubeSearchResponse(const QJsonObject& response)
{
    m_youtubeResults.clear();
    
    QJsonArray items = response["items"].toArray();
    for (const auto& item : items) {
        QJsonObject itemObj = item.toObject();
        QJsonObject snippet = itemObj["snippet"].toObject();
        
        StreamInfo info;
        info.id = itemObj["id"].toObject()["videoId"].toString();
        info.title = snippet["title"].toString();
        info.description = snippet["description"].toString();
        info.thumbnailUrl = snippet["thumbnails"].toObject()["medium"].toObject()["url"].toString();
        info.serviceType = YOUTUBE_SERVICE;
        info.uploader = snippet["channelTitle"].toString();
        info.publishDate = QDateTime::fromString(snippet["publishedAt"].toString(), Qt::ISODate);
        
        // Generate stream URL (this would typically require additional processing)
        info.streamUrl = QString("https://www.youtube.com/watch?v=%1").arg(info.id);
        
        m_youtubeResults.append(info);
    }
    
    emit youtubeSearchCompleted(m_youtubeResults);
    qCDebug(internetStreaming) << "YouTube search completed:" << m_youtubeResults.size() << "results";
}

void InternetStreamingService::processYouTubeVideoResponse(const QJsonObject& response)
{
    QJsonArray items = response["items"].toArray();
    if (items.isEmpty()) return;
    
    QJsonObject item = items[0].toObject();
    QJsonObject snippet = item["snippet"].toObject();
    QJsonObject contentDetails = item["contentDetails"].toObject();
    QJsonObject statistics = item["statistics"].toObject();
    
    StreamInfo info;
    info.id = item["id"].toString();
    info.title = snippet["title"].toString();
    info.description = snippet["description"].toString();
    info.thumbnailUrl = snippet["thumbnails"].toObject()["high"].toObject()["url"].toString();
    info.serviceType = YOUTUBE_SERVICE;
    info.uploader = snippet["channelTitle"].toString();
    info.publishDate = QDateTime::fromString(snippet["publishedAt"].toString(), Qt::ISODate);
    info.viewCount = statistics["viewCount"].toString().toInt();
    
    // Parse duration (ISO 8601 format)
    QString duration = contentDetails["duration"].toString();
    // Simplified duration parsing - would need proper ISO 8601 parser
    info.duration = 0; // TODO: Implement proper duration parsing
    
    info.streamUrl = QString("https://www.youtube.com/watch?v=%1").arg(info.id);
    
    emit youtubeVideoInfoReceived(info);
    qCDebug(internetStreaming) << "YouTube video info received:" << info.title;
}

// Twitch Integration
bool InternetStreamingService::searchTwitchStreams(const QString& query, int maxResults)
{
    if (m_twitchClientId.isEmpty()) {
        emit serviceError("Twitch Client ID not configured", TWITCH_SERVICE);
        return false;
    }
    
    QUrl url(buildTwitchSearchUrl(query, maxResults));
    makeApiRequest(url, "twitch_search");
    
    qCDebug(internetStreaming) << "Twitch search initiated:" << query;
    return true;
}

QString InternetStreamingService::buildTwitchSearchUrl(const QString& query, int maxResults) const
{
    QUrl url(TWITCH_API_BASE_URL + "/search/channels");
    QUrlQuery urlQuery;
    
    urlQuery.addQueryItem("query", query);
    urlQuery.addQueryItem("first", QString::number(maxResults));
    
    url.setQuery(urlQuery);
    return url.toString();
}

void InternetStreamingService::processTwitchSearchResponse(const QJsonObject& response)
{
    m_twitchResults.clear();
    
    QJsonArray data = response["data"].toArray();
    for (const auto& item : data) {
        QJsonObject itemObj = item.toObject();
        
        StreamInfo info;
        info.id = itemObj["id"].toString();
        info.title = itemObj["display_name"].toString();
        info.description = itemObj["description"].toString();
        info.thumbnailUrl = itemObj["thumbnail_url"].toString();
        info.serviceType = TWITCH_SERVICE;
        info.uploader = itemObj["display_name"].toString();
        info.isLive = itemObj["is_live"].toBool();
        
        // Generate stream URL
        info.streamUrl = QString("https://www.twitch.tv/%1").arg(itemObj["broadcaster_login"].toString());
        
        m_twitchResults.append(info);
    }
    
    emit twitchSearchCompleted(m_twitchResults);
    qCDebug(internetStreaming) << "Twitch search completed:" << m_twitchResults.size() << "results";
}

// Internet Radio Integration
bool InternetStreamingService::searchRadioStations(const QString& genre, const QString& query)
{
    // Search SHOUTcast directory
    QUrl url(buildRadioSearchUrl(genre, query));
    makeApiRequest(url, "radio_search");
    
    qCDebug(internetStreaming) << "Radio station search initiated - Genre:" << genre << "Query:" << query;
    return true;
}

QString InternetStreamingService::buildRadioSearchUrl(const QString& genre, const QString& query) const
{
    // Simplified SHOUTcast directory search
    QUrl url(SHOUTCAST_API_BASE_URL + "/legacy/stationsearch");
    QUrlQuery urlQuery;
    
    if (!genre.isEmpty()) {
        urlQuery.addQueryItem("genre", genre);
    }
    if (!query.isEmpty()) {
        urlQuery.addQueryItem("search", query);
    }
    urlQuery.addQueryItem("limit", "50");
    
    url.setQuery(urlQuery);
    return url.toString();
}

void InternetStreamingService::processRadioStationsResponse(const QString& response)
{
    // Parse SHOUTcast XML response
    m_radioStations = parseSHOUTcastDirectory(response);
    emit radioStationsLoaded(m_radioStations);
    
    qCDebug(internetStreaming) << "Radio stations loaded:" << m_radioStations.size();
}

QList<InternetStreamingService::RadioStation> InternetStreamingService::parseSHOUTcastDirectory(const QString& xml) const
{
    QList<RadioStation> stations;
    QXmlStreamReader reader(xml);
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement() && reader.name() == QStringView(u"station")) {
            RadioStation station;
            
            QXmlStreamAttributes attrs = reader.attributes();
            station.name = attrs.value("name").toString();
            station.url = attrs.value("mt").toString(); // Mount point
            station.genre = attrs.value("genre").toString();
            station.bitrate = attrs.value("br").toInt();
            station.format = attrs.value("mime").toString();
            station.listeners = attrs.value("lc").toInt();
            
            if (!station.name.isEmpty() && !station.url.isEmpty()) {
                stations.append(station);
            }
        }
    }
    
    return stations;
}

QStringList InternetStreamingService::getRadioGenres() const
{
    // Common radio genres
    return {
        "Rock", "Pop", "Jazz", "Classical", "Electronic", "Hip-Hop",
        "Country", "Blues", "Reggae", "Folk", "Alternative", "Indie",
        "Metal", "Punk", "R&B", "Soul", "Funk", "Dance", "Ambient",
        "News", "Talk", "Sports", "Comedy"
    };
}

// Podcast Integration
bool InternetStreamingService::loadPodcastFeed(const QString& feedUrl)
{
    QUrl url(feedUrl);
    makeApiRequest(url, "podcast_feed");
    
    qCDebug(internetStreaming) << "Podcast feed loading:" << feedUrl;
    return true;
}

void InternetStreamingService::processPodcastFeedResponse(const QString& response)
{
    m_currentPodcastFeed = parseRSSFeed(response);
    emit podcastFeedLoaded(m_currentPodcastFeed);
    
    qCDebug(internetStreaming) << "Podcast feed loaded:" << m_currentPodcastFeed.title;
}

InternetStreamingService::PodcastFeed InternetStreamingService::parseRSSFeed(const QString& xml) const
{
    PodcastFeed feed;
    QXmlStreamReader reader(xml);
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            if (reader.name() == QStringView(u"title") && feed.title.isEmpty()) {
                feed.title = reader.readElementText();
            } else if (reader.name() == QStringView(u"description") && feed.description.isEmpty()) {
                feed.description = reader.readElementText();
            } else if (reader.name() == QStringView(u"link") && feed.website.isEmpty()) {
                feed.website = reader.readElementText();
            } else if (reader.name() == QStringView(u"item")) {
                // Parse episode
                PodcastEpisode episode;
                
                while (!(reader.isEndElement() && reader.name() == QStringView(u"item"))) {
                    reader.readNext();
                    
                    if (reader.isStartElement()) {
                        if (reader.name() == QStringView(u"title")) {
                            episode.title = reader.readElementText();
                        } else if (reader.name() == QStringView(u"description")) {
                            episode.description = reader.readElementText();
                        } else if (reader.name() == QStringView(u"enclosure")) {
                            QXmlStreamAttributes attrs = reader.attributes();
                            episode.audioUrl = attrs.value("url").toString();
                            episode.fileSize = attrs.value("length").toLongLong();
                            episode.mimeType = attrs.value("type").toString();
                        } else if (reader.name() == QStringView(u"pubDate")) {
                            episode.publishDate = QDateTime::fromString(reader.readElementText(), Qt::RFC2822Date);
                        } else if (reader.name() == QStringView(u"guid")) {
                            episode.guid = reader.readElementText();
                        }
                    }
                }
                
                if (!episode.title.isEmpty() && !episode.audioUrl.isEmpty()) {
                    feed.episodes.append(episode);
                }
            }
        }
    }
    
    feed.lastUpdated = QDateTime::currentDateTime();
    return feed;
}

// Stream Recording
bool InternetStreamingService::startRecording(const QString& streamUrl, const QString& outputPath)
{
    if (m_isRecording) {
        stopRecording();
    }
    
    setupRecording(streamUrl, outputPath);
    
    QNetworkRequest request(streamUrl);
    request.setRawHeader("User-Agent", m_userAgent.toUtf8());
    
    m_recordingReply = m_networkManager->get(request);
    connect(m_recordingReply, &QNetworkReply::readyRead, [this]() {
        if (m_recordingReply && !m_recordingOutputPath.isEmpty()) {
            QFile file(m_recordingOutputPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
                file.write(m_recordingReply->readAll());
                file.close();
            }
        }
    });
    
    m_isRecording = true;
    m_recordingStartTime = QDateTime::currentMSecsSinceEpoch();
    m_recordingTimer->start();
    
    emit recordingStarted(outputPath);
    qCDebug(internetStreaming) << "Recording started:" << streamUrl << "to" << outputPath;
    
    return true;
}

void InternetStreamingService::stopRecording()
{
    if (!m_isRecording) return;
    
    m_recordingTimer->stop();
    
    if (m_recordingReply) {
        m_recordingReply->abort();
        m_recordingReply->deleteLater();
        m_recordingReply = nullptr;
    }
    
    qint64 duration = getRecordingDuration();
    qint64 fileSize = getRecordingFileSize();
    
    m_isRecording = false;
    m_recordingStartTime = 0;
    
    emit recordingStopped(duration, fileSize);
    qCDebug(internetStreaming) << "Recording stopped - Duration:" << duration << "ms, Size:" << fileSize << "bytes";
}

void InternetStreamingService::setupRecording(const QString& streamUrl, const QString& outputPath)
{
    m_recordingOutputPath = outputPath;
    
    // Ensure output directory exists
    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    
    // Remove existing file if it exists
    if (QFile::exists(outputPath)) {
        QFile::remove(outputPath);
    }
}

void InternetStreamingService::onRecordingTimerTimeout()
{
    updateRecordingStats();
}

void InternetStreamingService::updateRecordingStats()
{
    if (!m_isRecording) return;
    
    qint64 duration = getRecordingDuration();
    qint64 fileSize = getRecordingFileSize();
    
    emit recordingProgress(duration, fileSize);
}

qint64 InternetStreamingService::getRecordingDuration() const
{
    if (!m_isRecording || m_recordingStartTime == 0) return 0;
    return QDateTime::currentMSecsSinceEpoch() - m_recordingStartTime;
}

qint64 InternetStreamingService::getRecordingFileSize() const
{
    if (m_recordingOutputPath.isEmpty()) return 0;
    
    QFileInfo fileInfo(m_recordingOutputPath);
    return fileInfo.exists() ? fileInfo.size() : 0;
}

// Podcast Download
bool InternetStreamingService::downloadPodcastEpisode(const PodcastEpisode& episode, const QString& downloadPath)
{
    if (m_activeDownloads.size() >= m_maxConcurrentDownloads) {
        emit serviceError("Maximum concurrent downloads reached", PODCAST_SERVICE);
        return false;
    }
    
    QNetworkRequest request(episode.audioUrl);
    request.setRawHeader("User-Agent", m_userAgent.toUtf8());
    
    QNetworkReply* reply = m_networkManager->get(request);
    m_activeDownloads[episode.guid] = reply;
    
    connect(reply, &QNetworkReply::downloadProgress,
            this, &InternetStreamingService::onDownloadProgress);
    
    qCDebug(internetStreaming) << "Podcast download started:" << episode.title;
    return true;
}

void InternetStreamingService::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    // Find episode ID for this reply
    QString episodeId;
    for (auto it = m_activeDownloads.constBegin(); it != m_activeDownloads.constEnd(); ++it) {
        if (it.value() == reply) {
            episodeId = it.key();
            break;
        }
    }
    
    if (!episodeId.isEmpty() && bytesTotal > 0) {
        int percentage = (bytesReceived * 100) / bytesTotal;
        emit podcastDownloadProgress(episodeId, percentage);
    }
}

// Network request handling
void InternetStreamingService::makeApiRequest(const QUrl& url, const QString& requestId)
{
    QNetworkRequest request = createApiRequest(url);
    QNetworkReply* reply = m_networkManager->get(request);
    
    if (!requestId.isEmpty()) {
        m_pendingRequests[reply] = requestId;
    }
}

QNetworkRequest InternetStreamingService::createApiRequest(const QUrl& url) const
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", m_userAgent.toUtf8());
    
    // Add API-specific headers
    if (url.toString().contains("twitch.tv") || url.toString().contains("api.twitch.tv")) {
        request.setRawHeader("Client-ID", m_twitchClientId.toUtf8());
    }
    
    return request;
}

void InternetStreamingService::onNetworkReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString requestId = m_pendingRequests.value(reply);
    m_pendingRequests.remove(reply);
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        
        if (requestId == "youtube_search" || requestId == "youtube_video_info") {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject response = doc.object();
            
            if (requestId == "youtube_search") {
                processYouTubeSearchResponse(response);
            } else {
                processYouTubeVideoResponse(response);
            }
        } else if (requestId == "twitch_search") {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            processTwitchSearchResponse(doc.object());
        } else if (requestId == "radio_search") {
            processRadioStationsResponse(QString::fromUtf8(data));
        } else if (requestId == "podcast_feed") {
            processPodcastFeedResponse(QString::fromUtf8(data));
        }
    } else {
        ServiceType serviceType = UNKNOWN_SERVICE;
        if (requestId.startsWith("youtube")) serviceType = YOUTUBE_SERVICE;
        else if (requestId.startsWith("twitch")) serviceType = TWITCH_SERVICE;
        else if (requestId.startsWith("radio")) serviceType = INTERNET_RADIO;
        else if (requestId.startsWith("podcast")) serviceType = PODCAST_SERVICE;
        
        emit serviceError(reply->errorString(), serviceType);
        qCWarning(internetStreaming) << "Network error:" << reply->errorString();
    }
    
    reply->deleteLater();
}

void InternetStreamingService::onNetworkReplyError(QNetworkReply::NetworkError error)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    qCWarning(internetStreaming) << "Network reply error:" << error << reply->errorString();
}

// Service type detection
InternetStreamingService::ServiceType InternetStreamingService::detectServiceType(const QUrl& url) const
{
    QString host = url.host().toLower();
    
    if (host.contains("youtube.com") || host.contains("youtu.be")) {
        return YOUTUBE_SERVICE;
    } else if (host.contains("twitch.tv")) {
        return TWITCH_SERVICE;
    } else if (url.path().toLower().endsWith(".pls") || 
               url.path().toLower().endsWith(".m3u") ||
               host.contains("shoutcast") || 
               host.contains("icecast")) {
        return INTERNET_RADIO;
    } else if (url.path().toLower().endsWith(".xml") || 
               url.path().toLower().contains("rss") ||
               url.path().toLower().contains("feed")) {
        return PODCAST_SERVICE;
    }
    
    return UNKNOWN_SERVICE;
}

bool InternetStreamingService::isValidStreamUrl(const QUrl& url) const
{
    return url.isValid() && (url.scheme() == "http" || url.scheme() == "https");
}

// Getters
QList<InternetStreamingService::StreamInfo> InternetStreamingService::getYouTubeSearchResults() const
{
    return m_youtubeResults;
}

QList<InternetStreamingService::StreamInfo> InternetStreamingService::getTwitchSearchResults() const
{
    return m_twitchResults;
}

QList<InternetStreamingService::RadioStation> InternetStreamingService::getRadioStations() const
{
    return m_radioStations;
}

InternetStreamingService::PodcastFeed InternetStreamingService::getCurrentPodcastFeed() const
{
    return m_currentPodcastFeed;
}

bool InternetStreamingService::isRecording() const
{
    return m_isRecording;
}

// Configuration setters
void InternetStreamingService::setYouTubeApiKey(const QString& apiKey)
{
    m_youtubeApiKey = apiKey;
}

void InternetStreamingService::setTwitchClientId(const QString& clientId)
{
    m_twitchClientId = clientId;
}

void InternetStreamingService::setUserAgent(const QString& userAgent)
{
    m_userAgent = userAgent;
}

void InternetStreamingService::setMaxConcurrentDownloads(int maxDownloads)
{
    m_maxConcurrentDownloads = maxDownloads;
}