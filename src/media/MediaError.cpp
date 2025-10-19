#include "media/MediaError.h"
#include <QObject>

QString MediaError::userMessage() const
{
    switch (m_type) {
        case MediaErrorType::NoError:
            return QString();
            
        case MediaErrorType::MediaError:
            if (m_message.contains("codec", Qt::CaseInsensitive)) {
                return QObject::tr("This media format is not supported. Please try a different file.");
            } else if (m_message.contains("corrupt", Qt::CaseInsensitive)) {
                return QObject::tr("The media file appears to be corrupted and cannot be played.");
            } else {
                return QObject::tr("Unable to play this media file. The format may not be supported.");
            }
            
        case MediaErrorType::SystemError:
            if (m_message.contains("access", Qt::CaseInsensitive)) {
                return QObject::tr("Cannot access the media file. Please check file permissions.");
            } else if (m_message.contains("hardware", Qt::CaseInsensitive)) {
                return QObject::tr("Hardware acceleration failed. Playback will continue using software decoding.");
            } else {
                return QObject::tr("A system error occurred during media playback.");
            }
            
        case MediaErrorType::NetworkError:
            if (m_message.contains("connection", Qt::CaseInsensitive)) {
                return QObject::tr("Network connection failed. Please check your internet connection.");
            } else if (m_message.contains("timeout", Qt::CaseInsensitive)) {
                return QObject::tr("Network request timed out. Please try again.");
            } else {
                return QObject::tr("A network error occurred while streaming media.");
            }
            
        case MediaErrorType::SecurityError:
            return QObject::tr("This media file cannot be played for security reasons.");
            
        default:
            return QObject::tr("An unknown error occurred during media playback.");
    }
}

QString MediaError::technicalDetails() const
{
    if (m_details.isEmpty()) {
        return m_message;
    }
    
    return QString("%1\nDetails: %2").arg(m_message, m_details);
}

QString MediaError::toString() const
{
    QString typeStr;
    switch (m_type) {
        case MediaErrorType::NoError: typeStr = "NoError"; break;
        case MediaErrorType::MediaError: typeStr = "MediaError"; break;
        case MediaErrorType::SystemError: typeStr = "SystemError"; break;
        case MediaErrorType::NetworkError: typeStr = "NetworkError"; break;
        case MediaErrorType::SecurityError: typeStr = "SecurityError"; break;
    }
    
    QString severityStr;
    switch (m_severity) {
        case MediaErrorSeverity::Info: severityStr = "Info"; break;
        case MediaErrorSeverity::Warning: severityStr = "Warning"; break;
        case MediaErrorSeverity::Error: severityStr = "Error"; break;
        case MediaErrorSeverity::Critical: severityStr = "Critical"; break;
    }
    
    return QString("[%1:%2] %3").arg(typeStr, severityStr, m_message);
}

MediaError MediaError::mediaLoadError(const QString& details)
{
    return MediaError(MediaErrorType::MediaError, MediaErrorSeverity::Error,
                     QObject::tr("Failed to load media file"), details);
}

MediaError MediaError::codecError(const QString& details)
{
    return MediaError(MediaErrorType::MediaError, MediaErrorSeverity::Error,
                     QObject::tr("Unsupported codec or media format"), details);
}

MediaError MediaError::fileAccessError(const QString& details)
{
    return MediaError(MediaErrorType::SystemError, MediaErrorSeverity::Error,
                     QObject::tr("Cannot access media file"), details);
}

MediaError MediaError::networkError(const QString& details)
{
    return MediaError(MediaErrorType::NetworkError, MediaErrorSeverity::Error,
                     QObject::tr("Network streaming error"), details);
}

MediaError MediaError::securityError(const QString& details)
{
    return MediaError(MediaErrorType::SecurityError, MediaErrorSeverity::Critical,
                     QObject::tr("Security validation failed"), details);
}

MediaError MediaError::hardwareError(const QString& details)
{
    return MediaError(MediaErrorType::SystemError, MediaErrorSeverity::Warning,
                     QObject::tr("Hardware acceleration error"), details);
}