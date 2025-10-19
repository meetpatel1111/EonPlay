#include "ai/SubtitleLanguageDetector.h"
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QTimer>
#include <QSettings>
#include <QLoggingCategory>
#include <QDebug>
#include <QFileInfo>
#include <algorithm>

Q_LOGGING_CATEGORY(langDetector, "eonplay.ai.language")

namespace EonPlay {
namespace AI {

SubtitleLanguageDetector::SubtitleLanguageDetector(QObject* parent)
    : QObject(parent)
    , m_method(LocalNGram)
    , m_networkManager(nullptr)
    , m_modelsInitialized(false)
{
    setupNetworkManager();
    initializeLanguageModels();
    
    // Load API keys from settings
    QSettings settings;
    settings.beginGroup("AI/LanguageDetection");
    m_apiKeys[GoogleTranslate] = settings.value("GoogleTranslateApiKey").toString();
    m_apiKeys[AzureTranslator] = settings.value("AzureTranslatorApiKey").toString();
    m_apiKeys[AWSTranslate] = settings.value("AWSTranslateApiKey").toString();
    m_apiKeys[LibreTranslate] = settings.value("LibreTranslateApiKey").toString();
    settings.endGroup();
    
    qCInfo(langDetector) << "SubtitleLanguageDetector initialized";
}

void SubtitleLanguageDetector::setApiKey(DetectionMethod method, const QString& apiKey)
{
    m_apiKeys[method] = apiKey;
    
    // Save to settings
    QSettings settings;
    settings.beginGroup("AI/LanguageDetection");
    switch (method) {
        case GoogleTranslate:
            settings.setValue("GoogleTranslateApiKey", apiKey);
            break;
        case AzureTranslator:
            settings.setValue("AzureTranslatorApiKey", apiKey);
            break;
        case AWSTranslate:
            settings.setValue("AWSTranslateApiKey", apiKey);
            break;
        case LibreTranslate:
            settings.setValue("LibreTranslateApiKey", apiKey);
            break;
        default:
            break;
    }
    settings.endGroup();
}

QString SubtitleLanguageDetector::getApiKey(DetectionMethod method) const
{
    return m_apiKeys.value(method);
}

bool SubtitleLanguageDetector::isMethodAvailable(DetectionMethod method) const
{
    switch (method) {
        case LocalNGram:
            return m_modelsInitialized;
        case GoogleTranslate:
        case AzureTranslator:
        case AWSTranslate:
        case LibreTranslate:
            return !m_apiKeys.value(method).isEmpty();
        default:
            return false;
    }
}

void SubtitleLanguageDetector::detectLanguageFromFile(const QString& subtitleFilePath)
{
    if (!QFile::exists(subtitleFilePath)) {
        emit detectionFailed(subtitleFilePath, "File does not exist");
        return;
    }
    
    if (!isSupportedSubtitleFile(subtitleFilePath)) {
        emit detectionFailed(subtitleFilePath, "Unsupported subtitle format");
        return;
    }
    
    QString text = extractTextFromSubtitleFile(subtitleFilePath);
    if (text.isEmpty()) {
        emit detectionFailed(subtitleFilePath, "Failed to extract text from subtitle file");
        return;
    }
    
    if (m_method == LocalNGram) {
        DetectionResult result = detectWithLocalNGram(text);
        emit languageDetected(subtitleFilePath, result);
    } else {
        // Use API-based detection
        detectLanguageFromText(text);
    }
}

void SubtitleLanguageDetector::detectLanguageFromText(const QString& text)
{
    if (text.isEmpty()) {
        emit detectionFailed("", "Empty text provided");
        return;
    }
    
    switch (m_method) {
        case LocalNGram: {
            DetectionResult result = detectWithLocalNGram(text);
            emit languageDetected("", result);
            break;
        }
        case GoogleTranslate:
            detectWithGoogleTranslate(text);
            break;
        case AzureTranslator:
            detectWithAzureTranslator(text);
            break;
        case AWSTranslate:
            detectWithAWSTranslate(text);
            break;
        case LibreTranslate:
            detectWithLibreTranslate(text);
            break;
    }
}

void SubtitleLanguageDetector::detectLanguageFromMultipleFiles(const QStringList& filePaths)
{
    QHash<QString, DetectionResult> results;
    
    for (const QString& filePath : filePaths) {
        if (isSupportedSubtitleFile(filePath)) {
            QString text = extractTextFromSubtitleFile(filePath);
            if (!text.isEmpty()) {
                DetectionResult result = detectLanguageSync(text);
                results[filePath] = result;
            }
        }
    }
    
    emit multipleDetectionCompleted(results);
}

SubtitleLanguageDetector::DetectionResult SubtitleLanguageDetector::detectLanguageSync(const QString& text)
{
    return detectWithLocalNGram(text);
}

SubtitleLanguageDetector::Language SubtitleLanguageDetector::detectSimpleLanguage(const QString& text)
{
    DetectionResult result = detectLanguageSync(text);
    return result.language;
}

SubtitleLanguageDetector::DetectionResult SubtitleLanguageDetector::detectWithLocalNGram(const QString& text)
{
    DetectionResult result;
    
    if (!m_modelsInitialized || text.length() < 10) {
        return result;
    }
    
    QString normalizedText = normalizeText(text);
    QHash<QString, double> textNGrams = extractNGrams(normalizedText);
    
    QHash<Language, double> scores;
    
    // Calculate scores for each language
    for (auto it = m_ngramModels.begin(); it != m_ngramModels.end(); ++it) {
        Language lang = it.key();
        const QHash<QString, double>& langModel = it.value();
        
        double score = 0.0;
        int matches = 0;
        
        for (auto ngramIt = textNGrams.begin(); ngramIt != textNGrams.end(); ++ngramIt) {
            const QString& ngram = ngramIt.key();
            double textFreq = ngramIt.value();
            
            if (langModel.contains(ngram)) {
                score += textFreq * langModel[ngram];
                matches++;
            }
        }
        
        if (matches > 0) {
            scores[lang] = score / matches;
        }
    }
    
    // Find the best match
    Language bestLanguage = Unknown;
    double bestScore = 0.0;
    
    for (auto it = scores.begin(); it != scores.end(); ++it) {
        if (it.value() > bestScore) {
            bestScore = it.value();
            bestLanguage = it.key();
        }
    }
    
    // Set result
    result.language = bestLanguage;
    result.confidence = std::min(bestScore, 1.0);
    result.languageCode = languageToCode(bestLanguage);
    result.languageName = languageToDisplayName(bestLanguage);
    
    // Add alternative languages
    QList<QPair<Language, double>> sortedScores;
    for (auto it = scores.begin(); it != scores.end(); ++it) {
        if (it.key() != bestLanguage) {
            sortedScores.append(qMakePair(it.key(), it.value()));
        }
    }
    
    std::sort(sortedScores.begin(), sortedScores.end(),
              [](const QPair<Language, double>& a, const QPair<Language, double>& b) {
                  return a.second > b.second;
              });
    
    // Add top 3 alternatives
    for (int i = 0; i < std::min(3, static_cast<int>(sortedScores.size())); ++i) {
        result.alternativeLanguages[sortedScores[i].first] = sortedScores[i].second;
    }
    
    return result;
}

void SubtitleLanguageDetector::detectWithGoogleTranslate(const QString& text, const QString& identifier)
{
    if (!isMethodAvailable(GoogleTranslate)) {
        emit detectionFailed(identifier, "Google Translate API not configured");
        return;
    }
    
    QString apiKey = getApiKey(GoogleTranslate);
    QString url = QString("https://translation.googleapis.com/language/translate/v2/detect?key=%1").arg(apiKey);
    
    QNetworkRequest request = createApiRequest(url, GoogleTranslate);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject json;
    json["q"] = text.left(1000); // Limit text length for API
    
    QJsonDocument doc(json);
    QNetworkReply* reply = m_networkManager->post(request, doc.toJson());
    m_pendingRequests[reply] = identifier;
}

void SubtitleLanguageDetector::detectWithAzureTranslator(const QString& text, const QString& identifier)
{
    if (!isMethodAvailable(AzureTranslator)) {
        emit detectionFailed(identifier, "Azure Translator API not configured");
        return;
    }
    
    // Placeholder implementation for Azure Translator
    QTimer::singleShot(500, this, [this, identifier]() {
        DetectionResult result;
        result.language = English;
        result.confidence = 0.8;
        result.languageCode = "en";
        result.languageName = "English";
        emit languageDetected(identifier, result);
    });
}

void SubtitleLanguageDetector::detectWithAWSTranslate(const QString& text, const QString& identifier)
{
    if (!isMethodAvailable(AWSTranslate)) {
        emit detectionFailed(identifier, "AWS Translate API not configured");
        return;
    }
    
    // Placeholder implementation for AWS Translate
    QTimer::singleShot(500, this, [this, identifier]() {
        DetectionResult result;
        result.language = English;
        result.confidence = 0.8;
        result.languageCode = "en";
        result.languageName = "English";
        emit languageDetected(identifier, result);
    });
}

void SubtitleLanguageDetector::detectWithLibreTranslate(const QString& text, const QString& identifier)
{
    // LibreTranslate is open source and doesn't require API key
    QString url = "https://libretranslate.de/detect";
    
    QNetworkRequest request = createApiRequest(url, LibreTranslate);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject json;
    json["q"] = text.left(1000);
    
    QJsonDocument doc(json);
    QNetworkReply* reply = m_networkManager->post(request, doc.toJson());
    m_pendingRequests[reply] = identifier;
}

void SubtitleLanguageDetector::initializeLanguageModels()
{
    // Initialize common words and character sets for basic detection
    m_commonWords[English] = {"the", "and", "you", "that", "was", "for", "are", "with", "his", "they"};
    m_commonWords[Spanish] = {"que", "de", "no", "la", "el", "en", "es", "se", "te", "lo"};
    m_commonWords[French] = {"de", "le", "et", "à", "un", "il", "être", "et", "en", "avoir"};
    m_commonWords[German] = {"der", "die", "und", "in", "den", "von", "zu", "das", "mit", "sich"};
    m_commonWords[Italian] = {"di", "che", "e", "la", "il", "un", "a", "è", "per", "una"};
    m_commonWords[Portuguese] = {"de", "a", "o", "que", "e", "do", "da", "em", "um", "para"};
    m_commonWords[Russian] = {"в", "и", "не", "на", "я", "быть", "тот", "он", "оно", "мы"};
    
    // Character sets for script detection
    m_characterSets[Chinese] = {"的", "一", "是", "在", "不", "了", "有", "和", "人", "这"};
    m_characterSets[Japanese] = {"の", "に", "は", "を", "た", "が", "で", "て", "と", "し"};
    m_characterSets[Korean] = {"이", "의", "가", "을", "는", "에", "한", "다", "로", "과"};
    m_characterSets[Arabic] = {"في", "من", "إلى", "على", "أن", "هذا", "كان", "قد", "لا", "ما"};
    
    // Create simple n-gram models from common words
    for (auto it = m_commonWords.begin(); it != m_commonWords.end(); ++it) {
        Language lang = it.key();
        const QStringList& words = it.value();
        
        QHash<QString, double> ngramModel;
        for (const QString& word : words) {
            QHash<QString, double> wordNGrams = extractNGrams(word);
            for (auto ngramIt = wordNGrams.begin(); ngramIt != wordNGrams.end(); ++ngramIt) {
                ngramModel[ngramIt.key()] += ngramIt.value();
            }
        }
        
        // Normalize frequencies
        double total = 0.0;
        for (double freq : ngramModel.values()) {
            total += freq;
        }
        
        if (total > 0) {
            for (auto& freq : ngramModel) {
                freq /= total;
            }
        }
        
        m_ngramModels[lang] = ngramModel;
    }
    
    m_modelsInitialized = true;
    qCInfo(langDetector) << "Language models initialized for" << m_ngramModels.size() << "languages";
}

QHash<QString, double> SubtitleLanguageDetector::extractNGrams(const QString& text, int n)
{
    QHash<QString, double> ngrams;
    QString cleanText = normalizeText(text);
    
    if (cleanText.length() < n) {
        return ngrams;
    }
    
    for (int i = 0; i <= cleanText.length() - n; ++i) {
        QString ngram = cleanText.mid(i, n);
        ngrams[ngram] += 1.0;
    }
    
    return ngrams;
}

QString SubtitleLanguageDetector::normalizeText(const QString& text)
{
    QString normalized = text.toLower();
    
    // Remove HTML tags and subtitle formatting
    normalized.remove(QRegularExpression("<[^>]*>"));
    normalized.remove(QRegularExpression("\\{[^}]*\\}"));
    
    // Remove timestamps and numbers
    normalized.remove(QRegularExpression("\\d+:\\d+:\\d+[,.]\\d+"));
    normalized.remove(QRegularExpression("\\d+"));
    
    // Remove punctuation except spaces
    normalized.remove(QRegularExpression("[^\\w\\s]"));
    
    // Normalize whitespace
    normalized = normalized.simplified();
    
    return normalized;
}

bool SubtitleLanguageDetector::isSupportedSubtitleFile(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    QStringList supportedFormats = {"srt", "ass", "ssa", "vtt", "sub", "sbv"};
    return supportedFormats.contains(extension);
}

QString SubtitleLanguageDetector::extractTextFromSubtitleFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    if (extension == "srt") {
        return extractSRTText(filePath);
    } else if (extension == "ass" || extension == "ssa") {
        return extractASSText(filePath);
    } else if (extension == "vtt") {
        return extractVTTText(filePath);
    }
    
    // Generic text extraction for other formats
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.readAll();
    
    // Remove common subtitle formatting
    content.remove(QRegularExpression("\\d+:\\d+:\\d+[,.]\\d+ --> \\d+:\\d+:\\d+[,.]\\d+"));
    content.remove(QRegularExpression("^\\d+$", QRegularExpression::MultilineOption));
    content.remove(QRegularExpression("<[^>]*>"));
    
    return content.simplified();
}

QString SubtitleLanguageDetector::extractSRTText(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    QStringList textLines;
    QString line;
    bool inTextBlock = false;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        
        if (line.isEmpty()) {
            inTextBlock = false;
            continue;
        }
        
        // Skip sequence numbers
        if (line.contains(QRegularExpression("^\\d+$"))) {
            continue;
        }
        
        // Skip timestamps
        if (line.contains(QRegularExpression("\\d+:\\d+:\\d+[,.]\\d+ --> \\d+:\\d+:\\d+[,.]\\d+"))) {
            inTextBlock = true;
            continue;
        }
        
        if (inTextBlock) {
            // Remove HTML tags
            line.remove(QRegularExpression("<[^>]*>"));
            if (!line.isEmpty()) {
                textLines << line;
            }
        }
    }
    
    return textLines.join(" ");
}

QString SubtitleLanguageDetector::extractASSText(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    QStringList textLines;
    QString line;
    bool inEventsSection = false;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        
        if (line.startsWith("[Events]")) {
            inEventsSection = true;
            continue;
        }
        
        if (line.startsWith("[") && line.endsWith("]")) {
            inEventsSection = false;
            continue;
        }
        
        if (inEventsSection && line.startsWith("Dialogue:")) {
            // Extract text from ASS dialogue line
            QStringList parts = line.split(",");
            if (parts.size() >= 10) {
                QString text = parts.mid(9).join(",");
                text.remove(QRegularExpression("\\{[^}]*\\}"));
                text.remove(QRegularExpression("\\\\[nN]"));
                if (!text.isEmpty()) {
                    textLines << text;
                }
            }
        }
    }
    
    return textLines.join(" ");
}

QString SubtitleLanguageDetector::extractVTTText(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    QStringList textLines;
    QString line;
    bool inTextBlock = false;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        
        if (line.isEmpty()) {
            inTextBlock = false;
            continue;
        }
        
        // Skip WebVTT header
        if (line == "WEBVTT") {
            continue;
        }
        
        // Skip timestamps
        if (line.contains(QRegularExpression("\\d+:\\d+:\\d+\\.\\d+ --> \\d+:\\d+:\\d+\\.\\d+"))) {
            inTextBlock = true;
            continue;
        }
        
        if (inTextBlock) {
            // Remove WebVTT tags
            line.remove(QRegularExpression("<[^>]*>"));
            if (!line.isEmpty()) {
                textLines << line;
            }
        }
    }
    
    return textLines.join(" ");
}

void SubtitleLanguageDetector::setupNetworkManager()
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &SubtitleLanguageDetector::handleNetworkReply);
}

QNetworkRequest SubtitleLanguageDetector::createApiRequest(const QString& url, DetectionMethod method)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EonPlay/1.0");
    
    switch (method) {
        case GoogleTranslate:
            request.setRawHeader("Accept", "application/json");
            break;
        case AzureTranslator:
            request.setRawHeader("Ocp-Apim-Subscription-Key", getApiKey(method).toUtf8());
            break;
        case AWSTranslate:
            // AWS requires more complex authentication
            break;
        case LibreTranslate:
            request.setRawHeader("Accept", "application/json");
            break;
        default:
            break;
    }
    
    return request;
}

void SubtitleLanguageDetector::handleNetworkReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    QString identifier = m_pendingRequests.take(reply);
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        
        if (!doc.isNull()) {
            DetectionResult result;
            QJsonObject obj = doc.object();
            
            // Parse response based on API
            if (obj.contains("data")) {
                // Google Translate format
                QJsonArray detections = obj["data"].toObject()["detections"].toArray();
                if (!detections.isEmpty()) {
                    QJsonObject detection = detections[0].toArray()[0].toObject();
                    QString langCode = detection["language"].toString();
                    double confidence = detection["confidence"].toDouble();
                    
                    result.language = codeToLanguage(langCode);
                    result.confidence = confidence;
                    result.languageCode = langCode;
                    result.languageName = languageToDisplayName(result.language);
                }
            } else if (obj.contains("language")) {
                // LibreTranslate format
                QString langCode = obj["language"].toString();
                double confidence = obj["confidence"].toDouble();
                
                result.language = codeToLanguage(langCode);
                result.confidence = confidence;
                result.languageCode = langCode;
                result.languageName = languageToDisplayName(result.language);
            }
            
            emit languageDetected(identifier, result);
        } else {
            emit detectionFailed(identifier, "Invalid API response");
        }
    } else {
        emit detectionFailed(identifier, reply->errorString());
    }
    
    reply->deleteLater();
}

// Static utility methods
QString SubtitleLanguageDetector::languageToString(Language language)
{
    switch (language) {
        case English: return "English";
        case Spanish: return "Spanish";
        case French: return "French";
        case German: return "German";
        case Italian: return "Italian";
        case Portuguese: return "Portuguese";
        case Russian: return "Russian";
        case Chinese: return "Chinese";
        case Japanese: return "Japanese";
        case Korean: return "Korean";
        case Arabic: return "Arabic";
        case Hindi: return "Hindi";
        case Dutch: return "Dutch";
        case Swedish: return "Swedish";
        case Norwegian: return "Norwegian";
        case Danish: return "Danish";
        case Finnish: return "Finnish";
        case Polish: return "Polish";
        case Czech: return "Czech";
        case Hungarian: return "Hungarian";
        case Romanian: return "Romanian";
        case Bulgarian: return "Bulgarian";
        case Greek: return "Greek";
        case Turkish: return "Turkish";
        case Hebrew: return "Hebrew";
        case Thai: return "Thai";
        case Vietnamese: return "Vietnamese";
        case Indonesian: return "Indonesian";
        case Malay: return "Malay";
        default: return "Unknown";
    }
}

QString SubtitleLanguageDetector::languageToCode(Language language)
{
    switch (language) {
        case English: return "en";
        case Spanish: return "es";
        case French: return "fr";
        case German: return "de";
        case Italian: return "it";
        case Portuguese: return "pt";
        case Russian: return "ru";
        case Chinese: return "zh";
        case Japanese: return "ja";
        case Korean: return "ko";
        case Arabic: return "ar";
        case Hindi: return "hi";
        case Dutch: return "nl";
        case Swedish: return "sv";
        case Norwegian: return "no";
        case Danish: return "da";
        case Finnish: return "fi";
        case Polish: return "pl";
        case Czech: return "cs";
        case Hungarian: return "hu";
        case Romanian: return "ro";
        case Bulgarian: return "bg";
        case Greek: return "el";
        case Turkish: return "tr";
        case Hebrew: return "he";
        case Thai: return "th";
        case Vietnamese: return "vi";
        case Indonesian: return "id";
        case Malay: return "ms";
        default: return "unknown";
    }
}

QString SubtitleLanguageDetector::languageToDisplayName(Language language)
{
    return languageToString(language);
}

SubtitleLanguageDetector::Language SubtitleLanguageDetector::codeToLanguage(const QString& code)
{
    QString lower = code.toLower();
    if (lower == "en") return English;
    if (lower == "es") return Spanish;
    if (lower == "fr") return French;
    if (lower == "de") return German;
    if (lower == "it") return Italian;
    if (lower == "pt") return Portuguese;
    if (lower == "ru") return Russian;
    if (lower == "zh") return Chinese;
    if (lower == "ja") return Japanese;
    if (lower == "ko") return Korean;
    if (lower == "ar") return Arabic;
    if (lower == "hi") return Hindi;
    if (lower == "nl") return Dutch;
    if (lower == "sv") return Swedish;
    if (lower == "no") return Norwegian;
    if (lower == "da") return Danish;
    if (lower == "fi") return Finnish;
    if (lower == "pl") return Polish;
    if (lower == "cs") return Czech;
    if (lower == "hu") return Hungarian;
    if (lower == "ro") return Romanian;
    if (lower == "bg") return Bulgarian;
    if (lower == "el") return Greek;
    if (lower == "tr") return Turkish;
    if (lower == "he") return Hebrew;
    if (lower == "th") return Thai;
    if (lower == "vi") return Vietnamese;
    if (lower == "id") return Indonesian;
    if (lower == "ms") return Malay;
    return Unknown;
}

} // namespace AI
} // namespace EonPlay