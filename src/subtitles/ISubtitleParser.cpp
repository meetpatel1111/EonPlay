#include "subtitles/ISubtitleParser.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTextCodec>
#include <QRegularExpression>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(subtitleParser, "eonplay.subtitles.parser")

// SubtitleParseResult Implementation
QString SubtitleParseResult::summary() const
{
    if (success) {
        return QString("Successfully parsed %1 entries (%2 skipped)")
            .arg(parsedEntries)
            .arg(skippedEntries);
    } else {
        return QString("Parsing failed: %1").arg(errorMessage);
    }
}

// BaseSubtitleParser Implementation
BaseSubtitleParser::BaseSubtitleParser()
    : m_maxFileSize(10 * 1024 * 1024) // 10MB
    , m_maxEntries(50000) // 50k entries
{
    // Initialize suspicious patterns for security validation
    m_suspiciousPatterns << "javascript:" << "vbscript:" << "<script" << "</script>"
                        << "eval(" << "document." << "window." << "alert("
                        << "file://" << "ftp://" << "data:" << "blob:";
    
    // Initialize allowed HTML-like tags for subtitles
    m_allowedTags << "b" << "i" << "u" << "s" << "font" << "color" << "size"
                  << "br" << "p" << "div" << "span" << "strong" << "em";
}

bool BaseSubtitleParser::validateFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    
    // Check if file exists
    if (!fileInfo.exists()) {
        qCWarning(subtitleParser) << "Subtitle file does not exist:" << filePath;
        return false;
    }
    
    // Check file size
    if (fileInfo.size() > m_maxFileSize) {
        qCWarning(subtitleParser) << "Subtitle file too large:" << fileInfo.size() << "bytes";
        return false;
    }
    
    // Check file extension
    QString extension = fileInfo.suffix().toLower();
    if (!supportedExtensions().contains(extension)) {
        qCWarning(subtitleParser) << "Unsupported subtitle format:" << extension;
        return false;
    }
    
    // Read and validate content
    QString encoding;
    QString content = readFileContent(filePath, encoding);
    if (content.isEmpty()) {
        qCWarning(subtitleParser) << "Could not read subtitle file:" << filePath;
        return false;
    }
    
    return validateContent(content);
}

bool BaseSubtitleParser::validateContent(const QString& content) const
{
    if (content.isEmpty()) {
        return false;
    }
    
    // Check content length
    if (content.length() > m_maxFileSize) {
        qCWarning(subtitleParser) << "Subtitle content too large:" << content.length() << "characters";
        return false;
    }
    
    // Check for suspicious patterns
    QString lowerContent = content.toLower();
    for (const QString& pattern : m_suspiciousPatterns) {
        if (lowerContent.contains(pattern)) {
            qCWarning(subtitleParser) << "Suspicious content detected:" << pattern;
            return false;
        }
    }
    
    // Check for excessive HTML tags (potential XSS)
    QRegularExpression tagRegex("<[^>]*>");
    QRegularExpressionMatchIterator matches = tagRegex.globalMatch(content);
    int tagCount = 0;
    while (matches.hasNext()) {
        matches.next();
        tagCount++;
        if (tagCount > 1000) { // Arbitrary limit
            qCWarning(subtitleParser) << "Too many HTML tags in subtitle content";
            return false;
        }
    }
    
    return true;
}

QString BaseSubtitleParser::detectEncoding(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return "UTF-8"; // Default fallback
    }
    
    // Read first few bytes to detect BOM
    QByteArray header = file.read(4);
    file.close();
    
    // Check for BOM
    if (header.startsWith("\xEF\xBB\xBF")) {
        return "UTF-8";
    } else if (header.startsWith("\xFF\xFE")) {
        return "UTF-16LE";
    } else if (header.startsWith("\xFE\xFF")) {
        return "UTF-16BE";
    } else if (header.startsWith("\xFF\xFE\x00\x00")) {
        return "UTF-32LE";
    } else if (header.startsWith("\x00\x00\xFE\xFF")) {
        return "UTF-32BE";
    }
    
    // Try to detect encoding by content analysis
    // This is a simplified implementation
    // In practice, you might use libraries like ICU or chardet
    
    // For now, assume UTF-8 for most cases
    return "UTF-8";
}

QString BaseSubtitleParser::readFileContent(const QString& filePath, QString& encoding) const
{
    encoding = detectEncoding(filePath);
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(subtitleParser) << "Cannot open subtitle file:" << filePath;
        return QString();
    }
    
    QTextStream stream(&file);
    
    // Set encoding
    if (encoding == "UTF-8") {
        stream.setCodec("UTF-8");
    } else if (encoding == "UTF-16LE") {
        stream.setCodec("UTF-16LE");
    } else if (encoding == "UTF-16BE") {
        stream.setCodec("UTF-16BE");
    } else {
        // Try to find codec
        QTextCodec* codec = QTextCodec::codecForName(encoding.toUtf8());
        if (codec) {
            stream.setCodec(codec);
        } else {
            stream.setCodec("UTF-8"); // Fallback
            encoding = "UTF-8";
        }
    }
    
    QString content = stream.readAll();
    file.close();
    
    return content;
}

QString BaseSubtitleParser::sanitizeText(const QString& text) const
{
    QString sanitized = text;
    
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

bool BaseSubtitleParser::isSafeText(const QString& text) const
{
    // Check for suspicious patterns
    QString lowerText = text.toLower();
    for (const QString& pattern : m_suspiciousPatterns) {
        if (lowerText.contains(pattern)) {
            return false;
        }
    }
    
    // Check for excessive length
    if (text.length() > 10000) { // 10k characters per subtitle entry
        return false;
    }
    
    return true;
}

qint64 BaseSubtitleParser::parseGenericTime(const QString& timeStr) const
{
    // Try SRT format first
    qint64 time = SubtitleEntry::parseTime(timeStr);
    if (time >= 0) {
        return time;
    }
    
    // Try ASS format
    time = SubtitleEntry::parseTimeASS(timeStr);
    if (time >= 0) {
        return time;
    }
    
    // Try other common formats
    QRegularExpression genericRegex("^(\\d{1,2}):(\\d{2}):(\\d{2})(?:[,\\.](\\d{1,3}))?$");
    QRegularExpressionMatch match = genericRegex.match(timeStr.trimmed());
    
    if (match.hasMatch()) {
        int hours = match.captured(1).toInt();
        int minutes = match.captured(2).toInt();
        int seconds = match.captured(3).toInt();
        int milliseconds = 0;
        
        if (match.lastCapturedIndex() >= 4) {
            QString msStr = match.captured(4);
            // Pad or truncate to 3 digits
            if (msStr.length() == 1) {
                milliseconds = msStr.toInt() * 100;
            } else if (msStr.length() == 2) {
                milliseconds = msStr.toInt() * 10;
            } else {
                milliseconds = msStr.left(3).toInt();
            }
        }
        
        return static_cast<qint64>(hours) * 3600000 + 
               static_cast<qint64>(minutes) * 60000 + 
               static_cast<qint64>(seconds) * 1000 + 
               static_cast<qint64>(milliseconds);
    }
    
    return -1;
}

QString BaseSubtitleParser::cleanupText(const QString& text) const
{
    QString cleaned = text;
    
    // Remove excessive whitespace
    cleaned = cleaned.simplified();
    
    // Remove empty lines
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

// SubtitleParserFactory Implementation
QList<std::unique_ptr<ISubtitleParser>> SubtitleParserFactory::s_parsers;

void SubtitleParserFactory::registerParser(std::unique_ptr<ISubtitleParser> parser)
{
    if (parser) {
        qCDebug(subtitleParser) << "Registering subtitle parser:" << parser->parserName();
        s_parsers.append(std::move(parser));
    }
}

ISubtitleParser* SubtitleParserFactory::getParserForExtension(const QString& extension)
{
    QString ext = extension.toLower();
    if (ext.startsWith('.')) {
        ext = ext.mid(1);
    }
    
    for (const auto& parser : s_parsers) {
        if (parser->supportedExtensions().contains(ext)) {
            return parser.get();
        }
    }
    
    return nullptr;
}

ISubtitleParser* SubtitleParserFactory::getParserForFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    // First try by extension
    ISubtitleParser* parser = getParserForExtension(extension);
    if (parser && parser->canParse(filePath)) {
        return parser;
    }
    
    // Try all parsers
    for (const auto& p : s_parsers) {
        if (p->canParse(filePath)) {
            return p.get();
        }
    }
    
    return nullptr;
}

ISubtitleParser* SubtitleParserFactory::getParserForContent(const QString& content)
{
    for (const auto& parser : s_parsers) {
        if (parser->canParseContent(content)) {
            return parser.get();
        }
    }
    
    return nullptr;
}

QList<ISubtitleParser*> SubtitleParserFactory::getAllParsers()
{
    QList<ISubtitleParser*> parsers;
    for (const auto& parser : s_parsers) {
        parsers.append(parser.get());
    }
    return parsers;
}

QStringList SubtitleParserFactory::getSupportedExtensions()
{
    QStringList extensions;
    for (const auto& parser : s_parsers) {
        extensions.append(parser->supportedExtensions());
    }
    extensions.removeDuplicates();
    return extensions;
}

void SubtitleParserFactory::initializeDefaultParsers()
{
    // This will be implemented when we create the specific parsers
    qCDebug(subtitleParser) << "Initializing default subtitle parsers";
}