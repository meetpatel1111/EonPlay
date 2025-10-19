#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QTimer>
#include <QStringList>
#include <QMap>

/**
 * @brief Comprehensive parental control and content filtering system
 * 
 * Provides content rating filtering, time-based access controls, usage monitoring,
 * and parental oversight features for EonPlay media player.
 */
class ParentalControlManager : public QObject
{
    Q_OBJECT

public:
    enum ContentRating {
        RATING_UNRATED,
        RATING_G,           // General Audiences
        RATING_PG,          // Parental Guidance
        RATING_PG13,        // Parents Strongly Cautioned
        RATING_R,           // Restricted
        RATING_NC17,        // Adults Only
        RATING_TV_Y,        // TV - All Children
        RATING_TV_Y7,       // TV - Directed to Older Children
        RATING_TV_G,        // TV - General Audience
        RATING_TV_PG,       // TV - Parental Guidance Suggested
        RATING_TV_14,       // TV - Parents Strongly Cautioned
        RATING_TV_MA        // TV - Mature Audience Only
    };

    enum AccessLevel {
        ACCESS_UNRESTRICTED,
        ACCESS_SUPERVISED,
        ACCESS_RESTRICTED,
        ACCESS_BLOCKED
    };

    struct TimeRestriction {
        QTime startTime;
        QTime endTime;
        QStringList allowedDays; // Monday, Tuesday, etc.
        int maxDailyMinutes = 0; // 0 = unlimited
        int maxWeeklyMinutes = 0; // 0 = unlimited
        bool isActive = true;
    };

    struct ContentFilter {
        QStringList blockedKeywords;
        QStringList blockedGenres;
        QStringList allowedGenres;
        ContentRating maxRating = RATING_UNRATED;
        bool blockUnratedContent = false;
        bool blockExplicitContent = true;
        QStringList trustedSources;
        QStringList blockedSources;
    };

    struct UsageStatistics {
        QDateTime sessionStart;
        int dailyMinutesUsed = 0;
        int weeklyMinutesUsed = 0;
        int monthlyMinutesUsed = 0;
        QMap<QString, int> genreUsage;
        QMap<ContentRating, int> ratingUsage;
        QStringList recentlyWatched;
        QStringList blockedAttempts;
    };

    struct UserProfile {
        QString profileName;
        QString profileId;
        QDateTime birthDate;
        AccessLevel accessLevel = ACCESS_UNRESTRICTED;
        TimeRestriction timeRestrictions;
        ContentFilter contentFilters;
        UsageStatistics usageStats;
        QString parentalPin;
        bool requiresSupervision = false;
        QStringList allowedFeatures;
        QStringList blockedFeatures;
    };

    explicit ParentalControlManager(QObject *parent = nullptr);
    ~ParentalControlManager();

    // Profile management
    QString createUserProfile(const QString& name, const QDateTime& birthDate);
    bool deleteUserProfile(const QString& profileId);
    bool setActiveProfile(const QString& profileId);
    QString getActiveProfileId() const;
    UserProfile getActiveProfile() const;
    UserProfile getUserProfile(const QString& profileId) const;
    QStringList getAllProfileIds() const;
    bool updateUserProfile(const UserProfile& profile);

    // Parental controls
    void enableParentalControls(bool enabled);
    bool isParentalControlsEnabled() const;
    bool setParentalPin(const QString& pin);
    bool verifyParentalPin(const QString& pin);
    bool hasParentalPin() const;
    void lockParentalSettings(bool locked);
    bool areParentalSettingsLocked() const;

    // Content filtering
    bool isContentAllowed(const QString& filePath) const;
    bool isContentAllowed(const QString& title, const QString& genre, ContentRating rating) const;
    ContentRating detectContentRating(const QString& filePath) const;
    ContentRating parseRatingString(const QString& ratingStr) const;
    QString getRatingDescription(ContentRating rating) const;
    QStringList getBlockedReasons(const QString& filePath) const;

    // Time restrictions
    bool isAccessAllowed() const;
    bool isTimeAllowed() const;
    int getRemainingDailyMinutes() const;
    int getRemainingWeeklyMinutes() const;
    void recordUsageTime(int minutes);
    QTime getNextAllowedTime() const;

    // Usage monitoring
    void startUsageSession();
    void endUsageSession();
    bool isSessionActive() const;
    UsageStatistics getCurrentUsageStats() const;
    void recordContentAccess(const QString& title, const QString& genre, ContentRating rating);
    void recordBlockedAttempt(const QString& content, const QString& reason);

    // Feature restrictions
    bool isFeatureAllowed(const QString& featureName) const;
    void setFeatureRestriction(const QString& featureName, bool allowed);
    QStringList getAllowedFeatures() const;
    QStringList getBlockedFeatures() const;

    // Supervision and notifications
    void enableSupervisionMode(bool enabled);
    bool isSupervisionModeEnabled() const;
    void requestParentalApproval(const QString& content, const QString& reason);
    void approveContent(const QString& content);
    void denyContent(const QString& content);
    QStringList getPendingApprovals() const;

    // Reports and analytics
    QMap<QString, QVariant> generateUsageReport(const QDateTime& startDate, const QDateTime& endDate) const;
    QStringList getRecentActivity(int maxItems = 50) const;
    QMap<QString, int> getTopGenres(int maxGenres = 10) const;
    QMap<ContentRating, int> getRatingDistribution() const;
    int getTotalScreenTime(const QDateTime& startDate, const QDateTime& endDate) const;

    // Configuration
    void setDefaultProfile(const QString& profileId);
    QString getDefaultProfile() const;
    void setAutoLockTimeout(int minutes);
    int getAutoLockTimeout() const;
    void enableUsageReports(bool enabled);
    bool areUsageReportsEnabled() const;

signals:
    void parentalControlsStateChanged(bool enabled);
    void activeProfileChanged(const QString& profileId);
    void contentBlocked(const QString& content, const QString& reason);
    void timeRestrictionTriggered();
    void usageSessionStarted();
    void usageSessionEnded(int totalMinutes);
    void parentalApprovalRequested(const QString& content, const QString& reason);
    void parentalApprovalGranted(const QString& content);
    void parentalApprovalDenied(const QString& content);
    void usageLimitReached(const QString& limitType);
    void supervisionRequired(const QString& action);

private slots:
    void onUsageTimerTimeout();
    void onAutoLockTimerTimeout();

private:
    // Profile management helpers
    QString generateProfileId() const;
    void saveUserProfiles();
    void loadUserProfiles();
    UserProfile createDefaultProfile(const QString& name, const QDateTime& birthDate);
    
    // Content analysis helpers
    ContentRating analyzeContentRating(const QString& filePath) const;
    QStringList extractKeywords(const QString& title, const QString& description) const;
    bool containsBlockedKeywords(const QStringList& keywords, const QStringList& blockedKeywords) const;
    bool isGenreAllowed(const QString& genre) const;
    bool isSourceTrusted(const QString& source) const;
    
    // Time management helpers
    bool isCurrentTimeAllowed() const;
    bool isDayAllowed() const;
    void updateUsageStatistics();
    void resetDailyUsage();
    void resetWeeklyUsage();
    void resetMonthlyUsage();
    
    // Security helpers
    QString hashPin(const QString& pin) const;
    bool verifyPinHash(const QString& pin, const QString& hash) const;
    void lockApplication();
    void unlockApplication();
    
    // Supervision helpers
    void notifyParent(const QString& message);
    void logActivity(const QString& activity);
    void checkSupervisionRequirements();
    
    // Member variables
    bool m_parentalControlsEnabled;
    bool m_parentalSettingsLocked;
    bool m_supervisionModeEnabled;
    bool m_usageReportsEnabled;
    bool m_sessionActive;
    
    // Profile management
    QString m_activeProfileId;
    QString m_defaultProfileId;
    QMap<QString, UserProfile> m_userProfiles;
    
    // Security
    QString m_parentalPinHash;
    int m_autoLockTimeoutMinutes;
    
    // Usage tracking
    QTimer* m_usageTimer;
    QTimer* m_autoLockTimer;
    QDateTime m_sessionStartTime;
    QDateTime m_lastUsageReset;
    
    // Pending approvals
    QMap<QString, QString> m_pendingApprovals; // content -> reason
    QStringList m_approvedContent;
    
    // Activity logging
    QStringList m_activityLog;
    
    // Constants
    static const int USAGE_UPDATE_INTERVAL_MS = 60000; // 1 minute
    static const int DEFAULT_AUTO_LOCK_MINUTES = 30;
    static const int MAX_ACTIVITY_LOG_SIZE = 1000;
    static const int MAX_RECENT_WATCHED = 100;
};