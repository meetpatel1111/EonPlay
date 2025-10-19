#ifndef AUDIOPROCESSORWIDGET_H
#define AUDIOPROCESSORWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QProgressBar>
#include <QListWidget>
#include <QTabWidget>
#include <memory>

#include "audio/AudioProcessor.h"

/**
 * @brief Widget for advanced audio processing controls
 * 
 * Provides UI controls for karaoke mode, audio track switching,
 * lyrics display, audio tag editing, and format conversion.
 */
class AudioProcessorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AudioProcessorWidget(QWidget* parent = nullptr);
    ~AudioProcessorWidget() override;

    /**
     * @brief Initialize the widget with audio processor
     * @param processor Audio processor instance
     */
    void initialize(std::shared_ptr<AudioProcessor> processor);

    /**
     * @brief Update the widget from processor settings
     */
    void updateFromProcessor();

public slots:
    /**
     * @brief Handle processor enabled state change
     * @param enabled New enabled state
     */
    void onProcessorEnabledChanged(bool enabled);

    /**
     * @brief Handle audio track change
     * @param trackId New track ID
     * @param trackInfo Track information
     */
    void onAudioTrackChanged(int trackId, const AudioProcessor::AudioTrack& trackInfo);

    /**
     * @brief Handle karaoke mode change
     * @param mode New karaoke mode
     */
    void onKaraokeModeChanged(AudioProcessor::KaraokeMode mode);

    /**
     * @brief Handle lyrics loaded
     * @param lyricsCount Number of lyrics lines
     */
    void onLyricsLoaded(int lyricsCount);

    /**
     * @brief Handle current lyrics change
     * @param line Current lyrics line
     */
    void onCurrentLyricsChanged(const AudioProcessor::LyricsLine& line);

    /**
     * @brief Handle conversion progress
     * @param progress Progress percentage
     */
    void onConversionProgress(int progress);

    /**
     * @brief Handle conversion completed
     * @param success Success status
     * @param outputPath Output file path
     */
    void onConversionCompleted(bool success, const QString& outputPath);

    /**
     * @brief Handle audio delay change
     * @param delayMs New delay in milliseconds
     */
    void onAudioDelayChanged(int delayMs);

    /**
     * @brief Handle noise reduction change
     * @param enabled Enable state
     * @param strength Strength level
     */
    void onNoiseReductionChanged(bool enabled, float strength);

private slots:
    void onEnableCheckBoxToggled(bool enabled);
    void onAudioTrackComboChanged(int index);
    void onKaraokeModeComboChanged(int index);
    void onVocalStrengthSliderChanged(int value);
    void onFrequencyRangeChanged();
    void onLoadLyricsClicked();
    void onClearLyricsClicked();
    void onExportLyricsClicked();
    void onAudioDelaySpinBoxChanged(int value);
    void onNoiseReductionToggled(bool enabled);
    void onNoiseStrengthSliderChanged(int value);
    void onConvertAudioClicked();
    void onCancelConversionClicked();

private:
    void setupUI();
    void setupAudioTrackTab();
    void setupKaraokeTab();
    void setupLyricsTab();
    void setupConversionTab();
    void setupEffectsTab();
    void setupConnections();
    void updateAudioTrackList();
    void updateLyricsDisplay();
    void setWidgetsEnabled(bool enabled);

    // Main layout
    QVBoxLayout* m_mainLayout;
    QCheckBox* m_enableCheckBox;
    QTabWidget* m_tabWidget;

    // Audio Track Tab
    QWidget* m_audioTrackTab;
    QVBoxLayout* m_audioTrackLayout;
    QComboBox* m_audioTrackCombo;
    QLabel* m_trackInfoLabel;

    // Karaoke Tab
    QWidget* m_karaokeTab;
    QGridLayout* m_karaokeLayout;
    QComboBox* m_karaokeModeCombo;
    QSlider* m_vocalStrengthSlider;
    QLabel* m_vocalStrengthLabel;
    QSpinBox* m_lowFreqSpinBox;
    QSpinBox* m_highFreqSpinBox;

    // Lyrics Tab
    QWidget* m_lyricsTab;
    QVBoxLayout* m_lyricsLayout;
    QHBoxLayout* m_lyricsButtonLayout;
    QPushButton* m_loadLyricsButton;
    QPushButton* m_clearLyricsButton;
    QPushButton* m_exportLyricsButton;
    QTextEdit* m_lyricsDisplay;
    QLabel* m_lyricsStatusLabel;

    // Conversion Tab
    QWidget* m_conversionTab;
    QGridLayout* m_conversionLayout;
    QComboBox* m_inputFormatCombo;
    QComboBox* m_outputFormatCombo;
    QSlider* m_qualitySlider;
    QLabel* m_qualityLabel;
    QPushButton* m_convertButton;
    QPushButton* m_cancelConversionButton;
    QProgressBar* m_conversionProgress;
    QLabel* m_conversionStatusLabel;

    // Effects Tab
    QWidget* m_effectsTab;
    QGridLayout* m_effectsLayout;
    QSpinBox* m_audioDelaySpinBox;
    QCheckBox* m_noiseReductionCheckBox;
    QSlider* m_noiseStrengthSlider;
    QLabel* m_noiseStrengthLabel;

    // Processor instance
    std::shared_ptr<AudioProcessor> m_processor;

    // Update flags
    bool m_updatingFromProcessor;
};

#endif // AUDIOPROCESSORWIDGET_H