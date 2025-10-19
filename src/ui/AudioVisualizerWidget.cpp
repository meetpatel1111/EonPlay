#include "ui/AudioVisualizerWidget.h"
#include "audio/AudioVisualizer.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QtMath>
#include <QMutexLocker>
#include <algorithm>

AudioVisualizerWidget::AudioVisualizerWidget(QWidget* parent)
    : QWidget(parent)
    , m_visualizer(nullptr)
    , m_leftVULevel(0.0f)
    , m_rightVULevel(0.0f)
    , m_leftPeakLevel(0.0f)
    , m_rightPeakLevel(0.0f)
    , m_visualizationStyle(Modern)
    , m_backgroundEnabled(true)
    , m_backgroundColor(QColor(20, 20, 30))
    , m_primaryColor(QColor(0, 150, 255))
    , m_gradientEnabled(true)
    , m_glowEnabled(true)
    , m_animationSpeed(1.0f)
    , m_animationTimer(new QTimer(this))
    , m_animationPhase(0.0f)
{
    // Setup widget
    setMinimumSize(200, 100);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    
    // Initialize AI color palette
    m_aiColorPalette = {
        QColor(255, 0, 0),    // Red
        QColor(255, 127, 0),  // Orange
        QColor(255, 255, 0),  // Yellow
        QColor(0, 255, 0),    // Green
        QColor(0, 0, 255),    // Blue
        QColor(75, 0, 130),   // Indigo
        QColor(148, 0, 211)   // Violet
    };
    
    // Setup animation timer
    m_animationTimer->setInterval(1000 / ANIMATION_FPS);
    connect(m_animationTimer, &QTimer::timeout, this, &AudioVisualizerWidget::updateAnimation);
    m_animationTimer->start();
    
    // Setup gradients
    setupGradients();
}

AudioVisualizerWidget::~AudioVisualizerWidget()
{
    if (m_visualizer) {
        disconnect(m_visualizer, nullptr, this, nullptr);
    }
}

void AudioVisualizerWidget::setAudioVisualizer(AudioVisualizer* visualizer)
{
    if (m_visualizer) {
        disconnect(m_visualizer, nullptr, this, nullptr);
    }
    
    m_visualizer = visualizer;
    
    if (m_visualizer) {
        connect(m_visualizer, &AudioVisualizer::spectrumDataUpdated,
                this, &AudioVisualizerWidget::onSpectrumDataUpdated);
        connect(m_visualizer, &AudioVisualizer::waveformDataUpdated,
                this, &AudioVisualizerWidget::onWaveformDataUpdated);
        connect(m_visualizer, &AudioVisualizer::vuLevelsUpdated,
                this, &AudioVisualizerWidget::onVULevelsUpdated);
        connect(m_visualizer, &AudioVisualizer::peakLevelsUpdated,
                this, &AudioVisualizerWidget::onPeakLevelsUpdated);
        connect(m_visualizer, &AudioVisualizer::colorPaletteUpdated,
                this, &AudioVisualizerWidget::onColorPaletteUpdated);
    }
}

AudioVisualizerWidget::VisualizationStyle AudioVisualizerWidget::getVisualizationStyle() const
{
    return m_visualizationStyle;
}

void AudioVisualizerWidget::setVisualizationStyle(VisualizationStyle style)
{
    if (m_visualizationStyle != style) {
        m_visualizationStyle = style;
        setupGradients();
        update();
    }
}

QString AudioVisualizerWidget::getStyleName(VisualizationStyle style)
{
    switch (style) {
        case Classic: return "Classic";
        case Modern: return "Modern";
        case Neon: return "Neon";
        case Retro: return "Retro";
        case Minimal: return "Minimal";
        case AI_Driven: return "AI Driven";
        default: return "Unknown";
    }
}

bool AudioVisualizerWidget::isBackgroundEnabled() const
{
    return m_backgroundEnabled;
}

void AudioVisualizerWidget::setBackgroundEnabled(bool enabled)
{
    if (m_backgroundEnabled != enabled) {
        m_backgroundEnabled = enabled;
        update();
    }
}

QColor AudioVisualizerWidget::getBackgroundColor() const
{
    return m_backgroundColor;
}

void AudioVisualizerWidget::setBackgroundColor(const QColor& color)
{
    if (m_backgroundColor != color) {
        m_backgroundColor = color;
        setupGradients();
        update();
    }
}

QColor AudioVisualizerWidget::getPrimaryColor() const
{
    return m_primaryColor;
}

void AudioVisualizerWidget::setPrimaryColor(const QColor& color)
{
    if (m_primaryColor != color) {
        m_primaryColor = color;
        setupGradients();
        update();
    }
}

bool AudioVisualizerWidget::isGradientEnabled() const
{
    return m_gradientEnabled;
}

void AudioVisualizerWidget::setGradientEnabled(bool enabled)
{
    if (m_gradientEnabled != enabled) {
        m_gradientEnabled = enabled;
        update();
    }
}

bool AudioVisualizerWidget::isGlowEnabled() const
{
    return m_glowEnabled;
}

void AudioVisualizerWidget::setGlowEnabled(bool enabled)
{
    if (m_glowEnabled != enabled) {
        m_glowEnabled = enabled;
        update();
    }
}

float AudioVisualizerWidget::getAnimationSpeed() const
{
    return m_animationSpeed;
}

void AudioVisualizerWidget::setAnimationSpeed(float speed)
{
    m_animationSpeed = qBound(0.0f, speed, 2.0f);
}

void AudioVisualizerWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw background
    if (m_backgroundEnabled) {
        painter.fillRect(rect(), m_backgroundColor);
    }
    
    if (!m_visualizer) {
        // Draw placeholder text
        painter.setPen(m_primaryColor);
        painter.drawText(rect(), Qt::AlignCenter, "No Audio Visualizer");
        return;
    }
    
    // Draw visualization based on current mode
    auto mode = m_visualizer->getVisualizationMode();
    
    switch (mode) {
        case AudioVisualizer::SpectrumAnalyzer:
            drawSpectrumAnalyzer(painter);
            break;
        case AudioVisualizer::Waveform:
        case AudioVisualizer::Oscilloscope:
            drawWaveform(painter);
            break;
        case AudioVisualizer::VUMeter:
            drawVUMeter(painter);
            break;
        case AudioVisualizer::FrequencyBars:
            drawFrequencyBars(painter);
            break;
        case AudioVisualizer::CircularSpectrum:
            drawCircularSpectrum(painter);
            break;
        case AudioVisualizer::AIVisualizer:
            drawAIVisualizer(painter);
            break;
        case AudioVisualizer::None:
        default:
            painter.setPen(m_primaryColor);
            painter.drawText(rect(), Qt::AlignCenter, "Visualization Disabled");
            break;
    }
}

void AudioVisualizerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    
    // Resize animation arrays
    int bandCount = m_visualizer ? m_visualizer->getSpectrumBandCount() : 32;
    m_barAnimations.resize(bandCount);
    m_peakAnimations.resize(bandCount);
    
    setupGradients();
}

void AudioVisualizerWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_visualizer) {
        // Cycle through visualization modes
        auto modes = AudioVisualizer::getAvailableModes();
        auto currentMode = m_visualizer->getVisualizationMode();
        
        int currentIndex = modes.indexOf(currentMode);
        int nextIndex = (currentIndex + 1) % modes.size();
        
        m_visualizer->setVisualizationMode(modes[nextIndex]);
    }
    
    QWidget::mousePressEvent(event);
}

void AudioVisualizerWidget::onSpectrumDataUpdated(const AudioVisualizer::SpectrumData& spectrum)
{
    QMutexLocker locker(&m_dataMutex);
    m_spectrumData = spectrum;
    
    // Update animation arrays
    if (m_barAnimations.size() != spectrum.magnitudes.size()) {
        m_barAnimations.resize(spectrum.magnitudes.size());
        m_peakAnimations.resize(spectrum.magnitudes.size());
    }
    
    update();
}

void AudioVisualizerWidget::onWaveformDataUpdated(const QVector<float>& leftChannel, const QVector<float>& rightChannel)
{
    QMutexLocker locker(&m_dataMutex);
    m_leftWaveform = leftChannel;
    m_rightWaveform = rightChannel;
    update();
}

void AudioVisualizerWidget::onVULevelsUpdated(float leftLevel, float rightLevel)
{
    QMutexLocker locker(&m_dataMutex);
    m_leftVULevel = leftLevel;
    m_rightVULevel = rightLevel;
    update();
}

void AudioVisualizerWidget::onPeakLevelsUpdated(float leftPeak, float rightPeak)
{
    QMutexLocker locker(&m_dataMutex);
    m_leftPeakLevel = leftPeak;
    m_rightPeakLevel = rightPeak;
    update();
}

void AudioVisualizerWidget::onColorPaletteUpdated(const QVector<QColor>& colors)
{
    QMutexLocker locker(&m_dataMutex);
    m_aiColorPalette = colors;
    update();
}

void AudioVisualizerWidget::updateAnimation()
{
    m_animationPhase += 0.05f * m_animationSpeed;
    if (m_animationPhase >= 2.0f * M_PI) {
        m_animationPhase -= 2.0f * M_PI;
    }
    
    // Update bar animations with decay
    for (int i = 0; i < m_barAnimations.size(); ++i) {
        if (i < m_spectrumData.magnitudes.size()) {
            float target = m_spectrumData.magnitudes[i];
            if (target > m_barAnimations[i]) {
                m_barAnimations[i] = target;
            } else {
                m_barAnimations[i] *= ANIMATION_DECAY;
            }
        }
    }
    
    // Update peak animations with slower decay
    for (int i = 0; i < m_peakAnimations.size(); ++i) {
        if (i < m_barAnimations.size()) {
            if (m_barAnimations[i] > m_peakAnimations[i]) {
                m_peakAnimations[i] = m_barAnimations[i];
            } else {
                m_peakAnimations[i] *= PEAK_DECAY;
            }
        }
    }
    
    update();
}

void AudioVisualizerWidget::drawSpectrumAnalyzer(QPainter& painter)
{
    switch (m_visualizationStyle) {
        case Classic:
            drawClassicStyle(painter);
            break;
        case Modern:
            drawModernStyle(painter);
            break;
        case Neon:
            drawNeonStyle(painter);
            break;
        case Retro:
            drawRetroStyle(painter);
            break;
        case Minimal:
            drawMinimalStyle(painter);
            break;
        case AI_Driven:
            drawAIDrivenStyle(painter);
            break;
    }
}

void AudioVisualizerWidget::drawWaveform(QPainter& painter)
{
    if (m_leftWaveform.isEmpty()) {
        return;
    }
    
    QRectF drawRect = rect().adjusted(10, 10, -10, -10);
    float centerY = drawRect.center().y();
    float amplitude = drawRect.height() * 0.4f;
    
    // Draw left channel (top half)
    QPainterPath leftPath;
    leftPath.moveTo(drawRect.left(), centerY - amplitude * 0.5f);
    
    for (int i = 0; i < m_leftWaveform.size(); ++i) {
        float x = drawRect.left() + (i * drawRect.width()) / (m_leftWaveform.size() - 1);
        float y = centerY - amplitude * 0.5f - m_leftWaveform[i] * amplitude * 0.5f;
        leftPath.lineTo(x, y);
    }
    
    // Draw right channel (bottom half)
    QPainterPath rightPath;
    rightPath.moveTo(drawRect.left(), centerY + amplitude * 0.5f);
    
    QVector<float>& rightWaveform = m_rightWaveform.isEmpty() ? m_leftWaveform : m_rightWaveform;
    for (int i = 0; i < rightWaveform.size(); ++i) {
        float x = drawRect.left() + (i * drawRect.width()) / (rightWaveform.size() - 1);
        float y = centerY + amplitude * 0.5f + rightWaveform[i] * amplitude * 0.5f;
        rightPath.lineTo(x, y);
    }
    
    // Draw waveforms
    painter.setPen(QPen(m_primaryColor, 2));
    painter.drawPath(leftPath);
    
    if (!m_rightWaveform.isEmpty()) {
        painter.setPen(QPen(m_primaryColor.lighter(120), 2));
        painter.drawPath(rightPath);
    }
    
    // Draw center line
    painter.setPen(QPen(m_primaryColor.darker(150), 1, Qt::DashLine));
    painter.drawLine(drawRect.left(), centerY, drawRect.right(), centerY);
}

void AudioVisualizerWidget::drawVUMeter(QPainter& painter)
{
    QRectF drawRect = rect().adjusted(20, 20, -20, -20);
    
    // Draw left VU meter
    QRectF leftRect(drawRect.left(), drawRect.top(), VU_METER_WIDTH, drawRect.height());
    QRectF leftFillRect = leftRect;
    leftFillRect.setHeight(leftRect.height() * m_leftVULevel);
    leftFillRect.moveBottom(leftRect.bottom());
    
    painter.fillRect(leftRect, QColor(50, 50, 50));
    painter.fillRect(leftFillRect, m_gradientEnabled ? m_primaryGradient : QBrush(m_primaryColor));
    
    // Draw left peak indicator
    if (m_leftPeakLevel > 0.01f) {
        float peakY = leftRect.bottom() - leftRect.height() * m_leftPeakLevel;
        painter.fillRect(QRectF(leftRect.left(), peakY - 2, leftRect.width(), 4), Qt::white);
    }
    
    // Draw right VU meter
    QRectF rightRect(drawRect.right() - VU_METER_WIDTH, drawRect.top(), VU_METER_WIDTH, drawRect.height());
    QRectF rightFillRect = rightRect;
    rightFillRect.setHeight(rightRect.height() * m_rightVULevel);
    rightFillRect.moveBottom(rightRect.bottom());
    
    painter.fillRect(rightRect, QColor(50, 50, 50));
    painter.fillRect(rightFillRect, m_gradientEnabled ? m_primaryGradient : QBrush(m_primaryColor));
    
    // Draw right peak indicator
    if (m_rightPeakLevel > 0.01f) {
        float peakY = rightRect.bottom() - rightRect.height() * m_rightPeakLevel;
        painter.fillRect(QRectF(rightRect.left(), peakY - 2, rightRect.width(), 4), Qt::white);
    }
    
    // Draw labels
    painter.setPen(m_primaryColor);
    painter.drawText(leftRect.adjusted(0, -15, 0, 0), Qt::AlignCenter, "L");
    painter.drawText(rightRect.adjusted(0, -15, 0, 0), Qt::AlignCenter, "R");
}

void AudioVisualizerWidget::drawFrequencyBars(QPainter& painter)
{
    if (m_spectrumData.magnitudes.isEmpty()) {
        return;
    }
    
    QRectF drawRect = rect().adjusted(10, 10, -10, -10);
    int barCount = m_spectrumData.magnitudes.size();
    float barWidth = (drawRect.width() - (barCount - 1) * BAR_SPACING) / barCount;
    
    for (int i = 0; i < barCount; ++i) {
        float magnitude = i < m_barAnimations.size() ? m_barAnimations[i] : 0.0f;
        float barHeight = qMax(static_cast<float>(MIN_BAR_HEIGHT), magnitude * drawRect.height());
        
        QRectF barRect(drawRect.left() + i * (barWidth + BAR_SPACING),
                      drawRect.bottom() - barHeight,
                      barWidth,
                      barHeight);
        
        // Choose color based on frequency
        QColor barColor = m_primaryColor;
        if (m_visualizationStyle == AI_Driven && i < m_aiColorPalette.size()) {
            barColor = m_aiColorPalette[i % m_aiColorPalette.size()];
        }
        
        drawBar(painter, barRect, magnitude, barColor);
        
        // Draw peak indicator
        if (i < m_peakAnimations.size() && m_peakAnimations[i] > 0.01f) {
            float peakHeight = m_peakAnimations[i] * drawRect.height();
            QRectF peakRect(barRect.left(), drawRect.bottom() - peakHeight - 2, barRect.width(), 2);
            painter.fillRect(peakRect, Qt::white);
        }
    }
}

void AudioVisualizerWidget::drawCircularSpectrum(QPainter& painter)
{
    if (m_spectrumData.magnitudes.isEmpty()) {
        return;
    }
    
    QPointF center = rect().center();
    float radius = qMin(width(), height()) * 0.3f;
    int barCount = m_spectrumData.magnitudes.size();
    float angleStep = 360.0f / barCount;
    
    for (int i = 0; i < barCount; ++i) {
        float magnitude = i < m_barAnimations.size() ? m_barAnimations[i] : 0.0f;
        float angle = i * angleStep + m_animationPhase * 10.0f; // Rotate slowly
        float barLength = magnitude * radius * 0.8f;
        
        QColor barColor = m_primaryColor;
        if (m_visualizationStyle == AI_Driven && i < m_aiColorPalette.size()) {
            barColor = m_aiColorPalette[i % m_aiColorPalette.size()];
        }
        
        drawCircularBar(painter, center, radius, angle, magnitude, barColor);
    }
    
    // Draw center circle
    painter.setPen(QPen(m_primaryColor, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius * 0.2f, radius * 0.2f);
}

void AudioVisualizerWidget::drawAIVisualizer(QPainter& painter)
{
    // Combine multiple visualization elements with AI colors
    drawFrequencyBars(painter);
    
    // Add AI-driven effects
    if (!m_aiColorPalette.isEmpty()) {
        // Draw flowing particles based on spectrum
        painter.setOpacity(0.6f);
        
        for (int i = 0; i < qMin(m_spectrumData.magnitudes.size(), 20); ++i) {
            if (i >= m_barAnimations.size()) break;
            
            float magnitude = m_barAnimations[i];
            if (magnitude > 0.1f) {
                QColor particleColor = m_aiColorPalette[i % m_aiColorPalette.size()];
                
                float x = (i * width()) / 20.0f + qSin(m_animationPhase + i) * 20.0f;
                float y = height() * (1.0f - magnitude) + qCos(m_animationPhase + i) * 10.0f;
                float size = magnitude * 15.0f;
                
                painter.setBrush(particleColor);
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPointF(x, y), size, size);
            }
        }
        
        painter.setOpacity(1.0f);
    }
}

void AudioVisualizerWidget::drawClassicStyle(QPainter& painter)
{
    drawFrequencyBars(painter);
}

void AudioVisualizerWidget::drawModernStyle(QPainter& painter)
{
    drawFrequencyBars(painter);
}

void AudioVisualizerWidget::drawNeonStyle(QPainter& painter)
{
    if (m_glowEnabled) {
        // Draw glow effect
        painter.setCompositionMode(QPainter::CompositionMode_Plus);
        applyGlowEffect(painter, m_primaryColor);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    
    drawFrequencyBars(painter);
}

void AudioVisualizerWidget::drawRetroStyle(QPainter& painter)
{
    // Use retro colors and pixelated effect
    QColor retroColor = QColor(0, 255, 0); // Green monochrome
    
    if (m_spectrumData.magnitudes.isEmpty()) {
        return;
    }
    
    QRectF drawRect = rect().adjusted(10, 10, -10, -10);
    int barCount = m_spectrumData.magnitudes.size();
    float barWidth = (drawRect.width() - (barCount - 1) * BAR_SPACING) / barCount;
    
    painter.setPen(Qt::NoPen);
    
    for (int i = 0; i < barCount; ++i) {
        float magnitude = i < m_barAnimations.size() ? m_barAnimations[i] : 0.0f;
        float barHeight = qMax(static_cast<float>(MIN_BAR_HEIGHT), magnitude * drawRect.height());
        
        // Pixelated bars
        int pixelHeight = static_cast<int>(barHeight / 4) * 4; // Round to 4-pixel blocks
        
        QRectF barRect(drawRect.left() + i * (barWidth + BAR_SPACING),
                      drawRect.bottom() - pixelHeight,
                      barWidth,
                      pixelHeight);
        
        painter.fillRect(barRect, retroColor);
    }
}

void AudioVisualizerWidget::drawMinimalStyle(QPainter& painter)
{
    if (m_spectrumData.magnitudes.isEmpty()) {
        return;
    }
    
    // Simple line visualization
    QPainterPath path;
    QRectF drawRect = rect().adjusted(20, 20, -20, -20);
    
    path.moveTo(drawRect.left(), drawRect.bottom());
    
    for (int i = 0; i < m_spectrumData.magnitudes.size(); ++i) {
        float magnitude = i < m_barAnimations.size() ? m_barAnimations[i] : 0.0f;
        float x = drawRect.left() + (i * drawRect.width()) / (m_spectrumData.magnitudes.size() - 1);
        float y = drawRect.bottom() - magnitude * drawRect.height();
        path.lineTo(x, y);
    }
    
    painter.setPen(QPen(m_primaryColor, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

void AudioVisualizerWidget::drawAIDrivenStyle(QPainter& painter)
{
    drawAIVisualizer(painter);
}

void AudioVisualizerWidget::setupGradients()
{
    // Setup primary gradient
    m_primaryGradient = QLinearGradient(0, 0, 0, height());
    m_primaryGradient.setColorAt(0, m_primaryColor.lighter(150));
    m_primaryGradient.setColorAt(1, m_primaryColor.darker(150));
    
    // Setup glow gradient
    m_glowGradient = QRadialGradient(0, 0, 50);
    m_glowGradient.setColorAt(0, m_primaryColor);
    m_glowGradient.setColorAt(1, Qt::transparent);
}

void AudioVisualizerWidget::applyGlowEffect(QPainter& painter, const QColor& color)
{
    // Simple glow effect implementation
    painter.setBrush(QBrush(color));
    painter.setPen(Qt::NoPen);
    
    // Draw multiple layers with decreasing opacity
    for (int i = 5; i > 0; --i) {
        painter.setOpacity(0.1f);
        // This would draw a blurred version in a real implementation
        // For now, just draw a slightly larger version
    }
    
    painter.setOpacity(1.0f);
}

QColor AudioVisualizerWidget::interpolateColor(const QColor& color1, const QColor& color2, float ratio)
{
    ratio = qBound(0.0f, ratio, 1.0f);
    
    int r = static_cast<int>(color1.red() * (1.0f - ratio) + color2.red() * ratio);
    int g = static_cast<int>(color1.green() * (1.0f - ratio) + color2.green() * ratio);
    int b = static_cast<int>(color1.blue() * (1.0f - ratio) + color2.blue() * ratio);
    int a = static_cast<int>(color1.alpha() * (1.0f - ratio) + color2.alpha() * ratio);
    
    return QColor(r, g, b, a);
}

void AudioVisualizerWidget::drawBar(QPainter& painter, const QRectF& rect, float value, const QColor& color)
{
    if (m_gradientEnabled) {
        QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
        gradient.setColorAt(0, color.lighter(150));
        gradient.setColorAt(1, color.darker(150));
        painter.fillRect(rect, gradient);
    } else {
        painter.fillRect(rect, color);
    }
    
    // Add border for some styles
    if (m_visualizationStyle == Classic || m_visualizationStyle == Retro) {
        painter.setPen(QPen(color.darker(200), 1));
        painter.drawRect(rect);
    }
}

void AudioVisualizerWidget::drawCircularBar(QPainter& painter, const QPointF& center, float radius, float angle, float value, const QColor& color)
{
    float barLength = value * radius * 0.6f;
    float startRadius = radius * 0.4f;
    float endRadius = startRadius + barLength;
    
    float radians = qDegreesToRadians(angle);
    QPointF startPoint = center + QPointF(qCos(radians) * startRadius, qSin(radians) * startRadius);
    QPointF endPoint = center + QPointF(qCos(radians) * endRadius, qSin(radians) * endRadius);
    
    painter.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(startPoint, endPoint);
}