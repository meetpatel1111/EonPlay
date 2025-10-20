#include "platform/LinuxPlatform.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QLoggingCategory>
#include <QCoreApplication>
#include <QXmlStreamWriter>
#include <QTextStream>

Q_LOGGING_CATEGORY(linuxPlatform, "eonplay.platform.linux")

// Constants
const QString LinuxPlatform::MPRIS_SERVICE_NAME = "org.mpris.MediaPlayer2.eonplay";
const QString LinuxPlatform::MPRIS_OBJECT_PATH = "/org/mpris/MediaPlayer2";
const QString LinuxPlatform::MPRIS_INTERFACE = "org.mpris.MediaPlayer2";
const QString LinuxPlatform::MPRIS_PLAYER_INTERFACE = "org.mpris.MediaPlayer2.Player";
const QString LinuxPlatform::DESKTOP_FILE_NAME = "eonplay.desktop";
const QString LinuxPlatform::MIME_TYPES_DIR = "mime/packages";

LinuxPlatform::LinuxPlatform(QObject *parent)
    : QObject(parent)
    , m_mprisActive(false)
    , m_vaApiEnabled(false)
    , m_vdpauEnabled(false)
    , m_vaApiSupported(false)
    , m_vdpauSupported(false)
    , m_mprisInterface(nullptr)
    , m_dbusConnection(QDBusConnection::sessionBus())
    , m_currentPosition(0)
    , m_desktopEnvironment(DE_UNKNOWN)
    , m_canPlay(true)
    , m_canPause(true)
    , m_canSeek(true)
    , m_canGoNext(true)
    , m_canGoPrevious(true)
{
    // Detect desktop environment
    m_desktopEnvironment = detectDesktopEnvironment();
    
    // Check hardware acceleration support
    m_vaApiSupported = checkVAAPISupport();
    m_vdpauSupported = checkVDPAUSupport();
    
    qCDebug(linuxPlatform) << "LinuxPlatform initialized - DE:" << getDesktopEnvironmentName()
                           << "VA-API:" << m_vaApiSupported << "VDPAU:" << m_vdpauSupported;
}

LinuxPlatform::~LinuxPlatform()
{
    shutdownMPRIS();
}

// MPRIS D-Bus interface implementation
bool LinuxPlatform::initializeMPRIS()
{
    if (m_mprisActive) {
        return true;
    }
    
    if (!m_dbusConnection.isConnected()) {
        qCWarning(linuxPlatform) << "D-Bus session bus not available";
        return false;
    }
    
    // Register service
    if (!m_dbusConnection.registerService(MPRIS_SERVICE_NAME)) {
        qCWarning(linuxPlatform) << "Failed to register MPRIS service";
        return false;
    }
    
    setupMPRISInterface();
    m_mprisActive = true;
    
    qCDebug(linuxPlatform) << "MPRIS interface initialized";
    return true;
}

void LinuxPlatform::shutdownMPRIS()
{
    if (!m_mprisActive) return;
    
    if (m_mprisInterface) {
        delete m_mprisInterface;
        m_mprisInterface = nullptr;
    }
    
    m_dbusConnection.unregisterService(MPRIS_SERVICE_NAME);
    m_mprisActive = false;
    
    qCDebug(linuxPlatform) << "MPRIS interface shutdown";
}

bool LinuxPlatform::isMPRISActive() const
{
    return m_mprisActive;
}

void LinuxPlatform::updateMPRISMetadata(const MPRISMetadata& metadata)
{
    m_currentMetadata = metadata;
    
    if (m_mprisActive && m_mprisInterface) {
        QVariantMap metadataMap = getMPRISMetadata();
        
        // Send D-Bus property change signal
        QDBusMessage signal = QDBusMessage::createSignal(
            MPRIS_OBJECT_PATH,
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged"
        );
        
        signal << MPRIS_PLAYER_INTERFACE;
        signal << QVariantMap{{"Metadata", metadataMap}};
        signal << QStringList();
        
        m_dbusConnection.send(signal);
    }
}

void LinuxPlatform::updateMPRISPlaybackStatus(const QString& status)
{
    m_playbackStatus = status;
    
    if (m_mprisActive && m_mprisInterface) {
        QDBusMessage signal = QDBusMessage::createSignal(
            MPRIS_OBJECT_PATH,
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged"
        );
        
        signal << MPRIS_PLAYER_INTERFACE;
        signal << QVariantMap{{"PlaybackStatus", status}};
        signal << QStringList();
        
        m_dbusConnection.send(signal);
    }
}

void LinuxPlatform::updateMPRISPosition(qint64 position)
{
    m_currentPosition = position;
    
    if (m_mprisActive) {
        emit mprisPositionChanged(position);
    }
}

QVariantMap LinuxPlatform::getMPRISMetadata() const
{
    QVariantMap metadata;
    
    if (!m_currentMetadata.trackId.isEmpty()) {
        metadata["mpris:trackid"] = QDBusObjectPath(m_currentMetadata.trackId);
    }
    if (!m_currentMetadata.title.isEmpty()) {
        metadata["xesam:title"] = m_currentMetadata.title;
    }
    if (!m_currentMetadata.artist.isEmpty()) {
        metadata["xesam:artist"] = QStringList() << m_currentMetadata.artist;
    }
    if (!m_currentMetadata.album.isEmpty()) {
        metadata["xesam:album"] = m_currentMetadata.album;
    }
    if (!m_currentMetadata.albumArt.isEmpty()) {
        metadata["mpris:artUrl"] = m_currentMetadata.albumArt;
    }
    if (m_currentMetadata.length > 0) {
        metadata["mpris:length"] = m_currentMetadata.length;
    }
    if (!m_currentMetadata.genre.isEmpty()) {
        metadata["xesam:genre"] = m_currentMetadata.genre;
    }
    if (m_currentMetadata.trackNumber > 0) {
        metadata["xesam:trackNumber"] = m_currentMetadata.trackNumber;
    }
    
    return metadata;
}

// XDG MIME type registration
bool LinuxPlatform::registerMimeTypes(const QList<MimeTypeAssociation>& associations)
{
    bool allSuccess = true;
    
    for (const MimeTypeAssociation& assoc : associations) {
        QString xmlContent = generateMimeTypeXML(assoc);
        
        if (!installMimeTypeFile(assoc.mimeType, xmlContent)) {
            allSuccess = false;
            continue;
        }
        
        m_registeredMimeTypes.append(assoc);
        emit mimeTypeRegistered(assoc.mimeType);
    }
    
    // Update MIME database
    if (allSuccess) {
        updateMimeDatabase();
        updateDesktopDatabase();
    }
    
    qCDebug(linuxPlatform) << "MIME types registered:" << associations.size() << "Success:" << allSuccess;
    return allSuccess;
}

QString LinuxPlatform::generateMimeTypeXML(const MimeTypeAssociation& assoc) const
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    
    writer.writeStartElement("mime-info");
    writer.writeAttribute("xmlns", "http://www.freedesktop.org/standards/shared-mime-info");
    
    writer.writeStartElement("mime-type");
    writer.writeAttribute("type", assoc.mimeType);
    
    writer.writeStartElement("comment");
    writer.writeCharacters(assoc.description);
    writer.writeEndElement(); // comment
    
    for (const QString& ext : assoc.extensions) {
        writer.writeStartElement("glob");
        writer.writeAttribute("pattern", "*." + ext);
        writer.writeEndElement(); // glob
    }
    
    if (!assoc.iconName.isEmpty()) {
        writer.writeStartElement("icon");
        writer.writeAttribute("name", assoc.iconName);
        writer.writeEndElement(); // icon
    }
    
    writer.writeEndElement(); // mime-type
    writer.writeEndElement(); // mime-info
    writer.writeEndDocument();
    
    return xml;
}

bool LinuxPlatform::installMimeTypeFile(const QString& mimeType, const QString& xmlContent)
{
    QString mimeDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/" + MIME_TYPES_DIR;
    QDir().mkpath(mimeDir);
    
    QString fileName = QString("eonplay-%1.xml").arg(mimeType.split('/').last());
    QString filePath = mimeDir + "/" + fileName;
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(linuxPlatform) << "Failed to create MIME type file:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out << xmlContent;
    file.close();
    
    return true;
}

bool LinuxPlatform::updateMimeDatabase()
{
    QProcess process;
    process.start("update-mime-database", QStringList() << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/mime");
    process.waitForFinished(10000);
    
    return process.exitCode() == 0;
}

bool LinuxPlatform::updateDesktopDatabase()
{
    QProcess process;
    process.start("update-desktop-database", QStringList() << QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation));
    process.waitForFinished(10000);
    
    return process.exitCode() == 0;
}

// Desktop integration
bool LinuxPlatform::installDesktopEntry(const DesktopEntry& entry)
{
    QString desktopContent = generateDesktopFile(entry);
    
    if (installDesktopFile(desktopContent)) {
        m_installedDesktopEntry = entry;
        emit desktopEntryInstalled();
        qCDebug(linuxPlatform) << "Desktop entry installed";
        return true;
    }
    
    return false;
}

QString LinuxPlatform::generateDesktopFile(const DesktopEntry& entry) const
{
    QString content;
    QTextStream stream(&content);
    
    stream << "[Desktop Entry]\n";
    stream << "Version=1.0\n";
    stream << "Type=Application\n";
    stream << "Name=" << entry.name << "\n";
    
    if (!entry.comment.isEmpty()) {
        stream << "Comment=" << entry.comment << "\n";
    }
    
    stream << "Exec=" << entry.exec << "\n";
    
    if (!entry.icon.isEmpty()) {
        stream << "Icon=" << entry.icon << "\n";
    }
    
    if (!entry.categories.isEmpty()) {
        stream << "Categories=" << entry.categories.join(";") << ";\n";
    }
    
    if (!entry.mimeTypes.isEmpty()) {
        stream << "MimeType=" << entry.mimeTypes.join(";") << ";\n";
    }
    
    stream << "Terminal=" << (entry.terminal ? "true" : "false") << "\n";
    stream << "NoDisplay=" << (entry.noDisplay ? "true" : "false") << "\n";
    
    if (!entry.startupWMClass.isEmpty()) {
        stream << "StartupWMClass=" << entry.startupWMClass << "\n";
    }
    
    stream << "StartupNotify=true\n";
    
    return content;
}

bool LinuxPlatform::installDesktopFile(const QString& content)
{
    QString desktopPath = getDesktopFilePath();
    QDir().mkpath(QFileInfo(desktopPath).absolutePath());
    
    QFile file(desktopPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(linuxPlatform) << "Failed to create desktop file:" << desktopPath;
        return false;
    }
    
    QTextStream out(&file);
    out << content;
    file.close();
    
    // Make executable
    QFile::setPermissions(desktopPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                      QFile::ReadGroup | QFile::ReadOther);
    
    return true;
}

QString LinuxPlatform::getDesktopFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/" + DESKTOP_FILE_NAME;
}

LinuxPlatform::DesktopEnvironment LinuxPlatform::detectDesktopEnvironment() const
{
    QString desktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
    QString session = qgetenv("DESKTOP_SESSION").toLower();
    
    if (desktop.contains("gnome") || session.contains("gnome")) {
        return DE_GNOME;
    } else if (desktop.contains("kde") || session.contains("kde")) {
        return DE_KDE;
    } else if (desktop.contains("xfce") || session.contains("xfce")) {
        return DE_XFCE;
    } else if (desktop.contains("unity") || session.contains("unity")) {
        return DE_UNITY;
    } else if (desktop.contains("mate") || session.contains("mate")) {
        return DE_MATE;
    } else if (desktop.contains("cinnamon") || session.contains("cinnamon")) {
        return DE_CINNAMON;
    }
    
    return DE_UNKNOWN;
}

QString LinuxPlatform::getDesktopEnvironmentName() const
{
    switch (m_desktopEnvironment) {
        case DE_GNOME: return "GNOME";
        case DE_KDE: return "KDE";
        case DE_XFCE: return "XFCE";
        case DE_UNITY: return "Unity";
        case DE_MATE: return "MATE";
        case DE_CINNAMON: return "Cinnamon";
        default: return "Unknown";
    }
}

// Hardware acceleration support
bool LinuxPlatform::checkVAAPISupport()
{
    // Check for VA-API library and devices
    QProcess process;
    process.start("vainfo");
    process.waitForFinished(5000);
    
    return process.exitCode() == 0;
}

bool LinuxPlatform::checkVDPAUSupport()
{
    // Check for VDPAU library and devices
    QProcess process;
    process.start("vdpauinfo");
    process.waitForFinished(5000);
    
    return process.exitCode() == 0;
}

bool LinuxPlatform::isVAAPISupported() const
{
    return m_vaApiSupported;
}

bool LinuxPlatform::isVDPAUSupported() const
{
    return m_vdpauSupported;
}

bool LinuxPlatform::enableVAAPI(bool enabled)
{
    if (enabled && !m_vaApiSupported) {
        qCWarning(linuxPlatform) << "VA-API not supported on this system";
        return false;
    }
    
    m_vaApiEnabled = enabled;
    emit hardwareAccelerationChanged("VA-API", enabled);
    
    qCDebug(linuxPlatform) << "VA-API" << (enabled ? "enabled" : "disabled");
    return true;
}

bool LinuxPlatform::enableVDPAU(bool enabled)
{
    if (enabled && !m_vdpauSupported) {
        qCWarning(linuxPlatform) << "VDPAU not supported on this system";
        return false;
    }
    
    m_vdpauEnabled = enabled;
    emit hardwareAccelerationChanged("VDPAU", enabled);
    
    qCDebug(linuxPlatform) << "VDPAU" << (enabled ? "enabled" : "disabled");
    return true;
}

QStringList LinuxPlatform::getAvailableVAAPIDevices() const
{
    QStringList devices;
    
    if (m_vaApiSupported) {
        QProcess process;
        process.start("vainfo");
        process.waitForFinished(5000);
        
        QString output = process.readAllStandardOutput();
        // Parse vainfo output for available devices
        // This is a simplified implementation
        if (output.contains("VA-API version")) {
            devices << "/dev/dri/renderD128";
        }
    }
    
    return devices;
}

QStringList LinuxPlatform::getAvailableVDPAUDevices() const
{
    QStringList devices;
    
    if (m_vdpauSupported) {
        QProcess process;
        process.start("vdpauinfo");
        process.waitForFinished(5000);
        
        QString output = process.readAllStandardOutput();
        // Parse vdpauinfo output for available devices
        // This is a simplified implementation
        if (output.contains("VDPAU")) {
            devices << "nvidia";
        }
    }
    
    return devices;
}

QString LinuxPlatform::getHardwareAccelerationStatus() const
{
    QStringList status;
    
    if (m_vaApiSupported) {
        status << QString("VA-API: %1").arg(m_vaApiEnabled ? "Enabled" : "Available");
    }
    
    if (m_vdpauSupported) {
        status << QString("VDPAU: %1").arg(m_vdpauEnabled ? "Enabled" : "Available");
    }
    
    if (status.isEmpty()) {
        return "No hardware acceleration available";
    }
    
    return status.join(", ");
}

// System integration utilities
bool LinuxPlatform::addToAutostart(bool enabled)
{
    QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    QString autostartFile = autostartDir + "/" + DESKTOP_FILE_NAME;
    
    if (enabled) {
        QDir().mkpath(autostartDir);
        
        DesktopEntry entry;
        entry.name = "EonPlay";
        entry.comment = "Timeless, futuristic media player";
        entry.exec = QCoreApplication::applicationFilePath();
        entry.icon = "eonplay";
        entry.categories = QStringList() << "AudioVideo" << "Player";
        entry.noDisplay = true; // Hidden autostart
        
        QString content = generateDesktopFile(entry);
        
        QFile file(autostartFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << content;
            file.close();
            return true;
        }
    } else {
        return QFile::remove(autostartFile);
    }
    
    return false;
}

bool LinuxPlatform::isInAutostart() const
{
    QString autostartFile = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/" + DESKTOP_FILE_NAME;
    return QFile::exists(autostartFile);
}

QString LinuxPlatform::getConfigDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/eonplay";
}

QString LinuxPlatform::getDataDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

QString LinuxPlatform::getCacheDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

// System information
QString LinuxPlatform::getLinuxDistribution() const
{
    QString distro = readFileContent("/etc/os-release");
    
    if (distro.contains("PRETTY_NAME=")) {
        QRegularExpression regex("PRETTY_NAME=\"([^\"]+)\"");
        QRegularExpressionMatch match = regex.match(distro);
        if (match.hasMatch()) {
            return match.captured(1);
        }
    }
    
    return "Unknown Linux Distribution";
}

QString LinuxPlatform::getKernelVersion() const
{
    return executeCommand("uname -r").trimmed();
}

QString LinuxPlatform::getDesktopSession() const
{
    return qgetenv("XDG_SESSION_DESKTOP");
}

QString LinuxPlatform::getDisplayServer() const
{
    QString waylandDisplay = qgetenv("WAYLAND_DISPLAY");
    QString x11Display = qgetenv("DISPLAY");
    
    if (!waylandDisplay.isEmpty()) {
        return "Wayland";
    } else if (!x11Display.isEmpty()) {
        return "X11";
    }
    
    return "Unknown";
}

// Utility functions
QString LinuxPlatform::readFileContent(const QString& filePath) const
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return file.readAll();
    }
    return QString();
}

QString LinuxPlatform::executeCommand(const QString& command) const
{
    QProcess process;
    process.start("sh", QStringList() << "-c" << command);
    process.waitForFinished(5000);
    
    if (process.exitCode() == 0) {
        return process.readAllStandardOutput();
    }
    
    return QString();
}

bool LinuxPlatform::commandExists(const QString& command) const
{
    QProcess process;
    process.start("which", QStringList() << command);
    process.waitForFinished(3000);
    
    return process.exitCode() == 0;
}