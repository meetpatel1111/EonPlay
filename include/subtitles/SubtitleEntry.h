#pragma once

#include <QString>
#include <QTime>
#include <QColor>
#include <QFont>
#include <QRect>
#include <QList>

/**
 * @brief Subtitle text formatting information
 */
struct SubtitleFormat
{
    QFont font;
    QColor textColor = Qt::white;
    QColor backgroundColor = Qt::transparent;
    QColor outlineColor = Qt::black;
    int outlineWidth = 1;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    Qt::Alignment alignment = Qt::AlignCenter;
    int marginLeft = 0;
    int marginRight = 0;
    int marginTop = 0;
    int marginBottom = 0;
    
    SubtitleFormat() {
        font.setFamily("Arial");
        font.setPointSize(16);
        font.setBold(false);
    }
    
    /**
     * @brief Check if format is equal to another format
     * @param other Other format to compare
     * @return true if formats are equal
     */
    bool operator==(const SubtitleFormat& other) const;
    
    /**
     * @brief Check if format is different from another format
     * @param other Other format to compare
     * @return true if formats are different
     */
    bool operator!=(const SubtitleFormat& other) const { return !(*this == other); }
};

/**
 * @brief Individual subtitle entry with timing and text information
 */
class SubtitleEntry
{
public:
    SubtitleEntry() = default;
    
    /**
     * @brief Create subtitle entry with basic information
     * @param startTime Start time in milliseconds
     * @param endTime End time in milliseconds
     * @param text Subtitle text
     */
    SubtitleEntry(qint64 startTime, qint64 endTime, const QString& text);
    
    /**
     * @brief Create subtitle entry with formatting
     * @param startTime Start time in milliseconds
     * @param endTime End time in milliseconds
     * @param text Subtitle text
     * @param format Text formatting
     */
    SubtitleEntry(qint64 startTime, qint64 endTime, const QString& text, const SubtitleFormat& format);
    
    // Getters
    qint64 startTime() const { return m_startTime; }
    qint64 endTime() const { return m_endTime; }
    qint64 duration() const { return m_endTime - m_startTime; }
    QString text() const { return m_text; }
    QString plainText() const;
    SubtitleFormat format() const { return m_format; }
    QRect position() const { return m_position; }
    int layer() const { return m_layer; }
    QString style() const { return m_style; }
    QString actor() const { return m_actor; }
    QString effect() const { return m_effect; }
    
    // Setters
    void setStartTime(qint64 startTime) { m_startTime = startTime; }
    void setEndTime(qint64 endTime) { m_endTime = endTime; }
    void setText(const QString& text) { m_text = text; }
    void setFormat(const SubtitleFormat& format) { m_format = format; }
    void setPosition(const QRect& position) { m_position = position; }
    void setLayer(int layer) { m_layer = layer; }
    void setStyle(const QString& style) { m_style = style; }
    void setActor(const QString& actor) { m_actor = actor; }
    void setEffect(const QString& effect) { m_effect = effect; }
    
    /**
     * @brief Check if subtitle is active at given time
     * @param time Time in milliseconds
     * @return true if subtitle should be displayed at this time
     */
    bool isActiveAt(qint64 time) const;
    
    /**
     * @brief Check if subtitle overlaps with another subtitle
     * @param other Other subtitle entry
     * @return true if subtitles overlap in time
     */
    bool overlapsWith(const SubtitleEntry& other) const;
    
    /**
     * @brief Get subtitle text with timing information
     * @return Formatted string with timing and text
     */
    QString toString() const;
    
    /**
     * @brief Check if subtitle entry is valid
     * @return true if entry has valid timing and text
     */
    bool isValid() const;
    
    /**
     * @brief Parse HTML-like tags in subtitle text
     * @return List of text segments with individual formatting
     */
    QList<QPair<QString, SubtitleFormat>> parseFormattedText() const;
    
    /**
     * @brief Convert time in milliseconds to SRT format (HH:MM:SS,mmm)
     * @param timeMs Time in milliseconds
     * @return Formatted time string
     */
    static QString formatTime(qint64 timeMs);
    
    /**
     * @brief Parse time from SRT format to milliseconds
     * @param timeStr Time string in SRT format
     * @return Time in milliseconds, -1 if invalid
     */
    static qint64 parseTime(const QString& timeStr);
    
    /**
     * @brief Convert time in milliseconds to ASS format (H:MM:SS.cc)
     * @param timeMs Time in milliseconds
     * @return Formatted time string for ASS
     */
    static QString formatTimeASS(qint64 timeMs);
    
    /**
     * @brief Parse time from ASS format to milliseconds
     * @param timeStr Time string in ASS format
     * @return Time in milliseconds, -1 if invalid
     */
    static qint64 parseTimeASS(const QString& timeStr);

private:
    qint64 m_startTime = 0;     // Start time in milliseconds
    qint64 m_endTime = 0;       // End time in milliseconds
    QString m_text;             // Subtitle text (may contain formatting tags)
    SubtitleFormat m_format;    // Text formatting
    QRect m_position;           // Position on screen (for positioned subtitles)
    int m_layer = 0;            // Layer/z-order for overlapping subtitles
    QString m_style;            // Style name (for ASS format)
    QString m_actor;            // Actor/speaker name
    QString m_effect;           // Special effects
};

/**
 * @brief Collection of subtitle entries for a subtitle track
 */
class SubtitleTrack
{
public:
    SubtitleTrack() = default;
    
    /**
     * @brief Create subtitle track with metadata
     * @param title Track title
     * @param language Track language code
     */
    SubtitleTrack(const QString& title, const QString& language = QString());
    
    // Track metadata
    QString title() const { return m_title; }
    QString language() const { return m_language; }
    QString encoding() const { return m_encoding; }
    QString format() const { return m_format; }
    bool isDefault() const { return m_isDefault; }
    bool isForced() const { return m_isForced; }
    
    void setTitle(const QString& title) { m_title = title; }
    void setLanguage(const QString& language) { m_language = language; }
    void setEncoding(const QString& encoding) { m_encoding = encoding; }
    void setFormat(const QString& format) { m_format = format; }
    void setDefault(bool isDefault) { m_isDefault = isDefault; }
    void setForced(bool isForced) { m_isForced = isForced; }
    
    // Subtitle entries management
    void addEntry(const SubtitleEntry& entry);
    void removeEntry(int index);
    void clearEntries();
    
    QList<SubtitleEntry> entries() const { return m_entries; }
    SubtitleEntry entryAt(int index) const;
    int entryCount() const { return m_entries.size(); }
    bool isEmpty() const { return m_entries.isEmpty(); }
    
    /**
     * @brief Get active subtitle entries at given time
     * @param time Time in milliseconds
     * @return List of active subtitle entries
     */
    QList<SubtitleEntry> getActiveEntries(qint64 time) const;
    
    /**
     * @brief Get next subtitle entry after given time
     * @param time Time in milliseconds
     * @return Next subtitle entry, invalid entry if none found
     */
    SubtitleEntry getNextEntry(qint64 time) const;
    
    /**
     * @brief Get previous subtitle entry before given time
     * @param time Time in milliseconds
     * @return Previous subtitle entry, invalid entry if none found
     */
    SubtitleEntry getPreviousEntry(qint64 time) const;
    
    /**
     * @brief Sort entries by start time
     */
    void sortByTime();
    
    /**
     * @brief Validate all entries and remove invalid ones
     * @return Number of invalid entries removed
     */
    int validateEntries();
    
    /**
     * @brief Get total duration of subtitle track
     * @return Duration in milliseconds
     */
    qint64 totalDuration() const;
    
    /**
     * @brief Shift all subtitle timings by offset
     * @param offsetMs Offset in milliseconds (can be negative)
     */
    void shiftTiming(qint64 offsetMs);
    
    /**
     * @brief Scale all subtitle timings by factor
     * @param factor Scaling factor (1.0 = no change)
     */
    void scaleTiming(double factor);

private:
    QString m_title;
    QString m_language;
    QString m_encoding = "UTF-8";
    QString m_format;
    bool m_isDefault = false;
    bool m_isForced = false;
    
    QList<SubtitleEntry> m_entries;
};