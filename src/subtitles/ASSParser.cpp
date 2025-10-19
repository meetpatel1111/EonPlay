#include "subtitles/ASSParser.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(assParser, "eonplay.subtitles.ass")

// ASSStyle Implementation
SubtitleFormat ASSStyle::toSubtitleFormat() const
{
    SubtitleFormat format;
    
    format.font.setFamily(fontName);
    format.font.setPointSize(fontSize);
    format.font.setBold(bold);
    format.font.setItalic(italic);
    format.font.setUnderline(underline);
    format.font.setStrikeOut(strikeOut);
    
    format.textColor = primaryColor;
    format.outlineColor = outlineColor;
    format.outlineWidth = static_cast<int>(outline);
    format.alignment = parseASSAlignment(alignment);
    format.marginLeft = marginL;
    format.marginRight = marginR;
    format.marginTop = marginV;
    format.marginBottom = marginV;
    
    return format;
}

bool ASSStyle::parseFromString(const QString& styleString)
{
    // ASS Style format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
    
    QStringList fields = styleString.split(',');
    if (fields.size() < 23) {
        qCWarning(assParser) << "Invalid ASS style format - not enough fields:" << fields.size();
        return false;
    }
    
    name = fields[0].trimmed();
    fontName = fields[1].trimmed();
    fontSize = fields[2].trimmed().toInt();
    
    // Colors in ASS are in &Hbbggrr& format or decimal
    primaryColor = parseASSColor(fields[3].trimmed());
    secondaryColor = parseASSColor(fields[4].trimmed());
    outlineColor = parseASSColor(fields[5].trimmed());
    shadowColor = parseASSColor(fields[6].trimmed());
    
    bold = (fields[7].trimmed().toInt() != 0);
    italic = (fields[8].trimmed().toInt() != 0);
    underline = (fields[9].trimmed().toInt() != 0);
    strikeOut = (fields[10].trimmed().toInt() != 0);
    
    scaleX = fields[11].trimmed().toDouble();
    scaleY = fields[12].trimmed().toDouble();
    spacing = fields[13].trimmed().toDouble();
    angle = fields[14].trimmed().toDouble();
    borderStyle = fields[15].trimmed().toInt();
    outline = fields[16].trimmed().toDouble();
    shadow = fields[17].trimmed().toDouble();
    alignment = fields[18].trimmed().toInt();
    marginL = fields[19].trimmed().toInt();
    marginR = fields[20].trimmed().toInt();
    marginV = fields[21].trimmed().toInt();
    encoding = fields[22].trimmed().toInt();
    
    return true;
}

QColor ASSStyle::parseASSColor(const QString& colorStr) const
{
    QString color = colorStr.trimmed();
    
    // Handle &Hbbggrr& format
    if (color.startsWith("&H") && color.endsWith("&")) {
        color = color.mid(2, color.length() - 3); // Remove &H and &
        bool ok;
        uint colorValue = color.toUInt(&ok, 16);
        if (ok) {
            // ASS colors are in BGR format
            int blue = (colorValue >> 16) & 0xFF;
            int green = (colorValue >> 8) & 0xFF;
            int red = colorValue & 0xFF;
            return QColor(red, green, blue);
        }
    }
    
    // Handle decimal format
    bool ok;
    uint colorValue = color.toUInt(&ok);
    if (ok) {
        int blue = (colorValue >> 16) & 0xFF;
        int green = (colorValue >> 8) & 0xFF;
        int red = colorValue & 0xFF;
        return QColor(red, green, blue);
    }
    
    // Fallback to white
    return Qt::white;
}

Qt::Alignment ASSStyle::parseASSAlignment(int alignment) const
{
    // ASS alignment values:
    // 1=left, 2=center, 3=right
    // 5=left+middle, 6=center+middle, 7=right+middle
    // 9=left+top, 10=center+top, 11=right+top
    
    Qt::Alignment align = Qt::AlignCenter;
    
    switch (alignment) {
    case 1: case 5: case 9:
        align |= Qt::AlignLeft;
        break;
    case 2: case 6: case 10:
        align |= Qt::AlignHCenter;
        break;
    case 3: case 7: case 11:
        align |= Qt::AlignRight;
        break;
    }
    
    switch (alignment) {
    case 1: case 2: case 3:
        align |= Qt::AlignBottom;
        break;
    case 5: case 6: case 7:
        align |= Qt::AlignVCenter;
        break;
    case 9: case 10: case 11:
        align |= Qt::AlignTop;
        break;
    }
    
    return align;
}

// ASSParser Implementation
ASSParser::ASSParser()
{
    qCDebug(assParser) << "ASS Parser created";
}

bool ASSParser::canParse(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    // Check extension
    if (extension != "ass" && extension != "ssa") {
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

bool ASSParser::canParseContent(const QString& content) const
{
    if (content.isEmpty()) {
        return false;
    }
    
    return isValidASSStructure(content);
}

SubtitleTrack ASSParser::parseFile(const QString& filePath, SubtitleParseResult& result)
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
    result.detectedFormat = "ASS";
    
    // Parse content
    return parseContent(content, result);
}

SubtitleTrack ASSParser::parseContent(const QString& content, SubtitleParseResult& result)
{
    result = SubtitleParseResult();
    result.detectedFormat = "ASS";
    
    if (!validateContent(content)) {
        result.errorMessage = "Content validation failed";
        return SubtitleTrack();
    }
    
    if (!isValidASSStructure(content)) {
        result.errorMessage = "Invalid ASS format structure";
        return SubtitleTrack();
    }
    
    SubtitleTrack track("ASS Subtitles");
    track.setFormat("ASS");
    
    // Parse sections
    QString scriptInfo, styles, events;
    if (!parseSections(content, scriptInfo, styles, events)) {
        result.errorMessage = "Failed to parse ASS sections";
        return SubtitleTrack();
    }
    
    // Parse script info
    parseScriptInfo(scriptInfo, track);
    
    // Parse styles
    QHash<QString, ASSStyle> styleMap = parseStyles(styles);
    
    // Parse events
    if (!parseEvents(events, styleMap, track, result)) {
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = "Failed to parse ASS events";
        }
        return SubtitleTrack();
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
    
    qCDebug(assParser) << "ASS parsing completed:" << result.summary();
    
    return track;
}

bool ASSParser::parseSections(const QString& content, QString& scriptInfo, QString& styles, QString& events) const
{
    // Split content into sections
    QStringList lines = content.split('\n');
    QString currentSection;
    QString currentContent;
    
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        
        // Check for section headers
        if (trimmedLine.startsWith('[') && trimmedLine.endsWith(']')) {
            // Save previous section
            if (currentSection == "Script Info") {
                scriptInfo = currentContent;
            } else if (currentSection == "V4+ Styles" || currentSection == "V4 Styles") {
                styles = currentContent;
            } else if (currentSection == "Events") {
                events = currentContent;
            }
            
            // Start new section
            currentSection = trimmedLine.mid(1, trimmedLine.length() - 2);
            currentContent.clear();
        } else if (!currentSection.isEmpty()) {
            currentContent += line + '\n';
        }
    }
    
    // Save last section
    if (currentSection == "Script Info") {
        scriptInfo = currentContent;
    } else if (currentSection == "V4+ Styles" || currentSection == "V4 Styles") {
        styles = currentContent;
    } else if (currentSection == "Events") {
        events = currentContent;
    }
    
    return !events.isEmpty(); // At least Events section is required
}

bool ASSParser::parseScriptInfo(const QString& scriptInfo, SubtitleTrack& track) const
{
    QStringList lines = scriptInfo.split('\n');
    
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('!')) {
            continue; // Skip empty lines and comments
        }
        
        QStringList parts = trimmedLine.split(':', Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            QString key = parts[0].trimmed();
            QString value = parts.mid(1).join(':').trimmed();
            
            if (key == "Title") {
                track.setTitle(value);
            } else if (key == "Language") {
                track.setLanguage(value);
            }
        }
    }
    
    return true;
}

QHash<QString, ASSStyle> ASSParser::parseStyles(const QString& stylesSection) const
{
    QHash<QString, ASSStyle> styles;
    QStringList lines = stylesSection.split('\n');
    
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('!')) {
            continue; // Skip empty lines and comments
        }
        
        if (trimmedLine.startsWith("Style:")) {
            QString styleData = trimmedLine.mid(6).trimmed(); // Remove "Style:"
            ASSStyle style;
            if (style.parseFromString(styleData)) {
                styles[style.name] = style;
                qCDebug(assParser) << "Parsed style:" << style.name;
            }
        }
    }
    
    // Add default style if none found
    if (styles.isEmpty()) {
        ASSStyle defaultStyle("Default");
        styles["Default"] = defaultStyle;
    }
    
    return styles;
}

bool ASSParser::parseEvents(const QString& eventsSection, const QHash<QString, ASSStyle>& styles, 
                           SubtitleTrack& track, SubtitleParseResult& result) const
{
    QStringList lines = eventsSection.split('\n');
    
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('!')) {
            continue; // Skip empty lines and comments
        }
        
        if (trimmedLine.startsWith("Dialogue:")) {
            SubtitleEntry entry;
            if (parseDialogueLine(trimmedLine, styles, entry)) {
                track.addEntry(entry);
                result.parsedEntries++;
            } else {
                result.skippedEntries++;
                qCWarning(assParser) << "Failed to parse dialogue line:" << trimmedLine.left(50) + "...";
            }
            
            // Check limits
            if (result.parsedEntries >= maxEntries()) {
                qCWarning(assParser) << "Reached maximum entries limit:" << maxEntries();
                break;
            }
        }
    }
    
    return result.parsedEntries > 0;
}

bool ASSParser::parseDialogueLine(const QString& dialogueLine, const QHash<QString, ASSStyle>& styles, 
                                 SubtitleEntry& entry) const
{
    // Dialogue format: Dialogue: Layer,Start,End,Style,Name,MarginL,MarginR,MarginV,Effect,Text
    QString data = dialogueLine.mid(9).trimmed(); // Remove "Dialogue:"
    
    QStringList fields = splitCSVLine(data);
    if (fields.size() < 10) {
        qCWarning(assParser) << "Invalid dialogue format - not enough fields:" << fields.size();
        return false;
    }
    
    // Parse timing
    qint64 startTime = SubtitleEntry::parseTimeASS(fields[1].trimmed());
    qint64 endTime = SubtitleEntry::parseTimeASS(fields[2].trimmed());
    
    if (startTime < 0 || endTime < 0 || endTime <= startTime) {
        qCWarning(assParser) << "Invalid timing in dialogue line";
        return false;
    }
    
    // Get style
    QString styleName = fields[3].trimmed();
    ASSStyle style;
    if (styles.contains(styleName)) {
        style = styles[styleName];
    } else if (styles.contains("Default")) {
        style = styles["Default"];
    }
    
    // Parse text
    QString text = fields[9]; // Text field (may contain commas)
    for (int i = 10; i < fields.size(); ++i) {
        text += "," + fields[i]; // Rejoin text that was split by commas
    }
    
    text = cleanASSText(text);
    
    // Validate text safety
    if (!isSafeText(text)) {
        qCWarning(assParser) << "Unsafe subtitle text detected";
        return false;
    }
    
    // Parse override codes and get format
    SubtitleFormat format = style.toSubtitleFormat();
    QString plainText = parseOverrideCodes(text, format);
    
    // Create entry
    entry = SubtitleEntry(startTime, endTime, plainText, format);
    entry.setLayer(fields[0].trimmed().toInt());
    entry.setStyle(styleName);
    entry.setActor(fields[4].trimmed());
    entry.setEffect(fields[8].trimmed());
    
    return entry.isValid();
}

QString ASSParser::parseOverrideCodes(const QString& text, SubtitleFormat& baseFormat) const
{
    QString plainText = text;
    
    // Remove ASS override codes like {\b1}, {\i1}, etc.
    // This is a simplified implementation
    QRegularExpression overrideRegex("\\{[^}]*\\}");
    plainText.remove(overrideRegex);
    
    // Remove line breaks
    plainText.replace("\\N", "\n");
    plainText.replace("\\n", "\n");
    
    return plainText.trimmed();
}

bool ASSParser::isValidASSStructure(const QString& content) const
{
    if (content.isEmpty()) {
        return false;
    }
    
    // Check for required sections
    bool hasScriptInfo = content.contains("[Script Info]", Qt::CaseInsensitive);
    bool hasStyles = content.contains("[V4+ Styles]", Qt::CaseInsensitive) || 
                    content.contains("[V4 Styles]", Qt::CaseInsensitive);
    bool hasEvents = content.contains("[Events]", Qt::CaseInsensitive);
    
    if (!hasEvents) {
        return false; // Events section is mandatory
    }
    
    // Check for dialogue lines
    if (!content.contains("Dialogue:", Qt::CaseInsensitive)) {
        return false;
    }
    
    // Check for ASS timing format
    QRegularExpression timingRegex("\\d{1,2}:\\d{2}:\\d{2}\\.\\d{2}");
    if (!timingRegex.match(content).hasMatch()) {
        return false;
    }
    
    return true;
}

QColor ASSParser::parseASSColor(const QString& colorStr) const
{
    ASSStyle style;
    return style.parseASSColor(colorStr);
}

Qt::Alignment ASSParser::parseASSAlignment(int alignment) const
{
    ASSStyle style;
    return style.parseASSAlignment(alignment);
}

QString ASSParser::cleanASSText(const QString& text) const
{
    QString cleaned = text;
    
    // Remove BOM if present
    if (cleaned.startsWith(QChar(0xFEFF))) {
        cleaned = cleaned.mid(1);
    }
    
    // Normalize line endings
    cleaned.replace("\r\n", "\n");
    cleaned.replace("\r", "\n");
    
    return cleaned.trimmed();
}

QStringList ASSParser::splitCSVLine(const QString& line) const
{
    QStringList fields;
    QString currentField;
    bool inQuotes = false;
    
    for (int i = 0; i < line.length(); ++i) {
        QChar ch = line.at(i);
        
        if (ch == '"') {
            inQuotes = !inQuotes;
        } else if (ch == ',' && !inQuotes) {
            fields.append(currentField);
            currentField.clear();
        } else {
            currentField.append(ch);
        }
    }
    
    // Add last field
    fields.append(currentField);
    
    return fields;
}