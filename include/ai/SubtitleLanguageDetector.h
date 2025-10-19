#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace EonPlay {
namespace AI {

/**
 * @brief Detects the language of subtitle files and text content
 * 
 * Provides automatic language detection for subtitle files using
 * various detection methods including local algorithms and cloud APIs.
 */
class SubtitleLanguageDetector : public QObject
{
    Q_OBJECT

public:
    enum DetectionMethod {
        LocalNGram,     // Local n-gram based detection
        GoogleTranslate, // Google Translate API
        AzureTranslator, // Azure Translator
        AWSTranslate,    // AWS Translate
        LibreTranslate   // LibreTranslate (open source)
    };

    enum Language {
        Unknown,
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

    struct DetectionResult {
        Language language = Unknown;
        double confidence = 0.0;
        QString languageCode;
        QString languageName;
        QHash<Language, double> alternativeLanguages;
    };

    explicit SubtitleLanguageDetector(QObject* parent = nullptr);
    ~SubtitleLanguageDetector() = default;

    // Configuration
    void setDetectionMethod(DetectionMethod method) { m_method = method; }
    DetectionMethod detectionMethod() const { return m_method; }

    void setApiKey(DetectionMethod method, const QString& apiKey);
    QString getApiKey(DetectionMethod method) const;
    bool isMethodAvailable(DetectionMethod method) const;

    // Detection operations
    void detectLanguageFromFile(const QString& subtitleFilePath);
    void detectLanguageFromText(const QString& text);
    void detectLanguageFromMultipleFiles(const QStringList& filePaths);

    // Synchronous detection (local methods only)
    DetectionResult detectLanguageSync(const QString& text);
    Language detectSimpleLanguage(const QString& text);

    // Language utilities
    static QString languageToString(Language language);
    static QString languageToCode(Language language);
    static QString languageToDisplayName(Language language);
    static Language stringToLanguage(const QString& languageStr);
    static Language codeToLanguage(const QString& code);
    static QStringList getSupportedLanguages();
    static QStringList getSupportedLanguageCodes();

    // File format support
    bool isSupportedSubtitleFile(const QString& filePath) const;
    QString extractTextFromSubtitleFile(const QString& filePath);

signals:
    void languageDetected(const QString& filePath, const DetectionResult& result);
    void detectionFailed(const QString& filePath, const QString& error);
    void multipleDetectionCompleted(const QHash<QString, DetectionResult>& results);

private slots:
    void handleNetworkReply();

private:
    // Detection implementations
    DetectionResult detectWithLocalNGram(const QString& text);
    void detectWithGoogleTranslate(const QString& text, const QString& identifier = QString());
    void detectWithAzureTranslator(const QString& text, const QString& identifier = QString());
    void detectWithAWSTranslate(const QString& text, const QString& identifier = QString());
    void detectWithLibreTranslate(const QString& text, const QString& identifier = QString());

    // Local detection helpers
    void initializeLanguageModels();
    double calculateNGramScore(const QString& text, Language language);
    QHash<QString, double> extractNGrams(const QString& text, int n = 3);
    QString normalizeText(const QString& text);

    // Subtitle parsing
    QString extractSRTText(const QString& filePath);
    QString extractASSText(const QString& filePath);
    QString extractVTTText(const QString& filePath);

    // Network helpers
    void setupNetworkManager();
    QNetworkRequest createApiRequest(const QString& url, DetectionMethod method);

    // Member variables
    DetectionMethod m_method;
    QHash<DetectionMethod, QString> m_apiKeys;
    QNetworkAccessManager* m_networkManager;
    QHash<QNetworkReply*, QString> m_pendingRequests; // Maps reply to identifier

    // Language models for local detection
    QHash<Language, QHash<QString, double>> m_ngramModels;
    bool m_modelsInitialized;

    // Common words for quick detection
    QHash<Language, QStringList> m_commonWords;
    QHash<Language, QStringList> m_characterSets;
};

} // namespace AI
} // namespace EonPlay