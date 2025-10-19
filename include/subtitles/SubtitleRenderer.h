#pragma once

#include "SubtitleEntry.h"
#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QFont>
#include <QColor>
#include <QRect>

/**
 * @brief Subtitle rendering settings
 */
struct SubtitleRenderSettings
{
    // Font settings
    QFont font = QFont("Arial", 16, QFont::Bold);
    QColor textColor = Qt::white;
    QColor backgroundColor = Qt::transparent;
    QColor outlineColor = Qt::black;
    QColor shadowColor = QColor(0, 0, 0, 128);
    
    // Outline and shadow
    int outlineWidth = 2;
    int shadowOffset = 2;
    bool enableOutline = true;
    bool enableShadow = true;
    bool enableBackground = false;
    
    // Positioning
    Qt::Alignment alignment = Qt::AlignBottom | Qt::AlignHCenter;
    int marginLeft = 20;
    int marginRight = 20;
    int marginTop = 20;
    int marginBottom = 50;
    
    // Animation
    bool enableFadeIn = true;
    bool enableFadeOut = true;
    int fadeInDuration = 200;
    int fadeOutDuration = 200;
    
    // Advanced
    double opacity = 1.0;
    double scale = 1.0;
    int lineSpacing = 5;
    int maxLines = 3;
    bool wordWrap = true;
    
    SubtitleRenderSettings() = default;
    
    /**
     * @brief Check if settings are equal
     * @param other Other settings to compare
     * @return true if settings are equal
     */
    bool operator==(const SubtitleRenderSettings& other) const;
    
    /**
     * @brief Check if settings are different
     * @param other Other settings to compare
     * @return true if settings are different
     */
    bool operator!=(const SubtitleRenderSettings& other) const { return !(*this == other); }
};

/**
 * @brief Subtitle text overlay renderer widget
 * 
 * Renders subtitle text with advanced styling, positioning, and animations.
 * Supports multiple subtitle formats and customizable appearance.
 */
class SubtitleRenderer : public QWidget
{
    Q_OBJECT

public:
    explicit SubtitleRenderer(QWidget* parent = nullptr);
    ~SubtitleRenderer() override;
    
    /**
     * @brief Set subtitle entries to display
     * @param entries List of subtitle entries
     * @param time Current time in milliseconds
     */
    void setSubtitleEntries(const QList<SubtitleEntry>& entries, qint64 time);
    
    /**
     * @brief Clear all displayed subtitles
     */
    void clearSubtitles();
    
    /**
     * @brief Set rendering settings
     * @param settings New rendering settings
     */
    void setRenderSettings(const SubtitleRenderSettings& settings);
    
    /**
     * @brief Get current rendering settings
     * @return Current rendering settings
     */
    SubtitleRenderSettings renderSettings() const { return m_settings; }
    
    /**
     * @brief Enable or disable subtitle rendering
     * @param enabled true to enable rendering
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Check if subtitle rendering is enabled
     * @return true if rendering is enabled
     */
    bool isEnabled() const { return m_enabled; }
    
    /**
     * @brief Set subtitle opacity
     * @param opacity Opacity value (0.0 to 1.0)
     */
    void setOpacity(double opacity);
    
    /**
     * @brief Get current subtitle opacity
     * @return Current opacity value
     */
    double opacity() const { return m_settings.opacity; }
    
    /**
     * @brief Set subtitle scale factor
     * @param scale Scale factor (0.5 to 3.0)
     */
    void setScale(double scale);
    
    /**
     * @brief Get current subtitle scale
     * @return Current scale factor
     */
    double scale() const { return m_settings.scale; }
    
    /**
     * @brief Show subtitles with animation
     */
    void showSubtitles();
    
    /**
     * @brief Hide subtitles with animation
     */
    void hideSubtitles();
    
    /**
     * @brief Check if subtitles are currently visible
     * @return true if subtitles are visible
     */
    bool areSubtitlesVisible() const { return m_visible; }

public slots:
    /**
     * @brief Update subtitle position based on parent widget size
     */
    void updatePosition();

signals:
    /**
     * @brief Emitted when subtitle visibility changes
     * @param visible true if subtitles became visible
     */
    void visibilityChanged(bool visible);
    
    /**
     * @brief Emitted when subtitle rendering settings change
     * @param settings New rendering settings
     */
    void renderSettingsChanged(const SubtitleRenderSettings& settings);

protected:
    /**
     * @brief Paint event for rendering subtitles
     * @param event Paint event
     */
    void paintEvent(QPaintEvent* event) override;
    
    /**
     * @brief Resize event for updating position
     * @param event Resize event
     */
    void resizeEvent(QResizeEvent* event) override;
    
    /**
     * @brief Mouse press event for click-through behavior
     * @param event Mouse press event
     */
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    /**
     * @brief Handle fade in animation finished
     */
    void onFadeInFinished();
    
    /**
     * @brief Handle fade out animation finished
     */
    void onFadeOutFinished();

private:
    /**
     * @brief Render subtitle text with formatting
     * @param painter QPainter for rendering
     * @param entry Subtitle entry to render
     * @param rect Target rectangle
     */
    void renderSubtitleEntry(QPainter& painter, const SubtitleEntry& entry, const QRect& rect);
    
    /**
     * @brief Render text with outline and shadow
     * @param painter QPainter for rendering
     * @param text Text to render
     * @param rect Target rectangle
     * @param format Text format
     */
    void renderFormattedText(QPainter& painter, const QString& text, const QRect& rect, const SubtitleFormat& format);
    
    /**
     * @brief Calculate text layout rectangle
     * @param entry Subtitle entry
     * @return Calculated rectangle for text
     */
    QRect calculateTextRect(const SubtitleEntry& entry) const;
    
    /**
     * @brief Apply subtitle format to painter
     * @param painter QPainter to configure
     * @param format Subtitle format to apply
     */
    void applySubtitleFormat(QPainter& painter, const SubtitleFormat& format);
    
    /**
     * @brief Draw text with outline
     * @param painter QPainter for rendering
     * @param text Text to draw
     * @param rect Target rectangle
     * @param textColor Text color
     * @param outlineColor Outline color
     * @param outlineWidth Outline width
     */
    void drawTextWithOutline(QPainter& painter, const QString& text, const QRect& rect,
                            const QColor& textColor, const QColor& outlineColor, int outlineWidth);
    
    /**
     * @brief Draw text with shadow
     * @param painter QPainter for rendering
     * @param text Text to draw
     * @param rect Target rectangle
     * @param textColor Text color
     * @param shadowColor Shadow color
     * @param shadowOffset Shadow offset
     */
    void drawTextWithShadow(QPainter& painter, const QString& text, const QRect& rect,
                           const QColor& textColor, const QColor& shadowColor, int shadowOffset);
    
    /**
     * @brief Calculate total text height for multiple entries
     * @param entries Subtitle entries
     * @return Total height needed
     */
    int calculateTotalHeight(const QList<SubtitleEntry>& entries) const;
    
    /**
     * @brief Split text into lines based on width constraints
     * @param text Text to split
     * @param font Font to use for measurement
     * @param maxWidth Maximum width per line
     * @return List of text lines
     */
    QStringList splitTextToLines(const QString& text, const QFont& font, int maxWidth) const;
    
    QList<SubtitleEntry> m_currentEntries;
    SubtitleRenderSettings m_settings;
    
    bool m_enabled;
    bool m_visible;
    qint64 m_currentTime;
    
    // Animation components
    QPropertyAnimation* m_fadeInAnimation;
    QPropertyAnimation* m_fadeOutAnimation;
    QGraphicsOpacityEffect* m_opacityEffect;
    
    // Cached rendering data
    QRect m_lastRenderRect;
    QPixmap m_cachedPixmap;
    bool m_needsRedraw;
};