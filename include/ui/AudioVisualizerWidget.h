#ifndef AUDIOVISUALIZERWIDGET_H
#define AUDIOVISUALIZERWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QVector>
#include <QColor>
#include <memory>

#include "audio/AudioVisualizer.h"

/**
 * @brief Widget for displaying audio visualizations
 * 
 * Provides real-time audio visualization display with multiple modes
 * including spectrum analyzer, waveform, VU meters, and AI-driven visuals.
 */
class AudioVisualizerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AudioVisualizerWidget(QWidget* parent = nullptr);
    ~AudioVisualizerWidget() override;

    /**
     * @brief Initialize the widget with audio visualizer
     * @param visualizer Audio visualizer instance
     */
    void initialize(std::shared_ptr<AudioVisualizer> visualizer);

    /**
     * @brief Get minimum size hint
     * @return Minimum size
     */
    QSize minimumSizeHint() const override;

    /**
     * @brief Get size hint
     * @return Preferred size
     */
    QSize sizeHint() const override;

public slots:
    /**
     * @brief Handle spectrum data update
     * @param spectrum New spectrum data
     */
    void onSpectrumDataUpdated(const AudioVisualizer::SpectrumData& spectrum);

    /**
     * @brief Handle waveform data update
     * @param leftChannel Left channel waveform
     * @param rightChannel Right channel waveform
     */
    void onWaveformDataUpdated(const QVector<float>& leftChannel, const QVector<float>& rightChannel);

    /**
     * @brief Handle VU levels update
     * @param leftLevel Left channel VU level
     * @param rightLevel Right channel VU level
     */
    void onVULevelsUpdated(float leftLevel, float rightLevel);

    /**
     * @brief Handle peak levels update
     * @param leftPeak Left channel peak level
     * @param rightPeak Right channel peak level
     */
    void onPeakLevelsUpdated(float leftPeak, float rightPeak);

    /**
     * @brief Handle mood detection update
     * @param mood Detected mood
     * @param confidence Confidence level
     */
    void onMoodDetected(const QString& mood, float confidence);

    /**
     * @brief Handle color palette update
     * @param colors New color palette
     */
    void onColorPaletteUpdated(const QVector<QColor>& colors);

protected:
    /**
     * @brief Paint event for custom drawing
     * @param event Paint event
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief Resize event
     * @param event Resize event
     */
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onVisualizationModeChanged(int index);
    void onSensitivityChanged(int value);
    void onSmoothingToggled(bool enabled);
    void onPeakHoldToggled(bool enabled);
    void onUpdateRateChanged(int value);
    void onBandCountChanged(int index);

private:
    void setupUI();
    void setupControls();
    void setupConnections();
    void updateFromVisualizer();
    
    // Drawing methods
    void drawSpectrumAnalyzer(QPainter& painter, const QRect& rect);
    void drawWaveform(QPainter& painter, const QRect& rect);
    void drawOscilloscope(QPainter& painter, const QRect& rect);
    void drawVUMeter(QPainter& painter, const QRect& rect);
    void drawFrequencyBars(QPainter& painter, const QRect& rect);
    void drawCircularSpectrum(QPainter& painter, const QRect& rect);
    void drawAIVisualizer(QPainter& painter, const QRect& rect);
    
    // Helper methods
    QColor getBarColor(float magnitude, int index) const;
    QColor interpolateColor(const QColor& color1, const QColor& color2, float ratio) const;
    void drawGradientBar(QPainter& painter, const QRect& rect, float value, const QColor& color) const;
    void drawPeakIndicator(QPainter& painter, const QRect& rect, float peak) const;

    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_controlsLayout;
    
    // Controls
    QGroupBox* m_controlsGroup;
    QComboBox* m_modeCombo;
    QSlider* m_sensitivitySlider;
    QLabel* m_sensitivityLabel;
    QCheckBox* m_smoothingCheckBox;
    QCheckBox* m_peakHoldCheckBox;
    QSlider* m_updateRateSlider;
    QLabel* m_updateRateLabel;
    QComboBox* m_bandCountCombo;
    
    // Status labels
    QLabel* m_moodLabel;
    QLabel* m_peakFreqLabel;

    // Visualizer instance
    std::shared_ptr<AudioVisualizer> m_visualizer;

    // Visualization data
    AudioVisualizer::SpectrumData m_currentSpectrum;
    QVector<float> m_leftWaveform;
    QVector<float> m_rightWaveform;
    float m_leftVULevel;
    float m_rightVULevel;
    float m_leftPeakLevel;
    float m_rightPeakLevel;
    QString m_detectedMood;
    float m_moodConfidence;
    QVector<QColor> m_colorPalette;

    // Drawing state
    QTimer* m_repaintTimer;
    bool m_updatingControls;
    
    // Visual settings
    QColor m_backgroundColor;
    QColor m_foregroundColor;
    QColor m_accentColor;
    int m_barSpacing;
    int m_barWidth;
};

#endif // AUDIOVISUALIZERWIDGET_H