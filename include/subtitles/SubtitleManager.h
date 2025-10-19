#pragma once

#include "SubtitleEntry.h"
#include "ISubtitleParser.h"
#include <QObject>
#include <QTimer>
#include <QHash>
#include <memory>

/**
 * @brief Subtitle loading result
 */
struct SubtitleLoadResult
{
    bool success = false;
    QString errorMessage;
    QString filePath;
    QString detectedFormat;
    QString detectedEncoding;
    int loadedEntries = 0;
    qint64 totalDuration = 0;
    
    QString summary() const;
};

/**
 * @brief Central manager for subtitle operations in EonPlay
 * 
 * Handles subtitle loading, parsing, validation, and security checks.
 * Manages multiple subtitle tracks and provides timing synchronization.
 */
class SubtitleManager : public QObject
{
    Q_OBJECT

public:
    explicit SubtitleManager(QObject* parent = nullptr);
    ~SubtitleManager() override;
    
    /**
     * @brief Initialize subtitle manager
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Load subtitle file
     * @param filePath Path to subtitle file
     * @param result Load result information (output)
     * @return true if loading successful
     */
    bool loadSubtitleFile(const QString& filePath, SubtitleLoadResult& result);
    
    /**
     * @brief Load subtitle from content
     * @param content Subtitle content
     * @param format Subtitle format hint
     * @param result Load result information (output)
     * @return true if loading successful
     */
    bool loadSubtitleContent(const QString& content, const QString& format, SubtitleLoadResult& result);
    
    /**
     * @brief Get all loaded subtitle tracks
     * @return List of subtitle tracks
     */
    QList<SubtitleTrack> getSubtitleTracks() const { return m_tracks; }
    
    /**
     * @brief Get subtitle track by index
     * @param index Track index
     * @return Subtitle track or empty track if invalid index
     */
    SubtitleTrack getSubtitleTrack(int index) const;
    
    /**
     * @brief Get number of loaded tracks
     * @return Number of tracks
     */
    int trackCount() const { return m_tracks.size(); }
    
    /**
     * @brief Set active subtitle track
     * @param index Track index (-1 to disable subtitles)
     */
    void setActiveTrack(int index);
    
    /**
     * @brief Get active track index
     * @return Active track index (-1 if disabled)
     */
    int activeTrack() const { return m_activeTrackIndex; }
    
    /**
     * @brief Check if subtitles are enabled
     * @return true if subtitles are enabled
     */
    bool isEnabled() const { return m_enabled; }
    
    /**
     * @brief Enable or disable subtitles
     * @param enabled true to enable subtitles
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Get active subtitle entries at current time
     * @param time Time in milliseconds
     * @return List of active subtitle entries
     */
    QList<SubtitleEntry> getActiveSubtitles(qint64 time) const;
    
    /**
     * @brief Clear all loaded subtitle tracks
     */
    void clearSubtitles();
    
    /**
     * @brief Get supported subtitle file extensions
     * @return List of supported extensions
     */
    QStringList getSupportedExtensions() const;
    
    /**
     * @brief Validate subtitle file for security
     * @param filePath Path to subtitle file
     * @return true if file is safe to load
     */
    bool validateSubtitleFile(const QString& filePath) const;
    
    /**
     * @brief Get subtitle timing offset
     * @return Timing offset in milliseconds
     */
    qint64 timingOffset() const { return m_timingOffset; }
    
    /**
     * @brief Set subtitle timing offset
     * @param offsetMs Timing offset in milliseconds
     */
    void setTimingOffset(qint64 offsetMs);
    
    /**
     * @brief Shift subtitle timing for active track
     * @param offsetMs Offset in milliseconds
     */
    void shiftTiming(qint64 offsetMs);
    
    /**
     * @brief Scale subtitle timing for active track
     * @param factor Scaling factor
     */
    void scaleTiming(double factor);

public slots:
    /**
     * @brief Handle media position change
     * @param position Current media position in milliseconds
     */
    void onPositionChanged(qint64 position);
    
    /**
     * @brief Handle media loaded
     * @param mediaPath Path to loaded media file
     */
    void onMediaLoaded(const QString& mediaPath);

signals:
    /**
     * @brief Emitted when subtitle track is loaded
     * @param trackIndex Index of loaded track
     * @param track Loaded subtitle track
     */
    void subtitleTrackLoaded(int trackIndex, const SubtitleTrack& track);
    
    /**
     * @brief Emitted when active track changes
     * @param trackIndex New active track index (-1 if disabled)
     */
    void activeTrackChanged(int trackIndex);
    
    /**
     * @brief Emitted when subtitles are enabled/disabled
     * @param enabled true if subtitles are enabled
     */
    void subtitlesEnabledChanged(bool enabled);
    
    /**
     * @brief Emitted when active subtitles change
     * @param time Current time in milliseconds
     * @param entries List of active subtitle entries
     */
    void activeSubtitlesChanged(qint64 time, const QList<SubtitleEntry>& entries);
    
    /**
     * @brief Emitted when subtitle timing offset changes
     * @param offsetMs New timing offset in milliseconds
     */
    void timingOffsetChanged(qint64 offsetMs);
    
    /**
     * @brief Emitted when subtitle loading fails
     * @param filePath Path to failed file
     * @param errorMessage Error description
     */
    void subtitleLoadFailed(const QString& filePath, const QString& errorMessage);

private:
    /**
     * @brief Initialize subtitle parsers
     */
    void initializeParsers();
    
    /**
     * @brief Auto-detect subtitle files for media
     * @param mediaPath Path to media file
     * @return List of detected subtitle files
     */
    QStringList autoDetectSubtitles(const QString& mediaPath) const;
    
    /**
     * @brief Perform security validation on subtitle content
     * @param content Subtitle content
     * @param filePath File path for context
     * @return true if content is safe
     */
    bool performSecurityValidation(const QString& content, const QString& filePath) const;
    
    /**
     * @brief Check for malicious patterns in subtitle content
     * @param content Subtitle content
     * @return true if content appears safe
     */
    bool checkMaliciousPatterns(const QString& content) const;
    
    /**
     * @brief Sanitize subtitle content
     * @param content Input content
     * @return Sanitized content
     */
    QString sanitizeContent(const QString& content) const;
    
    QList<SubtitleTrack> m_tracks;
    int m_activeTrackIndex;
    bool m_enabled;
    qint64 m_timingOffset;
    qint64 m_currentPosition;
    
    // Security settings
    qint64 m_maxFileSize;
    int m_maxEntries;
    QStringList m_maliciousPatterns;
    QStringList m_allowedTags;
    
    // Last active subtitles (for change detection)
    QList<SubtitleEntry> m_lastActiveSubtitles;
    
    bool m_initialized;
};