#pragma once

#include "SubtitleEntry.h"
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <memory>

/**
 * @brief Subtitle parsing result information
 */
struct SubtitleParseResult
{
    bool success = false;
    QString errorMessage;
    int parsedEntries = 0;
    int skippedEntries = 0;
    QString detectedEncoding;
    QString detectedFormat;
    
    /**
     * @brief Check if parsing was successful
     * @return true if parsing succeeded
     */
    bool isSuccess() const { return success; }
    
    /**
     * @brief Get human-readable result summary
     * @return Result summary string
     */
    QString summary() const;
};

/**
 * @brief Abstract interface for subtitle format parsers
 * 
 * Defines the common interface for parsing different subtitle formats
 * like SRT, ASS/SSA, VTT, etc. with security validation and error handling.
 */
class ISubtitleParser
{
public:
    virtual ~ISubtitleParser() = default;
    
    /**
     * @brief Get parser name/identifier
     * @return Parser name
     */
    virtual QString parserName() const = 0;
    
    /**
     * @brief Get supported file extensions
     * @return List of supported extensions (without dots)
     */
    virtual QStringList supportedExtensions() const = 0;
    
    /**
     * @brief Check if parser can handle the given file
     * @param filePath Path to subtitle file
     * @return true if parser can handle this file
     */
    virtual bool canParse(const QString& filePath) const = 0;
    
    /**
     * @brief Check if parser can handle the given content
     * @param content Subtitle file content
     * @return true if parser can handle this content
     */
    virtual bool canParseContent(const QString& content) const = 0;
    
    /**
     * @brief Parse subtitle file and return subtitle track
     * @param filePath Path to subtitle file
     * @param result Parse result information (output)
     * @return Parsed subtitle track
     */
    virtual SubtitleTrack parseFile(const QString& filePath, SubtitleParseResult& result) = 0;
    
    /**
     * @brief Parse subtitle content and return subtitle track
     * @param content Subtitle file content
     * @param result Parse result information (output)
     * @return Parsed subtitle track
     */
    virtual SubtitleTrack parseContent(const QString& content, SubtitleParseResult& result) = 0;
    
    /**
     * @brief Validate subtitle file for security issues
     * @param filePath Path to subtitle file
     * @return true if file is safe to parse
     */
    virtual bool validateFile(const QString& filePath) const = 0;
    
    /**
     * @brief Validate subtitle content for security issues
     * @param content Subtitle content
     * @return true if content is safe to parse
     */
    virtual bool validateContent(const QString& content) const = 0;
    
    /**
     * @brief Get maximum allowed file size for this format
     * @return Maximum file size in bytes
     */
    virtual qint64 maxFileSize() const = 0;
    
    /**
     * @brief Get maximum allowed number of subtitle entries
     * @return Maximum number of entries
     */
    virtual int maxEntries() const = 0;
    
    /**
     * @brief Detect text encoding of subtitle file
     * @param filePath Path to subtitle file
     * @return Detected encoding name
     */
    virtual QString detectEncoding(const QString& filePath) const = 0;
};

/**
 * @brief Base implementation of ISubtitleParser with common functionality
 */
class BaseSubtitleParser : public ISubtitleParser
{
public:
    BaseSubtitleParser();
    virtual ~BaseSubtitleParser() = default;
    
    // Common validation implementation
    bool validateFile(const QString& filePath) const override;
    bool validateContent(const QString& content) const override;
    
    // Default limits
    qint64 maxFileSize() const override { return m_maxFileSize; }
    int maxEntries() const override { return m_maxEntries; }
    
    // Encoding detection
    QString detectEncoding(const QString& filePath) const override;

protected:
    /**
     * @brief Read file content with encoding detection
     * @param filePath Path to file
     * @param encoding Detected encoding (output)
     * @return File content as string
     */
    QString readFileContent(const QString& filePath, QString& encoding) const;
    
    /**
     * @brief Sanitize subtitle text for security
     * @param text Input text
     * @return Sanitized text
     */
    QString sanitizeText(const QString& text) const;
    
    /**
     * @brief Check if text contains suspicious content
     * @param text Text to check
     * @return true if text appears safe
     */
    bool isSafeText(const QString& text) const;
    
    /**
     * @brief Parse common time formats
     * @param timeStr Time string
     * @return Time in milliseconds, -1 if invalid
     */
    qint64 parseGenericTime(const QString& timeStr) const;
    
    /**
     * @brief Clean up subtitle text (remove extra whitespace, etc.)
     * @param text Input text
     * @return Cleaned text
     */
    QString cleanupText(const QString& text) const;

private:
    qint64 m_maxFileSize;
    int m_maxEntries;
    QStringList m_suspiciousPatterns;
    QStringList m_allowedTags;
};

/**
 * @brief Factory for creating subtitle parsers
 */
class SubtitleParserFactory
{
public:
    /**
     * @brief Register a subtitle parser
     * @param parser Parser instance (factory takes ownership)
     */
    static void registerParser(std::unique_ptr<ISubtitleParser> parser);
    
    /**
     * @brief Get parser for file extension
     * @param extension File extension (without dot)
     * @return Parser instance or nullptr if not found
     */
    static ISubtitleParser* getParserForExtension(const QString& extension);
    
    /**
     * @brief Get parser for file path
     * @param filePath Path to subtitle file
     * @return Parser instance or nullptr if not found
     */
    static ISubtitleParser* getParserForFile(const QString& filePath);
    
    /**
     * @brief Get parser for content
     * @param content Subtitle content
     * @return Parser instance or nullptr if not found
     */
    static ISubtitleParser* getParserForContent(const QString& content);
    
    /**
     * @brief Get all registered parsers
     * @return List of all parsers
     */
    static QList<ISubtitleParser*> getAllParsers();
    
    /**
     * @brief Get all supported extensions
     * @return List of all supported extensions
     */
    static QStringList getSupportedExtensions();
    
    /**
     * @brief Initialize default parsers
     */
    static void initializeDefaultParsers();

private:
    static QList<std::unique_ptr<ISubtitleParser>> s_parsers;
};