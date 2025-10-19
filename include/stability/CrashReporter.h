#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QTimer>
#include <QProcess>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * @brief Comprehensive crash reporting and stability monitoring system
 * 
 * Provides crash detection, dump collection, automatic reporting, stability
 * monitoring, and recovery mechanisms for EonPlay media player.
 */
class CrashReporter : public QObject
{
    Q_OBJECT

public:
    enum CrashType {
        CRASH_SEGFAULT,
        CRASH_ABORT,
        CRASH_EXCEPTION,
        CRASH_HANG,
        CRASH_MEMORY_LEAK,
        CRASH_STACK_OVERFLOW,
        CRASH_UNKNOWN
    };

    enum StabilityLevel {
        STABILITY_EXCELLENT,
        STABILITY_GOOD,
        STABILITY_FAIR,
        STABILITY_POOR,
        STABILITY_CRITICAL
    };

    struct CrashInfo {
        QString crashId;
        CrashType type;
        QDateTime timestamp;
        QString applicationVersion;
        QString operatingSystem;
        QString crashLocation;
        QString stackTrace;
        QString errorMessage;
        QStringList loadedModules;
        qint64 memoryUsage = 0;
        qint64 uptime = 0;
        QString lastAction;
        QMap<QString, QString> systemInfo;
        QMap<QString, QString> userSettings;
        bool wasReported = false;
    };

    struct StabilityMetrics {
        int totalSessions = 0;
        int crashCount = 0;
        int hangCount = 0;
        qint64 totalUptime = 0;
        qint64 averageSessionTime = 0;
        double crashRate = 0.0;
        StabilityLevel level = STABILITY_EXCELLENT;
        QDateTime lastCrash;
        QStringList frequentCrashLocations;
        QMap<QString, int> crashTypeCount;
    };

    explicit CrashReporter(QObject *parent = nullptr);
    ~CrashReporter();

    // Crash detection and handling
    void installCrashHandlers();
    void uninstallCrashHandlers();
    bool isCrashHandlerInstalled() const;
    void handleCrash(CrashType type, const QString& location, const QString& message = QString());
    void generateCrashDump(const QString& crashId);

    // Crash reporting
    void enableAutomaticReporting(bool enabled);
    bool isAutomaticReportingEnabled() const;
    void setReportingEndpoint(const QString& url);
    QString getReportingEndpoint() const;
    bool reportCrash(const CrashInfo& crashInfo);
    void reportCrashAsync(const CrashInfo& crashInfo);

    // Stability monitoring
    void startStabilityMonitoring();
    void stopStabilityMonitoring();
    bool isStabilityMonitoringActive() const;
    StabilityMetrics getStabilityMetrics() const;
    void recordSessionStart();
    void recordSessionEnd();
    void recordUserAction(const QString& action);

    // Crash data management
    QList<CrashInfo> getStoredCrashes() const;
    CrashInfo getCrashInfo(const QString& crashId) const;
    bool deleteCrashInfo(const QString& crashId);
    void clearAllCrashData();
    int getStoredCrashCount() const;

    // Recovery and safe mode
    bool shouldStartInSafeMode() const;
    void enableSafeMode(bool enabled);
    bool isSafeModeEnabled() const;
    void recordSuccessfulStart();
    void recordFailedStart();
    int getConsecutiveFailures() const;

    // System information collection
    QMap<QString, QString> collectSystemInfo() const;
    QStringList collectLoadedModules() const;
    QString collectStackTrace() const;
    qint64 getCurrentMemoryUsage() const;
    qint64 getApplicationUptime() const;

    // Configuration
    void setMaxStoredCrashes(int maxCrashes);
    int getMaxStoredCrashes() const;
    void setReportingTimeout(int timeoutMs);
    int getReportingTimeout() const;
    void setStabilityCheckInterval(int intervalMs);
    void setUserConsent(bool hasConsent);
    bool hasUserConsent() const;

signals:
    void crashDetected(const CrashInfo& crashInfo);
    void crashReported(const QString& crashId, bool success);
    void stabilityMetricsUpdated(const StabilityMetrics& metrics);
    void safeModeTriggered();
    void recoveryCompleted();
    void reportingStateChanged(bool enabled);

private slots:
    void onStabilityTimerTimeout();
    void onReportingReplyFinished();
    void onReportingReplyError();
    void onMemoryMonitorTimeout();

private:
    // Crash handling helpers
    void setupSignalHandlers();
    void setupExceptionHandlers();
    QString generateCrashId() const;
    void storeCrashInfo(const CrashInfo& crashInfo);
    void loadStoredCrashes();
    void saveStabilityMetrics();
    void loadStabilityMetrics();
    
    // System information helpers
    QString getOperatingSystemInfo() const;
    QString getApplicationVersion() const;
    QStringList getEnvironmentInfo() const;
    QString getCPUInfo() const;
    QString getMemoryInfo() const;
    QString getGPUInfo() const;
    
    // Stability analysis
    void updateStabilityMetrics();
    void analyzeStabilityTrends();
    StabilityLevel calculateStabilityLevel() const;
    void checkForMemoryLeaks();
    void checkForHangs();
    
    // Recovery helpers
    void performRecovery();
    void resetFailureCount();
    void backupUserData();
    void restoreUserData();
    
    // Reporting helpers
    QByteArray prepareCrashReport(const CrashInfo& crashInfo) const;
    void sendCrashReport(const QByteArray& reportData, const QString& crashId);
    bool shouldReportCrash(const CrashInfo& crashInfo) const;
    
    // Member variables
    bool m_crashHandlerInstalled;
    bool m_automaticReportingEnabled;
    bool m_stabilityMonitoringActive;
    bool m_safeModeEnabled;
    bool m_userConsent;
    
    // Configuration
    QString m_reportingEndpoint;
    int m_maxStoredCrashes;
    int m_reportingTimeout;
    int m_stabilityCheckInterval;
    
    // Crash data
    QList<CrashInfo> m_storedCrashes;
    QMap<QString, CrashInfo> m_crashDatabase;
    
    // Stability tracking
    StabilityMetrics m_stabilityMetrics;
    QDateTime m_sessionStartTime;
    QString m_lastUserAction;
    QStringList m_sessionActions;
    
    // Recovery tracking
    int m_consecutiveFailures;
    QDateTime m_lastSuccessfulStart;
    
    // Monitoring
    std::unique_ptr<QTimer> m_stabilityTimer;
    std::unique_ptr<QTimer> m_memoryMonitor;
    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    QList<QNetworkReply*> m_pendingReports;
    
    // Memory tracking
    qint64 m_initialMemoryUsage;
    qint64 m_peakMemoryUsage;
    QList<qint64> m_memoryHistory;
    
    // Constants
    static const int DEFAULT_MAX_STORED_CRASHES = 50;
    static const int DEFAULT_REPORTING_TIMEOUT_MS = 30000;
    static const int DEFAULT_STABILITY_CHECK_INTERVAL_MS = 60000;
    static const int DEFAULT_MEMORY_CHECK_INTERVAL_MS = 10000;
    static const int SAFE_MODE_FAILURE_THRESHOLD = 3;
    static const int MAX_MEMORY_HISTORY_SIZE = 100;
    
    // Static crash handler functions
    static CrashReporter* s_instance;
    static void signalHandler(int signal);
    static void terminateHandler();
    static void unexpectedHandler();
};