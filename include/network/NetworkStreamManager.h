#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QMutex>
#include <memory>

class QNetworkProxy;

/**
 * @brief Manages network streaming protocols and bandwidth monitoring
 * 
 * Supports HTTP/HLS/DASH streaming, RTSP/RTP live streams, SMB/NFS network shares,
 * bandwidth monitoring, and proxy configuration for EonPlay media player.
 */
class NetworkStreamManager : public QObject
{
    Q_OBJECT

public:
    enum StreamType {
        HTTP_STREAM,
        HLS_STREAM,
        DASH_STREAM,
        RTSP_STREAM,
        RTP_STREAM,
        SMB_SHARE,
        NFS_SHARE,
        UNKNOWN_STREAM
    };

    enum StreamState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        BUFFERING,
        ERROR_STATE
    };

    struct BandwidthStats {
        qint64 bytesReceived = 0;
        qint64 totalBytes = 0;
        double downloadSpeed = 0.0; // KB/s
        double averageSpeed = 0.0;   // KB/s
        int bufferLevel = 0;         // percentage
        qint64 timestamp = 0;
    };

    struct StreamInfo {
        QUrl url;
        StreamType type;
        QString protocol;
        QString contentType;
        qint64 contentLength = -1;
        bool isLive = false;
        QString userAgent;
        QMap<QString, QString> headers;
    };

    explicit NetworkStreamManager(QObject *parent = nullptr);
    ~NetworkStreamManager();

    // Stream management
    bool openStream(const QUrl& url);
    void closeStream();
    bool isStreamActive() const;
    StreamState currentState() const;
    StreamInfo currentStreamInfo() const;

    // Protocol detection and validation
    StreamType detectStreamType(const QUrl& url) const;
    bool isValidStreamUrl(const QUrl& url) const;
    bool supportsProtocol(const QString& protocol) const;

    // Bandwidth monitoring
    BandwidthStats getBandwidthStats() const;
    void startBandwidthMonitoring();
    void stopBandwidthMonitoring();
    bool isBandwidthMonitoringActive() const;

    // Proxy configuration
    void setProxy(const QNetworkProxy& proxy);
    void clearProxy();
    QNetworkProxy currentProxy() const;
    bool isProxyEnabled() const;

    // Network share access (SMB/NFS)
    bool mountNetworkShare(const QUrl& shareUrl, const QString& username = QString(), 
                          const QString& password = QString());
    void unmountNetworkShare();
    bool isNetworkShareMounted() const;
    QString getLocalMountPath() const;

    // Stream configuration
    void setUserAgent(const QString& userAgent);
    void setCustomHeaders(const QMap<QString, QString>& headers);
    void setConnectionTimeout(int timeoutMs);
    void setBufferSize(int bufferSizeKB);

    // Error handling
    QString lastError() const;
    int lastErrorCode() const;

signals:
    void streamStateChanged(StreamState newState);
    void streamOpened(const StreamInfo& info);
    void streamClosed();
    void streamError(const QString& error, int errorCode);
    void bandwidthStatsUpdated(const BandwidthStats& stats);
    void bufferLevelChanged(int percentage);
    void networkShareMounted(const QString& localPath);
    void networkShareUnmounted();

private slots:
    void onNetworkReplyFinished();
    void onNetworkReplyError(QNetworkReply::NetworkError error);
    void onBandwidthTimerTimeout();
    void updateBandwidthStats();

private:
    // Stream type detection helpers
    StreamType detectHttpStreamType(const QUrl& url) const;
    bool isHLSStream(const QUrl& url) const;
    bool isDASHStream(const QUrl& url) const;
    bool isRTSPStream(const QUrl& url) const;

    // Network operations
    void setupNetworkManager();
    void configureRequest(QNetworkRequest& request) const;
    bool validateStreamResponse(QNetworkReply* reply);
    
    // Stream opening helpers
    bool openHttpBasedStream(const QUrl& url);
    bool openRTSPStream(const QUrl& url);
    bool openNetworkShare(const QUrl& url);

    // Bandwidth calculation
    void calculateBandwidthStats();
    void resetBandwidthStats();

    // Network share helpers
    bool mountSMBShare(const QUrl& url, const QString& username, const QString& password);
    bool mountNFSShare(const QUrl& url);
    QString generateMountPoint() const;

    // Member variables
    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    QNetworkReply* m_currentReply;
    StreamState m_currentState;
    StreamInfo m_currentStreamInfo;
    
    // Bandwidth monitoring
    QTimer* m_bandwidthTimer;
    BandwidthStats m_bandwidthStats;
    QList<QPair<qint64, qint64>> m_bandwidthHistory; // timestamp, bytes
    mutable QMutex m_statsMutex;
    
    // Configuration
    QString m_userAgent;
    QMap<QString, QString> m_customHeaders;
    int m_connectionTimeout;
    int m_bufferSize;
    
    // Network share
    QString m_mountedSharePath;
    QString m_localMountPath;
    
    // Error tracking
    QString m_lastError;
    int m_lastErrorCode;
    
    // Proxy settings
    bool m_proxyEnabled;
    
    // Constants
    static const int DEFAULT_TIMEOUT_MS = 30000;
    static const int DEFAULT_BUFFER_SIZE_KB = 1024;
    static const int BANDWIDTH_UPDATE_INTERVAL_MS = 1000;
    static const int BANDWIDTH_HISTORY_SIZE = 30;
};