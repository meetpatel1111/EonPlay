#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QDateTime>

/**
 * @brief Cross-platform installer and package management system
 * 
 * Provides unified interface for creating installers, managing portable mode,
 * handling updates, and cross-platform deployment for EonPlay media player.
 */
class InstallerManager : public QObject
{
    Q_OBJECT

public:
    enum InstallationType {
        INSTALLATION_FULL,
        INSTALLATION_PORTABLE,
        INSTALLATION_UPDATE,
        INSTALLATION_REPAIR
    };

    enum DeploymentTarget {
        TARGET_WINDOWS_X64,
        TARGET_WINDOWS_X86,
        TARGET_LINUX_X64,
        TARGET_LINUX_ARM64,
        TARGET_MACOS_X64,
        TARGET_MACOS_ARM64
    };

    struct InstallerConfig {
        QString applicationName;
        QString version;
        QString publisher;
        QString description;
        QString website;
        QString supportUrl;
        QString licenseFile;
        QString iconPath;
        QStringList requiredComponents;
        QStringList optionalComponents;
        qint64 estimatedSize = 0;
        bool requiresAdmin = false;
        bool allowsPortable = true;
        bool createDesktopShortcut = true;
        bool createStartMenuShortcut = true;
        bool registerFileAssociations = true;
    };

    struct PortableConfig {
        QString portableDirectory;
        bool useRelativePaths = true;
        bool createLauncher = true;
        bool includeRuntime = true;
        QStringList excludedFiles;
        QStringList requiredFiles;
    };

    struct UpdateInfo {
        QString currentVersion;
        QString targetVersion;
        QString updateUrl;
        QString changelogUrl;
        qint64 downloadSize = 0;
        bool isSecurityUpdate = false;
        bool requiresRestart = true;
        QStringList updatedComponents;
    };

    explicit InstallerManager(QObject *parent = nullptr);
    ~InstallerManager();

    // Installer creation
    bool createInstaller(DeploymentTarget target, const InstallerConfig& config, const QString& outputPath);
    bool createPortablePackage(const PortableConfig& config, const QString& outputPath);
    bool createUpdatePackage(const UpdateInfo& updateInfo, const QString& outputPath);
    QString getInstallerPath(DeploymentTarget target) const;

    // Portable mode management
    bool enablePortableMode(bool enabled);
    bool isPortableModeEnabled() const;
    bool isRunningPortable() const;
    QString getPortableDirectory() const;
    bool createPortableLauncher(const QString& directory);

    // Installation detection and management
    bool isApplicationInstalled() const;
    QString getInstallationDirectory() const;
    QString getInstallationVersion() const;
    InstallationType getInstallationType() const;
    QDateTime getInstallationDate() const;

    // Update management
    bool prepareUpdate(const UpdateInfo& updateInfo);
    bool applyUpdate(const QString& updatePackagePath);
    bool rollbackUpdate();
    bool hasUpdateBackup() const;
    QString getUpdateBackupPath() const;

    // Uninstallation
    bool createUninstaller(const QString& installPath);
    bool runUninstaller();
    bool isUninstallerAvailable() const;
    QStringList getUninstallableComponents() const;

    // Deployment utilities
    QStringList getRequiredDependencies(DeploymentTarget target) const;
    bool validateDeploymentTarget(DeploymentTarget target) const;
    QString getTargetArchitecture(DeploymentTarget target) const;
    QString getTargetPlatform(DeploymentTarget target) const;

    // File and directory management
    bool copyApplicationFiles(const QString& sourceDir, const QString& targetDir);
    bool createDirectoryStructure(const QString& baseDir);
    bool registerApplication(const QString& installPath);
    bool unregisterApplication();

    // Configuration
    void setInstallerConfig(const InstallerConfig& config);
    InstallerConfig getInstallerConfig() const;
    void setPortableConfig(const PortableConfig& config);
    PortableConfig getPortableConfig() const;

    // Validation and verification
    bool validateInstallation(const QString& installPath) const;
    bool verifyApplicationIntegrity() const;
    QStringList checkMissingComponents() const;
    bool repairInstallation();

signals:
    void installerCreationStarted(DeploymentTarget target);
    void installerCreationProgress(int percentage);
    void installerCreationCompleted(const QString& installerPath);
    void installerCreationFailed(const QString& error);
    void portableModeChanged(bool enabled);
    void updatePrepared(const QString& updatePath);
    void updateApplied(const QString& version);
    void updateRolledBack(const QString& version);
    void uninstallationCompleted();
    void applicationRegistered(const QString& installPath);
    void applicationUnregistered();

private:
    // Platform-specific installer creation
    bool createWindowsInstaller(const InstallerConfig& config, const QString& outputPath);
    bool createLinuxPackages(const InstallerConfig& config, const QString& outputPath);
    bool createMacOSInstaller(const InstallerConfig& config, const QString& outputPath);
    
    // Installer template generation
    QString generateNSISScript(const InstallerConfig& config) const;
    QString generateWiXScript(const InstallerConfig& config) const;
    QString generateDebianControl(const InstallerConfig& config) const;
    QString generateRpmSpec(const InstallerConfig& config) const;
    QString generateAppImageStructure(const InstallerConfig& config) const;
    
    // Portable mode helpers
    bool setupPortableEnvironment(const QString& directory);
    bool createPortableConfig(const QString& directory);
    QString generatePortableLauncher(const QString& directory) const;
    
    // Update helpers
    bool createUpdateManifest(const UpdateInfo& updateInfo, const QString& packagePath);
    bool validateUpdatePackage(const QString& packagePath) const;
    bool backupCurrentInstallation();
    bool restoreFromBackup();
    
    // File operations
    bool copyFileWithProgress(const QString& source, const QString& destination);
    bool copyDirectoryRecursive(const QString& source, const QString& destination);
    bool removeDirectoryRecursive(const QString& directory);
    QString calculateDirectorySize(const QString& directory) const;
    
    // Registry and system integration
    bool createRegistryEntries(const QString& installPath);
    bool removeRegistryEntries();
    bool createFileAssociations();
    bool removeFileAssociations();
    bool createShortcuts(const QString& installPath);
    bool removeShortcuts();
    
    // Validation helpers
    bool validateInstallerConfig(const InstallerConfig& config) const;
    bool validatePortableConfig(const PortableConfig& config) const;
    bool checkDiskSpace(const QString& path, qint64 requiredSpace) const;
    bool checkPermissions(const QString& path) const;
    
    // Member variables
    InstallerConfig m_installerConfig;
    PortableConfig m_portableConfig;
    UpdateInfo m_currentUpdate;
    
    bool m_portableModeEnabled;
    QString m_installationDirectory;
    QString m_portableDirectory;
    InstallationType m_installationType;
    
    QString m_updateBackupPath;
    QString m_uninstallerPath;
    
    // Progress tracking
    int m_currentProgress;
    QString m_currentOperation;
    
    // Constants
    static const QString PORTABLE_CONFIG_FILE;
    static const QString UPDATE_BACKUP_DIR;
    static const QString UNINSTALLER_NAME;
    static const QStringList REQUIRED_FILES;
    static const QStringList OPTIONAL_FILES;
};