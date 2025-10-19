#include "platform/WindowsPlatform.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QLoggingCategory>
#include <QSettings>
#include <QMetaType>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winreg.h>
#include <dxva2api.h>
#include <d3d9.h>
#include <wintrust.h>
#include <softpub.h>
#include <comdef.h>
#include <shobjidl.h>
#endif

Q_LOGGING_CATEGORY(windowsPlatform, "eonplay.platform.windows")

// Static member initialization
#ifdef Q_OS_WIN
WindowsPlatform* WindowsPlatform::s_instance = nullptr;
#endif

// Registry paths
const QString WindowsPlatform::REGISTRY_APP_PATH = "HKEY_CURRENT_USER\\Software\\EonPlay";
const QString WindowsPlatform::REGISTRY_UNINSTALL_PATH = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EonPlay";
const QString WindowsPlatform::REGISTRY_FILE_ASSOC_PATH = "HKEY_CURRENT_USER\\Software\\Classes";

WindowsPlatform::WindowsPlatform(QObject *parent)
    : QObject(parent)
    , m_systemTray(nullptr)
    , m_systemTrayMenu(nullptr)
    , m_mediaKeysRegistered(false)
    , m_dxvaEnabled(false)
    , m_dxvaSupported(false)
#ifdef Q_OS_WIN
    , m_messageWindow(nullptr)
#endif
{
#ifdef Q_OS_WIN
    s_instance = this;
    
    // Check DXVA support
    m_dxvaSupported = isDXVASupported();
    
    qCDebug(windowsPlatform) << "WindowsPlatform initialized - DXVA supported:" << m_dxvaSupported;
#endif
}

WindowsPlatform::~WindowsPlatform()
{
    hideSystemTray();
    unregisterMediaKeys();
    
#ifdef Q_OS_WIN
    if (m_messageWindow) {
        DestroyWindow(m_messageWindow);
        m_messageWindow = nullptr;
    }
    
    s_instance = nullptr;
#endif
}

// System tray integration
bool WindowsPlatform::createSystemTray(const SystemTrayConfig& config)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qCWarning(windowsPlatform) << "System tray is not available";
        return false;
    }
    
    if (m_systemTray) {
        delete m_systemTray;
    }
    
    m_trayConfig = config;
    m_systemTray = new QSystemTrayIcon(this);
    
    // Set icon
    if (!config.iconPath.isEmpty() && QFile::exists(config.iconPath)) {
        m_systemTray->setIcon(QIcon(config.iconPath));
    } else {
        m_systemTray->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
    }
    
    // Set tooltip
    m_systemTray->setToolTip(config.tooltip.isEmpty() ? "EonPlay Media Player" : config.tooltip);
    
    // Setup context menu
    setupSystemTrayMenu();
    
    // Connect signals
    connect(m_systemTray, QOverload<QSystemTrayIcon::ActivationReason>::of(&QSystemTrayIcon::activated),
            this, &WindowsPlatform::onSystemTrayActivated);
    connect(m_systemTray, &QSystemTrayIcon::messageClicked,
            this, &WindowsPlatform::systemTrayMessageClicked);
    
    m_systemTray->show();
    
    qCDebug(windowsPlatform) << "System tray created successfully";
    return true;
}

void WindowsPlatform::setupSystemTrayMenu()
{
    if (!m_systemTray) return;
    
    if (m_systemTrayMenu) {
        delete m_systemTrayMenu;
    }
    
    m_systemTrayMenu = new QMenu();
    
    // Add default menu items
    QAction* showAction = m_systemTrayMenu->addAction("Show EonPlay");
    connect(showAction, &QAction::triggered, [this]() {
        emit systemTrayActivated();
    });
    
    QAction* playPauseAction = m_systemTrayMenu->addAction("Play/Pause");
    connect(playPauseAction, &QAction::triggered, [this]() {
        handleSystemTrayAction("play_pause");
    });
    
    QAction* stopAction = m_systemTrayMenu->addAction("Stop");
    connect(stopAction, &QAction::triggered, [this]() {
        handleSystemTrayAction("stop");
    });
    
    m_systemTrayMenu->addSeparator();
    
    // Add custom menu items
    for (const QString& item : m_trayConfig.menuItems) {
        QAction* action = m_systemTrayMenu->addAction(item);
        connect(action, &QAction::triggered, [this, item]() {
            handleSystemTrayAction(item.toLower().replace(" ", "_"));
        });
    }
    
    m_systemTrayMenu->addSeparator();
    
    QAction* exitAction = m_systemTrayMenu->addAction("Exit");
    connect(exitAction, &QAction::triggered, [this]() {
        handleSystemTrayAction("exit");
    });
    
    m_systemTray->setContextMenu(m_systemTrayMenu);
}

void WindowsPlatform::updateSystemTrayIcon(const QString& iconPath)
{
    if (m_systemTray && QFile::exists(iconPath)) {
        m_systemTray->setIcon(QIcon(iconPath));
        m_trayConfig.iconPath = iconPath;
    }
}

void WindowsPlatform::updateSystemTrayTooltip(const QString& tooltip)
{
    if (m_systemTray) {
        m_systemTray->setToolTip(tooltip);
        m_trayConfig.tooltip = tooltip;
    }
}

void WindowsPlatform::showSystemTrayMessage(const QString& title, const QString& message, int timeout)
{
    if (m_systemTray && m_trayConfig.showNotifications) {
        m_systemTray->showMessage(title, message, QSystemTrayIcon::Information, timeout);
    }
}

void WindowsPlatform::hideSystemTray()
{
    if (m_systemTray) {
        m_systemTray->hide();
        delete m_systemTray;
        m_systemTray = nullptr;
    }
    
    if (m_systemTrayMenu) {
        delete m_systemTrayMenu;
        m_systemTrayMenu = nullptr;
    }
}

bool WindowsPlatform::isSystemTrayAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void WindowsPlatform::handleSystemTrayAction(const QString& action)
{
    qCDebug(windowsPlatform) << "System tray action:" << action;
    
    if (action == "show" || action == "restore") {
        emit systemTrayActivated();
    } else if (action == "play_pause") {
        emit mediaKeyPressed("PlayPause");
    } else if (action == "stop") {
        emit mediaKeyPressed("Stop");
    } else if (action == "next") {
        emit mediaKeyPressed("Next");
    } else if (action == "previous") {
        emit mediaKeyPressed("Previous");
    } else if (action == "exit") {
        QApplication::quit();
    }
}

void WindowsPlatform::onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::DoubleClick:
            emit systemTrayActivated();
            break;
        case QSystemTrayIcon::Trigger:
            // Single click - could show/hide window
            break;
        default:
            break;
    }
}

// Media key support
bool WindowsPlatform::registerMediaKeys()
{
#ifdef Q_OS_WIN
    if (m_mediaKeysRegistered) {
        return true;
    }
    
    // Create a message-only window for receiving hotkey messages
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"EonPlayMediaKeys";
    
    RegisterClass(&wc);
    
    m_messageWindow = CreateWindow(L"EonPlayMediaKeys", L"EonPlay Media Keys",
                                  0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, 
                                  GetModuleHandle(nullptr), nullptr);
    
    if (!m_messageWindow) {
        qCWarning(windowsPlatform) << "Failed to create message window for media keys";
        return false;
    }
    
    // Register global hotkeys for media keys
    bool success = true;
    success &= RegisterHotKey(m_messageWindow, HOTKEY_PLAY_PAUSE, 0, VK_MEDIA_PLAY_PAUSE);
    success &= RegisterHotKey(m_messageWindow, HOTKEY_STOP, 0, VK_MEDIA_STOP);
    success &= RegisterHotKey(m_messageWindow, HOTKEY_NEXT, 0, VK_MEDIA_NEXT_TRACK);
    success &= RegisterHotKey(m_messageWindow, HOTKEY_PREVIOUS, 0, VK_MEDIA_PREV_TRACK);
    
    if (success) {
        m_mediaKeysRegistered = true;
        qCDebug(windowsPlatform) << "Media keys registered successfully";
    } else {
        qCWarning(windowsPlatform) << "Failed to register some media keys";
    }
    
    return success;
#else
    return false;
#endif
}

void WindowsPlatform::unregisterMediaKeys()
{
#ifdef Q_OS_WIN
    if (!m_mediaKeysRegistered || !m_messageWindow) {
        return;
    }
    
    UnregisterHotKey(m_messageWindow, HOTKEY_PLAY_PAUSE);
    UnregisterHotKey(m_messageWindow, HOTKEY_STOP);
    UnregisterHotKey(m_messageWindow, HOTKEY_NEXT);
    UnregisterHotKey(m_messageWindow, HOTKEY_PREVIOUS);
    
    if (m_messageWindow) {
        DestroyWindow(m_messageWindow);
        m_messageWindow = nullptr;
    }
    
    m_mediaKeysRegistered = false;
    qCDebug(windowsPlatform) << "Media keys unregistered";
#endif
}

bool WindowsPlatform::isMediaKeysRegistered() const
{
    return m_mediaKeysRegistered;
}

void WindowsPlatform::enableMediaKeyHandling(bool enabled)
{
    if (enabled && !m_mediaKeysRegistered) {
        registerMediaKeys();
    } else if (!enabled && m_mediaKeysRegistered) {
        unregisterMediaKeys();
    }
}

#ifdef Q_OS_WIN
LRESULT CALLBACK WindowsPlatform::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_HOTKEY && s_instance) {
        int hotkeyId = static_cast<int>(wParam);
        s_instance->processMediaKey(hotkeyId);
        return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
#endif

void WindowsPlatform::processMediaKey(int key)
{
    QString keyName;
    
    switch (key) {
        case HOTKEY_PLAY_PAUSE:
            keyName = "PlayPause";
            break;
        case HOTKEY_STOP:
            keyName = "Stop";
            break;
        case HOTKEY_NEXT:
            keyName = "Next";
            break;
        case HOTKEY_PREVIOUS:
            keyName = "Previous";
            break;
        default:
            return;
    }
    
    emit mediaKeyPressed(keyName);
    qCDebug(windowsPlatform) << "Media key pressed:" << keyName;
}

// File associations
bool WindowsPlatform::registerFileAssociations(const QList<FileAssociation>& associations)
{
#ifdef Q_OS_WIN
    bool allSuccess = true;
    
    for (const FileAssociation& assoc : associations) {
        QString progId = generateProgId(assoc.extension);
        
        // Create ProgID registration
        if (!createProgIdRegistration(progId, assoc)) {
            allSuccess = false;
            continue;
        }
        
        // Associate extension with ProgID
        if (!associateExtensionWithProgId(assoc.extension, progId)) {
            allSuccess = false;
            continue;
        }
        
        m_registeredAssociations.append(assoc);
        emit fileAssociationChanged(assoc.extension, true);
    }
    
    // Notify shell of changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
    qCDebug(windowsPlatform) << "File associations registered:" << associations.size() << "Success:" << allSuccess;
    return allSuccess;
#else
    Q_UNUSED(associations)
    return false;
#endif
}

QString WindowsPlatform::generateProgId(const QString& extension) const
{
    return QString("EonPlay%1File").arg(extension.toUpper().remove('.'));
}

bool WindowsPlatform::createProgIdRegistration(const QString& progId, const FileAssociation& assoc)
{
    QString progIdPath = QString("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(progId);
    
    // Set description
    if (!writeRegistryValue(progIdPath, "", assoc.description)) {
        return false;
    }
    
    // Set icon
    if (!assoc.iconPath.isEmpty()) {
        QString iconPath = QString("%1\\DefaultIcon").arg(progIdPath);
        if (!writeRegistryValue(iconPath, "", assoc.iconPath)) {
            return false;
        }
    }
    
    // Set command
    QString commandPath = QString("%1\\shell\\open\\command").arg(progIdPath);
    QString command = assoc.commandLine.isEmpty() ? 
                     QString("\"%1\" \"%2\"").arg(QApplication::applicationFilePath()).arg("%1") :
                     assoc.commandLine;
    
    return writeRegistryValue(commandPath, "", command);
}

bool WindowsPlatform::associateExtensionWithProgId(const QString& extension, const QString& progId)
{
    QString extPath = QString("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(extension);
    return writeRegistryValue(extPath, "", progId);
}

bool WindowsPlatform::unregisterFileAssociations(const QStringList& extensions)
{
    bool allSuccess = true;
    
    for (const QString& ext : extensions) {
        QString extPath = QString("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(ext);
        if (!deleteRegistryKey(extPath)) {
            allSuccess = false;
        }
        
        QString progId = generateProgId(ext);
        QString progIdPath = QString("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(progId);
        if (!deleteRegistryKey(progIdPath)) {
            allSuccess = false;
        }
        
        // Remove from registered list
        m_registeredAssociations.removeIf([ext](const FileAssociation& assoc) {
            return assoc.extension == ext;
        });
        
        emit fileAssociationChanged(ext, false);
    }
    
#ifdef Q_OS_WIN
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#endif
    
    return allSuccess;
}

bool WindowsPlatform::isFileAssociationRegistered(const QString& extension) const
{
    return std::any_of(m_registeredAssociations.begin(), m_registeredAssociations.end(),
                      [extension](const FileAssociation& assoc) {
                          return assoc.extension == extension;
                      });
}

QStringList WindowsPlatform::getRegisteredAssociations() const
{
    QStringList extensions;
    for (const FileAssociation& assoc : m_registeredAssociations) {
        extensions.append(assoc.extension);
    }
    return extensions;
}

// DXVA Hardware acceleration
bool WindowsPlatform::isDXVASupported() const
{
#ifdef Q_OS_WIN
    // Check if DXVA2 is available
    HMODULE dxva2Lib = LoadLibrary(L"dxva2.dll");
    if (!dxva2Lib) {
        return false;
    }
    
    FreeLibrary(dxva2Lib);
    
    // Check for D3D9 support
    HMODULE d3d9Lib = LoadLibrary(L"d3d9.dll");
    if (!d3d9Lib) {
        return false;
    }
    
    FreeLibrary(d3d9Lib);
    
    return true;
#else
    return false;
#endif
}

bool WindowsPlatform::enableDXVA(bool enabled)
{
    if (enabled && !m_dxvaSupported) {
        qCWarning(windowsPlatform) << "DXVA not supported on this system";
        return false;
    }
    
    m_dxvaEnabled = enabled;
    emit dxvaStateChanged(enabled);
    
    qCDebug(windowsPlatform) << "DXVA" << (enabled ? "enabled" : "disabled");
    return true;
}

bool WindowsPlatform::isDXVAEnabled() const
{
    return m_dxvaEnabled;
}

QStringList WindowsPlatform::getAvailableDecoders() const
{
    QStringList decoders;
    
#ifdef Q_OS_WIN
    if (m_dxvaSupported) {
        decoders << "H.264 DXVA" << "H.265 DXVA" << "MPEG-2 DXVA" << "VC-1 DXVA";
    }
#endif
    
    return decoders;
}

QString WindowsPlatform::getDXVAStatus() const
{
    if (!m_dxvaSupported) {
        return "Not supported";
    } else if (m_dxvaEnabled) {
        return "Enabled";
    } else {
        return "Disabled";
    }
}

// Registry operations
bool WindowsPlatform::writeRegistryValue(const QString& keyPath, const QString& valueName, 
                                        const QVariant& value, RegistryScope scope)
{
#ifdef Q_OS_WIN
    HKEY rootKey = getRegistryRoot(scope);
    QString formattedPath = formatRegistryPath(keyPath);
    
    HKEY hKey;
    LONG result = RegCreateKeyExA(rootKey, formattedPath.toLocal8Bit().constData(),
                                 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    
    if (result != ERROR_SUCCESS) {
        qCWarning(windowsPlatform) << "Failed to create registry key:" << keyPath;
        return false;
    }
    
    QByteArray valueData;
    DWORD valueType;
    
    switch (value.typeId()) {
        case QMetaType::QString: {
            QString str = value.toString();
            valueData = str.toUtf8();
            valueData.append('\0'); // Null terminator
            valueType = REG_SZ;
            break;
        }
        case QMetaType::Int:
        case QMetaType::UInt: {
            DWORD dwordValue = value.toUInt();
            valueData = QByteArray(reinterpret_cast<const char*>(&dwordValue), sizeof(DWORD));
            valueType = REG_DWORD;
            break;
        }
        default:
            RegCloseKey(hKey);
            return false;
    }
    
    result = RegSetValueExA(hKey, valueName.toLocal8Bit().constData(), 0, valueType,
                           reinterpret_cast<const BYTE*>(valueData.constData()), valueData.size());
    
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS) {
        qCWarning(windowsPlatform) << "Failed to write registry value:" << keyPath << valueName;
        return false;
    }
    
    return true;
#else
    Q_UNUSED(keyPath)
    Q_UNUSED(valueName)
    Q_UNUSED(value)
    Q_UNUSED(scope)
    return false;
#endif
}

QVariant WindowsPlatform::readRegistryValue(const QString& keyPath, const QString& valueName, 
                                           RegistryScope scope) const
{
#ifdef Q_OS_WIN
    HKEY rootKey = getRegistryRoot(scope);
    QString formattedPath = formatRegistryPath(keyPath);
    
    HKEY hKey;
    LONG result = RegOpenKeyExA(rootKey, formattedPath.toLocal8Bit().constData(), 0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS) {
        return QVariant();
    }
    
    DWORD valueType;
    DWORD dataSize = 0;
    
    // Get data size
    result = RegQueryValueExA(hKey, valueName.toLocal8Bit().constData(), nullptr, &valueType, nullptr, &dataSize);
    
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return QVariant();
    }
    
    QByteArray data(dataSize, 0);
    result = RegQueryValueExA(hKey, valueName.toLocal8Bit().constData(), nullptr, &valueType,
                             reinterpret_cast<BYTE*>(data.data()), &dataSize);
    
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS) {
        return QVariant();
    }
    
    switch (valueType) {
        case REG_SZ:
            return QString::fromUtf8(data.constData());
        case REG_DWORD:
            return QVariant::fromValue(*reinterpret_cast<const DWORD*>(data.constData()));
        default:
            return QVariant();
    }
#else
    Q_UNUSED(keyPath)
    Q_UNUSED(valueName)
    Q_UNUSED(scope)
    return QVariant();
#endif
}

bool WindowsPlatform::deleteRegistryKey(const QString& keyPath, RegistryScope scope)
{
#ifdef Q_OS_WIN
    HKEY rootKey = getRegistryRoot(scope);
    QString formattedPath = formatRegistryPath(keyPath);
    
    LONG result = RegDeleteTreeA(rootKey, formattedPath.toLocal8Bit().constData());
    return (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
#else
    Q_UNUSED(keyPath)
    Q_UNUSED(scope)
    return false;
#endif
}

bool WindowsPlatform::registryKeyExists(const QString& keyPath, RegistryScope scope) const
{
#ifdef Q_OS_WIN
    HKEY rootKey = getRegistryRoot(scope);
    QString formattedPath = formatRegistryPath(keyPath);
    
    HKEY hKey;
    LONG result = RegOpenKeyExA(rootKey, formattedPath.toLocal8Bit().constData(), 0, KEY_READ, &hKey);
    
    if (result == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    
    return false;
#else
    Q_UNUSED(keyPath)
    Q_UNUSED(scope)
    return false;
#endif
}

#ifdef Q_OS_WIN
HKEY WindowsPlatform::getRegistryRoot(RegistryScope scope) const
{
    switch (scope) {
        case REGISTRY_CURRENT_USER:
            return HKEY_CURRENT_USER;
        case REGISTRY_LOCAL_MACHINE:
            return HKEY_LOCAL_MACHINE;
        default:
            return HKEY_CURRENT_USER;
    }
}
#endif

QString WindowsPlatform::formatRegistryPath(const QString& path) const
{
    QString formatted = path;
    
    // Remove HKEY prefix if present
    if (formatted.startsWith("HKEY_CURRENT_USER\\")) {
        formatted = formatted.mid(18);
    } else if (formatted.startsWith("HKEY_LOCAL_MACHINE\\")) {
        formatted = formatted.mid(19);
    }
    
    return formatted;
}

// Windows-specific utilities
QString WindowsPlatform::getWindowsVersion() const
{
#ifdef Q_OS_WIN
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    
    if (GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&osvi))) {
        return QString("Windows %1.%2 Build %3")
               .arg(osvi.dwMajorVersion)
               .arg(osvi.dwMinorVersion)
               .arg(osvi.dwBuildNumber);
    }
#endif
    
    return "Unknown Windows Version";
}

QString WindowsPlatform::getSystemArchitecture() const
{
#ifdef Q_OS_WIN
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return "x64";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return "x86";
        case PROCESSOR_ARCHITECTURE_ARM:
            return "ARM";
        case PROCESSOR_ARCHITECTURE_ARM64:
            return "ARM64";
        default:
            return "Unknown";
    }
#else
    return "Unknown";
#endif
}

bool WindowsPlatform::isRunningAsAdmin() const
{
#ifdef Q_OS_WIN
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    
    return isAdmin == TRUE;
#else
    return false;
#endif
}

// Startup and autorun
bool WindowsPlatform::addToStartup(bool enabled)
{
    QString startupPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    
    if (enabled) {
        QString appPath = QApplication::applicationFilePath();
        return writeRegistryValue(startupPath, "EonPlay", QString("\"%1\"").arg(appPath));
    } else {
        // Remove from startup
        QSettings settings(startupPath, QSettings::NativeFormat);
        settings.remove("EonPlay");
        return true;
    }
}

bool WindowsPlatform::isInStartup() const
{
    QString startupPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    QVariant value = readRegistryValue(startupPath, "EonPlay");
    return !value.isNull();
}

// Toast notifications (Windows 10+)
void WindowsPlatform::showToastNotification(const QString& title, const QString& message, const QString& iconPath)
{
#ifdef Q_OS_WIN
    // Simplified toast notification - would need proper WinRT implementation for Windows 10+
    if (m_systemTray) {
        showSystemTrayMessage(title, message);
    }
#else
    Q_UNUSED(title)
    Q_UNUSED(message)
    Q_UNUSED(iconPath)
#endif
}

bool WindowsPlatform::isToastNotificationSupported() const
{
    // Check Windows version - Toast notifications available in Windows 8+
    QString version = getWindowsVersion();
    return version.contains("Windows 10") || version.contains("Windows 11");
}