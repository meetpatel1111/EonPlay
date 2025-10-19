#include "network/InternetStreamingManager.h"
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

Q_DECLARE_LOGGING_CATEGORY(internetStreaming)
Q_LOGGING_CATEGORY(internetStreaming, "network.internet")

InternetStreamingManager::InternetStreamingManager(QObject* parent)
    : QObject(parent)
    , m_recording(false)
    , m_recordingTimer(new QTimer(this))
{
    // Setup recording timer
    m_recordingTimer->setInterval(RECORDING_UPDATE_INTERVAL);
    connect(m_recordingTimer, &QTimer::timeout, this, [this]() {
        // Update recording progress
        emit recordingProgressUpdated(m_recordingProgress);
    });
    
    qCDebug(internetStreaming) << "InternetStreamingManager created";
}

InternetStreamingManager::~InternetStreamingManager()
{
    shutdown();
    qCDebug(internetStreaming) << "InternetStreamingManager destroyed";
}

bool InternetStreamingManager::initialize()
{
    qCDebug(internetStreaming) << "InternetStreamingManager initialized";
    return true;
}

void InternetStreamingManager::shutdown()
{
    stopRecording();
    qCDebug(internetStreaming) << "InternetStreamingManager shutdown";
}

bool InternetStreamingManager::searchYouTube(const QString& query, int maxResults)
{
    if (m_youtubeApiKey.isEmpty()) {
        qCWarning(internetStreaming) << "YouTube API key not configured";
        return false;
    }
    
    QUrl apiUrl = buildYouTubeApiUrl(query, maxResults);
    QNetworkRequest request(apiUrl);
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QVector<InternetStream> results = parseYouTubeResponse(reply->readAll());
            emit youTubeSearchCompleted(results);
        } else {
            qCWarning(internetStreaming) << "YouTube search error:" << reply->errorString();
            emit youTubeSearchCompleted(QVector<InternetStream>());
        }
        reply->deleteLater();
    });
    
    qCDebug(internetStreaming) << "YouTube search started:" << query;
    return true;
}

QUrl InternetStreamingManager::getYouTubeStreamUrl(const QString& videoId, StreamQuality quality)
{
    Q_UNUSED(quality)
    
    // This would use youtube-dl or similar library to extract stream URLs
    // For now, return a placeholder URL
    QString streamUrl = QString("https://www.youtube.com/watch?v=%1").arg(videoId);
    
    qCDebug(internetStreaming) << "YouTube stream URL requested for:" << videoId;
    return QUrl(streamUrl);
}

bool InternetStreamingManager::searchTwitch(const QString& query, const QString& category)
{
    if (m_twitchClientId.isEmpty()) {
        qCWarning(internetStreaming) << "Twitch client ID not configured";
        return false;
    }
    
    QUrl apiUrl = buildTwitchApiUrl(query, category);
    QNetworkRequest request(apiUrl);
    request.setRawHeader("Client-ID", m_twitchClientId.toUtf8());
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QVector<InternetStream> results = parseTwitchResponse(reply->readAll());
            emit twitchSearchCompleted(results);
        } else {
            qCWarning(internetStreaming) << "Twitch search error:" << reply->errorString();
            emit twitchSearchCompleted(QVector<InternetStream>());
        }
        reply->deleteLater();
    });
    
    qCDebug(internetStreaming) << "Twitch search started:" << query;
    return true;
}

QUrl InternetStreamingManager::getTwitchStreamUrl(const QString& channelName, StreamQuality quality)
{
    Q_UNUSED(quality)
    
    // This would use Twitch API to get actual stream URLs
    QString streamUrl = QString("https://www.twitch.tv/%1").arg(channelName);
    
    qCDebug(internetStreaming) << "Twitch stream URL requested for:" << channelName;
    return QUrl(streamUrl);
}

bool InternetStreamingManager::searchRadioStations(const QString& query, const QString& genre, const QString& country)
{
    QUrl apiUrl = buildRadioApiUrl(query, genre, country);
    QNetworkRequest request(apiUrl);
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QVector<RadioStation> results = parseRadioResponse(reply->readAll());
            emit radioSearchCompleted(results);
        } else {
            qCWarning(internetStreaming) << "Radio search error:" << reply->errorString();
            emit radioSearchCompleted(QVector<RadioStation>());
        }
        reply->deleteLater();
    });
    
    qCDebug(internetStreaming) << "Radio search started:" << query;
    return true;
}

bool InternetStreamingManager::startRecording(const RecordingOptions& options)
{
    if (m_recording) {
        qCWarning(internetStreaming) << "Recording already in progress";
        return false;
    }
    
    QMutexLocker locker(&m_recordingMutex);
    
    if (!initializeRecording(options)) {
        qCWarning(internetStreaming) << "Failed to initialize recording";
        return false;
    }
    
    m_recording = true;
    m_recordingOptions = options;
    m_recordingProgress = RecordingProgress();
    m_recordingProgress.currentFile = options.outputPath;
    
    m_recordingTimer->start();
    emit recordingStarted(m_recordingOptions);
    
    qCDebug(internetStreaming) << "Recording started:" << options.outputPath;
    return true;
}

void InternetStreamingManager::stopRecording()
{
    if (!m_recording) {
        return;
    }
    
    QMutexLocker locker(&m_recordingMutex);
    
    m_recording = false;
    m_recordingTimer->stop();
    
    finalizeRecording();
    
    emit recordingStopped(m_recordingProgress.currentFile);
    qCDebug(internetStreaming) << "Recording stopped";
}

bool InternetStreamingManager::isRecording() const
{
    QMutexLocker locker(&m_recordingMutex);
    return m_recording;
}

InternetStreamingManager::RecordingProgress InternetStreamingManager::getRecordingProgress() const
{
    QMutexLocker locker(&m_recordingMutex);
    return m_recordingProgress;
}