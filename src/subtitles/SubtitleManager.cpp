#include "subtitles/SubtitleManager.h"
#include "subtitles/SRTParser.h"
#include "subtitles/ASSParser.h"
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(subtitleManager, "eonplay.subtitles.manager")

// SubtitleLoadResult Implementation
QString SubtitleLoadResult::summary() const
{
    if (success) {
        return QString("Loaded %1 entries from %2 (%3, %4)")
            .arg(loadedEntries)
            .arg(QFileInfo(filePath).fileName())
            .arg(detectedFormat)
            .arg(detectedEncoding);
    } else {
        return QString("Failed to load %1: %2")
            .arg(QFileInfo(filePath).fileName())
            .arg(errorMessage);
    }
}

// SubtitleManager Implementation
SubtitleManager::SubtitleManager(QObject* parent)
    : QObject(parent)
    , m_activeTrackIndex(-1)
    , m_enabled(true)
    , m_timingOffset(0)
    , m_currentPosition(0)
    , m_maxFileSize(10 * 1024 * 1024) // 10MB
    , m_maxEntries(50000) // 50k entries
    , m_initialized(false)
{
    // Initialize malicious patterns for security
    m_maliciousPatterns << "javascript:" << "vbscript:" << "<script" << "</script>"
                       << "eval(" << "document." << "window." << "alert("
                       << "file://" << "ftp://" << "data:" << "blob:"
                       << "onload=" << "onerror=" << "onclick=" << "onmouseover="
                       << "\\x" << "\\u" << "%3c" << "%3e" << "&#x" << "&#"
                       << "expression(" << "behavior:" << "binding:"
                       << "import(" << "require(" << "fetch(" << "XMLHttpRequest";
    
    // Initialize allowed HTML-like tags
    m_allowedTags << "b" << "i" << "u" << "s" << "font" << "color" << "size"
                  << "br" << "p" << "div" << "span" << "strong" << "em";
    
    qCDebug(subtitleManager) << "SubtitleManager created";
}

SubtitleManager::~SubtitleManager()
{
    qCDebug(subtitleManager) << "SubtitleManager destroyed";
}

bool SubtitleManager::initialize()
{
    if (m_initialized) {
        return true;
    }
    
    // Initialize subtitle parsers
    initializeParsers();
    
    m_initialized = true;
    
    qCDebug(subtitleManager) << "SubtitleManager initialized";
    return true;
}

bool SubtitleManager::loadSubtitleFile(const QString& filePath, SubtitleLoadResult& result)
{
    result = SubtitleLoadResult();
    result.filePath = filePath;
    
    // Validate file path
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        result.errorMessage = "Subtitle file does not exist";
        return false;
    }
    
    // Security validation
    if (!validateSubtitleFile(filePath)) {
        result.errorMessage = "Subtitle file failed security validation";
        emit subtitleLoadFailed(filePath, result.errorMessage);
        return false;
    }
    
    // Find appropriate parser
    ISubtitleParser* parser = SubtitleParserFactory::getParserForFile(filePath);
    if (!parser) {
        result.errorMessage = "No suitable parser found for subtitle format";
        emit subtitleLoadFailed(filePath, result.errorMessage);
        return false;
    }
    
    // Parse subtitle file
    SubtitleParseResult parseResult;
    SubtitleTrack track = parser->parseFile(filePath, parseResult);
    
    if (!parseResult.success) {
        result.errorMessage = parseResult.errorMessage;
        emit subtitleLoadFailed(filePath, result.errorMessage);
        return false;
    }
    
    // Additional security validation on parsed content
    for (int i = 0; i < track.entryCount(); ++i) {
        SubtitleEntry entry = track.entryAt(i);
        if (!checkMaliciousPatterns(entry.text())) {
            result.errorMessage = "Malicious content detected in subtitle entry";
            emit subtitleLoadFailed(filePath, result.errorMessage);
            return false;
        }
    }
    
    // Set track metadata
    if (track.title().isEmpty()) {
        track.setTitle(fileInfo.baseName());
    }
    
    // Add track
    m_tracks.append(track);
    int trackIndex = m_tracks.size() - 1;
    
    // Set as active if it's the first track
    if (m_activeTrackIndex == -1) {
        setActiveTrack(trackIndex);
    }
    
    // Fill result
    result.success = true;
    result.detectedFormat = parseResult.detectedFormat;
    result.detectedEncoding = parseResult.detectedEncoding;
    result.loadedEntries = parseResult.parsedEntries;
    result.totalDuration = track.totalDuration();
    
    emit subtitleTrackLoaded(trackIndex, track);
    
    qCDebug(subtitleManager) << "Subtitle file loaded:" << result.summary();
    
    return true;
}

bool SubtitleManager::loadSubtitleContent(const QString& content, const QString& format, SubtitleLoadResult& result)
{
    result = SubtitleLoadResult();
    result.filePath = QString("(content)");
    
    // Security validation
    if (!performSecurityValidation(content, "(content)")) {
        result.errorMessage = "Subtitle content failed security validation";
        return false;
    }
    
    // Find appropriate parser
    ISubtitleParser* parser = nullptr;
    if (!format.isEmpty()) {
        parser = SubtitleParserFactory::getParserForExtension(format);
    }
    if (!parser) {
        parser = SubtitleParserFactory::getParserForContent(content);
    }
    
    if (!parser) {
        result.errorMessage = "No suitable parser found for subtitle content";
        return false;
    }
    
    // Parse subtitle content
    SubtitleParseResult parseResult;
    SubtitleTrack track = parser->parseContent(content, parseResult);
    
    if (!parseResult.success) {
        result.errorMessage = parseResult.errorMessage;
        return false;
    }
    
    // Set track metadata
    track.setTitle("Loaded Content");
    
    // Add track
    m_tracks.append(track);
    int trackIndex = m_tracks.size() - 1;
    
    // Set as active if it's the first track
    if (m_activeTrackIndex == -1) {
        setActiveTrack(trackIndex);
    }
    
    // Fill result
    result.success = true;
    result.detectedFormat = parseResult.detectedFormat;
    result.detectedEncoding = parseResult.detectedEncoding;
    result.loadedEntries = parseResult.parsedEntries;
    result.totalDuration = track.totalDuration();
    
    emit subtitleTrackLoaded(trackIndex, track);
    
    qCDebug(subtitleManager) << "Subtitle content loaded:" << result.summary();
    
    return true;
}

SubtitleTrack SubtitleManager::getSubtitleTrack(int index) const
{
    if (index >= 0 && index < m_tracks.size()) {
        return m_tracks.at(index);
    }
    return SubtitleTrack(); // Empty track
}

void SubtitleManager::setActiveTrack(int index)
{
    if (index < -1 || index >= m_tracks.size()) {
        return;
    }
    
    if (m_activeTrackIndex != index) {
        m_activeTrackIndex = index;
        emit activeTrackChanged(index);
        
        // Update active subtitles for current position
        onPositionChanged(m_currentPosition);
        
        qCDebug(subtitleManager) << "Active subtitle track changed to:" << index;
    }
}

void SubtitleManager::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit subtitlesEnabledChanged(enabled);
        
        if (!enabled) {
            // Clear active subtitles when disabled
            m_lastActiveSubtitles.clear();
            emit activeSubtitlesChanged(m_currentPosition, QList<SubtitleEntry>());
        } else {
            // Update active subtitles when re-enabled
            onPositionChanged(m_currentPosition);
        }
        
        qCDebug(subtitleManager) << "Subtitles" << (enabled ? "enabled" : "disabled");
    }
}

QList<SubtitleEntry> SubtitleManager::getActiveSubtitles(qint64 time) const
{
    QList<SubtitleEntry> activeEntries;
    
    if (!m_enabled || m_activeTrackIndex < 0 || m_activeTrackIndex >= m_tracks.size()) {
        return activeEntries;
    }
    
    const SubtitleTrack& track = m_tracks.at(m_activeTrackIndex);
    qint64 adjustedTime = time + m_timingOffset;
    
    activeEntries = track.getActiveEntries(adjustedTime);
    
    return activeEntries;
}

void SubtitleManager::clearSubtitles()
{
    m_tracks.clear();
    m_activeTrackIndex = -1;
    m_lastActiveSubtitles.clear();
    
    emit activeTrackChanged(-1);
    emit activeSubtitlesChanged(m_currentPosition, QList<SubtitleEntry>());
    
    qCDebug(subtitleManager) << "All subtitle tracks cleared";
}

QStringList SubtitleManager::getSupportedExtensions() const
{
    return SubtitleParserFactory::getSupportedExtensions();
}

bool SubtitleManager::validateSubtitleFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    
    // Check file size
    if (fileInfo.size() > m_maxFileSize) {
        qCWarning(subtitleManager) << "Subtitle file too large:" << fileInfo.size();
        return false;
    }
    
    // Check extension
    QString extension = fileInfo.suffix().toLower();
    QStringList supportedExts = getSupportedExtensions();
    if (!supportedExts.contains(extension)) {
        qCWarning(subtitleManager) << "Unsupported subtitle extension:" << extension;
        return false;
    }
    
    // Get parser and validate
    ISubtitleParser* parser = SubtitleParserFactory::getParserForFile(filePath);
    if (!parser) {
        qCWarning(subtitleManager) << "No parser available for file:" << filePath;
        return false;
    }
    
    return parser->validateFile(filePath);
}

void SubtitleManager::setTimingOffset(qint64 offsetMs)
{
    if (m_timingOffset != offsetMs) {
        m_timingOffset = offsetMs;
        emit timingOffsetChanged(offsetMs);
        
        // Update active subtitles with new timing
        onPositionChanged(m_currentPosition);
        
        qCDebug(subtitleManager) << "Subtitle timing offset set to:" << offsetMs << "ms";
    }
}

void SubtitleManager::shiftTiming(qint64 offsetMs)
{
    if (m_activeTrackIndex >= 0 && m_activeTrackIndex < m_tracks.size()) {
        m_tracks[m_activeTrackIndex].shiftTiming(offsetMs);
        
        // Update active subtitles
        onPositionChanged(m_currentPosition);
        
        qCDebug(subtitleManager) << "Subtitle timing shifted by:" << offsetMs << "ms";
    }
}

void SubtitleManager::scaleTiming(double factor)
{
    if (m_activeTrackIndex >= 0 && m_activeTrackIndex < m_tracks.size()) {
        m_tracks[m_activeTrackIndex].scaleTiming(factor);
        
        // Update active subtitles
        onPositionChanged(m_currentPosition);
        
        qCDebug(subtitleManager) << "Subtitle timing scaled by factor:" << factor;
    }
}

void SubtitleManager::onPositionChanged(qint64 position)
{
    m_currentPosition = position;
    
    if (!m_enabled) {
        return;
    }
    
    QList<SubtitleEntry> activeEntries = getActiveSubtitles(position);
    
    // Check if active subtitles changed
    bool changed = (activeEntries.size() != m_lastActiveSubtitles.size());
    if (!changed) {
        for (int i = 0; i < activeEntries.size(); ++i) {
            if (activeEntries[i].text() != m_lastActiveSubtitles[i].text() ||
                activeEntries[i].startTime() != m_lastActiveSubtitles[i].startTime()) {
                changed = true;
                break;
            }
        }
    }
    
    if (changed) {
        m_lastActiveSubtitles = activeEntries;
        emit activeSubtitlesChanged(position, activeEntries);
    }
}

void SubtitleManager::onMediaLoaded(const QString& mediaPath)
{
    // Auto-detect subtitle files
    QStringList subtitleFiles = autoDetectSubtitles(mediaPath);
    
    for (const QString& subtitleFile : subtitleFiles) {
        SubtitleLoadResult result;
        if (loadSubtitleFile(subtitleFile, result)) {
            qCDebug(subtitleManager) << "Auto-loaded subtitle:" << result.summary();
        }
    }
}

void SubtitleManager::initializeParsers()
{
    // Register default parsers
    SubtitleParserFactory::registerParser(std::make_unique<SRTParser>());
    SubtitleParserFactory::registerParser(std::make_unique<ASSParser>());
    
    qCDebug(subtitleManager) << "Subtitle parsers initialized";
}

QStringList SubtitleManager::autoDetectSubtitles(const QString& mediaPath) const
{
    QStringList subtitleFiles;
    
    QFileInfo mediaInfo(mediaPath);
    QString baseName = mediaInfo.completeBaseName();
    QString directory = mediaInfo.absolutePath();
    
    QStringList supportedExts = getSupportedExtensions();
    
    // Look for subtitle files with same base name
    for (const QString& ext : supportedExts) {
        QString subtitlePath = QDir(directory).absoluteFilePath(baseName + "." + ext);
        if (QFileInfo::exists(subtitlePath)) {
            subtitleFiles.append(subtitlePath);
        }
    }
    
    // Look for common subtitle naming patterns
    QStringList patterns = {
        baseName + ".eng",
        baseName + ".en",
        baseName + ".english",
        baseName + ".sub",
        baseName + ".subtitle"
    };
    
    for (const QString& pattern : patterns) {
        for (const QString& ext : supportedExts) {
            QString subtitlePath = QDir(directory).absoluteFilePath(pattern + "." + ext);
            if (QFileInfo::exists(subtitlePath) && !subtitleFiles.contains(subtitlePath)) {
                subtitleFiles.append(subtitlePath);
            }
        }
    }
    
    return subtitleFiles;
}

bool SubtitleManager::performSecurityValidation(const QString& content, const QString& filePath) const
{
    Q_UNUSED(filePath)
    
    // Check content size
    if (content.length() > m_maxFileSize) {
        qCWarning(subtitleManager) << "Subtitle content too large:" << content.length();
        return false;
    }
    
    // Check for malicious patterns
    if (!checkMaliciousPatterns(content)) {
        return false;
    }
    
    // Check for excessive HTML tags
    QRegularExpression tagRegex("<[^>]*>");
    QRegularExpressionMatchIterator matches = tagRegex.globalMatch(content);
    int tagCount = 0;
    while (matches.hasNext()) {
        matches.next();
        tagCount++;
        if (tagCount > 1000) { // Arbitrary limit
            qCWarning(subtitleManager) << "Too many HTML tags in subtitle content";
            return false;
        }
    }
    
    return true;
}

bool SubtitleManager::checkMaliciousPatterns(const QString& content) const
{
    QString lowerContent = content.toLower();
    
    for (const QString& pattern : m_maliciousPatterns) {
        if (lowerContent.contains(pattern)) {
            qCWarning(subtitleManager) << "Malicious pattern detected:" << pattern;
            return false;
        }
    }
    
    return true;
}

QString SubtitleManager::sanitizeContent(const QString& content) const
{
    QString sanitized = content;
    
    // Remove null characters
    sanitized.remove(QChar('\0'));
    
    // Remove or escape potentially dangerous characters
    sanitized.replace("&", "&amp;");
    sanitized.replace("<", "&lt;");
    sanitized.replace(">", "&gt;");
    sanitized.replace("\"", "&quot;");
    sanitized.replace("'", "&#39;");
    
    // Remove control characters except common ones
    for (int i = 0; i < sanitized.length(); ++i) {
        QChar ch = sanitized.at(i);
        if (ch.isControl() && ch != '\n' && ch != '\r' && ch != '\t') {
            sanitized[i] = ' ';
        }
    }
    
    return sanitized;
}