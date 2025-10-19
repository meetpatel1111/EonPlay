#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winreg.h>
#endif

/**
 * @brief Windows-specific platform integration and system features
 * 
 * Provides Windows system tray integration, media key support, file associations,
 * DXVA hardware acceleration, and installer package generation for EonPlay.
 */
class WindowsPlatform : public QObject
{
    Q_OBJECT

public:
    enum InstallerType {
        INSTALLER_MSI,
        INSTALLER_EXE,
        INSTALLER_PORTABLE
    };

    enum RegistryScope {
        REGISTRY_CURRENT_USER,
        REGISTRY_LOCAL_MACHINE
    };

    struct FileAssociation {
        QString extension;
        QString description;
        QString iconPath;
        QString commandLine;
        bool isDefault = false;
    };

    struct SystemTrayConfig {
        QString iconPath;
        QString tooltip;
        QStringList menuItems;
        bool showNotifications = true;
        bool minimizeToTray = true;
        bool closeToTray = false;
    };

    explicit WindowsPlatform(QObject *parent = nullptr);
    ~WindowsPlatform();

    // System tray integration
    bool createSystemTray(const SystemTrayConfig& config);
    void updateSystemTrayIcon(const QString& iconPath);
    void updateSystemTrayTooltip(const QString& tooltip);
    void showSystemTrayMessage(const QString& title, const QString& message, int timeout = 5000);
    void hideSystemTray();
    bool isSystemTrayAvailable() const;

    // Media key support
    bool registerMediaKeys();
    void unregisterMediaKeys();
    bool isMediaKeysRegistered() const;
    void enableMediaKeyHandling(bool enabled);

    // File associations
    bool registerFileAssociations(const QList<FileAssociation>& associations);
    bool unregisterFileAssociations(const QStringList& extensions);
    bool isFileAssociationRegistered(const QString& extension) const;
    QStringList getRegisteredAssociations() const;
    bool setAsDefaultPlayer(const QStringList& extensions);

    // Hardware acceleration (DXVA)
    bool isDXVASupported() const;
    bool enableDXVA(bool enabled);
    bool isDXVAEnabled() const;
    QStringList getAvailableDecoders() const;
    QString getDXVAStatus() const;

    // Registry operations
    bool writeRegistryValue(const QString& keyPath, const QString& valueName, 
                           const QVariant& value, RegistryScope scope = REGISTRY_CURRENT_USER);
    QVariant readRegistryValue(const QString& keyPath, const QString& valueName, 
                              RegistryScope scope = REGISTRY_CURRENT_USER) const;
    bool deleteRegistryKey(const QString& keyPath, RegistryScope scope = REGISTRY_CURRENT_USER);
    bool registryKeyExists(const QString& keyPath, RegistryScope scope = REGISTRY_CURRENT_USER) const;

    // Installer generation
    bool generateMSIInstaller(const QString& outputPath, const QMap<QString, QString>& properties);
    bool generateEXEInstaller(const QString& outputPath, const QMap<QString, QString>& properties);
    bool generatePortablePackage(const QString& outputPath);
    QString getInstallerTemplate(InstallerType type) const;

    // Code signing
    bool signExecutable(const QString& filePath, const QString& certificatePath, 
                       const QString& password = QString());
    bool verifySignature(const QString& filePath) const;
    QString getSignatureInfo(const QString& filePath) const;

    // Windows-specific utilities
    QString getWindowsVersion() const;
    QString getSystemArchitecture() const;
    bool isRunningAsAdmin() const;
    bool requestAdminPrivileges();
    QString getSpecialFolder(int csidl) const;
    bool createShortcut(const QString& targetPath, const QString& shortcutPath, 
                       const QString& description = QString());

    // Startup and autorun
    bool addToStartup(bool enabled);
    bool isInStartup() const;
    bool setAutorun(bool enabled);
    bool isAutorunEnabled() const;

    // Windows notifications
    void showToastNotification(const QString& title, const QString& message, 
                              const QString& iconPath = QString());
    bool isToastNotificationSupported() const;

signals:
    void systemTrayActivated();
    void systemTrayMessageClicked();
    void mediaKeyPressed(const QString& key);
    void fileAssociationChanged(const QString& extension, bool registered);
    void dxvaStateChanged(bool enabled);
    void installerGenerationProgress(int percentage);
    void installerGenerationCompleted(const QString& filePath);
    void codeSigningCompleted(bool success);

private slots:
    void onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onMediaKeyMessage();

private:
    // System tray helpers
    void setupSystemTrayMenu();
    void handleSystemTrayAction(const QString& action);
    
    // Media key helpers
    bool registerHotKeys();
    void unregisterHotKeys();
    void processMediaKey(int key);
    
    // File association helpers
    QString generateProgId(const QString& extension) const;
    bool createProgIdRegistration(const QString& progId, const FileAssociation& assoc);
    bool associateExtensionWithProgId(const QString& extension, const QString& progId);
    
    // DXVA helpers
    bool initializeDXVA();
    void cleanupDXVA();
    bool checkDXVACapabilities();
    
    // Registry helpers
    HKEY getRegistryRoot(RegistryScope scope) const;
    QString formatRegistryPath(const QString& path) const;
    
    // Installer helpers
    QString prepareMSIProperties(const QMap<QString, QString>& properties) const;
    QString prepareNSISScript(const QMap<QString, QString>& properties) const;
    bool compileInstaller(const QString& scriptPath, const QString& outputPath) const;
    
    // Code signing helpers
    bool signWithAuthenticode(const QString& filePath, const QString& certPath, const QString& password);
    
    // Member variables
    QSystemTrayIcon* m_systemTray;
    QMenu* m_systemTrayMenu;
    SystemTrayConfig m_trayConfig;
    
    bool m_mediaKeysRegistered;
    bool m_dxvaEnabled;
    bool m_dxvaSupported;
    
    QList<FileAssociation> m_registeredAssociations;
    QMap<int, QString> m_registeredHotKeys;
    
#ifdef Q_OS_WIN
    HWND m_messageWindow;
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static WindowsPlatform* s_instance;
#endif
    
    // Constants
    static const QString REGISTRY_APP_PATH;
    static const QString REGISTRY_UNINSTALL_PATH;
    static const QString REGISTRY_FILE_ASSOC_PATH;
    static const int WM_HOTKEY_BASE = 0x8000;
    static const int HOTKEY_PLAY_PAUSE = 1;
    static const int HOTKEY_STOP = 2;
    static const int HOTKEY_NEXT = 3;
    static const int HOTKEY_PREVIOUS = 4;
};