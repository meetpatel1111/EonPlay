#ifndef VIDEOCASTINGMANAGER_H
#define VIDEOCASTINGMANAGER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QVector>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUdpSocket>
#include <QTcpServer>
#ifdef HAVE_QT_WEBSOCKETS
#include <QWebSocketServer>
#include <QWebSocket>
#endif
#include <memory>

/**
 * @brief Video casting and streaming management system
 * 
 * Provides comprehensive video casting capabilities including DLNA/UPnP discovery,
 * Chromecast support, remote web control, and QR code device pairing.
 */
class VideoCastingManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Casting device types
     */
    enum DeviceType {
        Unknown = 0,
        DLNA,
        UPnP,
        Chromecast,
        AirPlay,
        Miracast,
        WebBrowser
    };

    /**
     * @brief Casting device information
     */
    struct CastingDevice {
        QString id;                 // Unique device ID
        QString name;               // Display name
        QString manufacturer;       // Device manufacturer
        QString model;              // Device model
        DeviceType type;           // Device type
        QUrl serviceUrl;           // Service URL
        QString ipAddress;         // IP address
        int port;                  // Port number
        bool isAvailable;          // Device availability
        int signalStrength;        // Signal strength (0-100)
        QStringList capabilities;  // Supported capabilities
        
        CastingDevice() : type(Unknown), port(0), isAvailable(false), signalStrength(0) {}
    };

    /**
     * @brief Casting session information
     */
    struct CastingSession {
        QString sessionId;         // Session ID
        CastingDevice device;      // Target device
        QString mediaUrl;          // Media URL being cast
        QString mediaTitle;        // Media title
        qint64 position;          // Current position in milliseconds
        qint64 duration;          // Total duration in milliseconds
        bool isPlaying;           // Playback state
        int volume;               // Volume level (0-100)
        bool isMuted;             // Mute state
        
        CastingSession() : position(0), duration(0), isPlaying(false), volume(50), isMuted(false) {}
    };

    /**
     * @brief Remote control commands
     */
    enum RemoteCommand {
        Play = 0,
        Pause,
        Stop,
        Seek,
        SetVolume,
        Mute,
        Unmute,
        Next,
        Previous,
        LoadMedia
    };

    explicit VideoCastingManager(QObject* parent = nullptr);
    ~VideoCastingManager() override;

    /**
     * @brief Initialize the casting manager
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Shutdown the casting manager
     */
    void shutdown();

    // Device discovery
    /**
     * @brief Start device discovery
     * @param timeout Discovery timeout in milliseconds
     */
    void startDeviceDiscovery(int timeout = 10000);

    /**
     * @brief Stop device discovery
     */
    void stopDeviceDiscovery();

    /**
     * @brief Check if discovery is active
     * @return true if discovering
     */
    bool isDiscovering() const;

    /**
     * @brief Get discovered devices
     * @return List of discovered devices
     */
    QVector<CastingDevice> getDiscoveredDevices() const;

    /**
     * @brief Refresh device list
     */
    void refreshDevices();

    // Casting operations
    /**
     * @brief Start casting to device
     * @param device Target device
     * @param mediaUrl Media URL to cast
     * @param mediaTitle Media title
     * @return true if casting started successfully
     */
    bool startCasting(const CastingDevice& device, const QUrl& mediaUrl, const QString& mediaTitle = QString());

    /**
     * @brief Stop current casting session
     */
    void stopCasting();

    /**
     * @brief Check if currently casting
     * @return true if casting is active
     */
    bool isCasting() const;

    /**
     * @brief Get current casting session
     * @return Current session info
     */
    CastingSession getCurrentSession() const;

    // Playback control
    /**
     * @brief Send remote command to casting device
     * @param command Remote command
     * @param parameter Command parameter (optional)
     * @return true if command sent successfully
     */
    bool sendRemoteCommand(RemoteCommand command, const QVariant& parameter = QVariant());

    /**
     * @brief Set playback position
     * @param position Position in milliseconds
     * @return true if successful
     */
    bool seekTo(qint64 position);

    /**
     * @brief Set volume level
     * @param volume Volume level (0-100)
     * @return true if successful
     */
    bool setVolume(int volume);

    /**
     * @brief Set mute state
     * @param muted Mute state
     * @return true if successful
     */
    bool setMuted(bool muted);

    // Remote web control
    /**
     * @brief Start web control server
     * @param port Server port (0 = auto)
     * @return true if server started successfully
     */
    bool startWebControlServer(int port = 0);

    /**
     * @brief Stop web control server
     */
    void stopWebControlServer();

    /**
     * @brief Check if web control server is running
     * @return true if running
     */
    bool isWebControlServerRunning() const;

    /**
     * @brief Get web control URL
     * @return Web control URL
     */
    QUrl getWebControlUrl() const;

    /**
     * @brief Generate QR code for device pairing
     * @return QR code data
     */
    QString generatePairingQRCode() const;

    /**
     * @brief Get connected remote clients
     * @return Number of connected clients
     */
    int getConnectedClients() const;

    // DLNA/UPnP specific
    /**
     * @brief Get DLNA media server URL
     * @return Media server URL
     */
    QUrl getDLNAMediaServerUrl() const;

    /**
     * @brief Set DLNA device description
     * @param description Device description
     */
    void setDLNADeviceDescription(const QString& description);

    // Chromecast specific
    /**
     * @brief Check if Chromecast is available
     * @return true if available
     */
    bool isChromecastAvailable() const;

    /**
     * @brief Get Chromecast app ID
     * @return App ID
     */
    QString getChromecastAppId() const;

    /**
     * @brief Set Chromecast app ID
     * @param appId App ID
     */
    void setChromecastAppId(const QString& appId);

signals:
    /**
     * @brief Emitted when device discovery starts
     */
    void discoveryStarted();

    /**
     * @brief Emitted when device discovery stops
     */
    void discoveryStopped();

    /**
     * @brief Emitted when a device is discovered
     * @param device Discovered device
     */
    void deviceDiscovered(const CastingDevice& device);

    /**
     * @brief Emitted when a device is lost
     * @param deviceId Lost device ID
     */
    void deviceLost(const QString& deviceId);

    /**
     * @brief Emitted when casting starts
     * @param session Casting session info
     */
    void castingStarted(const CastingSession& session);

    /**
     * @brief Emitted when casting stops
     */
    void castingStopped();

    /**
     * @brief Emitted when casting session updates
     * @param session Updated session info
     */
    void sessionUpdated(const CastingSession& session);

    /**
     * @brief Emitted when remote command is received
     * @param command Remote command
     * @param parameter Command parameter
     */
    void remoteCommandReceived(RemoteCommand command, const QVariant& parameter);

    /**
     * @brief Emitted when web control server starts
     * @param url Server URL
     */
    void webControlServerStarted(const QUrl& url);

    /**
     * @brief Emitted when web control server stops
     */
    void webControlServerStopped();

    /**
     * @brief Emitted when remote client connects
     * @param clientInfo Client information
     */
    void remoteClientConnected(const QString& clientInfo);

    /**
     * @brief Emitted when remote client disconnects
     * @param clientInfo Client information
     */
    void remoteClientDisconnected(const QString& clientInfo);

private slots:
    void onDiscoveryTimeout();
    void onUdpDataReceived();
#ifdef HAVE_QT_WEBSOCKETS
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketMessageReceived(const QString& message);
#endif
    void onNetworkReplyFinished();
    void updateSessionStatus();

private:
    // Discovery methods
    void discoverDLNADevices();
    void discoverChromecastDevices();
    void discoverAirPlayDevices();
    void sendSSDPDiscovery();
    void parseSSDPResponse(const QByteArray& data, const QHostAddress& sender);
    CastingDevice parseDeviceDescription(const QUrl& descriptionUrl);

    // Casting methods
    bool startDLNACasting(const CastingDevice& device, const QUrl& mediaUrl, const QString& mediaTitle);
    bool startChromecastCasting(const CastingDevice& device, const QUrl& mediaUrl, const QString& mediaTitle);
    bool stopDLNACasting();
    bool stopChromecastCasting();

    // Remote control methods
#ifdef HAVE_QT_WEBSOCKETS
    void setupWebControlServer();
    void handleWebControlRequest(QWebSocket* client, const QString& message);
    QString generateWebControlInterface() const;
    void broadcastSessionUpdate();
#endif

    // Helper methods
    QString generateSessionId() const;
    QString getLocalIPAddress() const;
    int findAvailablePort(int startPort = 8080) const;
    QByteArray createSSDPMessage(const QString& searchTarget) const;

    // Network components
    QNetworkAccessManager* m_networkManager;
    QUdpSocket* m_udpSocket;
    QTcpServer* m_tcpServer;
#ifdef HAVE_QT_WEBSOCKETS
    QWebSocketServer* m_webSocketServer;
    QVector<QWebSocket*> m_connectedClients;
#endif

    // Discovery state
    bool m_discovering;
    QTimer* m_discoveryTimer;
    QVector<CastingDevice> m_discoveredDevices;

    // Casting state
    bool m_casting;
    CastingSession m_currentSession;
    QTimer* m_sessionTimer;

    // Web control state
    bool m_webControlRunning;
    QUrl m_webControlUrl;
    QString m_pairingCode;

    // Configuration
    QString m_dlnaDeviceDescription;
    QString m_chromecastAppId;

    // Thread safety
    mutable QMutex m_deviceMutex;
    mutable QMutex m_sessionMutex;

    // Constants
    static constexpr int SSDP_PORT = 1900;
    static constexpr int DEFAULT_WEB_CONTROL_PORT = 8080;
    static constexpr int DEFAULT_DISCOVERY_TIMEOUT = 10000;
    static constexpr int SESSION_UPDATE_INTERVAL = 1000;
    static constexpr const char* SSDP_ADDRESS = "239.255.255.250";
    static constexpr const char* DEFAULT_CHROMECAST_APP_ID = "CC1AD845";
};

#endif // VIDEOCASTINGMANAGER_H