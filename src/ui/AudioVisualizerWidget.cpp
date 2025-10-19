#include "ui/AudioVisualizerWidget.h"
#include <QPaintEvent>
#include <QResizeEvent>
#include <QtMath>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(audioVisualizerWidget)
Q_LOGGING_CATEGORY(audioVisualizerWidget, "ui.audioVisualizer")

AudioVisualizerWidget::AudioVisualizerWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_controlsLayout(nullptr)
    , m_controlsGroup(nullptr)
    , m_modeCombo(nullptr)
    , m_sensitivitySlider(nullptr)
    , m_sensitivityLabel(nullptr)
    , m_smoothingCheckBox(nullptr)
    , m_peakHoldCheckBox(nullptr)
    , m_updateRateSlider(nullptr)
    , m_updateRateLabel(nullptr)
    , m_bandCountCombo(nullptr)
    , m_moodLabel(nullptr)
    , m_peakFreqLabel(nullptr)
    , m_leftVULevel(0.0f)
    , m_rightVULevel(0.0f)
    , m_leftPeakLevel(0.0f)
    , m_rightPeakLevel(0.0f)
    , m_moodConfidence(0.0f)
    , m_repaintTimer(new QTimer(this))
    , m_updatingControls(false)
    , m_backgroundColor(QColor(20, 20, 20))
    , m_foregroundColor(QColor(0, 255, 0))
    , m_accentColor(QColor(255, 255, 0))
    , m_barSpacing(2)
    , m_barWidth(4)
{
    setupUI();
    setupConnections();
    
    // Set up repaint timer for smooth animation
    m_repaintTimer->setSingleShot(false);
    m_repaintTimer->setInterval(33); // ~30 FPS
    connect(m_repaintTimer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    
    // Initialize color palette
    m_colorPalette = {
        QColor(255, 0, 0),    // Red
        QColor(255, 127, 0),  // Orange
        QColor(255, 255, 0),  // Yellow
        QColor(0, 255, 0),    // Green
        QColor(0, 0, 255),    // Blue
        QColor(75, 0, 130),   // Indigo
        QColor(148, 0, 211)   // Violet
    };
    
    qCDebug(audioVisualizerWidget) << "AudioVisualizerWidget created";
}

AudioVisualizerWidget::~AudioVisualizerWidget()
{
    qCDebug(audioVisualizerWidget) << "AudioVisualizerWidget destroyed";
}

void AudioVisualizerWidget::initialize(std::shared_ptr<AudioVisualizer> visualizer)
{
    m_visualizer = visualizer;
    
    if (m_visualizer) {
        // Connect to visualizer signals
        connect(m_visualizer.get(), &AudioVisualizer::spectrumDataUpdated,
                this, &AudioVisualizerWidget::onSpectrumDataUpdated);
        connect(m_visualizer.get(), &AudioVisualizer::waveformDataUpdated,
                this, &AudioVisualizerWidget::onWaveformDataUpdated);
        connect(m_visualizer.get(), &AudioVisualizer::vuLevelsUpdated,
                this, &AudioVisualizerWidget::onVULevelsUpdated);
        connect(m_visualizer.get(), &AudioVisualizer::peakLevelsUpdated,
                this, &AudioVisualizerWidget::onPeakLevelsUpdated);
        connect(m_visualizer.get(), &AudioVisualizer::moodDetected,
                this, &AudioVisualizerWidget::onMoodDetected);
        connect(m_visualizer.get(), &AudioVisualizer::colorPaletteUpdated,
                this, &AudioVisualizerWidget::onColorPaletteUpdated);
        
        updateFromVisualizer();
        m_repaintTimer->start();
    }
    
    qCDebug(audioVisualizerWidget) << "AudioVisualizerWidget initialized";
}

void AudioVisualizerWidget::setupUI()
{
    setMinimumSize(400, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(6);
    
    setupControls();
    
    // Add stretch to push controls to bottom
    m_mainLayout->addStretch();
    m_mainLayout->addWidget(m_controlsGroup);
}

void AudioVisualizerWidget::setupControls()
{
    m_controlsGroup = new QGroupBox("Visualization Controls", this);
    m_controlsLayout = new QHBoxLayout(m_controlsGroup);
    
    // Visualization mode
    m_controlsLayout->addWidget(new QLabel("Mode:", this));
    m_modeCombo = new QComboBox(this);
    auto modes = AudioVisualizer::getAvailableModes();
    for (auto mode : modes) {
        m_modeCombo->addItem(AudioVisualizer::getModeName(mode), static_cast<int>(mode));
    }
    m_controlsLayout->addWidget(m_modeCombo);
    
    m_controlsLayout->addWidget(new QLabel("|", this));
    
    // Sensitivity
    m_controlsLayout->addWidget(new QLabel("Sensitivity:", this));
    m_sensitivitySlider = new QSlider(Qt::Horizontal, this);
    m_sensitivitySlider->setRange(0, 100);
    m_sensitivitySlider->setValue(50);
    m_sensitivitySlider->setMaximumWidth(100);
    m_sensitivityLabel = new QLabel("50%", this);
    m_sensitivityLabel->setMinimumWidth(30);
    m_controlsLayout->addWidget(m_sensitivitySlider);
    m_controlsLayout->addWidget(m_sensitivityLabel);
    
    m_controlsLayout->addWidget(new QLabel("|", this));
    
    // Smoothing
    m_smoothingCheckBox = new QCheckBox("Smoothing", this);
    m_smoothingCheckBox->setChecked(true);
    m_controlsLayout->addWidget(m_smoothingCheckBox);
    
    // Peak hold
    m_peakHoldCheckBox = new QCheckBox("Peak Hold", this);
    m_peakHoldCheckBox->setChecked(true);
    m_controlsLayout->addWidget(m_peakHoldCheckBox);
    
    m_controlsLayout->addWidget(new QLabel("|", this));
    
    // Update rate
    m_controlsLayout->addWidget(new QLabel("Rate:", this));
    m_updateRateSlider = new QSlider(Qt::Horizontal, this);
    m_updateRateSlider->setRange(10, 60);
    m_updateRateSlider->setValue(30);
    m_updateRateSlider->setMaximumWidth(80);
    m_updateRateLabel = new QLabel("30 Hz", this);
    m_updateRateLabel->setMinimumWidth(40);
    m_controlsLayout->addWidget(m_updateRateSlider);
    m_controlsLayout->addWidget(m_updateRateLabel);
    
    m_controlsLayout->addWidget(new QLabel("|", this));
    
    // Band count
    m_controlsLayout->addWidget(new QLabel("Bands:", this));
    m_bandCountCombo = new QComboBox(this);
    QStringList bandCounts = {"8", "16", "32", "64", "128", "256"};
    m_bandCountCombo->addItems(bandCounts);
    m_bandCountCombo->setCurrentText("32");
    m_controlsLayout->addWidget(m_bandCountCombo);
    
    m_controlsLayout->addStretch();
    
    // Status labels
    m_moodLabel = new QLabel("Mood: -", this);
    m_moodLabel->setStyleSheet("QLabel { color: #888; font-size: 11px; }");
    m_controlsLayout->addWidget(m_moodLabel);
    
    m_peakFreqLabel = new QLabel("Peak: -", this);
    m_peakFreqLabel->setStyleSheet("QLabel { color: #888; font-size: 11px; }");
    m_controlsLayout->addWidget(m_peakFreqLabel);
}

void AudioVisualizerWidget::setupConnections()
{
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioVisualizerWidget::onVisualizationModeChanged);
    connect(m_sensitivitySlider, &QSlider::valueChanged,
            this, &AudioVisualizerWidget::onSensitivityChanged);
    connect(m_smoothingCheckBox, &QCheckBox::toggled,
            this, &AudioVisualizerWidget::onSmoothingToggled);
    connect(m_peakHoldCheckBox, &QCheckBox::toggled,
            this, &AudioVisualizerWidget::onPeakHoldToggled);
    connect(m_updateRateSlider, &QSlider::valueChanged,
            this, &AudioVisualizerWidget::onUpdateRateChanged);
    connect(m_bandCountCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioVisualizerWidget::onBandCountChanged);
}

void AudioVisualizerWidget::updateFromVisualizer()
{
    if (!m_visualizer) {
        return;
    }
    
    m_updatingControls = true;
    
    // Update mode combo
    int modeIndex = m_modeCombo->findData(static_cast<int>(m_visualizer->getVisualizationMode()));
    if (modeIndex >= 0) {
        m_modeCombo->setCurrentIndex(modeIndex);
    }
    
    // Update sensitivity
    int sensitivity = static_cast<int>(m_visualizer->getSensitivity() * 100);
    m_sensitivitySlider->setValue(sensitivity);
    m_sensitivityLabel->setText(QString("%1%").arg(sensitivity));
    
    // Update checkboxes
    m_smoothingCheckBox->setChecked(m_visualizer->isSmoothingEnabled());
    m_peakHoldCheckBox->setChecked(m_visualizer->isPeakHoldEnabled());
    
    // Update rate
    int rate = m_visualizer->getUpdateRate();
    m_updateRateSlider->setValue(rate);
    m_updateRateLabel->setText(QString("%1 Hz").arg(rate));
    
    // Update band count
    int bandCount = m_visualizer->getSpectrumBandCount();
    int bandIndex = m_bandCountCombo->findText(QString::number(bandCount));
    if (bandIndex >= 0) {
        m_bandCountCombo->setCurrentIndex(bandIndex);
    }
    
    m_updatingControls = false;
}

QSize AudioVisualizerWidget::minimumSizeHint() const
{
    return QSize(400, 200);
}

QSize AudioVisualizerWidget::sizeHint() const
{
    return QSize(600, 300);
}

void AudioVisualizerWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Fill background
    painter.fillRect(rect(), m_backgroundColor);
    
    // Calculate visualization area (exclude controls)
    QRect visualRect = rect();
    if (m_controlsGroup) {
        visualRect.setHeight(visualRect.height() - m_controlsGroup->height() - 12);
    }
    
    if (!m_visualizer || !m_visualizer->isEnabled()) {
        // Draw "No Signal" message
        painter.setPen(QColor(100, 100, 100));
        painter.setFont(QFont("Arial", 16));
        painter.drawText(visualRect, Qt::AlignCenter, "Audio Visualizer\n(No Signal)");
        return;
    }
    
    // Draw visualization based on mode
    auto mode = m_visualizer->getVisualizationMode();
    switch (mode) {
        case AudioVisualizer::SpectrumAnalyzer:
            drawSpectrumAnalyzer(painter, visualRect);
            break;
        case AudioVisualizer::Waveform:
            drawWaveform(painter, visualRect);
            break;
        case AudioVisualizer::Oscilloscope:
            drawOscilloscope(painter, visualRect);
            break;
        case AudioVisualizer::VUMeter:
            drawVUMeter(painter, visualRect);
            break;
        case AudioVisualizer::FrequencyBars:
            drawFrequencyBars(painter, visualRect);
            break;
        case AudioVisualizer::CircularSpectrum:
            drawCircularSpectrum(painter, visualRect);
            break;
        case AudioVisualizer::AIVisualizer:
            drawAIVisualizer(painter, visualRect);
            break;
        default:
            painter.setPen(QColor(100, 100, 100));
            painter.drawText(visualRect, Qt::AlignCenter, "Visualization Disabled");
            break;
    }
}

void AudioVisualizerWidget::drawSpectrumAnalyzer(QPainter& painter, const QRect& rect)
{
    if (m_currentSpectrum.magnitudes.isEmpty()) {
        return;
    }
    
    int bandCount = m_currentSpectrum.magnitudes.size();
    int totalWidth = rect.width() - 20; // Margins
    int barWidth = qMax(1, (totalWidth - (bandCount - 1) * m_barSpacing) / bandCount);
    
    painter.setPen(Qt::NoPen);
    
    for (int i = 0; i < bandCount; ++i) {
        float magnitude = qBound(0.0f, m_currentSpectrum.magnitudes[i], 1.0f);
        int barHeight = static_cast<int>(magnitude * (rect.height() - 40));
        
        int x = rect.left() + 10 + i * (barWidth + m_barSpacing);
        int y = rect.bottom() - 20 - barHeight;
        
        QRect barRect(x, y, barWidth, barHeight);
        QColor barColor = getBarColor(magnitude, i);
        
        painter.setBrush(barColor);
        painter.drawRect(barRect);
    }
    
    // Draw frequency labels
    painter.setPen(QColor(150, 150, 150));
    painter.setFont(QFont("Arial", 8));
    
    QStringList freqLabels = {"20", "100", "1K", "10K", "20K"};
    for (int i = 0; i < freqLabels.size(); ++i) {
        int x = rect.left() + 10 + (i * totalWidth / (freqLabels.size() - 1));
        painter.drawText(x - 10, rect.bottom() - 5, freqLabels[i]);
    }
}

void AudioVisualizerWidget::drawWaveform(QPainter& painter, const QRect& rect)
{
    if (m_leftWaveform.isEmpty()) {
        return;
    }
    
    painter.setPen(QPen(m_foregroundColor, 2));
    
    // Draw left channel (top half)
    QRect leftRect = rect;
    leftRect.setHeight(rect.height() / 2 - 5);
    
    QPainterPath leftPath;
    bool firstPoint = true;
    
    for (int i = 0; i < m_leftWaveform.size(); ++i) {
        float sample = qBound(-1.0f, m_leftWaveform[i], 1.0f);
        int x = leftRect.left() + (i * leftRect.width() / m_leftWaveform.size());
        int y = leftRect.center().y() - static_cast<int>(sample * leftRect.height() / 2);
        
        if (firstPoint) {
            leftPath.moveTo(x, y);
            firstPoint = false;
        } else {
            leftPath.lineTo(x, y);
        }
    }
    
    painter.drawPath(leftPath);
    
    // Draw right channel (bottom half)
    if (!m_rightWaveform.isEmpty()) {
        QRect rightRect = rect;
        rightRect.setTop(rect.center().y() + 5);
        
        painter.setPen(QPen(m_accentColor, 2));
        
        QPainterPath rightPath;
        firstPoint = true;
        
        for (int i = 0; i < m_rightWaveform.size(); ++i) {
            float sample = qBound(-1.0f, m_rightWaveform[i], 1.0f);
            int x = rightRect.left() + (i * rightRect.width() / m_rightWaveform.size());
            int y = rightRect.center().y() - static_cast<int>(sample * rightRect.height() / 2);
            
            if (firstPoint) {
                rightPath.moveTo(x, y);
                firstPoint = false;
            } else {
                rightPath.lineTo(x, y);
            }
        }
        
        painter.drawPath(rightPath);
    }
    
    // Draw center lines
    painter.setPen(QPen(QColor(100, 100, 100), 1, Qt::DashLine));
    painter.drawLine(leftRect.left(), leftRect.center().y(), leftRect.right(), leftRect.center().y());
    if (!m_rightWaveform.isEmpty()) {
        QRect rightRect = rect;
        rightRect.setTop(rect.center().y() + 5);
        painter.drawLine(rightRect.left(), rightRect.center().y(), rightRect.right(), rightRect.center().y());
    }
}

void AudioVisualizerWidget::drawOscilloscope(QPainter& painter, const QRect& rect)
{
    // Similar to waveform but with XY plot style
    if (m_leftWaveform.isEmpty() || m_rightWaveform.isEmpty()) {
        drawWaveform(painter, rect); // Fallback to waveform
        return;
    }
    
    painter.setPen(QPen(m_foregroundColor, 1));
    
    QPoint center = rect.center();
    int radius = qMin(rect.width(), rect.height()) / 2 - 20;
    
    QPainterPath path;
    bool firstPoint = true;
    
    int sampleCount = qMin(m_leftWaveform.size(), m_rightWaveform.size());
    for (int i = 0; i < sampleCount; ++i) {
        float leftSample = qBound(-1.0f, m_leftWaveform[i], 1.0f);
        float rightSample = qBound(-1.0f, m_rightWaveform[i], 1.0f);
        
        int x = center.x() + static_cast<int>(leftSample * radius);
        int y = center.y() + static_cast<int>(rightSample * radius);
        
        if (firstPoint) {
            path.moveTo(x, y);
            firstPoint = false;
        } else {
            path.lineTo(x, y);
        }
    }
    
    painter.drawPath(path);
    
    // Draw crosshairs
    painter.setPen(QPen(QColor(100, 100, 100), 1, Qt::DashLine));
    painter.drawLine(center.x() - radius, center.y(), center.x() + radius, center.y());
    painter.drawLine(center.x(), center.y() - radius, center.x(), center.y() + radius);
}

void AudioVisualizerWidget::drawVUMeter(QPainter& painter, const QRect& rect)
{
    // Draw VU meters for left and right channels
    int meterWidth = 40;
    int meterHeight = rect.height() - 40;
    int spacing = 20;
    
    QRect leftMeterRect(rect.center().x() - meterWidth - spacing/2, rect.top() + 20, meterWidth, meterHeight);
    QRect rightMeterRect(rect.center().x() + spacing/2, rect.top() + 20, meterWidth, meterHeight);
    
    // Draw meter backgrounds
    painter.setPen(QPen(QColor(50, 50, 50), 1));
    painter.setBrush(QColor(20, 20, 20));
    painter.drawRect(leftMeterRect);
    painter.drawRect(rightMeterRect);
    
    // Draw VU levels
    drawGradientBar(painter, leftMeterRect, m_leftVULevel, m_foregroundColor);
    drawGradientBar(painter, rightMeterRect, m_rightVULevel, m_foregroundColor);
    
    // Draw peak indicators
    if (m_peakHoldCheckBox->isChecked()) {
        drawPeakIndicator(painter, leftMeterRect, m_leftPeakLevel);
        drawPeakIndicator(painter, rightMeterRect, m_rightPeakLevel);
    }
    
    // Draw labels
    painter.setPen(QColor(150, 150, 150));
    painter.setFont(QFont("Arial", 10));
    painter.drawText(leftMeterRect.center().x() - 10, rect.bottom() - 5, "L");
    painter.drawText(rightMeterRect.center().x() - 10, rect.bottom() - 5, "R");
    
    // Draw scale
    painter.setPen(QColor(100, 100, 100));
    painter.setFont(QFont("Arial", 8));
    QStringList scaleLabels = {"0", "-10", "-20", "-30", "-40"};
    for (int i = 0; i < scaleLabels.size(); ++i) {
        int y = leftMeterRect.top() + (i * leftMeterRect.height() / (scaleLabels.size() - 1));
        painter.drawText(leftMeterRect.left() - 25, y + 3, scaleLabels[i]);
    }
}

void AudioVisualizerWidget::drawFrequencyBars(QPainter& painter, const QRect& rect)
{
    // Similar to spectrum analyzer but with 3D-style bars
    if (m_currentSpectrum.magnitudes.isEmpty()) {
        return;
    }
    
    int bandCount = m_currentSpectrum.magnitudes.size();
    int totalWidth = rect.width() - 40;
    int barWidth = qMax(2, (totalWidth - (bandCount - 1) * m_barSpacing) / bandCount);
    
    painter.setPen(Qt::NoPen);
    
    for (int i = 0; i < bandCount; ++i) {
        float magnitude = qBound(0.0f, m_currentSpectrum.magnitudes[i], 1.0f);
        int barHeight = static_cast<int>(magnitude * (rect.height() - 60));
        
        int x = rect.left() + 20 + i * (barWidth + m_barSpacing);
        int y = rect.bottom() - 30 - barHeight;
        
        // Draw 3D effect
        QColor barColor = getBarColor(magnitude, i);
        QColor topColor = barColor.lighter(150);
        QColor sideColor = barColor.darker(150);
        
        // Main bar
        painter.setBrush(barColor);
        painter.drawRect(x, y, barWidth, barHeight);
        
        // Top face
        if (barHeight > 0) {
            QPolygon topFace;
            topFace << QPoint(x, y) << QPoint(x + barWidth, y)
                   << QPoint(x + barWidth + 3, y - 3) << QPoint(x + 3, y - 3);
            painter.setBrush(topColor);
            painter.drawPolygon(topFace);
            
            // Side face
            QPolygon sideFace;
            sideFace << QPoint(x + barWidth, y) << QPoint(x + barWidth, y + barHeight)
                    << QPoint(x + barWidth + 3, y + barHeight - 3) << QPoint(x + barWidth + 3, y - 3);
            painter.setBrush(sideColor);
            painter.drawPolygon(sideFace);
        }
    }
}

void AudioVisualizerWidget::drawCircularSpectrum(QPainter& painter, const QRect& rect)
{
    if (m_currentSpectrum.magnitudes.isEmpty()) {
        return;
    }
    
    QPoint center = rect.center();
    int outerRadius = qMin(rect.width(), rect.height()) / 2 - 20;
    int innerRadius = outerRadius / 3;
    
    painter.setPen(Qt::NoPen);
    
    int bandCount = m_currentSpectrum.magnitudes.size();
    float angleStep = 360.0f / bandCount;
    
    for (int i = 0; i < bandCount; ++i) {
        float magnitude = qBound(0.0f, m_currentSpectrum.magnitudes[i], 1.0f);
        int barLength = static_cast<int>(magnitude * (outerRadius - innerRadius));
        
        float angle = i * angleStep;
        float radians = qDegreesToRadians(angle);
        
        int x1 = center.x() + static_cast<int>(innerRadius * qCos(radians));
        int y1 = center.y() + static_cast<int>(innerRadius * qSin(radians));
        int x2 = center.x() + static_cast<int>((innerRadius + barLength) * qCos(radians));
        int y2 = center.y() + static_cast<int>((innerRadius + barLength) * qSin(radians));
        
        QColor barColor = getBarColor(magnitude, i);
        painter.setPen(QPen(barColor, 3));
        painter.drawLine(x1, y1, x2, y2);
    }
    
    // Draw center circle
    painter.setPen(QPen(QColor(100, 100, 100), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, innerRadius, innerRadius);
}

void AudioVisualizerWidget::drawAIVisualizer(QPainter& painter, const QRect& rect)
{
    // Combine spectrum with AI-generated colors and effects
    drawSpectrumAnalyzer(painter, rect);
    
    // Add AI color overlay
    if (!m_colorPalette.isEmpty()) {
        painter.setCompositionMode(QPainter::CompositionMode_Overlay);
        
        QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
        for (int i = 0; i < m_colorPalette.size(); ++i) {
            float position = static_cast<float>(i) / (m_colorPalette.size() - 1);
            QColor color = m_colorPalette[i];
            color.setAlpha(50); // Semi-transparent
            gradient.setColorAt(position, color);
        }
        
        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        painter.drawRect(rect);
        
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    
    // Draw mood indicator
    if (!m_detectedMood.isEmpty()) {
        painter.setPen(QColor(255, 255, 255));
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        QString moodText = QString("Mood: %1 (%2%)").arg(m_detectedMood).arg(static_cast<int>(m_moodConfidence * 100));
        painter.drawText(rect.topLeft() + QPoint(10, 20), moodText);
    }
}

QColor AudioVisualizerWidget::getBarColor(float magnitude, int index) const
{
    // Use AI color palette if available
    if (!m_colorPalette.isEmpty()) {
        int colorIndex = index % m_colorPalette.size();
        QColor baseColor = m_colorPalette[colorIndex];
        
        // Adjust brightness based on magnitude
        int brightness = static_cast<int>(magnitude * 255);
        baseColor.setAlpha(qBound(50, brightness, 255));
        return baseColor;
    }
    
    // Default color scheme (green to red based on magnitude)
    if (magnitude < 0.5f) {
        return interpolateColor(QColor(0, 255, 0), QColor(255, 255, 0), magnitude * 2.0f);
    } else {
        return interpolateColor(QColor(255, 255, 0), QColor(255, 0, 0), (magnitude - 0.5f) * 2.0f);
    }
}

QColor AudioVisualizerWidget::interpolateColor(const QColor& color1, const QColor& color2, float ratio) const
{
    ratio = qBound(0.0f, ratio, 1.0f);
    
    int r = static_cast<int>(color1.red() * (1.0f - ratio) + color2.red() * ratio);
    int g = static_cast<int>(color1.green() * (1.0f - ratio) + color2.green() * ratio);
    int b = static_cast<int>(color1.blue() * (1.0f - ratio) + color2.blue() * ratio);
    
    return QColor(r, g, b);
}

void AudioVisualizerWidget::drawGradientBar(QPainter& painter, const QRect& rect, float value, const QColor& color) const
{
    value = qBound(0.0f, value, 1.0f);
    int fillHeight = static_cast<int>(value * rect.height());
    
    QRect fillRect = rect;
    fillRect.setTop(rect.bottom() - fillHeight);
    
    QLinearGradient gradient(fillRect.topLeft(), fillRect.bottomLeft());
    gradient.setColorAt(0.0, color.darker(150));
    gradient.setColorAt(0.5, color);
    gradient.setColorAt(1.0, color.lighter(150));
    
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRect(fillRect);
}

void AudioVisualizerWidget::drawPeakIndicator(QPainter& painter, const QRect& rect, float peak) const
{
    peak = qBound(0.0f, peak, 1.0f);
    int peakY = rect.bottom() - static_cast<int>(peak * rect.height());
    
    painter.setPen(QPen(m_accentColor, 2));
    painter.drawLine(rect.left(), peakY, rect.right(), peakY);
}

void AudioVisualizerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

// Slot implementations
void AudioVisualizerWidget::onSpectrumDataUpdated(const AudioVisualizer::SpectrumData& spectrum)
{
    m_currentSpectrum = spectrum;
    
    // Update peak frequency label
    if (spectrum.peakFrequency > 0) {
        QString freqText;
        if (spectrum.peakFrequency < 1000) {
            freqText = QString("Peak: %1 Hz").arg(static_cast<int>(spectrum.peakFrequency));
        } else {
            freqText = QString("Peak: %1 kHz").arg(spectrum.peakFrequency / 1000.0, 0, 'f', 1);
        }
        m_peakFreqLabel->setText(freqText);
    }
}

void AudioVisualizerWidget::onWaveformDataUpdated(const QVector<float>& leftChannel, const QVector<float>& rightChannel)
{
    m_leftWaveform = leftChannel;
    m_rightWaveform = rightChannel;
}

void AudioVisualizerWidget::onVULevelsUpdated(float leftLevel, float rightLevel)
{
    m_leftVULevel = leftLevel;
    m_rightVULevel = rightLevel;
}

void AudioVisualizerWidget::onPeakLevelsUpdated(float leftPeak, float rightPeak)
{
    m_leftPeakLevel = leftPeak;
    m_rightPeakLevel = rightPeak;
}

void AudioVisualizerWidget::onMoodDetected(const QString& mood, float confidence)
{
    m_detectedMood = mood;
    m_moodConfidence = confidence;
    
    m_moodLabel->setText(QString("Mood: %1 (%2%)").arg(mood).arg(static_cast<int>(confidence * 100)));
}

void AudioVisualizerWidget::onColorPaletteUpdated(const QVector<QColor>& colors)
{
    m_colorPalette = colors;
}

void AudioVisualizerWidget::onVisualizationModeChanged(int index)
{
    if (m_visualizer && !m_updatingControls) {
        int mode = m_modeCombo->itemData(index).toInt();
        m_visualizer->setVisualizationMode(static_cast<AudioVisualizer::VisualizationMode>(mode));
    }
}

void AudioVisualizerWidget::onSensitivityChanged(int value)
{
    if (m_visualizer && !m_updatingControls) {
        float sensitivity = value / 100.0f;
        m_visualizer->setSensitivity(sensitivity);
    }
    m_sensitivityLabel->setText(QString("%1%").arg(value));
}

void AudioVisualizerWidget::onSmoothingToggled(bool enabled)
{
    if (m_visualizer && !m_updatingControls) {
        m_visualizer->setSmoothingEnabled(enabled);
    }
}

void AudioVisualizerWidget::onPeakHoldToggled(bool enabled)
{
    if (m_visualizer && !m_updatingControls) {
        m_visualizer->setPeakHoldEnabled(enabled);
    }
}

void AudioVisualizerWidget::onUpdateRateChanged(int value)
{
    if (m_visualizer && !m_updatingControls) {
        m_visualizer->setUpdateRate(value);
    }
    m_updateRateLabel->setText(QString("%1 Hz").arg(value));
}

void AudioVisualizerWidget::onBandCountChanged(int index)
{
    if (m_visualizer && !m_updatingControls) {
        int bandCount = m_bandCountCombo->itemText(index).toInt();
        m_visualizer->setSpectrumBandCount(bandCount);
    }
}