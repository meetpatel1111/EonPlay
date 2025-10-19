#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

namespace EonPlay {
namespace AI {

/**
 * @brief Translates subtitle files between different languages
 * 
 * Provides automatic subtitle translation using various translation
 * services and maintains subtitle timing and formatting.
 */
class SubtitleTranslator : public QObject
{
    Q_OBJECT

public:
    enum TranslationService {
        GoogleTranslate,
        AzureTranslator,
        AWSTranslate,
        LibreTranslate,
        DeepL,
        LocalTranslator  // Offline translation
    };

    enum Language {
        Auto,
        English,
        Spanish,
        French,
        German,
        Italian,
        Portuguese,
        Russian,
        Chinese,
        Japanese,
        Korean,
        Arabic,
        Hindi,
        Dutch,
        Swedish,
        Norwegian,
        Danish,
        Finnish,
        Polish,
        Czech,
        Hungarian,
        Romanian,
        Bulgarian,
        Greek,
        Turkish,
        Hebrew,
        Thai,
        Vietnamese,
        Indonesian,
        Malay
    };

    struct TranslationOptions {
        TranslationService service = GoogleTranslate;
        Language sourceLanguage = Auto;
        Language targetLanguage = English;
        bool preserveFormatting = true;
        bool preserveTiming = true;
        bool translateSpeakerNames = false;
        int maxLineLength = 42;
        int batchSize = 50;  // Number of lines to translate in one request
        double requestDelay = 1.0;  // Delay between requests in seconds
    };

    struct TranslationProgress {
        int totalLines = 0;
        int translatedLines = 0;
        int failedLines = 0;
        double percentage = 0.0;
        QString currentStatus;
    };

    explicit SubtitleTranslator(QObject* parent = nullptr);
    ~SubtitleTranslator();

    // Configuration
    void setTranslationService(TranslationService service) { m_options.service = service; }
    TranslationService translationService() const { return m_options.service; }

    void setTranslationOptions(const TranslationOptions& options) { m_options = options; }
    TranslationOptions translationOptions() const { return m_options; }

    void setApiKey(TranslationService service, const QString& apiKey);
    QString getApiKey(TranslationService service) const;
    bool isServiceAvailable(TranslationService service) const;

    // Translation operations
    void translateSubtitleFile(const QString& inputPath, const QString& outputPath,
                              Language sourceLanguage = Auto, Language targetLanguage = English);
    void translateText(const QString& text, Language sourceLanguage = Auto, 
                      Language targetLanguage = English, const QString& identifier = QString());
    void translateMultipleFiles(const QStringList& inputPaths, const QString& outputDirectory,
                               Language sourceLanguage = Auto, Language targetLanguage = English);

    // Batch translation
    void translateTextBatch(const QStringList& texts, Language sourceLanguage = Auto,
                           Language targetLanguage = English, const QString& identifier = QString());

    // Progress and control
    bool isTranslating() const { return m_isTranslating; }
    TranslationProgress getProgress() const { return m_progress; }
    void cancelTranslation();

    // Supported languages
    QStringList getSupportedLanguages(TranslationService service) const;
    bool isLanguageSupported(Language language, TranslationService service) const;

    // Utility methods
    static QString languageToString(Language language);
    static QString languageToCode(Language language);
    static Language stringToLanguage(const QString& languageStr);
    static Language codeToLanguage(const QString& code);
    static QString serviceToString(TranslationService service);

signals:
    void translationStarted(const QString& filePath);
    void translationProgress(const TranslationProgress& progress);
    void translationCompleted(const QString& outputPath);
    void translationFailed(const QString& error);
    void translationCancelled();
    
    void textTranslated(const QString& identifier, const QString& originalText, 
                       const QString& translatedText, Language detectedLanguage);
    void batchTranslated(const QString& identifier, const QStringList& originalTexts,
                        const QStringList& translatedTexts);

private slots:
    void handleNetworkReply();
    void processBatchQueue();

private:
    // Service implementations
    void translateWithGoogleTranslate(const QStringList& texts, Language sourceLang, 
                                     Language targetLang, const QString& identifier);
    void translateWithAzureTranslator(const QStringList& texts, Language sourceLang,
                                     Language targetLang, const QString& identifier);
    void translateWithAWSTranslate(const QStringList& texts, Language sourceLang,
                                  Language targetLang, const QString& identifier);
    void translateWithLibreTranslate(const QStringList& texts, Language sourceLang,
                                    Language targetLang, const QString& identifier);
    void translateWithDeepL(const QStringList& texts, Language sourceLang,
                           Language targetLang, const QString& identifier);
    void translateWithLocalTranslator(const QStringList& texts, Language sourceLang,
                                     Language targetLang, const QString& identifier);

    // Subtitle file processing
    struct SubtitleEntry {
        int index;
        QString startTime;
        QString endTime;
        QString text;
        QString originalText;
    };

    QList<SubtitleEntry> parseSubtitleFile(const QString& filePath);
    bool writeSubtitleFile(const QString& filePath, const QList<SubtitleEntry>& entries);
    
    QList<SubtitleEntry> parseSRTFile(const QString& filePath);
    QList<SubtitleEntry> parseASSFile(const QString& filePath);
    QList<SubtitleEntry> parseVTTFile(const QString& filePath);
    
    bool writeSRTFile(const QString& filePath, const QList<SubtitleEntry>& entries);
    bool writeASSFile(const QString& filePath, const QList<SubtitleEntry>& entries);
    bool writeVTTFile(const QString& filePath, const QList<SubtitleEntry>& entries);

    // Text processing
    QString preprocessText(const QString& text);
    QString postprocessText(const QString& text, const QString& originalText);
    QStringList splitLongText(const QString& text, int maxLength);
    QString preserveFormatting(const QString& translatedText, const QString& originalText);

    // Network helpers
    void setupNetworkManager();
    QNetworkRequest createApiRequest(const QString& url, TranslationService service);
    void processTranslationResponse(const QByteArray& data, TranslationService service,
                                   const QString& identifier);
    void finalizeBatchTranslation(const QStringList& translatedTexts);

    // Batch processing
    struct BatchRequest {
        QStringList texts;
        Language sourceLanguage;
        Language targetLanguage;
        QString identifier;
        QString filePath;
    };

    void queueBatchRequest(const BatchRequest& request);
    void processBatchRequest(const BatchRequest& request);

    // Member variables
    TranslationOptions m_options;
    bool m_isTranslating;
    TranslationProgress m_progress;
    
    // Current translation state
    QList<SubtitleEntry> m_currentEntries;
    QString m_currentFilePath;
    
    // Network management
    QNetworkAccessManager* m_networkManager;
    QHash<QNetworkReply*, QString> m_pendingRequests;
    QHash<QNetworkReply*, TranslationService> m_requestServices;
    
    // API keys
    QHash<TranslationService, QString> m_apiKeys;
    
    // Batch processing
    QList<BatchRequest> m_batchQueue;
    QTimer* m_batchTimer;
    bool m_processingBatch;
    
    // Current translation state
    QString m_currentFilePath;
    QList<SubtitleEntry> m_currentEntries;
    int m_currentBatchIndex;
    QStringList m_translatedBatches;
};

} // namespace AI
} // namespace EonPlay