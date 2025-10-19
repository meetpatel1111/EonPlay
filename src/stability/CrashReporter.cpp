#include "stability/CrashReporter.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QSysInfo>
#include <QProcess>
#include <QThread>
#include <QLoggingCategory>
#include <QUuid>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <signal.h>
#else
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#endif

#include <csignal>
#include <exception>

Q_LOGGING_CATEGORY(crashReporter, "eonplay.stability.crashreporter")

// Static instance for signal handlers
CrashReporter* CrashReporter::s_instance = nullptr;

CrashReporter::CrashReporter(QObject *parent)
    : QObject(parent)
    , m_crashHandlerInstalled(false)
    , m_automaticReportingEnabled(false)
    , m_stabilityMonitoringActive(false)
    , m_safeModeEnabled(false)
    , m_userConsent(false)
    , m_reportingEndpoint("https://crash-reports.eonplay.com/api/v1/crashes")
    , m_maxStoredCrashes(DEFAULT_MAX_STORED_CRASHES)
    , m_reportingTimeout(DEFAULT_REPORTING_TIMEOUT_MS)
    , m_stabilityCheckInterval(DEFAULT_STABILITY_CHECK_INTERVAL_MS)
    , m_consecutiveFailures(0)
    , m_stabilityTimer(std::make_unique<QTimer>(this))
    , m_memoryMonitor(std::make_unique<QTimer>(this))
    , m_networkManager(std::make_unique<QNetworkAccessManager>(this))
    , m_initialMemoryUsage(0)
    , m_peakMemoryUsage(0)
{
    // Set static instance for signal handlers
    s_instance = this;
    
    // Setup timers
    m_stabilityTimer->setInterval(m_stabilityCheckInterval);
    connect(m_stabilityTimer.get(), &QTimer::timeout, this, &CrashReporter::onStabilityTimerTimeout);
    
    m_memoryMonitor->setInterval(DEFAULT_MEMORY_CHECK_INTERVAL_MS);
    connect(m_memoryMonitor.get(), &QTimer::timeout, this, &CrashReporter::onMemoryMonitorTimeout);
    
    // Setup network manager
    connect(m_networkManager.get(), &QNetworkAccessManager::finished,
            this, &CrashReporter::onReportingReplyFinished);
    
    // Load existing data
    loadStoredCrashes();
    loadStabilityMetrics();
    
    // Record initial memory usage
    m_initialMemoryUsage = getCurrentMemoryUsage();
    m_peakMemoryUsage = m_initialMemoryUsage;
    
    qCDebug(crashReporter) << "CrashReporter initialized";
}

CrashReporter::~CrashReporter()
{
    uninstallCrashHandlers();
    stopStabilityMonitoring();
    saveStabilityMetrics();
    
    // Cancel pending reports
    for (QNetworkReply* reply : m_pendingReports) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
    }
    m_pendingReports.clear();
    
    s_instance = nullptr;
}

// Crash detection and handling
void CrashReporter::installCrashHandlers()
{
    if (m_crashHandlerInstalled) return;
    
    setupSignalHandlers();
    setupExceptionHandlers();
    
    m_crashHandlerInstalled = true;
    qCDebug(crashReporter) << "Crash handlers installed";
}

void CrashReporter::uninstallCrashHandlers()
{
    if (!m_crashHandlerInstalled) return;
    
    // Reset signal handlers to default
    signal(SIGSEGV, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    
#ifndef Q_OS_WIN
    signal(SIGBUS, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
#endif
    
    // Reset exception handlers
    std::set_terminate(nullptr);
    std::set_unexpected(nullptr);
    
    m_crashHandlerInstalled = false;
    qCDebug(crashReporter) << "Crash handlers uninstalled";
}

bool CrashReporter::isCrashHandlerInstalled() const
{
    return m_crashHandlerInstalled;
}

void CrashReporter::setupSignalHandlers()
{
    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);
    signal(SIGFPE, signalHandler);
    signal(SIGILL, signalHandler);
    
#ifndef Q_OS_WIN
    signal(SIGBUS, signalHandler);
    signal(SIGPIPE, signalHandler);
#endif
}

void CrashReporter::setupExceptionHandlers()
{
    std::set_terminate(terminateHandler);
    std::set_unexpected(unexpectedHandler);
}

void CrashReporter::handleCrash(CrashType type, const QString& location, const QString& message)
{
    CrashInfo crashInfo;
    crashInfo.crashId = generateCrashId();
    crashInfo.type = type;
    crashInfo.timestamp = QDateTime::currentDateTime();
    crashInfo.applicationVersion = getApplicationVersion();
    crashInfo.operatingSystem = getOperatingSystemInfo();
    crashInfo.crashLocation = location;
    crashInfo.errorMessage = message;
    crashInfo.stackTrace = collectStackTrace();
    crashInfo.loadedModules = collectLoadedModules();
    crashInfo.memoryUsage = getCurrentMemoryUsage();
    crashInfo.uptime = getApplicationUptime();
    crashInfo.lastAction = m_lastUserAction;
    crashInfo.systemInfo = collectSystemInfo();
    
    // Store crash info
    storeCrashInfo(crashInfo);
    
    // Update stability metrics
    m_stabilityMetrics.crashCount++;
    m_stabilityMetrics.lastCrash = crashInfo.timestamp;
    m_stabilityMetrics.crashTypeCount[QString::number(static_cast<int>(type))]++;
    updateStabilityMetrics();
    
    // Emit signal
    emit crashDetected(crashInfo);
    
    // Report crash if enabled
    if (m_automaticReportingEnabled && m_userConsent) {
        reportCrashAsync(crashInfo);
    }
    
    qCCritical(crashReporter) << "Crash detected:" << crashInfo.crashId 
                              << "Type:" << type << "Location:" << location;
}

void CrashReporter::generateCrashDump(const QString& crashId)
{
#ifdef Q_OS_WIN
    // Windows minidump generation
    QString dumpPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) 
                      + QString("/eonplay_crash_%1.dmp").arg(crashId);
    
    HANDLE hFile = CreateFileA(dumpPath.toLocal8Bit().constData(),
                              GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = NULL;
        mdei.ClientPointers = FALSE;
        
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                         MiniDumpNormal, &mdei, NULL, NULL);
        
        CloseHandle(hFile);
        qCDebug(crashReporter) << "Crash dump generated:" << dumpPath;
    }
#else
    // Linux core dump handling
    QString dumpPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) 
                      + QString("/eonplay_crash_%1.core").arg(crashId);
    
    // Enable core dumps
    struct rlimit rlim;
    rlim.rlim_cur = RLIM_INFINITY;
    rlim.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &rlim);
    
    qCDebug(crashReporter) << "Core dump enabled for crash:" << crashId;
#endif
}

// Crash reporting
void CrashReporter::enableAutomaticReporting(bool enabled)
{
    if (m_automaticReportingEnabled != enabled) {
        m_automaticReportingEnabled = enabled;
        emit reportingStateChanged(enabled);
        qCDebug(crashReporter) << "Automatic reporting:" << (enabled ? "enabled" : "disabled");
    }
}

bool CrashReporter::isAutomaticReportingEnabled() const
{
    return m_automaticReportingEnabled;
}

void CrashReporter::setReportingEndpoint(const QString& url)
{
    m_reportingEndpoint = url;
    qCDebug(crashReporter) << "Reporting endpoint set to:" << url;
}

QString CrashReporter::getReportingEndpoint() const
{
    return m_reportingEndpoint;
}

bool CrashReporter::reportCrash(const CrashInfo& crashInfo)
{
    if (!shouldReportCrash(crashInfo)) {
        return false;
    }
    
    QByteArray reportData = prepareCrashReport(crashInfo);
    
    QNetworkRequest request(m_reportingEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "EonPlay-CrashReporter/1.0");
    
    QNetworkReply* reply = m_networkManager->post(request, reportData);
    reply->setProperty("crashId", crashInfo.crashId);
    
    // Wait for response (synchronous)
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(m_reportingTimeout, &loop, &QEventLoop::quit);
    loop.exec();
    
    bool success = (reply->error() == QNetworkReply::NoError);
    
    if (success) {
        // Mark crash as reported
        for (auto& crash : m_storedCrashes) {
            if (crash.crashId == crashInfo.crashId) {
                crash.wasReported = true;
                break;
            }
        }
    }
    
    emit crashReported(crashInfo.crashId, success);
    reply->deleteLater();
    
    qCDebug(crashReporter) << "Crash report sent:" << crashInfo.crashId << "Success:" << success;
    return success;
}

void CrashReporter::reportCrashAsync(const CrashInfo& crashInfo)
{
    if (!shouldReportCrash(crashInfo)) {
        return;
    }
    
    QByteArray reportData = prepareCrashReport(crashInfo);
    
    QNetworkRequest request(m_reportingEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "EonPlay-CrashReporter/1.0");
    
    QNetworkReply* reply = m_networkManager->post(request, reportData);
    reply->setProperty("crashId", crashInfo.crashId);
    
    m_pendingReports.append(reply);
    
    qCDebug(crashReporter) << "Async crash report initiated:" << crashInfo.crashId;
}

// Stability monitoring
void CrashReporter::startStabilityMonitoring()
{
    if (!m_stabilityMonitoringActive) {
        m_stabilityMonitoringActive = true;
        m_stabilityTimer->start();
        m_memoryMonitor->start();
        
        qCDebug(crashReporter) << "Stability monitoring started";
    }
}

void CrashReporter::stopStabilityMonitoring()
{
    if (m_stabilityMonitoringActive) {
        m_stabilityMonitoringActive = false;
        m_stabilityTimer->stop();
        m_memoryMonitor->stop();
        
        qCDebug(crashReporter) << "Stability monitoring stopped";
    }
}

bool CrashReporter::isStabilityMonitoringActive() const
{
    return m_stabilityMonitoringActive;
}

CrashReporter::StabilityMetrics CrashReporter::getStabilityMetrics() const
{
    return m_stabilityMetrics;
}

void CrashReporter::recordSessionStart()
{
    m_sessionStartTime = QDateTime::currentDateTime();
    m_stabilityMetrics.totalSessions++;
    m_sessionActions.clear();
    
    qCDebug(crashReporter) << "Session started";
}

void CrashReporter::recordSessionEnd()
{
    if (m_sessionStartTime.isValid()) {
        qint64 sessionDuration = m_sessionStartTime.msecsTo(QDateTime::currentDateTime());
        m_stabilityMetrics.totalUptime += sessionDuration;
        
        if (m_stabilityMetrics.totalSessions > 0) {
            m_stabilityMetrics.averageSessionTime = m_stabilityMetrics.totalUptime / m_stabilityMetrics.totalSessions;
        }
        
        updateStabilityMetrics();
        saveStabilityMetrics();
        
        qCDebug(crashReporter) << "Session ended, duration:" << sessionDuration << "ms";
    }
}

void CrashReporter::recordUserAction(const QString& action)
{
    m_lastUserAction = action;
    m_sessionActions.append(QString("%1: %2").arg(QDateTime::currentDateTime().toString()).arg(action));
    
    // Keep only recent actions
    if (m_sessionActions.size() > 100) {
        m_sessionActions.removeFirst();
    }
}

// System information collection
QMap<QString, QString> CrashReporter::collectSystemInfo() const
{
    QMap<QString, QString> info;
    
    info["os_name"] = QSysInfo::productType();
    info["os_version"] = QSysInfo::productVersion();
    info["kernel_type"] = QSysInfo::kernelType();
    info["kernel_version"] = QSysInfo::kernelVersion();
    info["cpu_architecture"] = QSysInfo::currentCpuArchitecture();
    info["build_abi"] = QSysInfo::buildAbi();
    info["qt_version"] = qVersion();
    info["application_name"] = QCoreApplication::applicationName();
    info["application_version"] = getApplicationVersion();
    info["memory_usage"] = QString::number(getCurrentMemoryUsage());
    info["uptime"] = QString::number(getApplicationUptime());
    
    // Additional system info
    info["cpu_info"] = getCPUInfo();
    info["memory_info"] = getMemoryInfo();
    info["gpu_info"] = getGPUInfo();
    
    return info;
}

QStringList CrashReporter::collectLoadedModules() const
{
    QStringList modules;
    
#ifdef Q_OS_WIN
    HANDLE hProcess = GetCurrentProcess();
    HMODULE hMods[1024];
    DWORD cbNeeded;
    
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            TCHAR szModName[MAX_PATH];
            if (GetModuleFileNameEx(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR))) {
                modules.append(QString::fromWCharArray(szModName));
            }
        }
    }
#else
    // Linux: Read from /proc/self/maps
    QFile mapsFile("/proc/self/maps");
    if (mapsFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&mapsFile);
        while (!stream.atEnd()) {
            QString line = stream.readLine();
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 6 && parts[5].startsWith('/')) {
                QString module = parts[5];
                if (!modules.contains(module)) {
                    modules.append(module);
                }
            }
        }
    }
#endif
    
    return modules;
}

QString CrashReporter::collectStackTrace() const
{
    QString stackTrace;
    
#ifdef Q_OS_WIN
    // Windows stack trace using StackWalk64
    HANDLE hProcess = GetCurrentProcess();
    HANDLE hThread = GetCurrentThread();
    
    CONTEXT context;
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);
    
    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(STACKFRAME64));
    
#ifdef _M_IX86
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rsp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#endif
    
    SymInitialize(hProcess, NULL, TRUE);
    
    for (int frame = 0; frame < 64; frame++) {
        if (!StackWalk64(machineType, hProcess, hThread, &stackFrame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
            break;
        }
        
        if (stackFrame.AddrPC.Offset != 0) {
            stackTrace += QString("0x%1\n").arg(stackFrame.AddrPC.Offset, 0, 16);
        }
    }
    
    SymCleanup(hProcess);
#else
    // Linux stack trace using backtrace
    void* array[256];
    size_t size = backtrace(array, 256);
    char** strings = backtrace_symbols(array, size);
    
    if (strings) {
        for (size_t i = 0; i < size; i++) {
            stackTrace += QString("%1\n").arg(strings[i]);
        }
        free(strings);
    }
#endif
    
    return stackTrace;
}

qint64 CrashReporter::getCurrentMemoryUsage() const
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#else
    QFile statusFile("/proc/self/status");
    if (statusFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&statusFile);
        while (!stream.atEnd()) {
            QString line = stream.readLine();
            if (line.startsWith("VmRSS:")) {
                QStringList parts = line.split(QRegularExpression("\\s+"));
                if (parts.size() >= 2) {
                    return parts[1].toLongLong() * 1024; // Convert KB to bytes
                }
            }
        }
    }
#endif
    
    return 0;
}

qint64 CrashReporter::getApplicationUptime() const
{
    static QDateTime startTime = QDateTime::currentDateTime();
    return startTime.msecsTo(QDateTime::currentDateTime());
}

// Recovery and safe mode
bool CrashReporter::shouldStartInSafeMode() const
{
    return m_consecutiveFailures >= SAFE_MODE_FAILURE_THRESHOLD;
}

void CrashReporter::enableSafeMode(bool enabled)
{
    if (m_safeModeEnabled != enabled) {
        m_safeModeEnabled = enabled;
        
        if (enabled) {
            emit safeModeTriggered();
            qCWarning(crashReporter) << "Safe mode enabled";
        } else {
            qCDebug(crashReporter) << "Safe mode disabled";
        }
    }
}

bool CrashReporter::isSafeModeEnabled() const
{
    return m_safeModeEnabled;
}

void CrashReporter::recordSuccessfulStart()
{
    m_consecutiveFailures = 0;
    m_lastSuccessfulStart = QDateTime::currentDateTime();
    
    if (m_safeModeEnabled) {
        enableSafeMode(false);
        emit recoveryCompleted();
    }
    
    qCDebug(crashReporter) << "Successful start recorded";
}

void CrashReporter::recordFailedStart()
{
    m_consecutiveFailures++;
    
    if (shouldStartInSafeMode() && !m_safeModeEnabled) {
        enableSafeMode(true);
    }
    
    qCWarning(crashReporter) << "Failed start recorded, consecutive failures:" << m_consecutiveFailures;
}

int CrashReporter::getConsecutiveFailures() const
{
    return m_consecutiveFailures;
}

// Private helper methods
QString CrashReporter::generateCrashId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void CrashReporter::storeCrashInfo(const CrashInfo& crashInfo)
{
    m_storedCrashes.append(crashInfo);
    m_crashDatabase[crashInfo.crashId] = crashInfo;
    
    // Limit stored crashes
    while (m_storedCrashes.size() > m_maxStoredCrashes) {
        CrashInfo removed = m_storedCrashes.takeFirst();
        m_crashDatabase.remove(removed.crashId);
    }
    
    // Save to disk
    QString crashDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/crashes";
    QDir().mkpath(crashDir);
    
    QString crashFile = crashDir + "/" + crashInfo.crashId + ".json";
    QFile file(crashFile);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject crashObj;
        crashObj["crashId"] = crashInfo.crashId;
        crashObj["type"] = static_cast<int>(crashInfo.type);
        crashObj["timestamp"] = crashInfo.timestamp.toString(Qt::ISODate);
        crashObj["applicationVersion"] = crashInfo.applicationVersion;
        crashObj["operatingSystem"] = crashInfo.operatingSystem;
        crashObj["crashLocation"] = crashInfo.crashLocation;
        crashObj["stackTrace"] = crashInfo.stackTrace;
        crashObj["errorMessage"] = crashInfo.errorMessage;
        crashObj["memoryUsage"] = crashInfo.memoryUsage;
        crashObj["uptime"] = crashInfo.uptime;
        crashObj["lastAction"] = crashInfo.lastAction;
        crashObj["wasReported"] = crashInfo.wasReported;
        
        QJsonDocument doc(crashObj);
        file.write(doc.toJson());
        file.close();
    }
}

void CrashReporter::loadStoredCrashes()
{
    QString crashDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/crashes";
    QDir dir(crashDir);
    
    if (!dir.exists()) return;
    
    QStringList crashFiles = dir.entryList(QStringList() << "*.json", QDir::Files);
    
    for (const QString& fileName : crashFiles) {
        QFile file(dir.absoluteFilePath(fileName));
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonObject crashObj = doc.object();
            
            CrashInfo crashInfo;
            crashInfo.crashId = crashObj["crashId"].toString();
            crashInfo.type = static_cast<CrashType>(crashObj["type"].toInt());
            crashInfo.timestamp = QDateTime::fromString(crashObj["timestamp"].toString(), Qt::ISODate);
            crashInfo.applicationVersion = crashObj["applicationVersion"].toString();
            crashInfo.operatingSystem = crashObj["operatingSystem"].toString();
            crashInfo.crashLocation = crashObj["crashLocation"].toString();
            crashInfo.stackTrace = crashObj["stackTrace"].toString();
            crashInfo.errorMessage = crashObj["errorMessage"].toString();
            crashInfo.memoryUsage = crashObj["memoryUsage"].toVariant().toLongLong();
            crashInfo.uptime = crashObj["uptime"].toVariant().toLongLong();
            crashInfo.lastAction = crashObj["lastAction"].toString();
            crashInfo.wasReported = crashObj["wasReported"].toBool();
            
            m_storedCrashes.append(crashInfo);
            m_crashDatabase[crashInfo.crashId] = crashInfo;
            
            file.close();
        }
    }
    
    qCDebug(crashReporter) << "Loaded" << m_storedCrashes.size() << "stored crashes";
}

void CrashReporter::saveStabilityMetrics()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    
    QString metricsFile = configDir + "/stability_metrics.json";
    QFile file(metricsFile);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject metricsObj;
        metricsObj["totalSessions"] = m_stabilityMetrics.totalSessions;
        metricsObj["crashCount"] = m_stabilityMetrics.crashCount;
        metricsObj["hangCount"] = m_stabilityMetrics.hangCount;
        metricsObj["totalUptime"] = static_cast<qint64>(m_stabilityMetrics.totalUptime);
        metricsObj["averageSessionTime"] = static_cast<qint64>(m_stabilityMetrics.averageSessionTime);
        metricsObj["crashRate"] = m_stabilityMetrics.crashRate;
        metricsObj["level"] = static_cast<int>(m_stabilityMetrics.level);
        metricsObj["lastCrash"] = m_stabilityMetrics.lastCrash.toString(Qt::ISODate);
        metricsObj["consecutiveFailures"] = m_consecutiveFailures;
        
        QJsonDocument doc(metricsObj);
        file.write(doc.toJson());
        file.close();
    }
}

void CrashReporter::loadStabilityMetrics()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString metricsFile = configDir + "/stability_metrics.json";
    
    QFile file(metricsFile);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject metricsObj = doc.object();
        
        m_stabilityMetrics.totalSessions = metricsObj["totalSessions"].toInt();
        m_stabilityMetrics.crashCount = metricsObj["crashCount"].toInt();
        m_stabilityMetrics.hangCount = metricsObj["hangCount"].toInt();
        m_stabilityMetrics.totalUptime = metricsObj["totalUptime"].toVariant().toLongLong();
        m_stabilityMetrics.averageSessionTime = metricsObj["averageSessionTime"].toVariant().toLongLong();
        m_stabilityMetrics.crashRate = metricsObj["crashRate"].toDouble();
        m_stabilityMetrics.level = static_cast<StabilityLevel>(metricsObj["level"].toInt());
        m_stabilityMetrics.lastCrash = QDateTime::fromString(metricsObj["lastCrash"].toString(), Qt::ISODate);
        m_consecutiveFailures = metricsObj["consecutiveFailures"].toInt();
        
        file.close();
    }
}

void CrashReporter::updateStabilityMetrics()
{
    // Calculate crash rate
    if (m_stabilityMetrics.totalSessions > 0) {
        m_stabilityMetrics.crashRate = static_cast<double>(m_stabilityMetrics.crashCount) / m_stabilityMetrics.totalSessions;
    }
    
    // Calculate stability level
    m_stabilityMetrics.level = calculateStabilityLevel();
    
    emit stabilityMetricsUpdated(m_stabilityMetrics);
}

CrashReporter::StabilityLevel CrashReporter::calculateStabilityLevel() const
{
    if (m_stabilityMetrics.crashRate > 0.1) {
        return STABILITY_CRITICAL;
    } else if (m_stabilityMetrics.crashRate > 0.05) {
        return STABILITY_POOR;
    } else if (m_stabilityMetrics.crashRate > 0.02) {
        return STABILITY_FAIR;
    } else if (m_stabilityMetrics.crashRate > 0.01) {
        return STABILITY_GOOD;
    } else {
        return STABILITY_EXCELLENT;
    }
}

QByteArray CrashReporter::prepareCrashReport(const CrashInfo& crashInfo) const
{
    QJsonObject report;
    report["crashId"] = crashInfo.crashId;
    report["type"] = static_cast<int>(crashInfo.type);
    report["timestamp"] = crashInfo.timestamp.toString(Qt::ISODate);
    report["applicationVersion"] = crashInfo.applicationVersion;
    report["operatingSystem"] = crashInfo.operatingSystem;
    report["crashLocation"] = crashInfo.crashLocation;
    report["stackTrace"] = crashInfo.stackTrace;
    report["errorMessage"] = crashInfo.errorMessage;
    report["memoryUsage"] = crashInfo.memoryUsage;
    report["uptime"] = crashInfo.uptime;
    report["lastAction"] = crashInfo.lastAction;
    
    // Add system info
    QJsonObject systemInfo;
    for (auto it = crashInfo.systemInfo.constBegin(); it != crashInfo.systemInfo.constEnd(); ++it) {
        systemInfo[it.key()] = it.value();
    }
    report["systemInfo"] = systemInfo;
    
    // Add loaded modules
    QJsonArray modules;
    for (const QString& module : crashInfo.loadedModules) {
        modules.append(module);
    }
    report["loadedModules"] = modules;
    
    QJsonDocument doc(report);
    return doc.toJson(QJsonDocument::Compact);
}

bool CrashReporter::shouldReportCrash(const CrashInfo& crashInfo) const
{
    return m_userConsent && !crashInfo.wasReported && !m_reportingEndpoint.isEmpty();
}

QString CrashReporter::getOperatingSystemInfo() const
{
    return QString("%1 %2 (%3)").arg(QSysInfo::productType(), QSysInfo::productVersion(), QSysInfo::currentCpuArchitecture());
}

QString CrashReporter::getApplicationVersion() const
{
    return QCoreApplication::applicationVersion();
}

QString CrashReporter::getCPUInfo() const
{
    // Simplified CPU info - would need platform-specific implementation
    return QString("CPU: %1").arg(QSysInfo::currentCpuArchitecture());
}

QString CrashReporter::getMemoryInfo() const
{
    return QString("Memory: %1 MB").arg(getCurrentMemoryUsage() / (1024 * 1024));
}

QString CrashReporter::getGPUInfo() const
{
    // Simplified GPU info - would need platform-specific implementation
    return QString("GPU: Unknown");
}

// Static signal handlers
void CrashReporter::signalHandler(int signal)
{
    if (s_instance) {
        CrashType type = CRASH_UNKNOWN;
        QString location = QString("Signal %1").arg(signal);
        
        switch (signal) {
            case SIGSEGV:
                type = CRASH_SEGFAULT;
                location = "Segmentation fault";
                break;
            case SIGABRT:
                type = CRASH_ABORT;
                location = "Abort signal";
                break;
            case SIGFPE:
                type = CRASH_EXCEPTION;
                location = "Floating point exception";
                break;
            case SIGILL:
                type = CRASH_EXCEPTION;
                location = "Illegal instruction";
                break;
#ifndef Q_OS_WIN
            case SIGBUS:
                type = CRASH_SEGFAULT;
                location = "Bus error";
                break;
#endif
        }
        
        s_instance->handleCrash(type, location);
        s_instance->generateCrashDump(s_instance->generateCrashId());
    }
    
    // Restore default handler and re-raise
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}

void CrashReporter::terminateHandler()
{
    if (s_instance) {
        s_instance->handleCrash(CRASH_EXCEPTION, "std::terminate called");
    }
    std::abort();
}

void CrashReporter::unexpectedHandler()
{
    if (s_instance) {
        s_instance->handleCrash(CRASH_EXCEPTION, "Unexpected exception");
    }
    std::terminate();
}

// Slot implementations
void CrashReporter::onStabilityTimerTimeout()
{
    checkForMemoryLeaks();
    checkForHangs();
    analyzeStabilityTrends();
}

void CrashReporter::onReportingReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString crashId = reply->property("crashId").toString();
    bool success = (reply->error() == QNetworkReply::NoError);
    
    if (success) {
        // Mark crash as reported
        for (auto& crash : m_storedCrashes) {
            if (crash.crashId == crashId) {
                crash.wasReported = true;
                break;
            }
        }
    }
    
    emit crashReported(crashId, success);
    
    m_pendingReports.removeAll(reply);
    reply->deleteLater();
    
    qCDebug(crashReporter) << "Async crash report completed:" << crashId << "Success:" << success;
}

void CrashReporter::onMemoryMonitorTimeout()
{
    qint64 currentMemory = getCurrentMemoryUsage();
    m_memoryHistory.append(currentMemory);
    
    if (currentMemory > m_peakMemoryUsage) {
        m_peakMemoryUsage = currentMemory;
    }
    
    // Keep memory history limited
    if (m_memoryHistory.size() > MAX_MEMORY_HISTORY_SIZE) {
        m_memoryHistory.removeFirst();
    }
}

void CrashReporter::checkForMemoryLeaks()
{
    if (m_memoryHistory.size() < 10) return;
    
    // Simple memory leak detection - check for consistent growth
    qint64 recent = 0, older = 0;
    int halfSize = m_memoryHistory.size() / 2;
    
    for (int i = 0; i < halfSize; ++i) {
        older += m_memoryHistory[i];
    }
    for (int i = halfSize; i < m_memoryHistory.size(); ++i) {
        recent += m_memoryHistory[i];
    }
    
    qint64 avgOlder = older / halfSize;
    qint64 avgRecent = recent / (m_memoryHistory.size() - halfSize);
    
    // If recent average is significantly higher, potential leak
    if (avgRecent > avgOlder * 1.5) {
        qCWarning(crashReporter) << "Potential memory leak detected - Recent avg:" 
                                 << avgRecent << "Older avg:" << avgOlder;
        
        reportThreat(THREAT_MEMORY_LEAK, "Memory usage consistently increasing", QString());
    }
}

void CrashReporter::checkForHangs()
{
    // Simplified hang detection - would need more sophisticated implementation
    static QDateTime lastCheck = QDateTime::currentDateTime();
    QDateTime now = QDateTime::currentDateTime();
    
    if (lastCheck.msecsTo(now) > m_stabilityCheckInterval * 2) {
        // Potential hang detected
        m_stabilityMetrics.hangCount++;
        qCWarning(crashReporter) << "Potential hang detected";
    }
    
    lastCheck = now;
}

void CrashReporter::analyzeStabilityTrends()
{
    updateStabilityMetrics();
    
    // Check if stability is degrading
    if (m_stabilityMetrics.level == STABILITY_CRITICAL || m_stabilityMetrics.level == STABILITY_POOR) {
        if (!m_safeModeEnabled && m_stabilityMetrics.crashCount > 5) {
            qCWarning(crashReporter) << "Stability degradation detected, considering safe mode";
        }
    }
}

// Configuration setters
void CrashReporter::setMaxStoredCrashes(int maxCrashes)
{
    m_maxStoredCrashes = maxCrashes;
}

int CrashReporter::getMaxStoredCrashes() const
{
    return m_maxStoredCrashes;
}

void CrashReporter::setReportingTimeout(int timeoutMs)
{
    m_reportingTimeout = timeoutMs;
}

int CrashReporter::getReportingTimeout() const
{
    return m_reportingTimeout;
}

void CrashReporter::setStabilityCheckInterval(int intervalMs)
{
    m_stabilityCheckInterval = intervalMs;
    m_stabilityTimer->setInterval(intervalMs);
}

void CrashReporter::setUserConsent(bool hasConsent)
{
    m_userConsent = hasConsent;
    qCDebug(crashReporter) << "User consent for crash reporting:" << hasConsent;
}

bool CrashReporter::hasUserConsent() const
{
    return m_userConsent;
}

// Data access methods
QList<CrashReporter::CrashInfo> CrashReporter::getStoredCrashes() const
{
    return m_storedCrashes;
}

CrashReporter::CrashInfo CrashReporter::getCrashInfo(const QString& crashId) const
{
    return m_crashDatabase.value(crashId);
}

bool CrashReporter::deleteCrashInfo(const QString& crashId)
{
    if (m_crashDatabase.contains(crashId)) {
        m_crashDatabase.remove(crashId);
        
        for (int i = 0; i < m_storedCrashes.size(); ++i) {
            if (m_storedCrashes[i].crashId == crashId) {
                m_storedCrashes.removeAt(i);
                break;
            }
        }
        
        // Delete file
        QString crashDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/crashes";
        QString crashFile = crashDir + "/" + crashId + ".json";
        QFile::remove(crashFile);
        
        return true;
    }
    
    return false;
}

void CrashReporter::clearAllCrashData()
{
    m_storedCrashes.clear();
    m_crashDatabase.clear();
    
    // Delete all crash files
    QString crashDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/crashes";
    QDir dir(crashDir);
    QStringList crashFiles = dir.entryList(QStringList() << "*.json", QDir::Files);
    
    for (const QString& fileName : crashFiles) {
        dir.remove(fileName);
    }
    
    qCDebug(crashReporter) << "All crash data cleared";
}

int CrashReporter::getStoredCrashCount() const
{
    return m_storedCrashes.size();
}