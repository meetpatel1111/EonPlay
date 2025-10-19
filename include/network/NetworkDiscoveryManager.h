#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QHostAddress>
#include <QUdpSocket>
#include <QTcpServer>
#include <QTimer>
// Bluetooth includes temporarily disabled for build compatibility
// #include <QBluetoothDeviceDiscoveryAgent>
// #include <QBluetoothSocket>
// #include <QBluetoothLocalDevice>
#include <memory>

class QNetworkAccessManager;
class QXmlStreamReader;

/**
 * @brief Manages network device discovery and media sharing
 * 
 * Provides DLNA/UPnP media server discovery, network media sharing,
 * Bluetooth playback output, and multi-device synchronization for EonPlay.
 */
class NetworkDiscoveryManager : public QObject
{
    Q_OBJECT

public:
    enum DeviceType {
        DLNA_MEDIA_SERVER,
        DLNA_MEDIA_RENDERER,
        UPNP_DEVICE,
        BLUETOOTH_AUDIO_DEVICE,
        EONPLAY_INSTANCE,
        UNKNOWN_DEVICE
    };

    enum DiscoveryState {
        DISCOVERY_IDLE,
        DISCOVERY_ACTIVE,
        DISCOVERY_ERROR
    };

    struct NetworkDevice {
        QString id;
        QString name;
        QString description;
        DeviceType type;
        QHostAddress address;
        int port = 0;
        QString manufacturer;
        QString model;
        QString version;
        QUrl descriptionUrl;
        QStringList services;
        QMap<QString, QString> capabilities;
        bool isOnline = false;
        qint64 lastSeen = 0;
    };

    struct MediaShare {
        QString shareId;
        QString name;
        QString description;
        QString rootPath;
        QStringList allowedDevices;
        bool isActive = false;
        int connectionCount = 0;
        qint64 bytesServed = 0;
    };

    struct SyncGroup {
        QString groupId;
        QString name;
        QStringList deviceIds;
        QString currentMedia;
        qint64 syncPosition = 0;
        bool isPlaying = false;
        QString masterDevice;
    };

    explicit NetworkDiscoveryManager(QObject *parent = nullptr);
    ~NetworkDiscoveryManager();

    // Device discovery
    void startDiscovery();
    void stopDiscovery();
    bool isDiscoveryActive() const;
    DiscoveryState discoveryState() const;
    
    // Device management
    QList<NetworkDevice> getDiscoveredDevices() const;
    QList<NetworkDevice> getDevicesByType(DeviceType type) const;
    NetworkDevice getDevice(const QString& deviceId) const;
    bool isDeviceOnline(const QString& deviceId) const;
    void refreshDevice(const QString& deviceId);

    // DLNA/UPnP discovery
    void startUPnPDiscovery();
    void stopUPnPDiscovery();
    bool browseMediaServer(const QString& deviceId, const QString& containerId = "0");
    bool connectToMediaRenderer(const QString& deviceId);
    QStringList getMediaServerContent(const QString& deviceId) const;

    // Media sharing
    QString createMediaShare(const QString& name, const QString& rootPath);
    bool removeMediaShare(const QString& shareId);
    bool startMediaShare(const QString& shareId);
    bool stopMediaShare(const QString& shareId);
    QList<MediaShare> getActiveShares() const;
    MediaShare getMediaShare(const QString& shareId) const;

    // Bluetooth integration - temporarily disabled for build compatibility
    // void startBluetoothDiscovery();
    // void stopBluetoothDiscovery();
    // bool connectBluetoothDevice(const QString& deviceAddress);
    // void disconnectBluetoothDevice(const QString& deviceAddress);
    // QList<NetworkDevice> getBluetoothDevices() const;
    // bool isBluetoothAvailable() const;

    // Multi-device synchronization
    QString createSyncGroup(const QString& name);
    bool joinSyncGroup(const QString& groupId);
    bool leaveSyncGroup(const QString& groupId);
    bool addDeviceToSyncGroup(const QString& groupId, const QString& deviceId);
    bool removeDeviceFromSyncGroup(const QString& groupId, const QString& deviceId);
    void syncPlayback(const QString& groupId, const QString& mediaUrl, qint64 position);
    void syncPlaybackState(const QString& groupId, bool isPlaying, qint64 position);
    QList<SyncGroup> getSyncGroups() const;

    // Network configuration
    void setDiscoveryInterval(int intervalMs);
    void setDiscoveryTimeout(int timeoutMs);
    void setMediaSharePort(int port);
    void setMaxConnections(int maxConnections);
    void enableAutoDiscovery(bool enabled);

signals:
    void discoveryStateChanged(DiscoveryState state);
    void deviceDiscovered(const NetworkDevice& device);
    void deviceLost(const QString& deviceId);
    void deviceUpdated(const NetworkDevice& device);
    void mediaServerContentReceived(const QString& deviceId, const QStringList& content);
    void mediaRendererConnected(const QString& deviceId);
    void mediaRendererDisconnected(const QString& deviceId);
    void mediaShareCreated(const QString& shareId);
    void mediaShareStarted(const QString& shareId);
    void mediaShareStopped(const QString& shareId);
    void mediaShareConnectionReceived(const QString& shareId, const QString& clientAddress);
    void bluetoothDeviceConnected(const QString& deviceAddress);
    void bluetoothDeviceDisconnected(const QString& deviceAddress);
    void syncGroupCreated(const QString& groupId);
    void syncGroupJoined(const QString& groupId);
    void syncGroupLeft(const QString& groupId);
    void syncPlaybackReceived(const QString& groupId, const QString& mediaUrl, qint64 position);
    void syncStateReceived(const QString& groupId, bool isPlaying, qint64 position);
    void networkError(const QString& error);

private slots:
    void onUdpSocketReadyRead();
    void onDiscoveryTimerTimeout();
    void onDeviceTimeoutTimerTimeout();
    void onTcpServerNewConnection();
    // Bluetooth slots - temporarily disabled
    // void onBluetoothDeviceDiscovered(const QBluetoothDeviceInfo& device);
    // void onBluetoothDiscoveryFinished();
    // void onBluetoothSocketConnected();
    // void onBluetoothSocketDisconnected();
    void onNetworkReplyFinished();

private:
    // UPnP/DLNA discovery
    void sendUPnPSearchRequest();
    void processUPnPResponse(const QByteArray& data, const QHostAddress& sender);
    void parseUPnPDevice(const QString& descriptionUrl);
    void parseDeviceDescription(const QByteArray& xml, NetworkDevice& device);
    
    // Device management
    void addOrUpdateDevice(const NetworkDevice& device);
    void removeDevice(const QString& deviceId);
    void checkDeviceTimeouts();
    
    // Media sharing server
    void setupMediaShareServer();
    void handleMediaShareRequest(QTcpSocket* socket);
    void serveMediaFile(QTcpSocket* socket, const QString& filePath);
    QString generateMediaShareResponse(const QString& shareId) const;
    
    // Bluetooth helpers - temporarily disabled
    // void setupBluetoothDiscovery();
    // void processBluetoothDevice(const QBluetoothDeviceInfo& deviceInfo);
    
    // Synchronization
    void setupSyncServer();
    void handleSyncMessage(const QByteArray& message, const QHostAddress& sender);
    void broadcastSyncMessage(const QString& groupId, const QByteArray& message);
    
    // Network utilities
    void setupNetworkComponents();
    QByteArray createUPnPSearchMessage(const QString& searchTarget) const;
    QByteArray createSyncMessage(const QString& type, const QJsonObject& data) const;
    
    // Member variables
    std::unique_ptr<QUdpSocket> m_udpSocket;
    std::unique_ptr<QTcpServer> m_mediaShareServer;
    std::unique_ptr<QTcpServer> m_syncServer;
    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    
    // Discovery state
    DiscoveryState m_discoveryState;
    QTimer* m_discoveryTimer;
    QTimer* m_deviceTimeoutTimer;
    bool m_autoDiscoveryEnabled;
    
    // Device storage
    QMap<QString, NetworkDevice> m_discoveredDevices;
    QMap<QString, qint64> m_deviceLastSeen;
    
    // Media sharing
    QMap<QString, MediaShare> m_mediaShares;
    QMap<QTcpSocket*, QString> m_activeConnections;
    int m_mediaSharePort;
    int m_maxConnections;
    
    // Bluetooth - temporarily disabled
    // std::unique_ptr<QBluetoothDeviceDiscoveryAgent> m_bluetoothDiscovery;
    // std::unique_ptr<QBluetoothLocalDevice> m_bluetoothDevice;
    // QMap<QString, QBluetoothSocket*> m_bluetoothConnections;
    
    // Synchronization
    QMap<QString, SyncGroup> m_syncGroups;
    QString m_currentSyncGroup;
    int m_syncPort;
    
    // Configuration
    int m_discoveryInterval;
    int m_discoveryTimeout;
    int m_deviceTimeout;
    
    // Constants
    static const int DEFAULT_DISCOVERY_INTERVAL_MS = 30000;
    static const int DEFAULT_DISCOVERY_TIMEOUT_MS = 5000;
    static const int DEFAULT_DEVICE_TIMEOUT_MS = 120000;
    static const int DEFAULT_MEDIA_SHARE_PORT = 8080;
    static const int DEFAULT_SYNC_PORT = 8081;
    static const int DEFAULT_MAX_CONNECTIONS = 10;
    static const QString UPNP_MULTICAST_ADDRESS;
    static const int UPNP_MULTICAST_PORT = 1900;
};