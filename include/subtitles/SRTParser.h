#pragma once

#include "ISubtitleParser.h"

/**
 * @brief Parser for SubRip (.srt) subtitle format
 * 
 * Supports the standard SRT format with timing and text.
 * Format: sequence number, timing, text lines, blank line separator.
 */
class SRTParser : public BaseSubtitleParser
{
public:
    SRTParser();
    ~SRTParser() override = default;
    
    // ISubtitleParser interface
    QString parserName() const override { return "SRT Parser"; }
    QStringList supportedExtensions() const override { return {"srt"}; }
    
    bool canParse(const QString& filePath) const override;
    bool canParseContent(const QString& content) const override;
    
    SubtitleTrack parseFile(const QString& filePath, SubtitleParseResult& result) override;
    SubtitleTrack parseContent(const QString& content, SubtitleParseResult& result) override;

private:
    /**
     * @brief Parse individual SRT subtitle block
     * @param block Text block containing one subtitle entry
     * @param entry Output subtitle entry
     * @return true if parsing successful
     */
    bool parseSubtitleBlock(const QString& block, SubtitleEntry& entry) const;
    
    /**
     * @brief Parse SRT timing line
     * @param timingLine Line containing start --> end timing
     * @param startTime Output start time in milliseconds
     * @param endTime Output end time in milliseconds
     * @return true if parsing successful
     */
    bool parseTimingLine(const QString& timingLine, qint64& startTime, qint64& endTime) const;
    
    /**
     * @brief Validate SRT format structure
     * @param content SRT file content
     * @return true if content appears to be valid SRT
     */
    bool isValidSRTStructure(const QString& content) const;
    
    /**
     * @brief Clean up SRT text content
     * @param text Raw subtitle text
     * @return Cleaned subtitle text
     */
    QString cleanSRTText(const QString& text) const;
    
    /**
     * @brief Parse SRT formatting tags
     * @param text Text with SRT formatting
     * @param format Output subtitle format
     * @return Plain text without formatting tags
     */
    QString parseSRTFormatting(const QString& text, SubtitleFormat& format) const;
};