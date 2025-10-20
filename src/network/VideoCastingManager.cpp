#include "network/VideoCastingManager.h"
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QUuid>
#include <QCryptographicHash>
#ifdef HAVE_QT_WEBSOCKETS
#include <QWebSocketServer>
#include <QWebSocket>
#endif
#include <QtMath>

Q_DECLARE_LOGGING_CATEGORY(videoCasting)
Q_LOGGING_CATEGORY(videoCasting, "network.casting")

VideoCastingManager::VideoCastingManager(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_udpSocket(new QUdpSocket(this))
    , m_tcpServer(new QTcpServer(this))
#ifdef HAVE_QT_WEBSOCKETS
    , m_webSocketServer(nullptr)
#endif
    , m_discovering(false)
    , m_discoveryTimer(new QTimer(this))
    , m_casting(false)
    , m_sessionTimer(new QTimer(this))
    , m_webControlRunning(false)
    , m_chromecastAppId(DEFAULT_CHROMECAST_APP_ID)
{
    // Setup discovery timer
    m_discoveryTimer->setSingleShot(true);
    connect(m_discoveryTimer, &QTimer::timeout, this, &VideoCastingManager::onDiscoveryTimeout);
    
    // Setup session timer
    m_sessionTimer->setInterval(SESSION_UPDATE_INTERVAL);
    connect(m_sessionTimer, &QTimer::timeout, this, &VideoCastingManager::updateSessionStatus);
    
    // Setup UDP socket for SSDP discovery
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &VideoCastingManager::onUdpDataReceived);
    
    qCDebug(videoCasting) << "VideoCastingManager created";
}

VideoCastingManager::~VideoCastingManager()
{
    shutdown();
    qCDebug(videoCasting) << "VideoCastingManager destroyed";
}

bool VideoCastingManager::initialize()
{
    // Bind UDP socket for SSDP
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, SSDP_PORT, QUdpSocket::ShareAddress)) {
        qCWarning(videoCasting) << "Failed to bind UDP socket for SSDP";
        return false;
    }
    
    // Join multicast group
    if (!m_udpSocket->joinMulticastGroup(QHostAddress(SSDP_ADDRESS))) {
        qCWarning(videoCasting) << "Failed to join SSDP multicast group";
    }
    
    qCDebug(videoCasting) << "VideoCastingManager initialized";
    return true;
}

void VideoCastingManager::shutdown()
{
    stopCasting();
    stopDeviceDiscovery();
    stopWebControlServer();
    
    if (m_udpSocket->state() == QAbstractSocket::BoundState) {
        m_udpSocket->leaveMulticastGroup(QHostAddress(SSDP_ADDRESS));
        m_udpSocket->close();
    }
    
    qCDebug(videoCasting) << "VideoCastingManager shutdown";
}

void VideoCastingManager::startDeviceDiscovery(int timeout)
{
    if (m_discovering) {
        qCDebug(videoCasting) << "Discovery already in progress";
        return;
    }
    
    QMutexLocker locker(&m_deviceMutex);
    m_discovering = true;
    m_discoveredDevices.clear();
    
    // Start discovery timer
    m_discoveryTimer->start(timeout);
    
    // Start different discovery methods
    discoverDLNADevices();
    discoverChromecastDevices();
    discoverAirPlayDevices();
    
    emit discoveryStarted();
    qCDebug(videoCasting) << "Device discovery started with timeout:" << timeout << "ms";
}

void VideoCastingManager::stopDeviceDiscovery()
{
    if (!m_discovering) {
        return;
    }
    
    QMutexLocker locker(&m_deviceMutex);
    m_discovering = false;
    m_discoveryTimer->stop();
    
    emit discoveryStopped();
    qCDebug(videoCasting) << "Device discovery stopped";
}

bool VideoCastingManager::isDiscovering() const
{
    QMutexLocker locker(&m_deviceMutex);
    return m_discovering;
}

QVector<VideoCastingManager::CastingDevice> VideoCastingManager::getDiscoveredDevices() const
{
    QMutexLocker locker(&m_deviceMutex);
    return m_discoveredDevices;
}

void VideoCastingManager::refreshDevices()
{
    if (m_discovering) {
        stopDeviceDiscovery();
    }
    startDeviceDiscovery();
}

bool VideoCastingManager::startCasting(const CastingDevice& device, const QUrl& mediaUrl, const QString& mediaTitle)
{
    if (m_casting) {
        qCWarning(videoCasting) << "Already casting to another device";
        return false;
    }
    
    bool success = false;
    
    switch (device.type) {
        case DLNA:
        case UPnP:
            success = startDLNACasting(device, mediaUrl, mediaTitle);
            break;
        case Chromecast:
            success = startChromecastCasting(device, mediaUrl, mediaTitle);
            break;
        default:
            qCWarning(videoCasting) << "Unsupported device type:" << device.type;
            break;
    }
    
    if (success) {
        QMutexLocker locker(&m_sessionMutex);
        m_casting = true;
        m_currentSession.sessionId = generateSessionId();
        m_currentSession.device = device;
        m_currentSession.mediaUrl = mediaUrl.toString();
        m_currentSession.mediaTitle = mediaTitle;
        m_currentSession.isPlaying = true;
        
        m_sessionTimer->start();
        emit castingStarted(m_currentSession);
        
        qCDebug(videoCasting) << "Casting started to device:" << device.name;
    }
    
    return success;
}