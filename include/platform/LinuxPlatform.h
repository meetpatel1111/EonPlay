#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusInterface>
#endif

/**
 * @brief Linux-specific platform integration and system features
 * 
 * Provides MPRIS D-Bus interface, XDG MIME type registration, VA-API/VDPAU
 * hardware acceleration, desktop integration, and package generation for EonPlay.
 */
class LinuxPlatform : public QObject
{
    Q_OBJECT

public:
    enum PackageType {
        PACKAGE_APPIMAGE,
        PACKAGE_DEB,
        PACKAGE_RPM,
        PACKAGE_FLATPAK,
        PACKAGE_SNAP
    };

    enum DesktopEnvironment {
        DE_GNOME,
        DE_KDE,
        DE_XFCE,
        DE_UNITY,
        DE_MATE,
        DE_CINNAMON,
        DE_UNKNOWN
    };

    struct MimeTypeAssociation {
        QString mimeType;
        QString description;
        QStringList extensions;
        QString iconName;
        bool isDefault = false;
    };

    struct DesktopEntry {
        QString name;
        QString comment;
        QString exec;
        QString icon;
        QStringList categories;
        QStringList mimeTypes;
        bool terminal = false;
        bool noDisplay = false;
        QString startupWMClass;
    };

    struct MPRISMetadata {
        QString trackId;
        QString title;
        QString artist;
        QString album;
        QString albumArt;
        qint64 length = 0; // microseconds
        QStringList genre;
        int trackNumber = 0;
        QDateTime lastUsed;
    };

    explicit LinuxPlatform(QObject *parent = nullptr);
    ~LinuxPlatform();

    // MPRIS D-Bus interface
    bool initializeMPRIS();
    void shutdownMPRIS();
    bool isMPRISActive() const;
    void updateMPRISMetadata(const MPRISMetadata& metadata);
    void updateMPRISPlaybackStatus(const QString& status); // Playing, Paused, Stopped
    void updateMPRISPosition(qint64 position); // microseconds
    void setMPRISCanControl(bool canPlay, bool canPause, bool canSeek, bool canGoNext, bool canGoPrevious);

    // XDG MIME type registration
    bool registerMimeTypes(const QList<MimeTypeAssociation>& associations);
    bool unregisterMimeTypes(const QStringList& mimeTypes);
    bool isMimeTypeRegistered(const QString& mimeType) const;
    QStringList getRegisteredMimeTypes() const;
    bool setAsDefaultApplication(const QStringList& mimeTypes);

    // Desktop integration
    bool installDesktopEntry(const DesktopEntry& entry);
    bool uninstallDesktopEntry();
    bool isDesktopEntryInstalled() const;
    DesktopEnvironment detectDesktopEnvironment() const;
    QString getDesktopEnvironmentName() const;

    // Hardware acceleration (VA-API/VDPAU)
    bool isVAAPISupported() const;
    bool isVDPAUSupported() const;
    bool enableVAAPI(bool enabled);
    bool enableVDPAU(bool enabled);
    bool isVAAPIEnabled() const;
    bool isVDPAUEnabled() const;
    QStringList getAvailableVAAPIDevices() const;
    QStringList getAvailableVDPAUDevices() const;
    QString getHardwareAccelerationStatus() const;

    // Package generation
    bool generateAppImage(const QString& outputPath, const QMap<QString, QString>& properties);
    bool generateDebPackage(const QString& outputPath, const QMap<QString, QString>& properties);
    bool generateRpmPackage(const QString& outputPath, const QMap<QString, QString>& properties);
    bool generateFlatpak(const QString& outputPath, const QMap<QString, QString>& properties);
    QString getPackageTemplate(PackageType type) const;

    // GPG package signing
    bool signPackage(const QString& packagePath, const QString& keyId);
    bool verifyPackageSignature(const QString& packagePath) const;
    QString getSignatureInfo(const QString& packagePath) const;
    bool hasGPGKey(const QString& keyId) const;

    // System integration
    bool addToAutostart(bool enabled);
    bool isInAutostart() const;
    QString getConfigDirectory() const;
    QString getDataDirectory() const;
    QString getCacheDirectory() const;
    QStringList getSystemThemes() const;
    QString getCurrentTheme() const;

    // Notifications
    bool showDesktopNotification(const QString& title, const QString& message, 
                                const QString& iconName = QString(), int timeout = 5000);
    bool isNotificationSupported() const;

    // System information
    QString getLinuxDistribution() const;
    QString getKernelVersion() const;
    QString getDesktopSession() const;
    QString getDisplayServer() const; // X11, Wayland
    QStringList getInstalledPackageManagers() const;

signals:
    void mprisCommandReceived(const QString& command);
    void mprisSeekRequested(qint64 offset);
    void mprisPositionChanged(qint64 position);
    void mimeTypeRegistered(const QString& mimeType);
    void mimeTypeUnregistered(const QString& mimeType);
    void desktopEntryInstalled();
    void desktopEntryUninstalled();
    void hardwareAccelerationChanged(const QString& type, bool enabled);
    void packageGenerationProgress(int percentage);
    void packageGenerationCompleted(const QString& filePath);
    void packageSigningCompleted(bool success);

private slots:
    void onMPRISMethodCall(const QString& method, const QVariantList& arguments);
    void onMPRISPropertyChanged(const QString& property, const QVariant& value);

private:
    // MPRIS helpers
    void setupMPRISInterface();
    void registerMPRISService();
    void unregisterMPRISService();
    void handleMPRISCommand(const QString& command);
    QVariantMap getMPRISMetadata() const;
    
    // MIME type helpers
    QString generateMimeTypeXML(const MimeTypeAssociation& assoc) const;
    bool installMimeTypeFile(const QString& mimeType, const QString& xmlContent);
    bool updateMimeDatabase();
    bool updateDesktopDatabase();
    
    // Desktop entry helpers
    QString generateDesktopFile(const DesktopEntry& entry) const;
    QString getDesktopFilePath() const;
    bool installDesktopFile(const QString& content);
    
    // Hardware acceleration helpers
    bool checkVAAPISupport();
    bool checkVDPAUSupport();
    QStringList scanVAAPIDevices() const;
    QStringList scanVDPAUDevices() const;
    
    // Package generation helpers
    QString prepareAppImageStructure(const QMap<QString, QString>& properties) const;
    QString prepareDebianControl(const QMap<QString, QString>& properties) const;
    QString prepareRpmSpec(const QMap<QString, QString>& properties) const;
    QString prepareFlatpakManifest(const QMap<QString, QString>& properties) const;
    bool buildPackage(PackageType type, const QString& sourcePath, const QString& outputPath);
    
    // GPG helpers
    bool signWithGPG(const QString& filePath, const QString& keyId);
    bool verifyGPGSignature(const QString& filePath) const;
    
    // System detection helpers
    QString readFileContent(const QString& filePath) const;
    QString executeCommand(const QString& command) const;
    bool commandExists(const QString& command) const;
    
    // Member variables
    bool m_mprisActive;
    bool m_vaApiEnabled;
    bool m_vdpauEnabled;
    bool m_vaApiSupported;
    bool m_vdpauSupported;
    
    QDBusInterface* m_mprisInterface;
    QDBusConnection m_dbusConnection;
    
    MPRISMetadata m_currentMetadata;
    QString m_playbackStatus;
    qint64 m_currentPosition;
    
    QList<MimeTypeAssociation> m_registeredMimeTypes;
    DesktopEntry m_installedDesktopEntry;
    DesktopEnvironment m_desktopEnvironment;
    
    // Capabilities
    bool m_canPlay;
    bool m_canPause;
    bool m_canSeek;
    bool m_canGoNext;
    bool m_canGoPrevious;
    
    // Constants
    static const QString MPRIS_SERVICE_NAME;
    static const QString MPRIS_OBJECT_PATH;
    static const QString MPRIS_INTERFACE;
    static const QString MPRIS_PLAYER_INTERFACE;
    static const QString DESKTOP_FILE_NAME;
    static const QString MIME_TYPES_DIR;
};