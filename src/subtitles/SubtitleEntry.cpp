#include "subtitles/SubtitleEntry.h"
#include <QRegularExpression>
#include <QStringList>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(subtitleEntry, "eonplay.subtitles.entry")

// SubtitleFormat Implementation
bool SubtitleFormat::operator==(const SubtitleFormat& other) const
{
    return font == other.font &&
           textColor == other.textColor &&
           backgroundColor == other.backgroundColor &&
           outlineColor == other.outlineColor &&
           outlineWidth == other.outlineWidth &&
           bold == other.bold &&
           italic == other.italic &&
           underline == other.underline &&
           strikethrough == other.strikethrough &&
           alignment == other.alignment &&
           marginLeft == other.marginLeft &&
           marginRight == other.marginRight &&
           marginTop == other.marginTop &&
           marginBottom == other.marginBottom;
}

// SubtitleEntry Implementation
SubtitleEntry::SubtitleEntry(qint64 startTime, qint64 endTime, const QString& text)
    : m_startTime(startTime)
    , m_endTime(endTime)
    , m_text(text)
{
}

SubtitleEntry::SubtitleEntry(qint64 startTime, qint64 endTime, const QString& text, const SubtitleFormat& format)
    : m_startTime(startTime)
    , m_endTime(endTime)
    , m_text(text)
    , m_format(format)
{
}

QString SubtitleEntry::plainText() const
{
    // Remove HTML-like tags and formatting
    QString plain = m_text;
    
    // Remove common subtitle tags
    QRegularExpression tagRegex("<[^>]*>");
    plain.remove(tagRegex);
    
    // Remove ASS formatting codes
    QRegularExpression assRegex("\\{[^}]*\\}");
    plain.remove(assRegex);
    
    // Clean up whitespace
    plain = plain.simplified();
    
    return plain;
}

bool SubtitleEntry::isActiveAt(qint64 time) const
{
    return time >= m_startTime && time <= m_endTime;
}

bool SubtitleEntry::overlapsWith(const SubtitleEntry& other) const
{
    return !(m_endTime < other.m_startTime || m_startTime > other.m_endTime);
}

QString SubtitleEntry::toString() const
{
    return QString("[%1 --> %2] %3")
        .arg(formatTime(m_startTime))
        .arg(formatTime(m_endTime))
        .arg(plainText());
}

bool SubtitleEntry::isValid() const
{
    return m_startTime >= 0 && 
           m_endTime > m_startTime && 
           !m_text.trimmed().isEmpty();
}

QList<QPair<QString, SubtitleFormat>> SubtitleEntry::parseFormattedText() const
{
    QList<QPair<QString, SubtitleFormat>> segments;
    
    // Simple implementation - just return the whole text with current format
    // In a full implementation, this would parse HTML-like tags and ASS formatting
    if (!m_text.isEmpty()) {
        segments.append(qMakePair(plainText(), m_format));
    }
    
    return segments;
}

QString SubtitleEntry::formatTime(qint64 timeMs)
{
    if (timeMs < 0) {
        return "00:00:00,000";
    }
    
    int hours = static_cast<int>(timeMs / 3600000);
    int minutes = static_cast<int>((timeMs % 3600000) / 60000);
    int seconds = static_cast<int>((timeMs % 60000) / 1000);
    int milliseconds = static_cast<int>(timeMs % 1000);
    
    return QString("%1:%2:%3,%4")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(milliseconds, 3, 10, QChar('0'));
}

qint64 SubtitleEntry::parseTime(const QString& timeStr)
{
    // Parse SRT time format: HH:MM:SS,mmm
    QRegularExpression timeRegex("^(\\d{1,2}):(\\d{2}):(\\d{2})[,\\.](\\d{3})$");
    QRegularExpressionMatch match = timeRegex.match(timeStr.trimmed());
    
    if (!match.hasMatch()) {
        qCWarning(subtitleEntry) << "Invalid time format:" << timeStr;
        return -1;
    }
    
    int hours = match.captured(1).toInt();
    int minutes = match.captured(2).toInt();
    int seconds = match.captured(3).toInt();
    int milliseconds = match.captured(4).toInt();
    
    // Validate ranges
    if (minutes >= 60 || seconds >= 60 || milliseconds >= 1000) {
        qCWarning(subtitleEntry) << "Invalid time values:" << timeStr;
        return -1;
    }
    
    return static_cast<qint64>(hours) * 3600000 + 
           static_cast<qint64>(minutes) * 60000 + 
           static_cast<qint64>(seconds) * 1000 + 
           static_cast<qint64>(milliseconds);
}

QString SubtitleEntry::formatTimeASS(qint64 timeMs)
{
    if (timeMs < 0) {
        return "0:00:00.00";
    }
    
    int hours = static_cast<int>(timeMs / 3600000);
    int minutes = static_cast<int>((timeMs % 3600000) / 60000);
    int seconds = static_cast<int>((timeMs % 60000) / 1000);
    int centiseconds = static_cast<int>((timeMs % 1000) / 10);
    
    return QString("%1:%2:%3.%4")
        .arg(hours)
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(centiseconds, 2, 10, QChar('0'));
}

qint64 SubtitleEntry::parseTimeASS(const QString& timeStr)
{
    // Parse ASS time format: H:MM:SS.cc
    QRegularExpression timeRegex("^(\\d{1,2}):(\\d{2}):(\\d{2})\\.(\\d{2})$");
    QRegularExpressionMatch match = timeRegex.match(timeStr.trimmed());
    
    if (!match.hasMatch()) {
        qCWarning(subtitleEntry) << "Invalid ASS time format:" << timeStr;
        return -1;
    }
    
    int hours = match.captured(1).toInt();
    int minutes = match.captured(2).toInt();
    int seconds = match.captured(3).toInt();
    int centiseconds = match.captured(4).toInt();
    
    // Validate ranges
    if (minutes >= 60 || seconds >= 60 || centiseconds >= 100) {
        qCWarning(subtitleEntry) << "Invalid ASS time values:" << timeStr;
        return -1;
    }
    
    return static_cast<qint64>(hours) * 3600000 + 
           static_cast<qint64>(minutes) * 60000 + 
           static_cast<qint64>(seconds) * 1000 + 
           static_cast<qint64>(centiseconds) * 10;
}

// SubtitleTrack Implementation
SubtitleTrack::SubtitleTrack(const QString& title, const QString& language)
    : m_title(title)
    , m_language(language)
{
}

void SubtitleTrack::addEntry(const SubtitleEntry& entry)
{
    if (entry.isValid()) {
        m_entries.append(entry);
    } else {
        qCWarning(subtitleEntry) << "Attempted to add invalid subtitle entry";
    }
}

void SubtitleTrack::removeEntry(int index)
{
    if (index >= 0 && index < m_entries.size()) {
        m_entries.removeAt(index);
    }
}

void SubtitleTrack::clearEntries()
{
    m_entries.clear();
}

SubtitleEntry SubtitleTrack::entryAt(int index) const
{
    if (index >= 0 && index < m_entries.size()) {
        return m_entries.at(index);
    }
    return SubtitleEntry(); // Invalid entry
}

QList<SubtitleEntry> SubtitleTrack::getActiveEntries(qint64 time) const
{
    QList<SubtitleEntry> activeEntries;
    
    for (const SubtitleEntry& entry : m_entries) {
        if (entry.isActiveAt(time)) {
            activeEntries.append(entry);
        }
    }
    
    return activeEntries;
}

SubtitleEntry SubtitleTrack::getNextEntry(qint64 time) const
{
    SubtitleEntry nextEntry;
    qint64 minStartTime = LLONG_MAX;
    
    for (const SubtitleEntry& entry : m_entries) {
        if (entry.startTime() > time && entry.startTime() < minStartTime) {
            minStartTime = entry.startTime();
            nextEntry = entry;
        }
    }
    
    return nextEntry;
}

SubtitleEntry SubtitleTrack::getPreviousEntry(qint64 time) const
{
    SubtitleEntry previousEntry;
    qint64 maxStartTime = -1;
    
    for (const SubtitleEntry& entry : m_entries) {
        if (entry.startTime() < time && entry.startTime() > maxStartTime) {
            maxStartTime = entry.startTime();
            previousEntry = entry;
        }
    }
    
    return previousEntry;
}

void SubtitleTrack::sortByTime()
{
    std::sort(m_entries.begin(), m_entries.end(), 
              [](const SubtitleEntry& a, const SubtitleEntry& b) {
        return a.startTime() < b.startTime();
    });
}

int SubtitleTrack::validateEntries()
{
    int removedCount = 0;
    
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (!m_entries[i].isValid()) {
            m_entries.removeAt(i);
            removedCount++;
        }
    }
    
    return removedCount;
}

qint64 SubtitleTrack::totalDuration() const
{
    if (m_entries.isEmpty()) {
        return 0;
    }
    
    qint64 maxEndTime = 0;
    for (const SubtitleEntry& entry : m_entries) {
        if (entry.endTime() > maxEndTime) {
            maxEndTime = entry.endTime();
        }
    }
    
    return maxEndTime;
}

void SubtitleTrack::shiftTiming(qint64 offsetMs)
{
    for (SubtitleEntry& entry : m_entries) {
        qint64 newStartTime = entry.startTime() + offsetMs;
        qint64 newEndTime = entry.endTime() + offsetMs;
        
        // Ensure times don't go negative
        if (newStartTime < 0) {
            newEndTime -= newStartTime; // Adjust end time to maintain duration
            newStartTime = 0;
        }
        
        entry.setStartTime(newStartTime);
        entry.setEndTime(newEndTime);
    }
}

void SubtitleTrack::scaleTiming(double factor)
{
    if (factor <= 0) {
        qCWarning(subtitleEntry) << "Invalid scaling factor:" << factor;
        return;
    }
    
    for (SubtitleEntry& entry : m_entries) {
        qint64 newStartTime = static_cast<qint64>(entry.startTime() * factor);
        qint64 newEndTime = static_cast<qint64>(entry.endTime() * factor);
        
        entry.setStartTime(newStartTime);
        entry.setEndTime(newEndTime);
    }
}