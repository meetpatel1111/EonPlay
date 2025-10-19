#include "network/NetworkDiscoveryManager.h"
#include <QUdpSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
// Bluetooth includes temporarily disabled for build compatibility
// #include <QBluetoothDeviceDiscoveryAgent>
// #include <QBluetoothSocket>
// #include <QBluetoothLocalDevice>
// #include <QBluetoothDeviceInfo>
#include <QHostAddress>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(networkDiscovery, "eonplay.network.discovery")

// Constants
const QString NetworkDiscoveryManager::UPNP_MULTICAST_ADDRESS = "239.255.255.250";

NetworkDiscoveryManager::NetworkDiscoveryManager(QObject *parent)
    : QObject(parent)
    , m_udpSocket(std::make_unique<QUdpSocket>(this))
    , m_mediaShareServer(std::make_unique<QTcpServer>(this))
    , m_syncServer(std::make_unique<QTcpServer>(this))
    , m_networkManager(std::make_unique<QNetworkAccessManager>(this))
    , m_discoveryState(DISCOVERY_IDLE)
    , m_discoveryTimer(new QTimer(this))
    , m_deviceTimeoutTimer(new QTimer(this))
    , m_autoDiscoveryEnabled(true)
    , m_mediaSharePort(DEFAULT_MEDIA_SHARE_PORT)
    , m_maxConnections(DEFAULT_MAX_CONNECTIONS)
    , m_syncPort(DEFAULT_SYNC_PORT)
    , m_discoveryInterval(DEFAULT_DISCOVERY_INTERVAL_MS)
    , m_discoveryTimeout(DEFAULT_DISCOVERY_TIMEOUT_MS)
    , m_deviceTimeout(DEFAULT_DEVICE_TIMEOUT_MS)
{
    setupNetworkComponents();
    // setupBluetoothDiscovery(); // Temporarily disabled
    setupMediaShareServer();
    setupSyncServer();
    
    qCDebug(networkDiscovery) << "NetworkDiscoveryManager initialized";
}

NetworkDiscoveryManager::~NetworkDiscoveryManager()
{
    stopDiscovery();
    // stopBluetoothDiscovery(); // Temporarily disabled
    
    // Clean up Bluetooth connections - temporarily disabled
    // for (auto socket : m_bluetoothConnections.values()) {
    //     if (socket) {
    //         socket->disconnectFromService();
    //         socket->deleteLater();
    //     }
    // }
    // m_bluetoothConnections.clear();
}

void NetworkDiscoveryManager::setupNetworkComponents()
{
    // Setup UDP socket for UPnP discovery
    connect(m_udpSocket.get(), &QUdpSocket::readyRead,
            this, &NetworkDiscoveryManager::onUdpSocketReadyRead);
    
    // Setup discovery timer
    m_discoveryTimer->setInterval(m_discoveryInterval);
    connect(m_discoveryTimer, &QTimer::timeout,
            this, &NetworkDiscoveryManager::onDiscoveryTimerTimeout);
    
    // Setup device timeout timer
    m_deviceTimeoutTimer->setInterval(30000); // Check every 30 seconds
    connect(m_deviceTimeoutTimer, &QTimer::timeout,
            this, &NetworkDiscoveryManager::onDeviceTimeoutTimerTimeout);
    
    // Setup network manager
    connect(m_networkManager.get(), &QNetworkAccessManager::finished,
            this, &NetworkDiscoveryManager::onNetworkReplyFinished);
}

// Bluetooth setup temporarily disabled for build compatibility
/*
void NetworkDiscoveryManager::setupBluetoothDiscovery()
{
    // Check if Bluetooth is available
    if (QBluetoothLocalDevice::allDevices().isEmpty()) {
        qCWarning(networkDiscovery) << "No Bluetooth adapters found";
        return;
    }
    
    m_bluetoothDevice = std::make_unique<QBluetoothLocalDevice>(this);
    m_bluetoothDiscovery = std::make_unique<QBluetoothDeviceDiscoveryAgent>(this);
    
    connect(m_bluetoothDiscovery.get(), &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &NetworkDiscoveryManager::onBluetoothDeviceDiscovered);
    connect(m_bluetoothDiscovery.get(), &QBluetoothDeviceDiscoveryAgent::finished,
            this, &NetworkDiscoveryManager::onBluetoothDiscoveryFinished);
}
*/

void NetworkDiscoveryManager::setupMediaShareServer()
{
    connect(m_mediaShareServer.get(), &QTcpServer::newConnection,
            this, &NetworkDiscoveryManager::onTcpServerNewConnection);
}

void NetworkDiscoveryManager::setupSyncServer()
{
    if (!m_syncServer->listen(QHostAddress::Any, m_syncPort)) {
        qCWarning(networkDiscovery) << "Failed to start sync server on port" << m_syncPort;
    } else {
        qCDebug(networkDiscovery) << "Sync server started on port" << m_syncPort;
    }
}

// Discovery management
void NetworkDiscoveryManager::startDiscovery()
{
    if (m_discoveryState == DISCOVERY_ACTIVE) return;
    
    m_discoveryState = DISCOVERY_ACTIVE;
    emit discoveryStateChanged(m_discoveryState);
    
    // Bind UDP socket for UPnP discovery
    if (!m_udpSocket->bind(QHostAddress::Any, 0)) {
        qCWarning(networkDiscovery) << "Failed to bind UDP socket for discovery";
        m_discoveryState = DISCOVERY_ERROR;
        emit discoveryStateChanged(m_discoveryState);
        return;
    }
    
    // Start UPnP discovery
    startUPnPDiscovery();
    
    // Start Bluetooth discovery if available - temporarily disabled
    // if (isBluetoothAvailable()) {
    //     startBluetoothDiscovery();
    // }
    
    // Start periodic discovery
    if (m_autoDiscoveryEnabled) {
        m_discoveryTimer->start();
    }
    
    // Start device timeout monitoring
    m_deviceTimeoutTimer->start();
    
    qCDebug(networkDiscovery) << "Network discovery started";
}

void NetworkDiscoveryManager::stopDiscovery()
{
    if (m_discoveryState == DISCOVERY_IDLE) return;
    
    m_discoveryState = DISCOVERY_IDLE;
    emit discoveryStateChanged(m_discoveryState);
    
    // Stop timers
    m_discoveryTimer->stop();
    m_deviceTimeoutTimer->stop();
    
    // Stop UPnP discovery
    stopUPnPDiscovery();
    
    // Stop Bluetooth discovery - temporarily disabled
    // stopBluetoothDiscovery();
    
    // Close UDP socket
    m_udpSocket->close();
    
    qCDebug(networkDiscovery) << "Network discovery stopped";
}

// UPnP/DLNA Discovery
void NetworkDiscoveryManager::startUPnPDiscovery()
{
    sendUPnPSearchRequest();
}

void NetworkDiscoveryManager::stopUPnPDiscovery()
{
    // UPnP discovery stops automatically after timeout
}

void NetworkDiscoveryManager::sendUPnPSearchRequest()
{
    QStringList searchTargets = {
        "upnp:rootdevice",
        "urn:schemas-upnp-org:device:MediaServer:1",
        "urn:schemas-upnp-org:device:MediaRenderer:1",
        "urn:schemas-upnp-org:service:ContentDirectory:1",
        "urn:schemas-upnp-org:service:AVTransport:1"
    };
    
    QHostAddress multicastAddress(UPNP_MULTICAST_ADDRESS);
    
    for (const QString& target : searchTargets) {
        QByteArray message = createUPnPSearchMessage(target);
        m_udpSocket->writeDatagram(message, multicastAddress, UPNP_MULTICAST_PORT);
    }
    
    qCDebug(networkDiscovery) << "UPnP search requests sent";
}

QByteArray NetworkDiscoveryManager::createUPnPSearchMessage(const QString& searchTarget) const
{
    QString message = QString(
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: %1:%2\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "ST: %3\r\n"
        "MX: 3\r\n\r\n"
    ).arg(UPNP_MULTICAST_ADDRESS).arg(UPNP_MULTICAST_PORT).arg(searchTarget);
    
    return message.toUtf8();
}

void NetworkDiscoveryManager::onUdpSocketReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray data;
        QHostAddress sender;
        quint16 senderPort;
        
        data.resize(m_udpSocket->pendingDatagramSize());
        m_udpSocket->readDatagram(data.data(), data.size(), &sender, &senderPort);
        
        processUPnPResponse(data, sender);
    }
}

void NetworkDiscoveryManager::processUPnPResponse(const QByteArray& data, const QHostAddress& sender)
{
    QString response = QString::fromUtf8(data);
    
    // Parse HTTP response headers
    QStringList lines = response.split("\r\n");
    if (lines.isEmpty() || !lines[0].startsWith("HTTP/1.1 200 OK")) {
        return; // Not a valid response
    }
    
    QString location;
    QString server;
    QString usn;
    QString st;
    
    for (const QString& line : lines) {
        if (line.startsWith("LOCATION:", Qt::CaseInsensitive)) {
            location = line.mid(9).trimmed();
        } else if (line.startsWith("SERVER:", Qt::CaseInsensitive)) {
            server = line.mid(7).trimmed();
        } else if (line.startsWith("USN:", Qt::CaseInsensitive)) {
            usn = line.mid(4).trimmed();
        } else if (line.startsWith("ST:", Qt::CaseInsensitive)) {
            st = line.mid(3).trimmed();
        }
    }
    
    if (!location.isEmpty()) {
        parseUPnPDevice(location);
    }
}

void NetworkDiscoveryManager::parseUPnPDevice(const QString& descriptionUrl)
{
    QNetworkRequest request(descriptionUrl);
    request.setRawHeader("User-Agent", "EonPlay/1.0 UPnP/1.0");
    
    QNetworkReply* reply = m_networkManager->get(request);
    reply->setProperty("descriptionUrl", descriptionUrl);
}

void NetworkDiscoveryManager::onNetworkReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QString descriptionUrl = reply->property("descriptionUrl").toString();
        if (!descriptionUrl.isEmpty()) {
            QByteArray xml = reply->readAll();
            
            NetworkDevice device;
            device.address = QHostAddress(reply->url().host());
            device.port = reply->url().port(80);
            device.descriptionUrl = QUrl(descriptionUrl);
            
            parseDeviceDescription(xml, device);
            
            if (!device.id.isEmpty()) {
                addOrUpdateDevice(device);
            }
        }
    }
    
    reply->deleteLater();
}

void NetworkDiscoveryManager::parseDeviceDescription(const QByteArray& xml, NetworkDevice& device)
{
    QXmlStreamReader reader(xml);
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QString name = reader.name().toString();
            
            if (name == "device") {
                // Parse device information
                while (!(reader.isEndElement() && reader.name() == "device")) {
                    reader.readNext();
                    
                    if (reader.isStartElement()) {
                        QString elementName = reader.name().toString();
                        QString elementText = reader.readElementText();
                        
                        if (elementName == "UDN") {
                            device.id = elementText;
                        } else if (elementName == "friendlyName") {
                            device.name = elementText;
                        } else if (elementName == "deviceType") {
                            if (elementText.contains("MediaServer")) {
                                device.type = DLNA_MEDIA_SERVER;
                            } else if (elementText.contains("MediaRenderer")) {
                                device.type = DLNA_MEDIA_RENDERER;
                            } else {
                                device.type = UPNP_DEVICE;
                            }
                        } else if (elementName == "manufacturer") {
                            device.manufacturer = elementText;
                        } else if (elementName == "modelName") {
                            device.model = elementText;
                        } else if (elementName == "modelNumber") {
                            device.version = elementText;
                        } else if (elementName == "modelDescription") {
                            device.description = elementText;
                        }
                    }
                }
            } else if (name == "service") {
                // Parse service information
                QString serviceType;
                while (!(reader.isEndElement() && reader.name() == "service")) {
                    reader.readNext();
                    
                    if (reader.isStartElement() && reader.name() == "serviceType") {
                        serviceType = reader.readElementText();
                        if (!serviceType.isEmpty()) {
                            device.services.append(serviceType);
                        }
                    }
                }
            }
        }
    }
    
    device.isOnline = true;
    device.lastSeen = QDateTime::currentMSecsSinceEpoch();
}

// Device management
void NetworkDiscoveryManager::addOrUpdateDevice(const NetworkDevice& device)
{
    bool isNewDevice = !m_discoveredDevices.contains(device.id);
    
    m_discoveredDevices[device.id] = device;
    m_deviceLastSeen[device.id] = QDateTime::currentMSecsSinceEpoch();
    
    if (isNewDevice) {
        emit deviceDiscovered(device);
        qCDebug(networkDiscovery) << "New device discovered:" << device.name << "(" << device.id << ")";
    } else {
        emit deviceUpdated(device);
        qCDebug(networkDiscovery) << "Device updated:" << device.name << "(" << device.id << ")";
    }
}

void NetworkDiscoveryManager::removeDevice(const QString& deviceId)
{
    if (m_discoveredDevices.contains(deviceId)) {
        m_discoveredDevices.remove(deviceId);
        m_deviceLastSeen.remove(deviceId);
        emit deviceLost(deviceId);
        qCDebug(networkDiscovery) << "Device lost:" << deviceId;
    }
}

void NetworkDiscoveryManager::checkDeviceTimeouts()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    QStringList devicesToRemove;
    
    for (auto it = m_deviceLastSeen.constBegin(); it != m_deviceLastSeen.constEnd(); ++it) {
        if (currentTime - it.value() > m_deviceTimeout) {
            devicesToRemove.append(it.key());
        }
    }
    
    for (const QString& deviceId : devicesToRemove) {
        removeDevice(deviceId);
    }
}

// Bluetooth Integration - temporarily disabled for build compatibility
/*
void NetworkDiscoveryManager::startBluetoothDiscovery()
{
    if (!isBluetoothAvailable()) return;
    
    m_bluetoothDiscovery->start();
    qCDebug(networkDiscovery) << "Bluetooth discovery started";
}

void NetworkDiscoveryManager::stopBluetoothDiscovery()
{
    if (m_bluetoothDiscovery && m_bluetoothDiscovery->isActive()) {
        m_bluetoothDiscovery->stop();
        qCDebug(networkDiscovery) << "Bluetooth discovery stopped";
    }
}

bool NetworkDiscoveryManager::isBluetoothAvailable() const
{
    return m_bluetoothDevice && m_bluetoothDevice->isValid();
}

void NetworkDiscoveryManager::onBluetoothDeviceDiscovered(const QBluetoothDeviceInfo& deviceInfo)
{
    processBluetoothDevice(deviceInfo);
}

void NetworkDiscoveryManager::processBluetoothDevice(const QBluetoothDeviceInfo& deviceInfo)
{
    // Only process audio devices
    if (!(deviceInfo.majorDeviceClass() == QBluetoothDeviceInfo::AudioVideoDevice)) {
        return;
    }
    
    NetworkDevice device;
    device.id = deviceInfo.address().toString();
    device.name = deviceInfo.name();
    device.type = BLUETOOTH_AUDIO_DEVICE;
    device.description = "Bluetooth Audio Device";
    device.isOnline = true;
    device.lastSeen = QDateTime::currentMSecsSinceEpoch();
    
    // Add device capabilities based on service classes
    QBluetoothDeviceInfo::ServiceClasses services = deviceInfo.serviceClasses();
    if (services & QBluetoothDeviceInfo::AudioService) {
        device.capabilities["audio"] = "true";
    }
    if (services & QBluetoothDeviceInfo::RenderingService) {
        device.capabilities["rendering"] = "true";
    }
    
    addOrUpdateDevice(device);
}

void NetworkDiscoveryManager::onBluetoothDiscoveryFinished()
{
    qCDebug(networkDiscovery) << "Bluetooth discovery finished";
}
*/

// Bluetooth connection functions - temporarily disabled for build compatibility
/*
bool NetworkDiscoveryManager::connectBluetoothDevice(const QString& deviceAddress)
{
    if (m_bluetoothConnections.contains(deviceAddress)) {
        return true; // Already connected
    }
    
    QBluetoothSocket* socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);
    
    connect(socket, &QBluetoothSocket::connected,
            this, &NetworkDiscoveryManager::onBluetoothSocketConnected);
    connect(socket, &QBluetoothSocket::disconnected,
            this, &NetworkDiscoveryManager::onBluetoothSocketDisconnected);
    
    QBluetoothAddress address(deviceAddress);
    socket->connectToService(address, QBluetoothUuid::SerialPort);
    
    m_bluetoothConnections[deviceAddress] = socket;
    
    qCDebug(networkDiscovery) << "Connecting to Bluetooth device:" << deviceAddress;
    return true;
}

void NetworkDiscoveryManager::disconnectBluetoothDevice(const QString& deviceAddress)
{
    if (m_bluetoothConnections.contains(deviceAddress)) {
        QBluetoothSocket* socket = m_bluetoothConnections[deviceAddress];
        socket->disconnectFromService();
        socket->deleteLater();
        m_bluetoothConnections.remove(deviceAddress);
        
        emit bluetoothDeviceDisconnected(deviceAddress);
        qCDebug(networkDiscovery) << "Disconnected from Bluetooth device:" << deviceAddress;
    }
}
*/

// Bluetooth socket functions - temporarily disabled for build compatibility
/*
void NetworkDiscoveryManager::onBluetoothSocketConnected()
{
    QBluetoothSocket* socket = qobject_cast<QBluetoothSocket*>(sender());
    if (!socket) return;
    
    QString deviceAddress = socket->peerAddress().toString();
    emit bluetoothDeviceConnected(deviceAddress);
    
    qCDebug(networkDiscovery) << "Bluetooth device connected:" << deviceAddress;
}

void NetworkDiscoveryManager::onBluetoothSocketDisconnected()
{
    QBluetoothSocket* socket = qobject_cast<QBluetoothSocket*>(sender());
    if (!socket) return;
    
    QString deviceAddress = socket->peerAddress().toString();
    m_bluetoothConnections.remove(deviceAddress);
    
    emit bluetoothDeviceDisconnected(deviceAddress);
    socket->deleteLater();
    
    qCDebug(networkDiscovery) << "Bluetooth device disconnected:" << deviceAddress;
}
*/

// Media Sharing
QString NetworkDiscoveryManager::createMediaShare(const QString& name, const QString& rootPath)
{
    QString shareId = QString("share_%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(qrand());
    
    MediaShare share;
    share.shareId = shareId;
    share.name = name;
    share.rootPath = rootPath;
    share.isActive = false;
    
    m_mediaShares[shareId] = share;
    
    emit mediaShareCreated(shareId);
    qCDebug(networkDiscovery) << "Media share created:" << name << "(" << shareId << ")";
    
    return shareId;
}

bool NetworkDiscoveryManager::startMediaShare(const QString& shareId)
{
    if (!m_mediaShares.contains(shareId)) return false;
    
    if (!m_mediaShareServer->isListening()) {
        if (!m_mediaShareServer->listen(QHostAddress::Any, m_mediaSharePort)) {
            qCWarning(networkDiscovery) << "Failed to start media share server on port" << m_mediaSharePort;
            return false;
        }
    }
    
    m_mediaShares[shareId].isActive = true;
    emit mediaShareStarted(shareId);
    
    qCDebug(networkDiscovery) << "Media share started:" << shareId;
    return true;
}

bool NetworkDiscoveryManager::stopMediaShare(const QString& shareId)
{
    if (!m_mediaShares.contains(shareId)) return false;
    
    m_mediaShares[shareId].isActive = false;
    emit mediaShareStopped(shareId);
    
    // Check if any shares are still active
    bool anyActive = false;
    for (const auto& share : m_mediaShares.values()) {
        if (share.isActive) {
            anyActive = true;
            break;
        }
    }
    
    // Stop server if no shares are active
    if (!anyActive && m_mediaShareServer->isListening()) {
        m_mediaShareServer->close();
    }
    
    qCDebug(networkDiscovery) << "Media share stopped:" << shareId;
    return true;
}

void NetworkDiscoveryManager::onTcpServerNewConnection()
{
    while (m_mediaShareServer->hasPendingConnections()) {
        QTcpSocket* socket = m_mediaShareServer->nextPendingConnection();
        
        connect(socket, &QTcpSocket::readyRead, [this, socket]() {
            handleMediaShareRequest(socket);
        });
        
        connect(socket, &QTcpSocket::disconnected, [this, socket]() {
            m_activeConnections.remove(socket);
            socket->deleteLater();
        });
        
        QString clientAddress = socket->peerAddress().toString();
        qCDebug(networkDiscovery) << "Media share connection from:" << clientAddress;
    }
}

void NetworkDiscoveryManager::handleMediaShareRequest(QTcpSocket* socket)
{
    QByteArray request = socket->readAll();
    QString requestStr = QString::fromUtf8(request);
    
    // Parse HTTP request (simplified)
    QStringList lines = requestStr.split("\r\n");
    if (lines.isEmpty()) return;
    
    QStringList requestLine = lines[0].split(" ");
    if (requestLine.size() < 2) return;
    
    QString method = requestLine[0];
    QString path = requestLine[1];
    
    if (method == "GET") {
        // Serve media file or directory listing
        serveMediaFile(socket, path);
    }
}

void NetworkDiscoveryManager::serveMediaFile(QTcpSocket* socket, const QString& filePath)
{
    // Find appropriate media share
    QString shareId;
    QString localPath;
    
    for (const auto& share : m_mediaShares.values()) {
        if (share.isActive && filePath.startsWith("/" + share.shareId)) {
            shareId = share.shareId;
            localPath = share.rootPath + filePath.mid(share.shareId.length() + 1);
            break;
        }
    }
    
    if (shareId.isEmpty()) {
        // Send 404 Not Found
        QString response = "HTTP/1.1 404 Not Found\r\n\r\n";
        socket->write(response.toUtf8());
        socket->disconnectFromHost();
        return;
    }
    
    QFileInfo fileInfo(localPath);
    if (!fileInfo.exists()) {
        // Send 404 Not Found
        QString response = "HTTP/1.1 404 Not Found\r\n\r\n";
        socket->write(response.toUtf8());
        socket->disconnectFromHost();
        return;
    }
    
    if (fileInfo.isDir()) {
        // Send directory listing (simplified)
        QString response = generateMediaShareResponse(shareId);
        socket->write(response.toUtf8());
    } else {
        // Send file
        QFile file(localPath);
        if (file.open(QIODevice::ReadOnly)) {
            QMimeDatabase mimeDb;
            QString mimeType = mimeDb.mimeTypeForFile(fileInfo).name();
            
            QString header = QString(
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: %1\r\n"
                "Content-Length: %2\r\n"
                "Accept-Ranges: bytes\r\n\r\n"
            ).arg(mimeType).arg(file.size());
            
            socket->write(header.toUtf8());
            socket->write(file.readAll());
            file.close();
        }
    }
    
    socket->disconnectFromHost();
}

QString NetworkDiscoveryManager::generateMediaShareResponse(const QString& shareId) const
{
    // Generate simple HTML directory listing
    QString html = QString(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<html><head><title>EonPlay Media Share</title></head>"
        "<body><h1>Media Share: %1</h1>"
        "<p>Media files available for streaming</p>"
        "</body></html>"
    ).arg(shareId);
    
    return html;
}

// Timer callbacks
void NetworkDiscoveryManager::onDiscoveryTimerTimeout()
{
    if (m_discoveryState == DISCOVERY_ACTIVE) {
        sendUPnPSearchRequest();
        
        if (isBluetoothAvailable() && !m_bluetoothDiscovery->isActive()) {
            startBluetoothDiscovery();
        }
    }
}

void NetworkDiscoveryManager::onDeviceTimeoutTimerTimeout()
{
    checkDeviceTimeouts();
}

// Getters
QList<NetworkDiscoveryManager::NetworkDevice> NetworkDiscoveryManager::getDiscoveredDevices() const
{
    return m_discoveredDevices.values();
}

QList<NetworkDiscoveryManager::NetworkDevice> NetworkDiscoveryManager::getDevicesByType(DeviceType type) const
{
    QList<NetworkDevice> devices;
    for (const auto& device : m_discoveredDevices.values()) {
        if (device.type == type) {
            devices.append(device);
        }
    }
    return devices;
}

NetworkDiscoveryManager::NetworkDevice NetworkDiscoveryManager::getDevice(const QString& deviceId) const
{
    return m_discoveredDevices.value(deviceId);
}

bool NetworkDiscoveryManager::isDeviceOnline(const QString& deviceId) const
{
    return m_discoveredDevices.contains(deviceId) && m_discoveredDevices[deviceId].isOnline;
}

// Bluetooth device getter - temporarily disabled for build compatibility
/*
QList<NetworkDiscoveryManager::NetworkDevice> NetworkDiscoveryManager::getBluetoothDevices() const
{
    return getDevicesByType(BLUETOOTH_AUDIO_DEVICE);
}
*/

QList<NetworkDiscoveryManager::MediaShare> NetworkDiscoveryManager::getActiveShares() const
{
    QList<MediaShare> activeShares;
    for (const auto& share : m_mediaShares.values()) {
        if (share.isActive) {
            activeShares.append(share);
        }
    }
    return activeShares;
}

NetworkDiscoveryManager::MediaShare NetworkDiscoveryManager::getMediaShare(const QString& shareId) const
{
    return m_mediaShares.value(shareId);
}

bool NetworkDiscoveryManager::isDiscoveryActive() const
{
    return m_discoveryState == DISCOVERY_ACTIVE;
}

NetworkDiscoveryManager::DiscoveryState NetworkDiscoveryManager::discoveryState() const
{
    return m_discoveryState;
}

// Configuration
void NetworkDiscoveryManager::setDiscoveryInterval(int intervalMs)
{
    m_discoveryInterval = intervalMs;
    m_discoveryTimer->setInterval(intervalMs);
}

void NetworkDiscoveryManager::setDiscoveryTimeout(int timeoutMs)
{
    m_discoveryTimeout = timeoutMs;
}

void NetworkDiscoveryManager::setMediaSharePort(int port)
{
    m_mediaSharePort = port;
}

void NetworkDiscoveryManager::setMaxConnections(int maxConnections)
{
    m_maxConnections = maxConnections;
}

void NetworkDiscoveryManager::enableAutoDiscovery(bool enabled)
{
    m_autoDiscoveryEnabled = enabled;
    
    if (enabled && m_discoveryState == DISCOVERY_ACTIVE) {
        m_discoveryTimer->start();
    } else {
        m_discoveryTimer->stop();
    }
}

// Synchronization (simplified implementation)
QString NetworkDiscoveryManager::createSyncGroup(const QString& name)
{
    QString groupId = QString("group_%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(qrand());
    
    SyncGroup group;
    group.groupId = groupId;
    group.name = name;
    
    m_syncGroups[groupId] = group;
    emit syncGroupCreated(groupId);
    
    qCDebug(networkDiscovery) << "Sync group created:" << name << "(" << groupId << ")";
    return groupId;
}

bool NetworkDiscoveryManager::joinSyncGroup(const QString& groupId)
{
    if (!m_syncGroups.contains(groupId)) return false;
    
    m_currentSyncGroup = groupId;
    emit syncGroupJoined(groupId);
    
    qCDebug(networkDiscovery) << "Joined sync group:" << groupId;
    return true;
}

QList<NetworkDiscoveryManager::SyncGroup> NetworkDiscoveryManager::getSyncGroups() const
{
    return m_syncGroups.values();
}