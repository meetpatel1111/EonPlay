#ifndef AUDIOVISUALIZERWIDGET_H
#define AUDIOVISUALIZERWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QColor>
#include <QVector>
#include <QMutex>
#include <memory>

// Forward declarations
class AudioVisualizer;

/**
 * @brief Widget for displaying audio visualizations
 * 
 * Provides a comprehensive UI widget for displaying various audio visualizations
 * including spectrum analyzer, waveform, VU meters, and AI-driven visualizations.
 */
class AudioVisualizerWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Visualization styles
     */
    enum VisualizationStyle {
        Classic = 0,
        Modern,
        Neon,
        Retro,
        Minimal,
        AI_Driven
    };

    explicit AudioVisualizerWidget(QWidget* parent = nullptr);
    ~AudioVisualizerWidget() override;

    /**
     * @brief Set the audio visualizer backend
     * @param visualizer Audio visualizer instance
     */
    void setAudioVisualizer(AudioVisualizer* visualizer);

    /**
     * @brief Get current visualization style
     * @return Current style
     */
    VisualizationStyle getVisualizationStyle() const;

    /**
     * @brief Set visualization style
     * @param style Visualization style
     */
    void setVisualizationStyle(VisualizationStyle style);

    /**
     * @brief Get style name
     * @param style Visualization style
     * @return Style name
     */
    static QString getStyleName(VisualizationStyle style);

    /**
     * @brief Check if background is enabled
     * @return true if enabled
     */
    bool isBackgroundEnabled() const;

    /**
     * @brief Enable or disable background
     * @param enabled Enable state
     */
    void setBackgroundEnabled(bool enabled);

    /**
     * @brief Get background color
     * @return Background color
     */
    QColor getBackgroundColor() const;

    /**
     * @brief Set background color
     * @param color Background color
     */
    void setBackgroundColor(const QColor& color);

    /**
     * @brief Get primary color
     * @return Primary color
     */
    QColor getPrimaryColor() const;

    /**
     * @brief Set primary color
     * @param color Primary color
     */
    void setPrimaryColor(const QColor& color);

    /**
     * @brief Check if gradient is enabled
     * @return true if enabled
     */
    bool isGradientEnabled() const;

    /**
     * @brief Enable or disable gradient
     * @param enabled Enable state
     */
    void setGradientEnabled(bool enabled);

    /**
     * @brief Check if glow effect is enabled
     * @return true if enabled
     */
    bool isGlowEnabled() const;

    /**
     * @brief Enable or disable glow effect
     * @param enabled Enable state
     */
    void setGlowEnabled(bool enabled);

    /**
     * @brief Get animation speed
     * @return Animation speed (0.0 to 2.0)
     */
    float getAnimationSpeed() const;

    /**
     * @brief Set animation speed
     * @param speed Animation speed (0.0 to 2.0)
     */
    void setAnimationSpeed(float speed);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onSpectrumDataUpdated(const AudioVisualizer::SpectrumData& spectrum);
    void onWaveformDataUpdated(const QVector<float>& leftChannel, const QVector<float>& rightChannel);
    void onVULevelsUpdated(float leftLevel, float rightLevel);
    void onPeakLevelsUpdated(float leftPeak, float rightPeak);
    void onColorPaletteUpdated(const QVector<QColor>& colors);
    void updateAnimation();

private:
    void drawSpectrumAnalyzer(QPainter& painter);
    void drawWaveform(QPainter& painter);
    void drawVUMeter(QPainter& painter);
    void drawFrequencyBars(QPainter& painter);
    void drawCircularSpectrum(QPainter& painter);
    void drawAIVisualizer(QPainter& painter);
    
    void drawClassicStyle(QPainter& painter);
    void drawModernStyle(QPainter& painter);
    void drawNeonStyle(QPainter& painter);
    void drawRetroStyle(QPainter& painter);
    void drawMinimalStyle(QPainter& painter);
    void drawAIDrivenStyle(QPainter& painter);
    
    void setupGradients();
    void applyGlowEffect(QPainter& painter, const QColor& color);
    QColor interpolateColor(const QColor& color1, const QColor& color2, float ratio);
    void drawBar(QPainter& painter, const QRectF& rect, float value, const QColor& color);
    void drawCircularBar(QPainter& painter, const QPointF& center, float radius, float angle, float value, const QColor& color);

    // Visualizer backend
    AudioVisualizer* m_visualizer;

    // Visualization data
    AudioVisualizer::SpectrumData m_spectrumData;
    QVector<float> m_leftWaveform;
    QVector<float> m_rightWaveform;
    float m_leftVULevel;
    float m_rightVULevel;
    float m_leftPeakLevel;
    float m_rightPeakLevel;
    QVector<QColor> m_aiColorPalette;

    // Visual settings
    VisualizationStyle m_visualizationStyle;
    bool m_backgroundEnabled;
    QColor m_backgroundColor;
    QColor m_primaryColor;
    bool m_gradientEnabled;
    bool m_glowEnabled;
    float m_animationSpeed;

    // Animation state
    QTimer* m_animationTimer;
    float m_animationPhase;
    QVector<float> m_barAnimations;
    QVector<float> m_peakAnimations;

    // Gradients
    QLinearGradient m_primaryGradient;
    QRadialGradient m_glowGradient;

    // Thread safety
    mutable QMutex m_dataMutex;

    // Constants
    static constexpr int ANIMATION_FPS = 60;
    static constexpr float ANIMATION_DECAY = 0.95f;
    static constexpr float PEAK_DECAY = 0.98f;
    static constexpr int MIN_BAR_HEIGHT = 2;
    static constexpr int BAR_SPACING = 2;
    static constexpr int VU_METER_WIDTH = 20;
    static constexpr int CIRCULAR_RADIUS = 100;
};

#endif // AUDIOVISUALIZERWIDGET_H