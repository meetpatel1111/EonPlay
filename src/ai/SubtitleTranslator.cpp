#include "ai/SubtitleTranslator.h"
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QSettings>
#include <QLoggingCategory>
#include <QDebug>
#include <QTimer>

Q_LOGGING_CATEGORY(subtitleTranslator, "eonplay.ai.translator")

namespace EonPlay {
namespace AI {

SubtitleTranslator::SubtitleTranslator(QObject* parent)
    : QObject(parent)
    , m_isTranslating(false)
    , m_networkManager(nullptr)
    , m_batchTimer(new QTimer(this))
    , m_processingBatch(false)
    , m_currentBatchIndex(0)
{
    setupNetworkManager();
    
    // Setup batch processing timer
    m_batchTimer->setSingleShot(true);
    connect(m_batchTimer, &QTimer::timeout, this, &SubtitleTranslator::processBatchQueue);
    
    // Load API keys from settings
    QSettings settings;
    settings.beginGroup("AI/Translation");
    m_apiKeys[GoogleTranslate] = settings.value("GoogleTranslateApiKey").toString();
    m_apiKeys[AzureTranslator] = settings.value("AzureTranslatorApiKey").toString();
    m_apiKeys[AWSTranslate] = settings.value("AWSTranslateApiKey").toString();
    m_apiKeys[LibreTranslate] = settings.value("LibreTranslateApiKey").toString();
    m_apiKeys[DeepL] = settings.value("DeepLApiKey").toString();
    settings.endGroup();
    
    qCInfo(subtitleTranslator) << "SubtitleTranslator initialized";
}

SubtitleTranslator::~SubtitleTranslator()
{
    cancelTranslation();
}

void SubtitleTranslator::setApiKey(TranslationService service, const QString& apiKey)
{
    m_apiKeys[service] = apiKey;
    
    // Save to settings
    QSettings settings;
    settings.beginGroup("AI/Translation");
    switch (service) {
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
        case DeepL:
            settings.setValue("DeepLApiKey", apiKey);
            break;
        default:
            break;
    }
    settings.endGroup();
}

QString SubtitleTranslator::getApiKey(TranslationService service) const
{
    return m_apiKeys.value(service);
}

bool SubtitleTranslator::isServiceAvailable(TranslationService service) const
{
    switch (service) {
        case LocalTranslator:
            return true; // Always available
        case LibreTranslate:
            return true; // Public API, no key required
        case GoogleTranslate:
        case AzureTranslator:
        case AWSTranslate:
        case DeepL:
            return !m_apiKeys.value(service).isEmpty();
        default:
            return false;
    }
}

void SubtitleTranslator::translateSubtitleFile(const QString& inputPath, const QString& outputPath,
                                              Language sourceLanguage, Language targetLanguage)
{
    if (m_isTranslating) {
        emit translationFailed("Translation already in progress");
        return;
    }
    
    if (!QFile::exists(inputPath)) {
        emit translationFailed("Input file does not exist: " + inputPath);
        return;
    }
    
    if (!isServiceAvailable(m_options.service)) {
        emit translationFailed("Selected translation service is not available");
        return;
    }
    
    m_isTranslating = true;
    m_currentFilePath = outputPath;
    m_currentBatchIndex = 0;
    m_translatedBatches.clear();
    
    // Reset progress
    m_progress = TranslationProgress();
    m_progress.currentStatus = "Parsing subtitle file...";
    
    emit translationStarted(inputPath);
    emit translationProgress(m_progress);
    
    // Parse subtitle file
    m_currentEntries = parseSubtitleFile(inputPath);
    if (m_currentEntries.isEmpty()) {
        m_isTranslating = false;
        emit translationFailed("Failed to parse subtitle file or file is empty");
        return;
    }
    
    m_progress.totalLines = m_currentEntries.size();
    m_progress.currentStatus = "Starting translation...";
    emit translationProgress(m_progress);
    
    // Extract texts for translation
    QStringList textsToTranslate;
    for (const SubtitleEntry& entry : m_currentEntries) {
        textsToTranslate << entry.text;
    }
    
    // Start batch translation
    BatchRequest request;
    request.texts = textsToTranslate;
    request.sourceLanguage = sourceLanguage;
    request.targetLanguage = targetLanguage;
    request.identifier = "subtitle_file";
    request.filePath = outputPath;
    
    queueBatchRequest(request);
}

void SubtitleTranslator::translateText(const QString& text, Language sourceLanguage,
                                      Language targetLanguage, const QString& identifier)
{
    if (!isServiceAvailable(m_options.service)) {
        emit translationFailed("Selected translation service is not available");
        return;
    }
    
    QStringList texts;
    texts << text;
    
    BatchRequest request;
    request.texts = texts;
    request.sourceLanguage = sourceLanguage;
    request.targetLanguage = targetLanguage;
    request.identifier = identifier;
    
    queueBatchRequest(request);
}

void SubtitleTranslator::translateTextBatch(const QStringList& texts, Language sourceLanguage,
                                           Language targetLanguage, const QString& identifier)
{
    if (!isServiceAvailable(m_options.service)) {
        emit translationFailed("Selected translation service is not available");
        return;
    }
    
    BatchRequest request;
    request.texts = texts;
    request.sourceLanguage = sourceLanguage;
    request.targetLanguage = targetLanguage;
    request.identifier = identifier;
    
    queueBatchRequest(request);
}

void SubtitleTranslator::translateMultipleFiles(const QStringList& inputPaths, const QString& outputDirectory,
                                               Language sourceLanguage, Language targetLanguage)
{
    if (m_isTranslating) {
        emit translationFailed("Translation already in progress");
        return;
    }
    
    // Queue all files for translation
    for (const QString& inputPath : inputPaths) {
        QFileInfo inputInfo(inputPath);
        QString outputPath = QDir(outputDirectory).absoluteFilePath(
            inputInfo.completeBaseName() + "_translated." + inputInfo.suffix());
        
        // This will be implemented to queue multiple file translations
        // For now, translate one at a time
        translateSubtitleFile(inputPath, outputPath, sourceLanguage, targetLanguage);
        break; // Only translate first file for now
    }
}

void SubtitleTranslator::cancelTranslation()
{
    if (!m_isTranslating) {
        return;
    }
    
    m_isTranslating = false;
    m_processingBatch = false;
    m_batchTimer->stop();
    m_batchQueue.clear();
    
    // Cancel pending network requests
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
        QNetworkReply* reply = it.key();
        if (reply) {
            reply->abort();
        }
    }
    m_pendingRequests.clear();
    m_requestServices.clear();
    
    emit translationCancelled();
}

QStringList SubtitleTranslator::getSupportedLanguages(TranslationService service) const
{
    // Return common languages supported by most services
    QStringList languages;
    languages << "English" << "Spanish" << "French" << "German" << "Italian"
              << "Portuguese" << "Russian" << "Chinese" << "Japanese" << "Korean"
              << "Arabic" << "Hindi" << "Dutch" << "Swedish" << "Norwegian";
    return languages;
}

bool SubtitleTranslator::isLanguageSupported(Language language, TranslationService service) const
{
    // Most services support common languages
    return language != Auto; // Auto is only for source language detection
}

void SubtitleTranslator::translateWithGoogleTranslate(const QStringList& texts, Language sourceLang,
                                                     Language targetLang, const QString& identifier)
{
    if (!isServiceAvailable(GoogleTranslate)) {
        emit translationFailed("Google Translate API not configured");
        return;
    }
    
    QString apiKey = getApiKey(GoogleTranslate);
    QString url = QString("https://translation.googleapis.com/language/translate/v2?key=%1").arg(apiKey);
    
    QNetworkRequest request = createApiRequest(url, GoogleTranslate);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject json;
    QJsonArray textArray;
    for (const QString& text : texts) {
        textArray.append(preprocessText(text));
    }
    json["q"] = textArray;
    json["target"] = languageToCode(targetLang);
    
    if (sourceLang != Auto) {
        json["source"] = languageToCode(sourceLang);
    }
    
    QJsonDocument doc(json);
    QNetworkReply* reply = m_networkManager->post(request, doc.toJson());
    m_pendingRequests[reply] = identifier;
    m_requestServices[reply] = GoogleTranslate;
}

void SubtitleTranslator::translateWithLibreTranslate(const QStringList& texts, Language sourceLang,
                                                    Language targetLang, const QString& identifier)
{
    QString url = "https://libretranslate.de/translate";
    
    QNetworkRequest request = createApiRequest(url, LibreTranslate);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // LibreTranslate processes one text at a time, so we'll send the first text
    // In a real implementation, you'd want to handle multiple texts properly
    QString textToTranslate = texts.isEmpty() ? "" : texts.first();
    
    QJsonObject json;
    json["q"] = preprocessText(textToTranslate);
    json["source"] = sourceLang == Auto ? "auto" : languageToCode(sourceLang);
    json["target"] = languageToCode(targetLang);
    
    QJsonDocument doc(json);
    QNetworkReply* reply = m_networkManager->post(request, doc.toJson());
    m_pendingRequests[reply] = identifier;
    m_requestServices[reply] = LibreTranslate;
}

void SubtitleTranslator::translateWithLocalTranslator(const QStringList& texts, Language sourceLang,
                                                     Language targetLang, const QString& identifier)
{
    // Placeholder for local translation
    // In a real implementation, this would use offline translation models
    
    QStringList translatedTexts;
    for (const QString& text : texts) {
        QString translated = QString("[%1->%2] %3")
                           .arg(languageToCode(sourceLang))
                           .arg(languageToCode(targetLang))
                           .arg(text);
        translatedTexts << translated;
    }
    
    // Simulate processing delay
    QTimer::singleShot(500, this, [this, identifier, texts, translatedTexts]() {
        if (texts.size() == 1) {
            emit textTranslated(identifier, texts.first(), translatedTexts.first(), English);
        } else {
            emit batchTranslated(identifier, texts, translatedTexts);
        }
        
        if (identifier == "subtitle_file") {
            finalizeBatchTranslation(translatedTexts);
        }
    });
}

void SubtitleTranslator::translateWithAzureTranslator(const QStringList& texts, Language sourceLang,
                                                     Language targetLang, const QString& identifier)
{
    // Placeholder implementation
    translateWithLocalTranslator(texts, sourceLang, targetLang, identifier);
}

void SubtitleTranslator::translateWithAWSTranslate(const QStringList& texts, Language sourceLang,
                                                  Language targetLang, const QString& identifier)
{
    // Placeholder implementation
    translateWithLocalTranslator(texts, sourceLang, targetLang, identifier);
}

void SubtitleTranslator::translateWithDeepL(const QStringList& texts, Language sourceLang,
                                           Language targetLang, const QString& identifier)
{
    // Placeholder implementation
    translateWithLocalTranslator(texts, sourceLang, targetLang, identifier);
}

QList<SubtitleTranslator::SubtitleEntry> SubtitleTranslator::parseSubtitleFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    if (extension == "srt") {
        return parseSRTFile(filePath);
    } else if (extension == "ass" || extension == "ssa") {
        return parseASSFile(filePath);
    } else if (extension == "vtt") {
        return parseVTTFile(filePath);
    }
    
    return QList<SubtitleEntry>();
}

QList<SubtitleTranslator::SubtitleEntry> SubtitleTranslator::parseSRTFile(const QString& filePath)
{
    QList<SubtitleEntry> entries;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return entries;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    SubtitleEntry currentEntry;
    QString line;
    int state = 0; // 0: index, 1: timestamp, 2: text
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        
        if (line.isEmpty()) {
            if (state == 2 && !currentEntry.text.isEmpty()) {
                entries.append(currentEntry);
                currentEntry = SubtitleEntry();
            }
            state = 0;
            continue;
        }
        
        switch (state) {
            case 0: // Index
                currentEntry.index = line.toInt();
                state = 1;
                break;
            case 1: // Timestamp
                if (line.contains(" --> ")) {
                    QStringList parts = line.split(" --> ");
                    if (parts.size() == 2) {
                        currentEntry.startTime = parts[0];
                        currentEntry.endTime = parts[1];
                        state = 2;
                    }
                }
                break;
            case 2: // Text
                if (!currentEntry.text.isEmpty()) {
                    currentEntry.text += "\n";
                }
                currentEntry.text += line;
                break;
        }
    }
    
    // Add last entry if exists
    if (state == 2 && !currentEntry.text.isEmpty()) {
        entries.append(currentEntry);
    }
    
    return entries;
}

QList<SubtitleTranslator::SubtitleEntry> SubtitleTranslator::parseASSFile(const QString& filePath)
{
    // Simplified ASS parsing - in a real implementation, this would be more comprehensive
    QList<SubtitleEntry> entries;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return entries;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    QString line;
    bool inEventsSection = false;
    int index = 1;
    
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
            QStringList parts = line.split(",");
            if (parts.size() >= 10) {
                SubtitleEntry entry;
                entry.index = index++;
                entry.startTime = parts[1]; // Start time
                entry.endTime = parts[2];   // End time
                entry.text = parts.mid(9).join(","); // Text (everything after 9th comma)
                
                // Remove ASS formatting
                entry.text.remove(QRegularExpression("\\{[^}]*\\}"));
                entry.text.replace("\\N", "\n");
                
                entries.append(entry);
            }
        }
    }
    
    return entries;
}

QList<SubtitleTranslator::SubtitleEntry> SubtitleTranslator::parseVTTFile(const QString& filePath)
{
    QList<SubtitleEntry> entries;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return entries;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    SubtitleEntry currentEntry;
    QString line;
    int state = 0; // 0: waiting for timestamp, 1: reading text
    int index = 1;
    
    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        
        if (line == "WEBVTT") {
            continue;
        }
        
        if (line.isEmpty()) {
            if (state == 1 && !currentEntry.text.isEmpty()) {
                entries.append(currentEntry);
                currentEntry = SubtitleEntry();
            }
            state = 0;
            continue;
        }
        
        if (line.contains(" --> ")) {
            QStringList parts = line.split(" --> ");
            if (parts.size() == 2) {
                currentEntry.index = index++;
                currentEntry.startTime = parts[0];
                currentEntry.endTime = parts[1];
                state = 1;
            }
        } else if (state == 1) {
            if (!currentEntry.text.isEmpty()) {
                currentEntry.text += "\n";
            }
            currentEntry.text += line;
        }
    }
    
    // Add last entry if exists
    if (state == 1 && !currentEntry.text.isEmpty()) {
        entries.append(currentEntry);
    }
    
    return entries;
}

bool SubtitleTranslator::writeSubtitleFile(const QString& filePath, const QList<SubtitleEntry>& entries)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    if (extension == "srt") {
        return writeSRTFile(filePath, entries);
    } else if (extension == "ass" || extension == "ssa") {
        return writeASSFile(filePath, entries);
    } else if (extension == "vtt") {
        return writeVTTFile(filePath, entries);
    }
    
    return false;
}

bool SubtitleTranslator::writeSRTFile(const QString& filePath, const QList<SubtitleEntry>& entries)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    for (const SubtitleEntry& entry : entries) {
        stream << entry.index << "\n";
        stream << entry.startTime << " --> " << entry.endTime << "\n";
        stream << entry.text << "\n\n";
    }
    
    return true;
}

bool SubtitleTranslator::writeASSFile(const QString& filePath, const QList<SubtitleEntry>& entries)
{
    // Simplified ASS writing - would need full ASS header in real implementation
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    // Write basic ASS header
    stream << "[Script Info]\n";
    stream << "Title: Translated Subtitles\n";
    stream << "ScriptType: v4.00+\n\n";
    
    stream << "[V4+ Styles]\n";
    stream << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n";
    stream << "Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,2,0,2,10,10,10,1\n\n";
    
    stream << "[Events]\n";
    stream << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
    
    for (const SubtitleEntry& entry : entries) {
        QString text = entry.text;
        text.replace("\n", "\\N");
        stream << "Dialogue: 0," << entry.startTime << "," << entry.endTime 
               << ",Default,,0,0,0,," << text << "\n";
    }
    
    return true;
}

bool SubtitleTranslator::writeVTTFile(const QString& filePath, const QList<SubtitleEntry>& entries)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    stream << "WEBVTT\n\n";
    
    for (const SubtitleEntry& entry : entries) {
        stream << entry.startTime << " --> " << entry.endTime << "\n";
        stream << entry.text << "\n\n";
    }
    
    return true;
}

QString SubtitleTranslator::preprocessText(const QString& text)
{
    QString processed = text;
    
    // Remove HTML tags but preserve structure
    processed.remove(QRegularExpression("<[^>]*>"));
    
    // Normalize whitespace
    processed = processed.simplified();
    
    return processed;
}

QString SubtitleTranslator::postprocessText(const QString& text, const QString& originalText)
{
    QString processed = text;
    
    // Apply line length limits if specified
    if (m_options.maxLineLength > 0) {
        QStringList lines = splitLongText(processed, m_options.maxLineLength);
        processed = lines.join("\n");
    }
    
    return processed;
}

QStringList SubtitleTranslator::splitLongText(const QString& text, int maxLength)
{
    QStringList result;
    QStringList words = text.split(" ", Qt::SkipEmptyParts);
    
    QString currentLine;
    for (const QString& word : words) {
        if (currentLine.length() + word.length() + 1 <= maxLength) {
            if (!currentLine.isEmpty()) {
                currentLine += " ";
            }
            currentLine += word;
        } else {
            if (!currentLine.isEmpty()) {
                result << currentLine;
                currentLine = word;
            } else {
                // Word is longer than max length, split it
                result << word.left(maxLength);
                currentLine = word.mid(maxLength);
            }
        }
    }
    
    if (!currentLine.isEmpty()) {
        result << currentLine;
    }
    
    return result;
}

void SubtitleTranslator::setupNetworkManager()
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &SubtitleTranslator::handleNetworkReply);
}

QNetworkRequest SubtitleTranslator::createApiRequest(const QString& url, TranslationService service)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EonPlay/1.0");
    
    switch (service) {
        case GoogleTranslate:
            request.setRawHeader("Accept", "application/json");
            break;
        case AzureTranslator:
            request.setRawHeader("Ocp-Apim-Subscription-Key", getApiKey(service).toUtf8());
            break;
        case DeepL:
            request.setRawHeader("Authorization", ("DeepL-Auth-Key " + getApiKey(service)).toUtf8());
            break;
        case LibreTranslate:
            request.setRawHeader("Accept", "application/json");
            break;
        default:
            break;
    }
    
    return request;
}

void SubtitleTranslator::queueBatchRequest(const BatchRequest& request)
{
    m_batchQueue.append(request);
    
    if (!m_processingBatch) {
        m_batchTimer->start(static_cast<int>(m_options.requestDelay * 1000));
    }
}

void SubtitleTranslator::processBatchQueue()
{
    if (m_batchQueue.isEmpty() || m_processingBatch) {
        return;
    }
    
    m_processingBatch = true;
    BatchRequest request = m_batchQueue.takeFirst();
    
    processBatchRequest(request);
}

void SubtitleTranslator::processBatchRequest(const BatchRequest& request)
{
    // Split large batches into smaller chunks
    int batchSize = m_options.batchSize;
    QStringList texts = request.texts;
    
    if (texts.size() <= batchSize) {
        // Process entire batch at once
        switch (m_options.service) {
            case GoogleTranslate:
                translateWithGoogleTranslate(texts, request.sourceLanguage, 
                                           request.targetLanguage, request.identifier);
                break;
            case AzureTranslator:
                translateWithAzureTranslator(texts, request.sourceLanguage,
                                           request.targetLanguage, request.identifier);
                break;
            case AWSTranslate:
                translateWithAWSTranslate(texts, request.sourceLanguage,
                                        request.targetLanguage, request.identifier);
                break;
            case LibreTranslate:
                translateWithLibreTranslate(texts, request.sourceLanguage,
                                          request.targetLanguage, request.identifier);
                break;
            case DeepL:
                translateWithDeepL(texts, request.sourceLanguage,
                                 request.targetLanguage, request.identifier);
                break;
            case LocalTranslator:
                translateWithLocalTranslator(texts, request.sourceLanguage,
                                           request.targetLanguage, request.identifier);
                break;
        }
    } else {
        // Split into smaller batches
        for (int i = 0; i < texts.size(); i += batchSize) {
            QStringList batch = texts.mid(i, batchSize);
            BatchRequest subRequest = request;
            subRequest.texts = batch;
            subRequest.identifier = QString("%1_batch_%2").arg(request.identifier).arg(i / batchSize);
            
            queueBatchRequest(subRequest);
        }
    }
    
    m_processingBatch = false;
    
    // Process next batch if available
    if (!m_batchQueue.isEmpty()) {
        m_batchTimer->start(static_cast<int>(m_options.requestDelay * 1000));
    }
}

void SubtitleTranslator::handleNetworkReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    QString identifier = m_pendingRequests.take(reply);
    TranslationService service = m_requestServices.take(reply);
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        processTranslationResponse(data, service, identifier);
    } else {
        emit translationFailed("Network error: " + reply->errorString());
    }
    
    reply->deleteLater();
}

void SubtitleTranslator::processTranslationResponse(const QByteArray& data, TranslationService service,
                                                   const QString& identifier)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        emit translationFailed("Invalid response from translation service");
        return;
    }
    
    QJsonObject obj = doc.object();
    QStringList translatedTexts;
    
    switch (service) {
        case GoogleTranslate: {
            QJsonArray translations = obj["data"].toObject()["translations"].toArray();
            for (const QJsonValue& value : translations) {
                translatedTexts << value.toObject()["translatedText"].toString();
            }
            break;
        }
        case LibreTranslate: {
            translatedTexts << obj["translatedText"].toString();
            break;
        }
        default:
            break;
    }
    
    if (!translatedTexts.isEmpty()) {
        if (identifier == "subtitle_file") {
            finalizeBatchTranslation(translatedTexts);
        } else {
            emit batchTranslated(identifier, QStringList(), translatedTexts);
        }
    }
}

void SubtitleTranslator::finalizeBatchTranslation(const QStringList& translatedTexts)
{
    // Update subtitle entries with translated text
    for (int i = 0; i < translatedTexts.size() && i < m_currentEntries.size(); ++i) {
        m_currentEntries[i].text = postprocessText(translatedTexts[i], m_currentEntries[i].originalText);
    }
    
    // Write translated subtitle file
    if (writeSubtitleFile(m_currentFilePath, m_currentEntries)) {
        m_progress.translatedLines = m_currentEntries.size();
        m_progress.percentage = 100.0;
        m_progress.currentStatus = "Translation completed";
        
        m_isTranslating = false;
        emit translationProgress(m_progress);
        emit translationCompleted(m_currentFilePath);
    } else {
        m_isTranslating = false;
        emit translationFailed("Failed to write translated subtitle file");
    }
}

// Static utility methods
QString SubtitleTranslator::languageToString(Language language)
{
    // Same implementation as in SubtitleLanguageDetector
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
        default: return "Unknown";
    }
}

QString SubtitleTranslator::languageToCode(Language language)
{
    // Same implementation as in SubtitleLanguageDetector
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
        default: return "auto";
    }
}

QString SubtitleTranslator::serviceToString(TranslationService service)
{
    switch (service) {
        case GoogleTranslate: return "Google Translate";
        case AzureTranslator: return "Azure Translator";
        case AWSTranslate: return "AWS Translate";
        case LibreTranslate: return "LibreTranslate";
        case DeepL: return "DeepL";
        case LocalTranslator: return "Local Translator";
        default: return "Unknown";
    }
}

} // namespace AI
} // namespace EonPlay