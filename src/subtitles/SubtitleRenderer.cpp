#include "subtitles/SubtitleRenderer.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QApplication>
#include <QMouseEvent>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(subtitleRenderer, "eonplay.subtitles.renderer")

// SubtitleRenderSettings Implementation
bool SubtitleRenderSettings::operator==(const SubtitleRenderSettings& other) const
{
    return font == other.font &&
           textColor == other.textColor &&
           backgroundColor == other.backgroundColor &&
           outlineColor == other.outlineColor &&
           shadowColor == other.shadowColor &&
           outlineWidth == other.outlineWidth &&
           shadowOffset == other.shadowOffset &&
           enableOutline == other.enableOutline &&
           enableShadow == other.enableShadow &&
           enableBackground == other.enableBackground &&
           alignment == other.alignment &&
           marginLeft == other.marginLeft &&
           marginRight == other.marginRight &&
           marginTop == other.marginTop &&
           marginBottom == other.marginBottom &&
           enableFadeIn == other.enableFadeIn &&
           enableFadeOut == other.enableFadeOut &&
           fadeInDuration == other.fadeInDuration &&
           fadeOutDuration == other.fadeOutDuration &&
           qAbs(opacity - other.opacity) < 0.01 &&
           qAbs(scale - other.scale) < 0.01 &&
           lineSpacing == other.lineSpacing &&
           maxLines == other.maxLines &&
           wordWrap == other.wordWrap;
}

// SubtitleRenderer Implementation
SubtitleRenderer::SubtitleRenderer(QWidget* parent)
    : QWidget(parent)
    , m_enabled(true)
    , m_visible(false)
    , m_currentTime(0)
    , m_fadeInAnimation(nullptr)
    , m_fadeOutAnimation(nullptr)
    , m_opacityEffect(nullptr)
    , m_needsRedraw(true)
{
    // Set widget properties
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);
    
    // Create opacity effect
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);
    
    // Create fade animations
    m_fadeInAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeInAnimation->setStartValue(0.0);
    m_fadeInAnimation->setEndValue(1.0);
    connect(m_fadeInAnimation, &QPropertyAnimation::finished, this, &SubtitleRenderer::onFadeInFinished);
    
    m_fadeOutAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeOutAnimation->setStartValue(1.0);
    m_fadeOutAnimation->setEndValue(0.0);
    connect(m_fadeOutAnimation, &QPropertyAnimation::finished, this, &SubtitleRenderer::onFadeOutFinished);
    
    // Set initial settings
    m_settings = SubtitleRenderSettings();
    
    qCDebug(subtitleRenderer) << "SubtitleRenderer created";
}

SubtitleRenderer::~SubtitleRenderer()
{
    qCDebug(subtitleRenderer) << "SubtitleRenderer destroyed";
}

void SubtitleRenderer::setSubtitleEntries(const QList<SubtitleEntry>& entries, qint64 time)
{
    bool entriesChanged = (entries.size() != m_currentEntries.size());
    if (!entriesChanged) {
        for (int i = 0; i < entries.size(); ++i) {
            if (entries[i].text() != m_currentEntries[i].text() ||
                entries[i].startTime() != m_currentEntries[i].startTime()) {
                entriesChanged = true;
                break;
            }
        }
    }
    
    if (entriesChanged) {
        m_currentEntries = entries;
        m_currentTime = time;
        m_needsRedraw = true;
        
        if (entries.isEmpty()) {
            if (m_visible) {
                hideSubtitles();
            }
        } else {
            if (!m_visible) {
                showSubtitles();
            } else {
                update(); // Redraw with new content
            }
        }
        
        qCDebug(subtitleRenderer) << "Subtitle entries updated:" << entries.size() << "entries";
    }
}

void SubtitleRenderer::clearSubtitles()
{
    if (!m_currentEntries.isEmpty()) {
        m_currentEntries.clear();
        m_needsRedraw = true;
        
        if (m_visible) {
            hideSubtitles();
        }
        
        qCDebug(subtitleRenderer) << "Subtitles cleared";
    }
}

void SubtitleRenderer::setRenderSettings(const SubtitleRenderSettings& settings)
{
    if (m_settings != settings) {
        m_settings = settings;
        m_needsRedraw = true;
        
        // Update animation durations
        m_fadeInAnimation->setDuration(settings.fadeInDuration);
        m_fadeOutAnimation->setDuration(settings.fadeOutDuration);
        
        update();
        emit renderSettingsChanged(settings);
        
        qCDebug(subtitleRenderer) << "Render settings updated";
    }
}

void SubtitleRenderer::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        
        if (!enabled && m_visible) {
            hideSubtitles();
        } else if (enabled && !m_currentEntries.isEmpty()) {
            showSubtitles();
        }
        
        qCDebug(subtitleRenderer) << "Subtitle rendering" << (enabled ? "enabled" : "disabled");
    }
}

void SubtitleRenderer::setOpacity(double opacity)
{
    opacity = qBound(0.0, opacity, 1.0);
    if (qAbs(m_settings.opacity - opacity) > 0.01) {
        m_settings.opacity = opacity;
        m_opacityEffect->setOpacity(opacity);
        update();
    }
}

void SubtitleRenderer::setScale(double scale)
{
    scale = qBound(0.5, scale, 3.0);
    if (qAbs(m_settings.scale - scale) > 0.01) {
        m_settings.scale = scale;
        m_needsRedraw = true;
        update();
    }
}

void SubtitleRenderer::showSubtitles()
{
    if (!m_enabled || m_currentEntries.isEmpty()) {
        return;
    }
    
    if (!m_visible) {
        m_visible = true;
        show();
        
        if (m_settings.enableFadeIn) {
            m_fadeInAnimation->start();
        } else {
            m_opacityEffect->setOpacity(m_settings.opacity);
            onFadeInFinished();
        }
        
        emit visibilityChanged(true);
        qCDebug(subtitleRenderer) << "Showing subtitles";
    }
}

void SubtitleRenderer::hideSubtitles()
{
    if (m_visible) {
        if (m_settings.enableFadeOut) {
            m_fadeOutAnimation->start();
        } else {
            m_opacityEffect->setOpacity(0.0);
            onFadeOutFinished();
        }
        
        qCDebug(subtitleRenderer) << "Hiding subtitles";
    }
}

void SubtitleRenderer::updatePosition()
{
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        m_needsRedraw = true;
        update();
    }
}

void SubtitleRenderer::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    if (!m_enabled || m_currentEntries.isEmpty()) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    // Apply global scale
    if (qAbs(m_settings.scale - 1.0) > 0.01) {
        painter.scale(m_settings.scale, m_settings.scale);
    }
    
    // Calculate total height needed
    int totalHeight = calculateTotalHeight(m_currentEntries);
    int startY = 0;
    
    // Position based on alignment
    if (m_settings.alignment & Qt::AlignTop) {
        startY = m_settings.marginTop;
    } else if (m_settings.alignment & Qt::AlignBottom) {
        startY = height() - totalHeight - m_settings.marginBottom;
    } else {
        startY = (height() - totalHeight) / 2;
    }
    
    // Render each subtitle entry
    int currentY = startY;
    for (const SubtitleEntry& entry : m_currentEntries) {
        QRect entryRect = calculateTextRect(entry);
        entryRect.moveTop(currentY);
        
        renderSubtitleEntry(painter, entry, entryRect);
        
        currentY += entryRect.height() + m_settings.lineSpacing;
    }
}

void SubtitleRenderer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_needsRedraw = true;
}

void SubtitleRenderer::mousePressEvent(QMouseEvent* event)
{
    // Make widget click-through
    event->ignore();
}

void SubtitleRenderer::onFadeInFinished()
{
    // Fade in completed
}

void SubtitleRenderer::onFadeOutFinished()
{
    m_visible = false;
    hide();
    emit visibilityChanged(false);
}

void SubtitleRenderer::renderSubtitleEntry(QPainter& painter, const SubtitleEntry& entry, const QRect& rect)
{
    if (entry.text().isEmpty()) {
        return;
    }
    
    // Apply entry-specific format
    SubtitleFormat format = entry.format();
    applySubtitleFormat(painter, format);
    
    // Render formatted text
    renderFormattedText(painter, entry.text(), rect, format);
}

void SubtitleRenderer::renderFormattedText(QPainter& painter, const QString& text, const QRect& rect, const SubtitleFormat& format)
{
    // Use format colors or fall back to settings
    QColor textColor = (format.textColor != Qt::transparent) ? format.textColor : m_settings.textColor;
    QColor outlineColor = (format.outlineColor != Qt::transparent) ? format.outlineColor : m_settings.outlineColor;
    QColor backgroundColor = (format.backgroundColor != Qt::transparent) ? format.backgroundColor : m_settings.backgroundColor;
    
    // Draw background if enabled
    if (m_settings.enableBackground && backgroundColor != Qt::transparent) {
        painter.fillRect(rect, backgroundColor);
    }
    
    // Split text into lines if word wrap is enabled
    QStringList lines;
    if (m_settings.wordWrap) {
        lines = splitTextToLines(text, format.font, rect.width());
    } else {
        lines = text.split('\n');
    }
    
    // Limit number of lines
    if (lines.size() > m_settings.maxLines) {
        lines = lines.mid(0, m_settings.maxLines);
        if (!lines.isEmpty()) {
            lines.last() += "...";
        }
    }
    
    // Calculate line height
    QFontMetrics fm(format.font);
    int lineHeight = fm.height();
    int totalTextHeight = lines.size() * lineHeight + (lines.size() - 1) * m_settings.lineSpacing;
    
    // Calculate starting Y position
    int startY = rect.top();
    if (format.alignment & Qt::AlignVCenter) {
        startY = rect.center().y() - totalTextHeight / 2;
    } else if (format.alignment & Qt::AlignBottom) {
        startY = rect.bottom() - totalTextHeight;
    }
    
    // Render each line
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (line.isEmpty()) continue;
        
        QRect lineRect = rect;
        lineRect.setTop(startY + i * (lineHeight + m_settings.lineSpacing));
        lineRect.setHeight(lineHeight);
        
        // Draw shadow first
        if (m_settings.enableShadow && m_settings.shadowOffset > 0) {
            QRect shadowRect = lineRect.translated(m_settings.shadowOffset, m_settings.shadowOffset);
            drawTextWithShadow(painter, line, shadowRect, textColor, m_settings.shadowColor, m_settings.shadowOffset);
        }
        
        // Draw outline
        if (m_settings.enableOutline && m_settings.outlineWidth > 0) {
            drawTextWithOutline(painter, line, lineRect, textColor, outlineColor, format.outlineWidth > 0 ? format.outlineWidth : m_settings.outlineWidth);
        } else {
            // Draw plain text
            painter.setPen(textColor);
            painter.drawText(lineRect, format.alignment, line);
        }
    }
}

QRect SubtitleRenderer::calculateTextRect(const SubtitleEntry& entry) const
{
    QFontMetrics fm(entry.format().font);
    
    // Calculate text size
    QStringList lines = entry.text().split('\n');
    int maxWidth = 0;
    int totalHeight = 0;
    
    for (const QString& line : lines) {
        QRect lineRect = fm.boundingRect(line);
        maxWidth = qMax(maxWidth, lineRect.width());
        totalHeight += fm.height();
    }
    
    if (lines.size() > 1) {
        totalHeight += (lines.size() - 1) * m_settings.lineSpacing;
    }
    
    // Apply margins
    int availableWidth = width() - m_settings.marginLeft - m_settings.marginRight;
    int textWidth = qMin(maxWidth, availableWidth);
    
    // Calculate position
    int x = m_settings.marginLeft;
    if (m_settings.alignment & Qt::AlignHCenter) {
        x = (width() - textWidth) / 2;
    } else if (m_settings.alignment & Qt::AlignRight) {
        x = width() - textWidth - m_settings.marginRight;
    }
    
    return QRect(x, 0, textWidth, totalHeight);
}

void SubtitleRenderer::applySubtitleFormat(QPainter& painter, const SubtitleFormat& format)
{
    // Set font
    QFont font = format.font;
    if (font.family().isEmpty()) {
        font = m_settings.font;
    }
    
    // Apply scale to font size
    if (qAbs(m_settings.scale - 1.0) > 0.01) {
        int scaledSize = static_cast<int>(font.pointSize() * m_settings.scale);
        font.setPointSize(qMax(8, scaledSize));
    }
    
    painter.setFont(font);
}

void SubtitleRenderer::drawTextWithOutline(QPainter& painter, const QString& text, const QRect& rect,
                                          const QColor& textColor, const QColor& outlineColor, int outlineWidth)
{
    if (outlineWidth <= 0) {
        painter.setPen(textColor);
        painter.drawText(rect, Qt::AlignCenter, text);
        return;
    }
    
    // Create text path
    QPainterPath textPath;
    QFont font = painter.font();
    textPath.addText(rect.center() - QPoint(painter.fontMetrics().horizontalAdvance(text) / 2, 
                                           -painter.fontMetrics().ascent() / 2), font, text);
    
    // Draw outline
    painter.setPen(QPen(outlineColor, outlineWidth * 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(textPath);
    
    // Draw text
    painter.setPen(Qt::NoPen);
    painter.setBrush(textColor);
    painter.drawPath(textPath);
}

void SubtitleRenderer::drawTextWithShadow(QPainter& painter, const QString& text, const QRect& rect,
                                         const QColor& textColor, const QColor& shadowColor, int shadowOffset)
{
    // Draw shadow
    painter.setPen(shadowColor);
    QRect shadowRect = rect.translated(shadowOffset, shadowOffset);
    painter.drawText(shadowRect, Qt::AlignCenter, text);
    
    // Draw text
    painter.setPen(textColor);
    painter.drawText(rect, Qt::AlignCenter, text);
}

int SubtitleRenderer::calculateTotalHeight(const QList<SubtitleEntry>& entries) const
{
    if (entries.isEmpty()) {
        return 0;
    }
    
    int totalHeight = 0;
    
    for (const SubtitleEntry& entry : entries) {
        QFontMetrics fm(entry.format().font);
        QStringList lines = entry.text().split('\n');
        
        int entryHeight = lines.size() * fm.height();
        if (lines.size() > 1) {
            entryHeight += (lines.size() - 1) * m_settings.lineSpacing;
        }
        
        totalHeight += entryHeight;
    }
    
    // Add spacing between entries
    if (entries.size() > 1) {
        totalHeight += (entries.size() - 1) * m_settings.lineSpacing;
    }
    
    return totalHeight;
}

QStringList SubtitleRenderer::splitTextToLines(const QString& text, const QFont& font, int maxWidth) const
{
    QStringList lines;
    QFontMetrics fm(font);
    
    QStringList paragraphs = text.split('\n');
    
    for (const QString& paragraph : paragraphs) {
        if (paragraph.isEmpty()) {
            lines.append(QString());
            continue;
        }
        
        QStringList words = paragraph.split(' ', Qt::SkipEmptyParts);
        QString currentLine;
        
        for (const QString& word : words) {
            QString testLine = currentLine.isEmpty() ? word : currentLine + " " + word;
            
            if (fm.horizontalAdvance(testLine) <= maxWidth) {
                currentLine = testLine;
            } else {
                if (!currentLine.isEmpty()) {
                    lines.append(currentLine);
                    currentLine = word;
                } else {
                    // Single word is too long, add it anyway
                    lines.append(word);
                }
            }
        }
        
        if (!currentLine.isEmpty()) {
            lines.append(currentLine);
        }
    }
    
    return lines;
}