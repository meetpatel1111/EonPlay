#pragma once

#include "ISubtitleParser.h"
#include <QHash>

/**
 * @brief ASS/SSA style definition
 */
struct ASSStyle
{
    QString name;
    QString fontName = "Arial";
    int fontSize = 16;
    QColor primaryColor = Qt::white;
    QColor secondaryColor = Qt::red;
    QColor outlineColor = Qt::black;
    QColor shadowColor = Qt::black;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikeOut = false;
    double scaleX = 100.0;
    double scaleY = 100.0;
    double spacing = 0.0;
    double angle = 0.0;
    int borderStyle = 1;
    double outline = 2.0;
    double shadow = 0.0;
    int alignment = 2;
    int marginL = 10;
    int marginR = 10;
    int marginV = 10;
    int encoding = 1;
    
    ASSStyle() = default;
    ASSStyle(const QString& styleName) : name(styleName) {}
    
    /**
     * @brief Convert ASS style to SubtitleFormat
     * @return Equivalent SubtitleFormat
     */
    SubtitleFormat toSubtitleFormat() const;
    
    /**
     * @brief Parse ASS style from string
     * @param styleString ASS style definition string
     * @return true if parsing successful
     */
    bool parseFromString(const QString& styleString);
};

/**
 * @brief Parser for Advanced SubStation Alpha (.ass/.ssa) subtitle format
 * 
 * Supports ASS/SSA format with advanced styling, positioning, and effects.
 * Handles [Script Info], [V4+ Styles], and [Events] sections.
 */
class ASSParser : public BaseSubtitleParser
{
public:
    ASSParser();
    ~ASSParser() override = default;
    
    // ISubtitleParser interface
    QString parserName() const override { return "ASS/SSA Parser"; }
    QStringList supportedExtensions() const override { return {"ass", "ssa"}; }
    
    bool canParse(const QString& filePath) const override;
    bool canParseContent(const QString& content) const override;
    
    SubtitleTrack parseFile(const QString& filePath, SubtitleParseResult& result) override;
    SubtitleTrack parseContent(const QString& content, SubtitleParseResult& result) override;

private:
    /**
     * @brief Parse ASS file sections
     * @param content File content
     * @param scriptInfo Output script info section
     * @param styles Output styles section
     * @param events Output events section
     * @return true if parsing successful
     */
    bool parseSections(const QString& content, QString& scriptInfo, QString& styles, QString& events) const;
    
    /**
     * @brief Parse [Script Info] section
     * @param scriptInfo Script info section content
     * @param track Output subtitle track
     * @return true if parsing successful
     */
    bool parseScriptInfo(const QString& scriptInfo, SubtitleTrack& track) const;
    
    /**
     * @brief Parse [V4+ Styles] section
     * @param stylesSection Styles section content
     * @return Hash map of style name to ASSStyle
     */
    QHash<QString, ASSStyle> parseStyles(const QString& stylesSection) const;
    
    /**
     * @brief Parse [Events] section
     * @param eventsSection Events section content
     * @param styles Available styles
     * @param track Output subtitle track
     * @param result Parse result (for statistics)
     * @return true if parsing successful
     */
    bool parseEvents(const QString& eventsSection, const QHash<QString, ASSStyle>& styles, 
                    SubtitleTrack& track, SubtitleParseResult& result) const;
    
    /**
     * @brief Parse individual ASS dialogue line
     * @param dialogueLine Dialogue line from Events section
     * @param styles Available styles
     * @param entry Output subtitle entry
     * @return true if parsing successful
     */
    bool parseDialogueLine(const QString& dialogueLine, const QHash<QString, ASSStyle>& styles, 
                          SubtitleEntry& entry) const;
    
    /**
     * @brief Parse ASS override codes in text
     * @param text Text with ASS override codes
     * @param baseFormat Base format from style
     * @return Plain text with format modifications applied
     */
    QString parseOverrideCodes(const QString& text, SubtitleFormat& baseFormat) const;
    
    /**
     * @brief Validate ASS format structure
     * @param content ASS file content
     * @return true if content appears to be valid ASS
     */
    bool isValidASSStructure(const QString& content) const;
    
    /**
     * @brief Parse ASS color value
     * @param colorStr ASS color string (&Hbbggrr& or decimal)
     * @return QColor object
     */
    QColor parseASSColor(const QString& colorStr) const;
    
    /**
     * @brief Parse ASS alignment value
     * @param alignment ASS alignment number
     * @return Qt alignment flags
     */
    Qt::Alignment parseASSAlignment(int alignment) const;
    
    /**
     * @brief Clean ASS text content
     * @param text Raw ASS text
     * @return Cleaned text
     */
    QString cleanASSText(const QString& text) const;
    
    /**
     * @brief Split CSV line respecting quoted fields
     * @param line CSV line
     * @return List of fields
     */
    QStringList splitCSVLine(const QString& line) const;
};