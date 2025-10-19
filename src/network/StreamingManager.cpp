#include "network/StreamingManager.h"
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QHostInfo>
#include <QElapsedTimer>
#include <QtMath>

Q_DECLARE_LOGGING_CATEGORY(streamingManager)
Q_LOGGING_CATEGORY(streamingManager, "network.streaming")

StreamingManager::StreamingManager(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_udpSocket(new QUdpSocket(this))
    , m_tcpSocket(new QTcpSocket(this))
    , m_sslSocket(new QSslSocket(this))
    , m_streaming(false)
    , m_bandwidthMonitoring(false)
    , m_statsTimer(new QTimer(this))
    , m_lastBytesReceived(0)
    , m_lastBytesSent(0)
    , m_lastStatsUpdate(0)
{
    // Setup network manager
    m_networkManager->setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &StreamingManager::onNetworkReplyFinished);
    
    // Setup stats timer
    m_statsTimer->setInterval(DEFAULT_STATS_INTERVAL);
    connect(m_statsTimer, &QTimer::timeout, this, &StreamingManager::updateNetworkStats);
    
    // Setup socket error handling
    connect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &StreamingManager::onSocketError);
    connect(m_sslSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &StreamingManager::onSocketError);
    
    qCDebug(streamingManager) << "StreamingManager created";
}

StreamingManager::~StreamingManager()
{
    shutdown();
    qCDebug(streamingManager) << "StreamingManager destroyed";
}

bool StreamingManager::initialize()
{
    // Initialize network components
    m_networkStats = NetworkStats();
    m_networkStats.connectionType = "Unknown";
    
    qCDebug(streamingManager) << "StreamingManager initialized";
    return true;
}

void StreamingManager::shutdown()
{
    stopStream();
    stopBandwidthMonitoring();
    
    // Disconnect from all shares
    for (const auto& share : m_networkShares) {
        if (share.isConnected) {
            disconnectFromShare(share.id);
        }
    }
    
    qCDebug(streamingManager) << "StreamingManager shutdown";
}

bool StreamingManager::startStream(const QUrl& url, StreamQuality preferredQuality)
{
    if (m_streaming) {
        qCWarning(streamingManager) << "Already streaming";
        return false;
    }
    
    if (!validateStreamUrl(url)) {
        qCWarning(streamingManager) << "Invalid stream URL:" << url;
        return false;
    }
    
    QMutexLocker locker(&m_streamMutex);
    
    StreamProtocol protocol = detectProtocol(url);
    bool success = false;
    
    switch (protocol) {
        case HTTP:
        case HTTPS:
            success = processHTTPStream(url);
            break;
        case HLS:
            success = processHLSStream(url);
            break;
        case DASH:
            success = processDASHStream(url);
            break;
        case RTSP:
        case RTP:
            success = processRTSPStream(url);
            break;
        default:
            qCWarning(streamingManager) << "Unsupported protocol:" << protocol;
            break;
    }
    
    if (success) {
        m_streaming = true;
        m_currentStream.url = url;
        m_currentStream.protocol = protocol;
        m_currentStream.quality = preferredQuality;
        m_currentStream.id = generateStreamId();
        
        // Start bandwidth monitoring if not already active
        if (!m_bandwidthMonitoring) {
            startBandwidthMonitoring();
        }
        
        emit streamStarted(m_currentStream);
        qCDebug(streamingManager) << "Stream started:" << url;
    }
    
    return success;
}