#pragma once

#include <QString>
#include <QMetaType>

/**
 * @brief Media player error categories
 */
enum class MediaErrorType
{
    NoError,
    MediaError,      // Codec issues, corrupted files, unsupported formats
    SystemError,     // File access, hardware acceleration failures
    NetworkError,    // Streaming failures, connection issues
    SecurityError    // Malicious content, validation failures
};

/**
 * @brief Error severity levels
 */
enum class MediaErrorSeverity
{
    Info,
    Warning,
    Error,
    Critical
};

/**
 * @brief Comprehensive error information for media operations
 */
class MediaError
{
public:
    MediaError() 
        : m_type(MediaErrorType::NoError)
        , m_severity(MediaErrorSeverity::Info) 
    {}
    
    MediaError(MediaErrorType type, MediaErrorSeverity severity, 
               const QString& message, const QString& details = QString())
        : m_type(type)
        , m_severity(severity)
        , m_message(message)
        , m_details(details)
    {}
    
    // Getters
    MediaErrorType type() const { return m_type; }
    MediaErrorSeverity severity() const { return m_severity; }
    QString message() const { return m_message; }
    QString details() const { return m_details; }
    
    // Convenience methods
    bool isError() const { return m_type != MediaErrorType::NoError; }
    bool isCritical() const { return m_severity == MediaErrorSeverity::Critical; }
    
    /**
     * @brief Get user-friendly error message
     * @return Localized error message suitable for display
     */
    QString userMessage() const;
    
    /**
     * @brief Get technical error details
     * @return Technical details for debugging
     */
    QString technicalDetails() const;
    
    /**
     * @brief Convert error to string representation
     * @return String representation of the error
     */
    QString toString() const;
    
    // Static factory methods for common errors
    static MediaError mediaLoadError(const QString& details = QString());
    static MediaError codecError(const QString& details = QString());
    static MediaError fileAccessError(const QString& details = QString());
    static MediaError networkError(const QString& details = QString());
    static MediaError securityError(const QString& details = QString());
    static MediaError hardwareError(const QString& details = QString());

private:
    MediaErrorType m_type;
    MediaErrorSeverity m_severity;
    QString m_message;
    QString m_details;
};

Q_DECLARE_METATYPE(MediaError)
Q_DECLARE_METATYPE(MediaErrorType)
Q_DECLARE_METATYPE(MediaErrorSeverity)