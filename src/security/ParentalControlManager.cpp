#include "security/ParentalControlManager.h"
#include <QSettings>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QLoggingCategory>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

Q_LOGGING_CATEGORY(parentalControl, "eonplay.security.parental")

ParentalControlManager::ParentalControlManager(QObject *parent)
    : QObject(parent)
    , m_parentalControlsEnabled(false)
    , m_parentalSettingsLocked(false)
    , m_supervisionModeEnabled(false)
    , m_usageReportsEnabled(true)
    , m_sessionActive(false)
    , m_autoLockTimeoutMinutes(DEFAULT_AUTO_LOCK_MINUTES)
    , m_usageTimer(new QTimer(this))
    , m_autoLockTimer(new QTimer(this))
{
    // Setup usage tracking timer
    m_usageTimer->setInterval(USAGE_UPDATE_INTERVAL_MS);
    connect(m_usageTimer, &QTimer::timeout, this, &ParentalControlManager::onUsageTimerTimeout);
    
    // Setup auto-lock timer
    m_autoLockTimer->setSingleShot(true);
    connect(m_autoLockTimer, &QTimer::timeout, this, &ParentalControlManager::onAutoLockTimerTimeout);
    
    // Load existing profiles
    loadUserProfiles();
    
    qCDebug(parentalControl) << "ParentalControlManager initialized";
}

ParentalControlManager::~ParentalControlManager()
{
    if (m_sessionActive) {
        endUsageSession();
    }
    saveUserProfiles();
}

// Profile management
QString ParentalControlManager::createUserProfile(const QString& name, const QDateTime& birthDate)
{
    QString profileId = generateProfileId();
    UserProfile profile = createDefaultProfile(name, birthDate);
    profile.profileId = profileId;
    
    m_userProfiles[profileId] = profile;
    saveUserProfiles();
    
    qCDebug(parentalControl) << "Created user profile:" << name << "(" << profileId << ")";
    return profileId;
}

bool ParentalControlManager::deleteUserProfile(const QString& profileId)
{
    if (!m_userProfiles.contains(profileId)) {
        return false;
    }
    
    // Don't delete if it's the active profile
    if (profileId == m_activeProfileId) {
        m_activeProfileId.clear();
    }
    
    m_userProfiles.remove(profileId);
    saveUserProfiles();
    
    qCDebug(parentalControl) << "Deleted user profile:" << profileId;
    return true;
}

bool ParentalControlManager::setActiveProfile(const QString& profileId)
{
    if (!m_userProfiles.contains(profileId)) {
        return false;
    }
    
    m_activeProfileId = profileId;
    emit activeProfileChanged(profileId);
    
    qCDebug(parentalControl) << "Set active profile:" << profileId;
    return true;
}

QString ParentalControlManager::getActiveProfileId() const
{
    return m_activeProfileId;
}

ParentalControlManager::UserProfile ParentalControlManager::getActiveProfile() const
{
    if (m_activeProfileId.isEmpty() || !m_userProfiles.contains(m_activeProfileId)) {
        return UserProfile(); // Return empty profile
    }
    return m_userProfiles[m_activeProfileId];
}

ParentalControlManager::UserProfile ParentalControlManager::getUserProfile(const QString& profileId) const
{
    return m_userProfiles.value(profileId);
}

QStringList ParentalControlManager::getAllProfileIds() const
{
    return m_userProfiles.keys();
}

bool ParentalControlManager::updateUserProfile(const UserProfile& profile)
{
    if (profile.profileId.isEmpty() || !m_userProfiles.contains(profile.profileId)) {
        return false;
    }
    
    m_userProfiles[profile.profileId] = profile;
    saveUserProfiles();
    
    qCDebug(parentalControl) << "Updated user profile:" << profile.profileId;
    return true;
}

// Parental controls
void ParentalControlManager::enableParentalControls(bool enabled)
{
    if (m_parentalControlsEnabled != enabled) {
        m_parentalControlsEnabled = enabled;
        emit parentalControlsStateChanged(enabled);
        
        qCDebug(parentalControl) << "Parental controls" << (enabled ? "enabled" : "disabled");
    }
}

bool ParentalControlManager::isParentalControlsEnabled() const
{
    return m_parentalControlsEnabled;
}

bool ParentalControlManager::setParentalPin(const QString& pin)
{
    if (pin.length() < 4) {
        return false; // PIN too short
    }
    
    m_parentalPinHash = hashPin(pin);
    
    QSettings settings;
    settings.setValue("parental/pin_hash", m_parentalPinHash);
    
    qCDebug(parentalControl) << "Parental PIN set";
    return true;
}

bool ParentalControlManager::verifyParentalPin(const QString& pin)
{
    return verifyPinHash(pin, m_parentalPinHash);
}

bool ParentalControlManager::hasParentalPin() const
{
    return !m_parentalPinHash.isEmpty();
}

void ParentalControlManager::lockParentalSettings(bool locked)
{
    m_parentalSettingsLocked = locked;
    qCDebug(parentalControl) << "Parental settings" << (locked ? "locked" : "unlocked");
}

bool ParentalControlManager::areParentalSettingsLocked() const
{
    return m_parentalSettingsLocked;
}

// Content filtering
bool ParentalControlManager::isContentAllowed(const QString& filePath) const
{
    if (!m_parentalControlsEnabled) {
        return true;
    }
    
    UserProfile profile = getActiveProfile();
    if (profile.accessLevel == ACCESS_UNRESTRICTED) {
        return true;
    }
    
    // Extract content information
    QFileInfo fileInfo(filePath);
    QString title = fileInfo.baseName();
    ContentRating rating = detectContentRating(filePath);
    
    // Check rating restrictions
    if (rating > profile.contentFilters.maxRating) {
        recordBlockedAttempt(title, "Rating too high");
        return false;
    }
    
    // Check for blocked keywords
    QStringList keywords = extractKeywords(title, "");
    if (containsBlockedKeywords(keywords, profile.contentFilters.blockedKeywords)) {
        recordBlockedAttempt(title, "Contains blocked keywords");
        return false;
    }
    
    return true;
}

bool ParentalControlManager::isContentAllowed(const QString& title, const QString& genre, ContentRating rating) const
{
    if (!m_parentalControlsEnabled) {
        return true;
    }
    
    UserProfile profile = getActiveProfile();
    if (profile.accessLevel == ACCESS_UNRESTRICTED) {
        return true;
    }
    
    // Check rating
    if (rating > profile.contentFilters.maxRating) {
        return false;
    }
    
    // Check genre restrictions
    if (!isGenreAllowed(genre)) {
        return false;
    }
    
    // Check keywords
    QStringList keywords = extractKeywords(title, "");
    if (containsBlockedKeywords(keywords, profile.contentFilters.blockedKeywords)) {
        return false;
    }
    
    return true;
}

ParentalControlManager::ContentRating ParentalControlManager::detectContentRating(const QString& filePath) const
{
    // Simple rating detection based on filename patterns
    QString fileName = QFileInfo(filePath).baseName().toLower();
    
    if (fileName.contains(QRegularExpression("\\b(nc-?17|x-rated|adult)\\b"))) {
        return RATING_NC17;
    }
    if (fileName.contains(QRegularExpression("\\b(r-rated|restricted)\\b"))) {
        return RATING_R;
    }
    if (fileName.contains(QRegularExpression("\\b(pg-?13|pg13)\\b"))) {
        return RATING_PG13;
    }
    if (fileName.contains(QRegularExpression("\\b(pg|parental)\\b"))) {
        return RATING_PG;
    }
    if (fileName.contains(QRegularExpression("\\b(g-rated|general)\\b"))) {
        return RATING_G;
    }
    
    return RATING_UNRATED;
}

ParentalControlManager::ContentRating ParentalControlManager::parseRatingString(const QString& ratingStr) const
{
    QString rating = ratingStr.toUpper().trimmed();
    
    if (rating == "G") return RATING_G;
    if (rating == "PG") return RATING_PG;
    if (rating == "PG-13" || rating == "PG13") return RATING_PG13;
    if (rating == "R") return RATING_R;
    if (rating == "NC-17" || rating == "NC17") return RATING_NC17;
    if (rating == "TV-Y") return RATING_TV_Y;
    if (rating == "TV-Y7") return RATING_TV_Y7;
    if (rating == "TV-G") return RATING_TV_G;
    if (rating == "TV-PG") return RATING_TV_PG;
    if (rating == "TV-14") return RATING_TV_14;
    if (rating == "TV-MA") return RATING_TV_MA;
    
    return RATING_UNRATED;
}

QString ParentalControlManager::getRatingDescription(ContentRating rating) const
{
    switch (rating) {
        case RATING_G: return "General Audiences";
        case RATING_PG: return "Parental Guidance Suggested";
        case RATING_PG13: return "Parents Strongly Cautioned";
        case RATING_R: return "Restricted";
        case RATING_NC17: return "Adults Only";
        case RATING_TV_Y: return "All Children";
        case RATING_TV_Y7: return "Directed to Older Children";
        case RATING_TV_G: return "General Audience";
        case RATING_TV_PG: return "Parental Guidance Suggested";
        case RATING_TV_14: return "Parents Strongly Cautioned";
        case RATING_TV_MA: return "Mature Audience Only";
        default: return "Unrated";
    }
}

QStringList ParentalControlManager::getBlockedReasons(const QString& filePath) const
{
    QStringList reasons;
    
    if (!m_parentalControlsEnabled) {
        return reasons;
    }
    
    UserProfile profile = getActiveProfile();
    if (profile.accessLevel == ACCESS_UNRESTRICTED) {
        return reasons;
    }
    
    QFileInfo fileInfo(filePath);
    QString title = fileInfo.baseName();
    ContentRating rating = detectContentRating(filePath);
    
    if (rating > profile.contentFilters.maxRating) {
        reasons << QString("Rating %1 exceeds maximum allowed rating %2")
                   .arg(getRatingDescription(rating))
                   .arg(getRatingDescription(profile.contentFilters.maxRating));
    }
    
    QStringList keywords = extractKeywords(title, "");
    QStringList blockedFound;
    for (const QString& keyword : keywords) {
        if (profile.contentFilters.blockedKeywords.contains(keyword, Qt::CaseInsensitive)) {
            blockedFound << keyword;
        }
    }
    
    if (!blockedFound.isEmpty()) {
        reasons << QString("Contains blocked keywords: %1").arg(blockedFound.join(", "));
    }
    
    return reasons;
}

// Time restrictions
bool ParentalControlManager::isAccessAllowed() const
{
    return isTimeAllowed() && (getRemainingDailyMinutes() > 0 || getRemainingDailyMinutes() == -1);
}

bool ParentalControlManager::isTimeAllowed() const
{
    if (!m_parentalControlsEnabled) {
        return true;
    }
    
    UserProfile profile = getActiveProfile();
    return isCurrentTimeAllowed() && isDayAllowed();
}

int ParentalControlManager::getRemainingDailyMinutes() const
{
    UserProfile profile = getActiveProfile();
    if (profile.timeRestrictions.maxDailyMinutes == 0) {
        return -1; // Unlimited
    }
    
    return qMax(0, profile.timeRestrictions.maxDailyMinutes - profile.usageStats.dailyMinutesUsed);
}

int ParentalControlManager::getRemainingWeeklyMinutes() const
{
    UserProfile profile = getActiveProfile();
    if (profile.timeRestrictions.maxWeeklyMinutes == 0) {
        return -1; // Unlimited
    }
    
    return qMax(0, profile.timeRestrictions.maxWeeklyMinutes - profile.usageStats.weeklyMinutesUsed);
}

void ParentalControlManager::recordUsageTime(int minutes)
{
    if (m_activeProfileId.isEmpty()) {
        return;
    }
    
    UserProfile& profile = m_userProfiles[m_activeProfileId];
    profile.usageStats.dailyMinutesUsed += minutes;
    profile.usageStats.weeklyMinutesUsed += minutes;
    profile.usageStats.monthlyMinutesUsed += minutes;
    
    saveUserProfiles();
}

// Usage monitoring
void ParentalControlManager::startUsageSession()
{
    if (m_sessionActive) {
        return;
    }
    
    m_sessionActive = true;
    m_sessionStartTime = QDateTime::currentDateTime();
    m_usageTimer->start();
    
    emit usageSessionStarted();
    qCDebug(parentalControl) << "Usage session started";
}

void ParentalControlManager::endUsageSession()
{
    if (!m_sessionActive) {
        return;
    }
    
    m_sessionActive = false;
    m_usageTimer->stop();
    
    int totalMinutes = m_sessionStartTime.secsTo(QDateTime::currentDateTime()) / 60;
    recordUsageTime(totalMinutes);
    
    emit usageSessionEnded(totalMinutes);
    qCDebug(parentalControl) << "Usage session ended, total minutes:" << totalMinutes;
}

bool ParentalControlManager::isSessionActive() const
{
    return m_sessionActive;
}

// Feature restrictions
bool ParentalControlManager::isFeatureAllowed(const QString& featureName) const
{
    if (!m_parentalControlsEnabled) {
        return true;
    }
    
    UserProfile profile = getActiveProfile();
    if (profile.accessLevel == ACCESS_UNRESTRICTED) {
        return true;
    }
    
    return profile.allowedFeatures.contains(featureName) && 
           !profile.blockedFeatures.contains(featureName);
}

// Private helper methods
QString ParentalControlManager::generateProfileId() const
{
    return QString("profile_%1").arg(QDateTime::currentMSecsSinceEpoch());
}

void ParentalControlManager::saveUserProfiles()
{
    QSettings settings;
    settings.beginGroup("parental/profiles");
    
    // Clear existing profiles
    settings.remove("");
    
    // Save each profile
    for (auto it = m_userProfiles.begin(); it != m_userProfiles.end(); ++it) {
        const UserProfile& profile = it.value();
        settings.beginGroup(profile.profileId);
        
        settings.setValue("name", profile.profileName);
        settings.setValue("birthDate", profile.birthDate);
        settings.setValue("accessLevel", static_cast<int>(profile.accessLevel));
        settings.setValue("parentalPin", profile.parentalPin);
        settings.setValue("requiresSupervision", profile.requiresSupervision);
        
        // Save time restrictions
        settings.beginGroup("timeRestrictions");
        settings.setValue("startTime", profile.timeRestrictions.startTime);
        settings.setValue("endTime", profile.timeRestrictions.endTime);
        settings.setValue("allowedDays", profile.timeRestrictions.allowedDays);
        settings.setValue("maxDailyMinutes", profile.timeRestrictions.maxDailyMinutes);
        settings.setValue("maxWeeklyMinutes", profile.timeRestrictions.maxWeeklyMinutes);
        settings.setValue("isActive", profile.timeRestrictions.isActive);
        settings.endGroup();
        
        // Save content filters
        settings.beginGroup("contentFilters");
        settings.setValue("blockedKeywords", profile.contentFilters.blockedKeywords);
        settings.setValue("blockedGenres", profile.contentFilters.blockedGenres);
        settings.setValue("allowedGenres", profile.contentFilters.allowedGenres);
        settings.setValue("maxRating", static_cast<int>(profile.contentFilters.maxRating));
        settings.setValue("blockUnratedContent", profile.contentFilters.blockUnratedContent);
        settings.setValue("blockExplicitContent", profile.contentFilters.blockExplicitContent);
        settings.endGroup();
        
        settings.endGroup();
    }
    
    settings.endGroup();
    
    // Save active profile
    settings.setValue("parental/activeProfile", m_activeProfileId);
}

void ParentalControlManager::loadUserProfiles()
{
    QSettings settings;
    
    // Load parental PIN
    m_parentalPinHash = settings.value("parental/pin_hash").toString();
    
    // Load active profile
    m_activeProfileId = settings.value("parental/activeProfile").toString();
    
    // Load profiles
    settings.beginGroup("parental/profiles");
    QStringList profileIds = settings.childGroups();
    
    for (const QString& profileId : profileIds) {
        settings.beginGroup(profileId);
        
        UserProfile profile;
        profile.profileId = profileId;
        profile.profileName = settings.value("name").toString();
        profile.birthDate = settings.value("birthDate").toDateTime();
        profile.accessLevel = static_cast<AccessLevel>(settings.value("accessLevel", ACCESS_UNRESTRICTED).toInt());
        profile.parentalPin = settings.value("parentalPin").toString();
        profile.requiresSupervision = settings.value("requiresSupervision", false).toBool();
        
        // Load time restrictions
        settings.beginGroup("timeRestrictions");
        profile.timeRestrictions.startTime = settings.value("startTime", QTime(0, 0)).toTime();
        profile.timeRestrictions.endTime = settings.value("endTime", QTime(23, 59)).toTime();
        profile.timeRestrictions.allowedDays = settings.value("allowedDays").toStringList();
        profile.timeRestrictions.maxDailyMinutes = settings.value("maxDailyMinutes", 0).toInt();
        profile.timeRestrictions.maxWeeklyMinutes = settings.value("maxWeeklyMinutes", 0).toInt();
        profile.timeRestrictions.isActive = settings.value("isActive", true).toBool();
        settings.endGroup();
        
        // Load content filters
        settings.beginGroup("contentFilters");
        profile.contentFilters.blockedKeywords = settings.value("blockedKeywords").toStringList();
        profile.contentFilters.blockedGenres = settings.value("blockedGenres").toStringList();
        profile.contentFilters.allowedGenres = settings.value("allowedGenres").toStringList();
        profile.contentFilters.maxRating = static_cast<ContentRating>(settings.value("maxRating", RATING_UNRATED).toInt());
        profile.contentFilters.blockUnratedContent = settings.value("blockUnratedContent", false).toBool();
        profile.contentFilters.blockExplicitContent = settings.value("blockExplicitContent", true).toBool();
        settings.endGroup();
        
        m_userProfiles[profileId] = profile;
        settings.endGroup();
    }
    
    settings.endGroup();
}

ParentalControlManager::UserProfile ParentalControlManager::createDefaultProfile(const QString& name, const QDateTime& birthDate)
{
    UserProfile profile;
    profile.profileName = name;
    profile.birthDate = birthDate;
    profile.accessLevel = ACCESS_UNRESTRICTED;
    profile.requiresSupervision = false;
    
    // Set age-appropriate defaults
    QDate today = QDate::currentDate();
    int age = birthDate.date().daysTo(today) / 365;
    
    if (age < 13) {
        profile.accessLevel = ACCESS_RESTRICTED;
        profile.contentFilters.maxRating = RATING_PG;
        profile.contentFilters.blockExplicitContent = true;
        profile.timeRestrictions.maxDailyMinutes = 120; // 2 hours
        profile.requiresSupervision = true;
    } else if (age < 17) {
        profile.accessLevel = ACCESS_SUPERVISED;
        profile.contentFilters.maxRating = RATING_PG13;
        profile.timeRestrictions.maxDailyMinutes = 180; // 3 hours
    }
    
    return profile;
}

QString ParentalControlManager::hashPin(const QString& pin) const
{
    return QCryptographicHash::hash(pin.toUtf8(), QCryptographicHash::Sha256).toHex();
}

bool ParentalControlManager::verifyPinHash(const QString& pin, const QString& hash) const
{
    return hashPin(pin) == hash;
}

QStringList ParentalControlManager::extractKeywords(const QString& title, const QString& description) const
{
    QStringList keywords;
    
    // Extract words from title
    QStringList titleWords = title.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts);
    keywords.append(titleWords);
    
    // Extract words from description
    QStringList descWords = description.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts);
    keywords.append(descWords);
    
    // Remove duplicates and convert to lowercase
    keywords.removeDuplicates();
    for (QString& keyword : keywords) {
        keyword = keyword.toLower();
    }
    
    return keywords;
}

bool ParentalControlManager::containsBlockedKeywords(const QStringList& keywords, const QStringList& blockedKeywords) const
{
    for (const QString& keyword : keywords) {
        if (blockedKeywords.contains(keyword, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool ParentalControlManager::isGenreAllowed(const QString& genre) const
{
    UserProfile profile = getActiveProfile();
    
    // If blocked genres list is not empty and genre is in it, block
    if (!profile.contentFilters.blockedGenres.isEmpty() && 
        profile.contentFilters.blockedGenres.contains(genre, Qt::CaseInsensitive)) {
        return false;
    }
    
    // If allowed genres list is not empty and genre is not in it, block
    if (!profile.contentFilters.allowedGenres.isEmpty() && 
        !profile.contentFilters.allowedGenres.contains(genre, Qt::CaseInsensitive)) {
        return false;
    }
    
    return true;
}

bool ParentalControlManager::isCurrentTimeAllowed() const
{
    UserProfile profile = getActiveProfile();
    if (!profile.timeRestrictions.isActive) {
        return true;
    }
    
    QTime currentTime = QTime::currentTime();
    QTime startTime = profile.timeRestrictions.startTime;
    QTime endTime = profile.timeRestrictions.endTime;
    
    if (startTime <= endTime) {
        // Same day restriction (e.g., 9:00 AM to 6:00 PM)
        return currentTime >= startTime && currentTime <= endTime;
    } else {
        // Overnight restriction (e.g., 6:00 PM to 9:00 AM next day)
        return currentTime >= startTime || currentTime <= endTime;
    }
}

bool ParentalControlManager::isDayAllowed() const
{
    UserProfile profile = getActiveProfile();
    if (!profile.timeRestrictions.isActive || profile.timeRestrictions.allowedDays.isEmpty()) {
        return true;
    }
    
    QString currentDay = QDate::currentDate().toString("dddd"); // e.g., "Monday"
    return profile.timeRestrictions.allowedDays.contains(currentDay, Qt::CaseInsensitive);
}

void ParentalControlManager::recordBlockedAttempt(const QString& content, const QString& reason)
{
    if (m_activeProfileId.isEmpty()) {
        return;
    }
    
    // This would be implemented to log blocked attempts
    qCDebug(parentalControl) << "Blocked content access:" << content << "Reason:" << reason;
    emit const_cast<ParentalControlManager*>(this)->contentBlocked(content, reason);
}

// Timer slots
void ParentalControlManager::onUsageTimerTimeout()
{
    updateUsageStatistics();
    
    // Check if daily limit reached
    int remaining = getRemainingDailyMinutes();
    if (remaining == 0) {
        emit usageLimitReached("daily");
        endUsageSession();
    }
}

void ParentalControlManager::onAutoLockTimerTimeout()
{
    lockApplication();
}

void ParentalControlManager::updateUsageStatistics()
{
    // Update usage statistics - this would be more comprehensive in a real implementation
    if (m_sessionActive && !m_activeProfileId.isEmpty()) {
        recordUsageTime(1); // Record 1 minute of usage
    }
}

void ParentalControlManager::lockApplication()
{
    // Implementation would lock the application UI
    qCDebug(parentalControl) << "Application auto-locked due to inactivity";
}