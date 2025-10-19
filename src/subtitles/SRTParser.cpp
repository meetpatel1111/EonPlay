#include "subtitles/SRTParser.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(srtParser, "eonplay.subtitles.srt")

SRTParser::SRTParser()
{
    qCDebug(srtParser) << "SRT Parser created";
}

bool SRTParser::canParse(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    
    // Check extension
    if (fileInfo.suffix().toLower() != "srt") {
        return false;
    }
    
    // Validate file
    if (!validateFile(filePath)) {
        return false;
    }
    
    // Read content and check structure
    QString encoding;
    QString content = readFileContent(filePath, encoding);
    return canParseContent(content);
}

bool SRTParser::canParseContent(const QString& content) const
{
    if (content.isEmpty()) {
        return false;
    }
    
    return isValidSRTStructure(content);
}

SubtitleTrack SRTParser::parseFile(const QString& filePath, SubtitleParseResult& result)
{
    result = SubtitleParseResult();
    
    // Validate file
    if (!validateFile(filePath)) {
        result.errorMessage = "File validation failed";
        return SubtitleTrack();
    }
    
    // Read content
    QString encoding;
    QString content = readFileContent(filePath, encoding);
    if (content.isEmpty()) {
        result.errorMessage = "Could not read file content";
        return SubtitleTrack();
    }
    
    result.detectedEncoding = encoding;
    result.detectedFormat = "SRT";
    
    // Parse content
    return parseContent(content, result);
}

SubtitleTrack SRTParser::parseContent(const QString& content, SubtitleParseResult& result)
{
    result = SubtitleParseResult();
    result.detectedFormat = "SRT";
    
    if (!validateContent(content)) {
        result.errorMessage = "Content validation failed";
        return SubtitleTrack();
    }
    
    if (!isValidSRTStructure(content)) {
        result.errorMessage = "Invalid SRT format structure";
        return SubtitleTrack();
    }
    
    SubtitleTrack track("SRT Subtitles");
    track.setFormat("SRT");
    
    // Split content into blocks (separated by double newlines)
    QStringList blocks = content.split(QRegularExpression("\\n\\s*\\n"), Qt::SkipEmptyParts);
    
    qCDebug(srtParser) << "Found" << blocks.size() << "subtitle blocks";
    
    for (const QString& block : blocks) {
        SubtitleEntry entry;
        if (parseSubtitleBlock(block.trimmed(), entry)) {
            track.addEntry(entry);
            result.parsedEntries++;
        } else {
            result.skippedEntries++;
            qCWarning(srtParser) << "Failed to parse subtitle block:" << block.left(50) + "...";
        }
        
        // Check limits
        if (result.parsedEntries >= maxEntries()) {
            qCWarning(srtParser) << "Reached maximum entries limit:" << maxEntries();
            break;
        }
    }
    
    // Sort entries by time
    track.sortByTime();
    
    // Validate parsed entries
    int removedInvalid = track.validateEntries();
    if (removedInvalid > 0) {
        result.skippedEntries += removedInvalid;
        result.parsedEntries -= removedInvalid;
    }
    
    result.success = result.parsedEntries > 0;
    if (!result.success && result.errorMessage.isEmpty()) {
        result.errorMessage = "No valid subtitle entries found";
    }
    
    qCDebug(srtParser) << "SRT parsing completed:" << result.summary();
    
    return track;
}

bool SRTParser::parseSubtitleBlock(const QString& block, SubtitleEntry& entry) const
{
    if (block.isEmpty()) {
        return false;
    }
    
    QStringList lines = block.split('\n');
    if (lines.size() < 3) {
        qCWarning(srtParser) << "Subtitle block has too few lines:" << lines.size();
        return false;
    }
    
    // First line should be sequence number
    bool ok;
    int sequenceNumber = lines[0].trimmed().toInt(&ok);
    if (!ok || sequenceNumber <= 0) {
        qCWarning(srtParser) << "Invalid sequence number:" << lines[0];
        return false;
    }
    
    // Second line should be timing
    qint64 startTime, endTime;
    if (!parseTimingLine(lines[1], startTime, endTime)) {
        qCWarning(srtParser) << "Invalid timing line:" << lines[1];
        return false;
    }
    
    // Remaining lines are subtitle text
    QStringList textLines;
    for (int i = 2; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (!line.isEmpty()) {
            textLines.append(line);
        }
    }
    
    if (textLines.isEmpty()) {
        qCWarning(srtParser) << "No subtitle text found";
        return false;
    }
    
    QString text = textLines.join('\n');
    text = cleanSRTText(text);
    
    // Validate text safety
    if (!isSafeText(text)) {
        qCWarning(srtParser) << "Unsafe subtitle text detected";
        return false;
    }
    
    // Parse formatting
    SubtitleFormat format;
    QString plainText = parseSRTFormatting(text, format);
    
    // Create entry
    entry = SubtitleEntry(startTime, endTime, plainText, format);
    
    return entry.isValid();
}

bool SRTParser::parseTimingLine(const QString& timingLine, qint64& startTime, qint64& endTime) const
{
    // SRT timing format: 00:00:20,000 --> 00:00:24,400
    QRegularExpression timingRegex("^([0-9]{2}:[0-9]{2}:[0-9]{2},[0-9]{3})\\s*-->\\s*([0-9]{2}:[0-9]{2}:[0-9]{2},[0-9]{3})");
    QRegularExpressionMatch match = timingRegex.match(timingLine.trimmed());
    
    if (!match.hasMatch()) {
        return false;
    }
    
    QString startTimeStr = match.captured(1);
    QString endTimeStr = match.captured(2);
    
    startTime = SubtitleEntry::parseTime(startTimeStr);
    endTime = SubtitleEntry::parseTime(endTimeStr);
    
    if (startTime < 0 || endTime < 0 || endTime <= startTime) {
        return false;
    }
    
    return true;
}

bool SRTParser::isValidSRTStructure(const QString& content) const
{
    if (content.isEmpty()) {
        return false;
    }
    
    // Check for basic SRT patterns
    QRegularExpression srtPattern("\\d+\\s*\\n\\s*\\d{2}:\\d{2}:\\d{2},\\d{3}\\s*-->\\s*\\d{2}:\\d{2}:\\d{2},\\d{3}");
    
    if (!srtPattern.match(content).hasMatch()) {
        return false;
    }
    
    // Count the number of timing lines vs sequence numbers
    QRegularExpression sequenceRegex("^\\d+$", QRegularExpression::MultilineOption);
    QRegularExpression timingRegex("\\d{2}:\\d{2}:\\d{2},\\d{3}\\s*-->\\s*\\d{2}:\\d{2}:\\d{2},\\d{3}");
    
    int sequenceCount = content.count(sequenceRegex);
    int timingCount = content.count(timingRegex);
    
    // Should have roughly equal numbers (allowing for some parsing tolerance)
    if (sequenceCount == 0 || timingCount == 0) {
        return false;
    }
    
    double ratio = static_cast<double>(sequenceCount) / timingCount;
    if (ratio < 0.8 || ratio > 1.2) {
        qCWarning(srtParser) << "SRT structure validation failed - sequence/timing ratio:" << ratio;
        return false;
    }
    
    return true;
}

QString SRTParser::cleanSRTText(const QString& text) const
{
    QString cleaned = text;
    
    // Remove BOM if present
    if (cleaned.startsWith(QChar(0xFEFF))) {
        cleaned = cleaned.mid(1);
    }
    
    // Normalize line endings
    cleaned.replace("\r\n", "\n");
    cleaned.replace("\r", "\n");
    
    // Clean up whitespace but preserve intentional line breaks
    QStringList lines = cleaned.split('\n');
    QStringList cleanedLines;
    
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (!trimmedLine.isEmpty()) {
            cleanedLines.append(trimmedLine);
        }
    }
    
    return cleanedLines.join('\n');
}

QString SRTParser::parseSRTFormatting(const QString& text, SubtitleFormat& format) const
{
    QString plainText = text;
    
    // SRT supports basic HTML-like tags
    // <b>, <i>, <u>, <font color="...">
    
    // Check for bold
    if (plainText.contains("<b>") || plainText.contains("<B>")) {
        format.bold = true;
        plainText.remove(QRegularExpression("</?[bB]>"));
    }
    
    // Check for italic
    if (plainText.contains("<i>") || plainText.contains("<I>")) {
        format.italic = true;
        plainText.remove(QRegularExpression("</?[iI]>"));
    }
    
    // Check for underline
    if (plainText.contains("<u>") || plainText.contains("<U>")) {
        format.underline = true;
        plainText.remove(QRegularExpression("</?[uU]>"));
    }
    
    // Parse font color
    QRegularExpression colorRegex("<font\\s+color\\s*=\\s*[\"']?([^\"'>]+)[\"']?[^>]*>", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch colorMatch = colorRegex.match(plainText);
    if (colorMatch.hasMatch()) {
        QString colorName = colorMatch.captured(1);
        QColor color(colorName);
        if (color.isValid()) {
            format.textColor = color;
        }
        plainText.remove(QRegularExpression("</?font[^>]*>", QRegularExpression::CaseInsensitiveOption));
    }
    
    // Remove any remaining HTML tags for security
    plainText.remove(QRegularExpression("<[^>]*>"));
    
    // Decode HTML entities
    plainText.replace("&lt;", "<");
    plainText.replace("&gt;", ">");
    plainText.replace("&amp;", "&");
    plainText.replace("&quot;", "\"");
    plainText.replace("&#39;", "'");
    
    return plainText.trimmed();
}