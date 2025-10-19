#ifndef STREAMINGMANAGER_H
#define STREAMINGMANAGER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QVector>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QSslSocket>
#include <memory>

/**
 * @brief Comprehensive network streaming management system
 * 
 * Provides support for HTTP/HLS/DASH streaming, RTSP/RTP live streams,
 * network shares access, bandwidth monitoring, and proxy support.
 */
class StreamingManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Streaming protocol types
     */
    enum StreamProtocol {
        HTTP = 0,
        HTTPS,
        HLS,            // HTTP Live Streaming
        DASH,           // Dynamic Adaptive Streaming over HTTP
        RTSP,           // Real Time Streaming Protocol
        RTP,            // Real-time Transport Protocol
        RTMP,           // Real-Time Messaging Protocol
        UDP,            // User Datagram Protocol
        TCP,            // Transmission Control Protocol
        SMB,            // Server Message Block
        NFS,            // Network File System
        FTP,            // File Transfer Protocol
        SFTP            // SSH File Transfer Protocol
    };

    /**
     * @brief Stream quality levels
     */
    enum StreamQuality {
        Auto = 0,
        Low_240p,
        Medium_480p,
        High_720p,
        FullHD_1080p,
        UltraHD_4K,
        Custom
    };

    /**
     * @brief Stream information
     */
    struct StreamInfo {
        QString id;                 // Stream ID
        QString title;              // Stream title
        QString description;        // Stream description
        QUrl url;                  // Stream URL
        StreamProtocol protocol;   // Protocol type
        StreamQuality quality;     // Quality level
        int bitrate;               // Bitrate in kbps
        QSize resolution;          // Video resolution
        int frameRate;             // Frame rate
        QString codec;             // Video codec
        QString audioCodec;        // Audio codec
        qint64 duration;          // Duration in milliseconds (-1 for live)
        bool isLive;              // Is live stream
        bool isSecure;            // Uses secure connection
        
        StreamInfo() : protocol(HTTP), quality(Auto), bitrate(0), frameRate(0), 
                      duration(-1), isLive(false), isSecure(false) {}
    };

    /**
     * @brief Network statistics
     */
    struct NetworkStats {
        qint64 bytesReceived;      // Total bytes received
        qint64 bytesSent;          // Total bytes sent
        float downloadSpeed;       // Download speed in KB/s
        float uploadSpeed;         // Upload speed in KB/s
        int ping;                  // Ping time in milliseconds
        float packetLoss;          // Packet loss percentage
        int bufferHealth;          // Buffer health percentage
        qint64 bufferSize;         // Current buffer size in bytes
        QString connectionType;    // Connection type description
        
        NetworkStats() : bytesReceived(0), bytesSent(0), downloadSpeed(0.0f), 
                        uploadSpeed(0.0f), ping(0), packetLoss(0.0f), 
                        bufferHealth(100), bufferSize(0) {}
    };

    /**
     * @brief Proxy configuration
     */
    struct ProxyConfig {
        bool enabled;              // Proxy enabled
        QNetworkProxy::ProxyType type; // Proxy type
        QString hostname;          // Proxy hostname
        int port;                  // Proxy port
        QString username;          // Username (optional)
        QString password;          // Password (optional)
        
        ProxyConfig() : enabled(false), type(QNetworkProxy::HttpProxy), port(8080) {}
    };

    /**
     * @brief Network share information
     */
    struct NetworkShare {
        QString id;                // Share ID
        QString name;              // Display name
        QString path;              // Network path
        QString hostname;          // Server hostname
        QString username;          // Username
        QString password;          // Password
        StreamProtocol protocol;   // Protocol (SMB, NFS, FTP, etc.)
        bool isConnected;          // Connection status
        bool isReadOnly;           // Read-only access
        qint64 totalSpace;         // Total space in bytes
        qint64 freeSpace;          // Free space in bytes
        
        NetworkShare() : protocol(SMB), isConnected(false), isReadOnly(false),
                        totalSpace(0), freeSpace(0) {}
    };

    explicit StreamingManager(QObject* parent = nullptr);
    ~StreamingManager() override;

    /**
     * @brief Initialize the streaming manager
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Shutdown the streaming manager
     */
    void shutdown();

    // Stream playback
    /**
     * @brief Start streaming from URL
     * @param url Stream URL
     * @param preferredQuality Preferred quality level
     * @return true if stream started successfully
     */
    bool startStream(const QUrl& url, StreamQuality preferredQuality = Auto);

    /**
     * @brief Stop current stream
     */
    void stopStream();

    /**
     * @brief Check if currently streaming
     * @return true if streaming is active
     */
    bool isStreaming() const;

    /**
     * @brief Get current stream information
     * @return Current stream info
     */
    StreamInfo getCurrentStreamInfo() const;

    /**
     * @brief Get available stream qualities
     * @param url Stream URL
     * @return List of available qualities
     */
    QVector<StreamQuality> getAvailableQualities(const QUrl& url);

    /**
     * @brief Set stream quality
     * @param quality Quality level
     * @return true if successful
     */
    bool setStreamQuality(StreamQuality quality);

    // Network monitoring
    /**
     * @brief Get current network statistics
     * @return Network statistics
     */
    NetworkStats getNetworkStats() const;

    /**
     * @brief Start bandwidth monitoring
     * @param interval Update interval in milliseconds
     */
    void startBandwidthMonitoring(int interval = 1000);

    /**
     * @brief Stop bandwidth monitoring
     */
    void stopBandwidthMonitoring();

    /**
     * @brief Check if bandwidth monitoring is active
     * @return true if monitoring
     */
    bool isBandwidthMonitoring() const;

    /**
     * @brief Test network connection
     * @param url URL to test
     * @return true if connection successful
     */
    bool testConnection(const QUrl& url);

    // Proxy support
    /**
     * @brief Get proxy configuration
     * @return Current proxy config
     */
    ProxyConfig getProxyConfig() const;

    /**
     * @brief Set proxy configuration
     * @param config Proxy configuration
     */
    void setProxyConfig(const ProxyConfig& config);

    /**
     * @brief Test proxy connection
     * @param config Proxy configuration to test
     * @return true if proxy works
     */
    bool testProxy(const ProxyConfig& config);

    // Network shares
    /**
     * @brief Add network share
     * @param share Network share configuration
     * @return true if added successfully
     */
    bool addNetworkShare(const NetworkShare& share);

    /**
     * @brief Remove network share
     * @param shareId Share ID to remove
     * @return true if removed successfully
     */
    bool removeNetworkShare(const QString& shareId);

    /**
     * @brief Connect to network share
     * @param shareId Share ID to connect to
     * @return true if connected successfully
     */
    bool connectToShare(const QString& shareId);

    /**
     * @brief Disconnect from network share
     * @param shareId Share ID to disconnect from
     */
    void disconnectFromShare(const QString& shareId);

    /**
     * @brief Get network shares
     * @return List of configured shares
     */
    QVector<NetworkShare> getNetworkShares() const;

    /**
     * @brief Browse network share contents
     * @param shareId Share ID
     * @param path Path within share
     * @return List of files and directories
     */
    QStringList browseShare(const QString& shareId, const QString& path = "/");

    // Protocol-specific methods
    /**
     * @brief Parse HLS manifest
     * @param url HLS manifest URL
     * @return List of available streams
     */
    QVector<StreamInfo> parseHLSManifest(const QUrl& url);

    /**
     * @brief Parse DASH manifest
     * @param url DASH manifest URL
     * @return List of available streams
     */
    QVector<StreamInfo> parseDASHManifest(const QUrl& url);

    /**
     * @brief Connect to RTSP stream
     * @param url RTSP URL
     * @return true if connected successfully
     */
    bool connectRTSP(const QUrl& url);

    /**
     * @brief Get supported protocols
     * @return List of supported protocols
     */
    static QVector<StreamProtocol> getSupportedProtocols();

    /**
     * @brief Get protocol name
     * @param protocol Protocol type
     * @return Protocol name
     */
    static QString getProtocolName(StreamProtocol protocol);

signals:
    /**
     * @brief Emitted when stream starts
     * @param streamInfo Stream information
     */
    void streamStarted(const StreamInfo& streamInfo);

    /**
     * @brief Emitted when stream stops
     */
    void streamStopped();

    /**
     * @brief Emitted when stream quality changes
     * @param quality New quality level
     */
    void streamQualityChanged(StreamQuality quality);

    /**
     * @brief Emitted when network stats update
     * @param stats Network statistics
     */
    void networkStatsUpdated(const NetworkStats& stats);

    /**
     * @brief Emitted when bandwidth monitoring starts
     */
    void bandwidthMonitoringStarted();

    /**
     * @brief Emitted when bandwidth monitoring stops
     */
    void bandwidthMonitoringStopped();

    /**
     * @brief Emitted when network share connects
     * @param shareId Share ID
     */
    void shareConnected(const QString& shareId);

    /**
     * @brief Emitted when network share disconnects
     * @param shareId Share ID
     */
    void shareDisconnected(const QString& shareId);

    /**
     * @brief Emitted when stream error occurs
     * @param error Error message
     */
    void streamError(const QString& error);

private slots:
    void updateNetworkStats();
    void onNetworkReplyFinished();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    // Stream processing methods
    bool processHTTPStream(const QUrl& url);
    bool processHLSStream(const QUrl& url);
    bool processDASHStream(const QUrl& url);
    bool processRTSPStream(const QUrl& url);
    bool processSMBShare(const NetworkShare& share);
    bool processNFSShare(const NetworkShare& share);

    // Network monitoring methods
    void calculateBandwidth();
    void updateBufferHealth();
    int measurePing(const QString& hostname);
    float calculatePacketLoss();

    // Helper methods
    StreamProtocol detectProtocol(const QUrl& url);
    QString generateStreamId() const;
    bool validateStreamUrl(const QUrl& url);
    QNetworkRequest createNetworkRequest(const QUrl& url);

    // Network components
    QNetworkAccessManager* m_networkManager;
    QUdpSocket* m_udpSocket;
    QTcpSocket* m_tcpSocket;
    QSslSocket* m_sslSocket;

    // Streaming state
    bool m_streaming;
    StreamInfo m_currentStream;
    QVector<StreamInfo> m_availableStreams;

    // Network monitoring
    bool m_bandwidthMonitoring;
    QTimer* m_statsTimer;
    NetworkStats m_networkStats;
    qint64 m_lastBytesReceived;
    qint64 m_lastBytesSent;
    qint64 m_lastStatsUpdate;

    // Proxy configuration
    ProxyConfig m_proxyConfig;

    // Network shares
    QVector<NetworkShare> m_networkShares;

    // Thread safety
    mutable QMutex m_streamMutex;
    mutable QMutex m_statsMutex;

    // Constants
    static constexpr int DEFAULT_BUFFER_SIZE = 1024 * 1024; // 1MB
    static constexpr int DEFAULT_STATS_INTERVAL = 1000;     // 1 second
    static constexpr int CONNECTION_TIMEOUT = 10000;        // 10 seconds
    static constexpr int MAX_REDIRECTS = 5;
};

#endif // STREAMINGMANAGER_H