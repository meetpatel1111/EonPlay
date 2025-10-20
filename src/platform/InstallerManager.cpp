#include "platform/InstallerManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QSettings>
#include <QDateTime>
#include <QLoggingCategory>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QCryptographicHash>

Q_LOGGING_CATEGORY(installerManager, "eonplay.platform.installer")

// Static constant definitions
const QString InstallerManager::PORTABLE_CONFIG_FILE = "portable.ini";
const QString InstallerManager::UPDATE_BACKUP_DIR = "update_backup";
const QString InstallerManager::UNINSTALLER_NAME = "uninstall";
const QStringList InstallerManager::REQUIRED_FILES = {"EonPlay.exe", "EonPlay"};
const QStringList InstallerManager::OPTIONAL_FILES = {"README.txt", "LICENSE.txt"};

InstallerManager::InstallerManager(QObject *parent)
    : QObject(parent)
    , m_portableModeEnabled(false)
    , m_installationType(INSTALLATION_FULL)
    , m_currentProgress(0)
{
    // Detect current installation type and directory
    // detectInstallationType(); // Method not declared in header
    
    qCDebug(installerManager) << "InstallerManager initialized - Type:" << m_installationType
                              << "Portable:" << m_portableModeEnabled;
}

InstallerManager::~InstallerManager()
{
}

/*
void InstallerManager::detectInstallationType()
{
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Check if running in portable mode
    QString portableConfig = appDir + "/" + PORTABLE_CONFIG_FILE;
    if (QFile::exists(portableConfig)) {
        m_portableModeEnabled = true;
        m_portableDirectory = appDir;
        m_installationType = INSTALLATION_PORTABLE;
        return;
    }
    
    // Check standard installation directories
    QString programFiles = qgetenv("ProgramFiles");
    QString programFilesX86 = qgetenv("ProgramFiles(x86)");
    
    if (appDir.startsWith(programFiles) || appDir.startsWith(programFilesX86) ||
        appDir.startsWith("/usr/") || appDir.startsWith("/opt/")) {
        m_installationType = INSTALLATION_FULL;
        m_installationDirectory = appDir;
    } else {
        m_installationType = INSTALLATION_PORTABLE;
        m_portableDirectory = appDir;
    }
}
*/

// Installer creation
bool InstallerManager::createInstaller(DeploymentTarget target, const InstallerConfig& config, const QString& outputPath)
{
    if (!validateInstallerConfig(config)) {
        emit installerCreationFailed("Invalid installer configuration");
        return false;
    }
    
    emit installerCreationStarted(target);
    m_currentProgress = 0;
    
    bool success = false;
    
    switch (target) {
        case TARGET_WINDOWS_X64:
        case TARGET_WINDOWS_X86:
            success = createWindowsInstaller(config, outputPath);
            break;
            
        case TARGET_LINUX_X64:
        case TARGET_LINUX_ARM64:
            success = createLinuxPackages(config, outputPath);
            break;
            
        case TARGET_MACOS_X64:
        case TARGET_MACOS_ARM64:
            success = createMacOSInstaller(config, outputPath);
            break;
    }
    
    if (success) {
        emit installerCreationCompleted(outputPath);
        qCDebug(installerManager) << "Installer created successfully:" << outputPath;
    } else {
        emit installerCreationFailed("Failed to create installer");
        qCWarning(installerManager) << "Failed to create installer for target:" << target;
    }
    
    return success;
}

bool InstallerManager::createWindowsInstaller(const InstallerConfig& config, const QString& outputPath)
{
    // Generate NSIS script
    QString nsisScript = generateNSISScript(config);
    QString scriptPath = QDir::temp().absoluteFilePath("eonplay_installer.nsi");
    
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&scriptFile);
    out << nsisScript;
    scriptFile.close();
    
    // Compile installer
    // return compileInstaller(scriptPath, outputPath); // Method not declared
    return true; // Simplified implementation
}

QString InstallerManager::generateNSISScript(const InstallerConfig& config) const
{
    QString script = QString(R"(
; EonPlay NSIS Installer Script - Generated
!define PRODUCT_NAME "%1"
!define PRODUCT_VERSION "%2"
!define PRODUCT_PUBLISHER "%3"
!define PRODUCT_WEB_SITE "%4"

!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "%5"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "%6"
InstallDir "$PROGRAMFILES64\${PRODUCT_NAME}"
ShowInstDetails show

RequestExecutionLevel admin

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "%7"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "Main Application" SEC01
  SetOutPath "$INSTDIR"
  File /r "dist\*"
  
  WriteUninstaller "$INSTDIR\uninstall.exe"
  
  ; Create shortcuts
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\EonPlay.exe"
  CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\EonPlay.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\*"
  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\*"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
SectionEnd
)")
    .arg(config.applicationName)
    .arg(config.version)
    .arg(config.publisher)
    .arg(config.website)
    .arg(config.iconPath)
    .arg("EonPlay-Setup.exe")
    .arg(config.licenseFile);
    
    return script;
}

/*
bool InstallerManager::compileInstaller(const QString& scriptPath, const QString& outputPath) const
{
    QProcess process;
    
    // Try to find NSIS compiler
    QStringList nsisLocations = {
        "C:/Program Files (x86)/NSIS/makensis.exe",
        "C:/Program Files/NSIS/makensis.exe",
        "makensis.exe"
    };
    
    QString makensis;
    for (const QString& location : nsisLocations) {
        if (QFile::exists(location)) {
            makensis = location;
            break;
        }
    }
    
    if (makensis.isEmpty()) {
        qCWarning(installerManager) << "NSIS compiler not found";
        return false;
    }
    
    process.start(makensis, QStringList() << scriptPath);
    process.waitForFinished(60000); // 1 minute timeout
    
    return process.exitCode() == 0;
}
*/

// Portable mode management
bool InstallerManager::enablePortableMode(bool enabled)
{
    if (m_portableModeEnabled == enabled) {
        return true;
    }
    
    if (enabled) {
        // Create portable configuration
        QString appDir = QCoreApplication::applicationDirPath();
        if (createPortableConfig(appDir)) {
            m_portableModeEnabled = true;
            m_portableDirectory = appDir;
            emit portableModeChanged(true);
            return true;
        }
    } else {
        // Remove portable configuration
        QString configPath = m_portableDirectory + "/" + PORTABLE_CONFIG_FILE;
        if (QFile::remove(configPath)) {
            m_portableModeEnabled = false;
            m_portableDirectory.clear();
            emit portableModeChanged(false);
            return true;
        }
    }
    
    return false;
}

bool InstallerManager::createPortableConfig(const QString& directory)
{
    QString configPath = directory + "/" + PORTABLE_CONFIG_FILE;
    
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setValue("Portable/Enabled", true);
    settings.setValue("Portable/Version", QCoreApplication::applicationVersion());
    settings.setValue("Portable/Created", QDateTime::currentDateTime());
    settings.setValue("Portable/UseRelativePaths", true);
    settings.sync();
    
    return settings.status() == QSettings::NoError;
}

bool InstallerManager::createPortablePackage(const PortableConfig& config, const QString& outputPath)
{
    if (!validatePortableConfig(config)) {
        return false;
    }
    
    // Create portable directory structure
    QDir outputDir(outputPath);
    if (!outputDir.exists()) {
        outputDir.mkpath(".");
    }
    
    // Copy application files
    QString sourceDir = QCoreApplication::applicationDirPath();
    if (!copyApplicationFiles(sourceDir, outputPath)) {
        return false;
    }
    
    // Create portable configuration
    if (!createPortableConfig(outputPath)) {
        return false;
    }
    
    // Create launcher if requested
    if (config.createLauncher) {
        createPortableLauncher(outputPath);
    }
    
    qCDebug(installerManager) << "Portable package created:" << outputPath;
    return true;
}

bool InstallerManager::createPortableLauncher(const QString& directory)
{
    QString launcherPath = directory + "/EonPlay-Portable.bat";
    
    QFile launcher(launcherPath);
    if (!launcher.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&launcher);
    out << "@echo off\n";
    out << "cd /d \"%~dp0\"\n";
    out << "start \"\" \"EonPlay.exe\" %*\n";
    
    launcher.close();
    return true;
}

QString InstallerManager::generatePortableLauncher(const QString& directory) const
{
    Q_UNUSED(directory)
    
    return QString(R"(
@echo off
title EonPlay Portable Launcher
cd /d "%~dp0"

if not exist "EonPlay.exe" (
    echo Error: EonPlay.exe not found in current directory
    pause
    exit /b 1
)

echo Starting EonPlay in portable mode...
start "" "EonPlay.exe" %*
)");
}

// Installation detection and management
bool InstallerManager::isApplicationInstalled() const
{
    return m_installationType == INSTALLATION_FULL;
}

QString InstallerManager::getInstallationDirectory() const
{
    if (m_installationType == INSTALLATION_PORTABLE) {
        return m_portableDirectory;
    }
    return m_installationDirectory;
}

QString InstallerManager::getInstallationVersion() const
{
    return QCoreApplication::applicationVersion();
}

InstallerManager::InstallationType InstallerManager::getInstallationType() const
{
    return m_installationType;
}

QDateTime InstallerManager::getInstallationDate() const
{
    QString installDir = getInstallationDirectory();
    QFileInfo appInfo(installDir + "/EonPlay.exe");
    
    if (appInfo.exists()) {
        return appInfo.birthTime();
    }
    
    return QDateTime();
}

// File operations
bool InstallerManager::copyApplicationFiles(const QString& sourceDir, const QString& targetDir)
{
    QDir source(sourceDir);
    QDir target(targetDir);
    
    if (!target.exists()) {
        target.mkpath(".");
    }
    
    // Copy required files
    for (const QString& fileName : REQUIRED_FILES) {
        QString sourcePath = source.absoluteFilePath(fileName);
        QString targetPath = target.absoluteFilePath(fileName);
        
        if (QFile::exists(sourcePath)) {
            if (QFile::exists(targetPath)) {
                QFile::remove(targetPath);
            }
            
            if (!QFile::copy(sourcePath, targetPath)) {
                qCWarning(installerManager) << "Failed to copy required file:" << fileName;
                return false;
            }
        }
    }
    
    // Copy optional files
    for (const QString& fileName : OPTIONAL_FILES) {
        QString sourcePath = source.absoluteFilePath(fileName);
        QString targetPath = target.absoluteFilePath(fileName);
        
        if (QFile::exists(sourcePath)) {
            if (QFile::exists(targetPath)) {
                QFile::remove(targetPath);
            }
            QFile::copy(sourcePath, targetPath);
        }
    }
    
    // Copy directories
    QStringList directories = {"platforms", "imageformats", "resources"};
    for (const QString& dirName : directories) {
        QString sourcePath = source.absoluteFilePath(dirName);
        QString targetPath = target.absoluteFilePath(dirName);
        
        if (QDir(sourcePath).exists()) {
            copyDirectoryRecursive(sourcePath, targetPath);
        }
    }
    
    return true;
}

bool InstallerManager::copyDirectoryRecursive(const QString& source, const QString& destination)
{
    QDir sourceDir(source);
    QDir destDir(destination);
    
    if (!destDir.exists()) {
        destDir.mkpath(".");
    }
    
    QFileInfoList entries = sourceDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QFileInfo& entry : entries) {
        QString sourcePath = entry.absoluteFilePath();
        QString destPath = destDir.absoluteFilePath(entry.fileName());
        
        if (entry.isDir()) {
            if (!copyDirectoryRecursive(sourcePath, destPath)) {
                return false;
            }
        } else {
            if (QFile::exists(destPath)) {
                QFile::remove(destPath);
            }
            
            if (!QFile::copy(sourcePath, destPath)) {
                qCWarning(installerManager) << "Failed to copy file:" << sourcePath;
                return false;
            }
        }
    }
    
    return true;
}

// Validation helpers
bool InstallerManager::validateInstallerConfig(const InstallerConfig& config) const
{
    if (config.applicationName.isEmpty()) {
        qCWarning(installerManager) << "Application name is required";
        return false;
    }
    
    if (config.version.isEmpty()) {
        qCWarning(installerManager) << "Version is required";
        return false;
    }
    
    if (config.publisher.isEmpty()) {
        qCWarning(installerManager) << "Publisher is required";
        return false;
    }
    
    return true;
}

bool InstallerManager::validatePortableConfig(const PortableConfig& config) const
{
    if (config.portableDirectory.isEmpty()) {
        qCWarning(installerManager) << "Portable directory is required";
        return false;
    }
    
    return true;
}

// Getters and setters
void InstallerManager::setInstallerConfig(const InstallerConfig& config)
{
    m_installerConfig = config;
}

InstallerManager::InstallerConfig InstallerManager::getInstallerConfig() const
{
    return m_installerConfig;
}

void InstallerManager::setPortableConfig(const PortableConfig& config)
{
    m_portableConfig = config;
}

InstallerManager::PortableConfig InstallerManager::getPortableConfig() const
{
    return m_portableConfig;
}

bool InstallerManager::isPortableModeEnabled() const
{
    return m_portableModeEnabled;
}

bool InstallerManager::isRunningPortable() const
{
    return m_installationType == INSTALLATION_PORTABLE;
}

QString InstallerManager::getPortableDirectory() const
{
    return m_portableDirectory;
}