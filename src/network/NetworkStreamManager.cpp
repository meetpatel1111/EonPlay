#include "network/NetworkStreamManager.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QDateTime>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(networkStream, "eonplay.network.stream")

NetworkStreamManager::NetworkStreamManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(std::make_unique<QNetworkAccessManager>(this))
    , m_currentReply(nullptr)
    , m_currentState(DISCONNECTED)
    , m_bandwidthTimer(new QTimer(this))
    , m_connectionTimeout(DEFAULT_TIMEOUT_MS)
    , m_bufferSize(DEFAULT_BUFFER_SIZE_KB)
    , m_lastErrorCode(0)
    , m_proxyEnabled(false)
{
    setupNetworkManager();
    
    // Setup bandwidth monitoring timer
    m_bandwidthTimer->setInterval(BANDWIDTH_UPDATE_INTERVAL_MS);
    connect(m_bandwidthTimer, &QTimer::timeout, this, &NetworkStreamManager::onBandwidthTimerTimeout);
    
    // Set default user agent
    m_userAgent = QString("EonPlay/1.0 (Cross-Platform Media Player)");
    
    qCDebug(networkStream) << "NetworkStreamManager initialized";
}

NetworkStreamManager::~NetworkStreamManager()
{
    closeStream();
    unmountNetworkShare();
}

void NetworkStreamManager::setupNetworkManager()
{
    // Configure network manager
    m_networkManager->setTransferTimeout(m_connectionTimeout);
    
    // Connect signals
    connect(m_networkManager.get(), &QNetworkAccessManager::finished,
            this, &NetworkStreamManager::onNetworkReplyFinished);
}

bool NetworkStreamManager::openStream(const QUrl& url)
{
    if (!isValidStreamUrl(url)) {
        m_lastError = "Invalid stream URL";
        m_lastErrorCode = -1;
        emit streamError(m_lastError, m_lastErrorCode);
        return false;
    }
    
    // Close any existing stream
    closeStream();
    
    m_currentState = CONNECTING;
    emit streamStateChanged(m_currentState);
    
    // Detect stream type
    StreamType streamType = detectStreamType(url);
    
    // Setup stream info
    m_currentStreamInfo.url = url;
    m_currentStreamInfo.type = streamType;
    m_currentStreamInfo.protocol = url.scheme().toUpper();
    m_currentStreamInfo.userAgent = m_userAgent;
    m_currentStreamInfo.headers = m_customHeaders;
    
    // Handle different stream types
    switch (streamType) {
        case HTTP_STREAM:
        case HLS_STREAM:
        case DASH_STREAM:
            return openHttpBasedStream(url);
            
        case RTSP_STREAM:
        case RTP_STREAM:
            return openRTSPStream(url);
            
        case SMB_SHARE:
        case NFS_SHARE:
            return openNetworkShare(url);
            
        default:
            m_lastError = "Unsupported stream type";
            m_lastErrorCode = -2;
            m_currentState = ERROR_STATE;
            emit streamStateChanged(m_currentState);
            emit streamError(m_lastError, m_lastErrorCode);
            return false;
    }
}

bool NetworkStreamManager::openHttpBasedStream(const QUrl& url)
{
    QNetworkRequest request(url);
    configureRequest(request);
    
    // Add specific headers for streaming
    request.setRawHeader("Accept", "*/*");
    request.setRawHeader("Connection", "keep-alive");
    
    // For HLS streams, request the playlist
    if (m_currentStreamInfo.type == HLS_STREAM) {
        request.setRawHeader("Accept", "application/vnd.apple.mpegurl, application/x-mpegurl, */*");
    }
    
    // For DASH streams, request the manifest
    if (m_currentStreamInfo.type == DASH_STREAM) {
        request.setRawHeader("Accept", "application/dash+xml, */*");
    }
    
    m_currentReply = m_networkManager->get(request);
    
    connect(m_currentReply, &QNetworkReply::errorOccurred,
            this, &NetworkStreamManager::onNetworkReplyError);
    
    qCDebug(networkStream) << "Opening HTTP-based stream:" << url.toString();
    return true;
}

bool NetworkStreamManager::openRTSPStream(const QUrl& url)
{
    // RTSP streams are typically handled by libVLC directly
    // We validate and prepare the URL for libVLC consumption
    
    if (!url.isValid() || url.scheme().toLower() != "rtsp") {
        m_lastError = "Invalid RTSP URL";
        m_lastErrorCode = -3;
        return false;
    }
    
    m_currentStreamInfo.isLive = true;
    m_currentState = CONNECTED;
    emit streamStateChanged(m_currentState);
    emit streamOpened(m_currentStreamInfo);
    
    qCDebug(networkStream) << "RTSP stream prepared:" << url.toString();
    return true;
}

bool NetworkStreamManager::openNetworkShare(const QUrl& url)
{
    QString username, password;
    
    // Extract credentials from URL if present
    if (!url.userName().isEmpty()) {
        username = url.userName();
        password = url.password();
    }
    
    return mountNetworkShare(url, username, password);
}

bool NetworkStreamManager::openRTSPStream(const QUrl& url)
{
    // RTSP streams are typically handled by libVLC directly
    // We validate and prepare the URL for libVLC consumption
    
    if (!url.isValid() || url.scheme().toLower() != "rtsp") {
        m_lastError = "Invalid RTSP URL";
        m_lastErrorCode = -3;
        return false;
    }
    
    m_currentStreamInfo.isLive = true;
    m_currentState = CONNECTED;
    emit streamStateChanged(m_currentState);
    emit streamOpened(m_currentStreamInfo);
    
    qCDebug(networkStream) << "RTSP stream prepared:" << url.toString();
    return true;
}

bool NetworkStreamManager::openNetworkShare(const QUrl& url)
{
    QString username, password;
    
    // Extract credentials from URL if present
    if (!url.userName().isEmpty()) {
        username = url.userName();
        password = url.password();
    }
    
    return mountNetworkShare(url, username, password);
}

void NetworkStreamManager::closeStream()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    stopBandwidthMonitoring();
    
    if (m_currentState != DISCONNECTED) {
        m_currentState = DISCONNECTED;
        emit streamStateChanged(m_currentState);
        emit streamClosed();
    }
    
    // Reset stream info
    m_currentStreamInfo = StreamInfo();
    resetBandwidthStats();
    
    qCDebug(networkStream) << "Stream closed";
}

NetworkStreamManager::StreamType NetworkStreamManager::detectStreamType(const QUrl& url) const
{
    QString scheme = url.scheme().toLower();
    QString path = url.path().toLower();
    
    // Protocol-based detection
    if (scheme == "rtsp") return RTSP_STREAM;
    if (scheme == "rtp") return RTP_STREAM;
    if (scheme == "smb" || scheme == "cifs") return SMB_SHARE;
    if (scheme == "nfs") return NFS_SHARE;
    
    // HTTP-based stream detection
    if (scheme == "http" || scheme == "https") {
        if (isHLSStream(url)) return HLS_STREAM;
        if (isDASHStream(url)) return DASH_STREAM;
        return HTTP_STREAM;
    }
    
    return UNKNOWN_STREAM;
}

bool NetworkStreamManager::isHLSStream(const QUrl& url) const
{
    QString path = url.path().toLower();
    return path.endsWith(".m3u8") || path.contains("/hls/") || 
           url.toString().contains("m3u8");
}

bool NetworkStreamManager::isDASHStream(const QUrl& url) const
{
    QString path = url.path().toLower();
    return path.endsWith(".mpd") || path.contains("/dash/") ||
           url.toString().contains("manifest.mpd");
}

bool NetworkStreamManager::isRTSPStream(const QUrl& url) const
{
    return url.scheme().toLower() == "rtsp";
}

bool NetworkStreamManager::isValidStreamUrl(const QUrl& url) const
{
    if (!url.isValid()) return false;
    
    QString scheme = url.scheme().toLower();
    QStringList supportedSchemes = {"http", "https", "rtsp", "rtp", "smb", "cifs", "nfs"};
    
    return supportedSchemes.contains(scheme);
}

bool NetworkStreamManager::supportsProtocol(const QString& protocol) const
{
    QStringList supported = {"HTTP", "HTTPS", "HLS", "DASH", "RTSP", "RTP", "SMB", "CIFS", "NFS"};
    return supported.contains(protocol.toUpper());
}

void NetworkStreamManager::configureRequest(QNetworkRequest& request) const
{
    // Set user agent
    request.setRawHeader("User-Agent", m_userAgent.toUtf8());
    
    // Add custom headers
    for (auto it = m_customHeaders.constBegin(); it != m_customHeaders.constEnd(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }
    
    // Set timeout
    request.setTransferTimeout(m_connectionTimeout);
}

void NetworkStreamManager::onNetworkReplyFinished()
{
    if (!m_currentReply) return;
    
    if (m_currentReply->error() == QNetworkReply::NoError) {
        if (validateStreamResponse(m_currentReply)) {
            m_currentState = CONNECTED;
            
            // Extract content information
            m_currentStreamInfo.contentType = m_currentReply->header(QNetworkRequest::ContentTypeHeader).toString();
            m_currentStreamInfo.contentLength = m_currentReply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
            
            emit streamStateChanged(m_currentState);
            emit streamOpened(m_currentStreamInfo);
            
            startBandwidthMonitoring();
            qCDebug(networkStream) << "Stream opened successfully";
        } else {
            m_lastError = "Invalid stream response";
            m_lastErrorCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            m_currentState = ERROR_STATE;
            emit streamStateChanged(m_currentState);
            emit streamError(m_lastError, m_lastErrorCode);
        }
    }
    
    // Don't delete reply here as it might be used for streaming
}

void NetworkStreamManager::onNetworkReplyError(QNetworkReply::NetworkError error)
{
    if (!m_currentReply) return;
    
    m_lastError = m_currentReply->errorString();
    m_lastErrorCode = static_cast<int>(error);
    m_currentState = ERROR_STATE;
    
    emit streamStateChanged(m_currentState);
    emit streamError(m_lastError, m_lastErrorCode);
    
    qCWarning(networkStream) << "Network error:" << m_lastError;
}

bool NetworkStreamManager::validateStreamResponse(QNetworkReply* reply)
{
    if (!reply) return false;
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    // Check for successful HTTP status codes
    if (statusCode < 200 || statusCode >= 300) {
        return false;
    }
    
    // Additional validation based on stream type
    QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
    
    switch (m_currentStreamInfo.type) {
        case HLS_STREAM:
            return contentType.contains("mpegurl") || contentType.contains("m3u8");
            
        case DASH_STREAM:
            return contentType.contains("dash+xml") || contentType.contains("xml");
            
        case HTTP_STREAM:
            return contentType.contains("video/") || contentType.contains("audio/") ||
                   contentType.contains("application/octet-stream");
            
        default:
            return true;
    }
}

// Bandwidth monitoring implementation
void NetworkStreamManager::startBandwidthMonitoring()
{
    if (!m_bandwidthTimer->isActive()) {
        resetBandwidthStats();
        m_bandwidthTimer->start();
        qCDebug(networkStream) << "Bandwidth monitoring started";
    }
}

void NetworkStreamManager::stopBandwidthMonitoring()
{
    if (m_bandwidthTimer->isActive()) {
        m_bandwidthTimer->stop();
        qCDebug(networkStream) << "Bandwidth monitoring stopped";
    }
}

void NetworkStreamManager::onBandwidthTimerTimeout()
{
    updateBandwidthStats();
}

void NetworkStreamManager::updateBandwidthStats()
{
    if (!m_currentReply) return;
    
    QMutexLocker locker(&m_statsMutex);
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 bytesReceived = m_currentReply->bytesAvailable();
    
    // Add to history
    m_bandwidthHistory.append(qMakePair(currentTime, bytesReceived));
    
    // Keep only recent history
    while (m_bandwidthHistory.size() > BANDWIDTH_HISTORY_SIZE) {
        m_bandwidthHistory.removeFirst();
    }
    
    calculateBandwidthStats();
    emit bandwidthStatsUpdated(m_bandwidthStats);
}

void NetworkStreamManager::calculateBandwidthStats()
{
    if (m_bandwidthHistory.size() < 2) return;
    
    // Calculate current download speed
    auto latest = m_bandwidthHistory.last();
    auto previous = m_bandwidthHistory[m_bandwidthHistory.size() - 2];
    
    qint64 timeDiff = latest.first - previous.first;
    qint64 bytesDiff = latest.second - previous.second;
    
    if (timeDiff > 0) {
        m_bandwidthStats.downloadSpeed = (bytesDiff * 1000.0) / (timeDiff * 1024.0); // KB/s
    }
    
    // Calculate average speed over history
    if (m_bandwidthHistory.size() > 1) {
        auto oldest = m_bandwidthHistory.first();
        qint64 totalTime = latest.first - oldest.first;
        qint64 totalBytes = latest.second - oldest.second;
        
        if (totalTime > 0) {
            m_bandwidthStats.averageSpeed = (totalBytes * 1000.0) / (totalTime * 1024.0); // KB/s
        }
    }
    
    m_bandwidthStats.bytesReceived = latest.second;
    m_bandwidthStats.timestamp = latest.first;
    
    // Estimate buffer level (simplified)
    if (m_currentStreamInfo.contentLength > 0) {
        m_bandwidthStats.bufferLevel = (m_bandwidthStats.bytesReceived * 100) / m_currentStreamInfo.contentLength;
    }
}

void NetworkStreamManager::resetBandwidthStats()
{
    QMutexLocker locker(&m_statsMutex);
    m_bandwidthStats = BandwidthStats();
    m_bandwidthHistory.clear();
}

// Proxy configuration
void NetworkStreamManager::setProxy(const QNetworkProxy& proxy)
{
    m_networkManager->setProxy(proxy);
    m_proxyEnabled = true;
    qCDebug(networkStream) << "Proxy configured:" << proxy.hostName() << ":" << proxy.port();
}

void NetworkStreamManager::clearProxy()
{
    m_networkManager->setProxy(QNetworkProxy::NoProxy);
    m_proxyEnabled = false;
    qCDebug(networkStream) << "Proxy cleared";
}

QNetworkProxy NetworkStreamManager::currentProxy() const
{
    return m_networkManager->proxy();
}

// Network share implementation
bool NetworkStreamManager::mountNetworkShare(const QUrl& shareUrl, const QString& username, const QString& password)
{
    if (m_currentStreamInfo.type == SMB_SHARE) {
        return mountSMBShare(shareUrl, username, password);
    } else if (m_currentStreamInfo.type == NFS_SHARE) {
        return mountNFSShare(shareUrl);
    }
    
    m_lastError = "Unsupported network share type";
    return false;
}

bool NetworkStreamManager::mountSMBShare(const QUrl& url, const QString& username, const QString& password)
{
    // Generate local mount point
    m_localMountPath = generateMountPoint();
    
    QDir().mkpath(m_localMountPath);
    
    // Prepare mount command (platform-specific)
#ifdef Q_OS_WIN
    // Windows: use net use command
    QString command = QString("net use \"%1\" \"%2\"").arg(m_localMountPath, url.toString());
    if (!username.isEmpty()) {
        command += QString(" /user:%1").arg(username);
        if (!password.isEmpty()) {
            command += QString(" %1").arg(password);
        }
    }
#else
    // Linux: use mount.cifs
    QString command = QString("mount -t cifs \"%1\" \"%2\"").arg(url.toString(), m_localMountPath);
    if (!username.isEmpty()) {
        command += QString(" -o username=%1").arg(username);
        if (!password.isEmpty()) {
            command += QString(",password=%1").arg(password);
        }
    }
#endif
    
    QProcess mountProcess;
    mountProcess.start(command);
    mountProcess.waitForFinished(10000); // 10 second timeout
    
    if (mountProcess.exitCode() == 0) {
        m_mountedSharePath = url.toString();
        m_currentState = CONNECTED;
        emit streamStateChanged(m_currentState);
        emit networkShareMounted(m_localMountPath);
        qCDebug(networkStream) << "SMB share mounted:" << m_localMountPath;
        return true;
    } else {
        m_lastError = mountProcess.readAllStandardError();
        qCWarning(networkStream) << "Failed to mount SMB share:" << m_lastError;
        return false;
    }
}

bool NetworkStreamManager::mountNFSShare(const QUrl& url)
{
    m_localMountPath = generateMountPoint();
    QDir().mkpath(m_localMountPath);
    
    QString command = QString("mount -t nfs \"%1\" \"%2\"").arg(url.toString(), m_localMountPath);
    
    QProcess mountProcess;
    mountProcess.start(command);
    mountProcess.waitForFinished(10000);
    
    if (mountProcess.exitCode() == 0) {
        m_mountedSharePath = url.toString();
        m_currentState = CONNECTED;
        emit streamStateChanged(m_currentState);
        emit networkShareMounted(m_localMountPath);
        qCDebug(networkStream) << "NFS share mounted:" << m_localMountPath;
        return true;
    } else {
        m_lastError = mountProcess.readAllStandardError();
        qCWarning(networkStream) << "Failed to mount NFS share:" << m_lastError;
        return false;
    }
}

void NetworkStreamManager::unmountNetworkShare()
{
    if (m_localMountPath.isEmpty()) return;
    
#ifdef Q_OS_WIN
    QString command = QString("net use \"%1\" /delete").arg(m_localMountPath);
#else
    QString command = QString("umount \"%1\"").arg(m_localMountPath);
#endif
    
    QProcess unmountProcess;
    unmountProcess.start(command);
    unmountProcess.waitForFinished(5000);
    
    // Clean up directory
    QDir(m_localMountPath).removeRecursively();
    
    m_localMountPath.clear();
    m_mountedSharePath.clear();
    
    emit networkShareUnmounted();
    qCDebug(networkStream) << "Network share unmounted";
}

QString NetworkStreamManager::generateMountPoint() const
{
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString mountPoint = QString("%1/eonplay_mount_%2").arg(tempDir).arg(QDateTime::currentMSecsSinceEpoch());
    return mountPoint;
}

// Getters and utility methods
NetworkStreamManager::BandwidthStats NetworkStreamManager::getBandwidthStats() const
{
    QMutexLocker locker(&m_statsMutex);
    return m_bandwidthStats;
}

bool NetworkStreamManager::isStreamActive() const
{
    return m_currentState == CONNECTED || m_currentState == BUFFERING;
}

NetworkStreamManager::StreamState NetworkStreamManager::currentState() const
{
    return m_currentState;
}

NetworkStreamManager::StreamInfo NetworkStreamManager::currentStreamInfo() const
{
    return m_currentStreamInfo;
}

bool NetworkStreamManager::isBandwidthMonitoringActive() const
{
    return m_bandwidthTimer->isActive();
}

bool NetworkStreamManager::isProxyEnabled() const
{
    return m_proxyEnabled;
}

bool NetworkStreamManager::isNetworkShareMounted() const
{
    return !m_localMountPath.isEmpty();
}

QString NetworkStreamManager::getLocalMountPath() const
{
    return m_localMountPath;
}

void NetworkStreamManager::setUserAgent(const QString& userAgent)
{
    m_userAgent = userAgent;
}

void NetworkStreamManager::setCustomHeaders(const QMap<QString, QString>& headers)
{
    m_customHeaders = headers;
}

void NetworkStreamManager::setConnectionTimeout(int timeoutMs)
{
    m_connectionTimeout = timeoutMs;
    m_networkManager->setTransferTimeout(timeoutMs);
}

void NetworkStreamManager::setBufferSize(int bufferSizeKB)
{
    m_bufferSize = bufferSizeKB;
}

QString NetworkStreamManager::lastError() const
{
    return m_lastError;
}

int NetworkStreamManager::lastErrorCode() const
{
    return m_lastErrorCode;
}