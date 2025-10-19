#pragma once

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QTimer>
#include <QTime>
#include <QComboBox>
#include <QToolButton>
#include <QButtonGroup>
#include <QFrame>
#include <memory>

class PlaybackController;
class ComponentManager;

/**
 * @brief Playback controls widget for EonPlay media player
 * 
 * Provides comprehensive media playback controls including play/pause/stop buttons,
 * seek slider with position display, volume control, time display, skip buttons,
 * and playback speed control. Features futuristic design with thumbnail preview.
 */
class PlaybackControls : public QWidget
{
    Q_OBJECT

public:
    explicit PlaybackControls(QWidget* parent = nullptr);
    ~PlaybackControls() override;
    
    /**
     * @brief Initialize with component manager
     * @param componentManager Pointer to component manager
     * @return true if initialization successful
     */
    bool initialize(ComponentManager* componentManager);
    
    /**
     * @brief Set the current playback position
     * @param position Position in milliseconds
     */
    void setPosition(qint64 position);
    
    /**
     * @brief Set the media duration
     * @param duration Duration in milliseconds
     */
    void setDuration(qint64 duration);
    
    /**
     * @brief Set the current volume level
     * @param volume Volume level (0-100)
     */
    void setVolume(int volume);
    
    /**
     * @brief Set the current playback speed
     * @param speed Playback speed multiplier (e.g., 1.0 = normal)
     */
    void setPlaybackSpeed(double speed);
    
    /**
     * @brief Update playback state
     * @param isPlaying true if media is playing
     */
    void setPlaybackState(bool isPlaying);
    
    /**
     * @brief Enable or disable controls
     * @param enabled true to enable controls
     */
    void setControlsEnabled(bool enabled);
    
    /**
     * @brief Set compact mode for smaller displays
     * @param compact true for compact layout
     */
    void setCompactMode(bool compact);
    
    /**
     * @brief Get current position
     * @return Current position in milliseconds
     */
    qint64 position() const { return m_currentPosition; }
    
    /**
     * @brief Get current duration
     * @return Duration in milliseconds
     */
    qint64 duration() const { return m_duration; }
    
    /**
     * @brief Get current volume
     * @return Volume level (0-100)
     */
    int volume() const { return m_volume; }
    
    /**
     * @brief Get current playback speed
     * @return Playback speed multiplier
     */
    double playbackSpeed() const { return m_playbackSpeed; }

public slots:
    /**
     * @brief Update position display
     * @param position New position in milliseconds
     */
    void updatePosition(qint64 position);
    
    /**
     * @brief Update duration display
     * @param duration New duration in milliseconds
     */
    void updateDuration(qint64 duration);
    
    /**
     * @brief Reset all controls to default state
     */
    void resetControls();

signals:
    /**
     * @brief Emitted when play button is clicked
     */
    void playRequested();
    
    /**
     * @brief Emitted when pause button is clicked
     */
    void pauseRequested();
    
    /**
     * @brief Emitted when stop button is clicked
     */
    void stopRequested();
    
    /**
     * @brief Emitted when user seeks to a position
     * @param position Target position in milliseconds
     */
    void seekRequested(qint64 position);
    
    /**
     * @brief Emitted when volume changes
     * @param volume New volume level (0-100)
     */
    void volumeChanged(int volume);
    
    /**
     * @brief Emitted when playback speed changes
     * @param speed New playback speed multiplier
     */
    void playbackSpeedChanged(double speed);
    
    /**
     * @brief Emitted when skip previous is requested
     */
    void skipPreviousRequested();
    
    /**
     * @brief Emitted when skip next is requested
     */
    void skipNextRequested();
    
    /**
     * @brief Emitted when mute toggle is requested
     */
    void muteToggleRequested();

protected:
    /**
     * @brief Handle mouse press events for seek preview
     * @param event Mouse press event
     */
    void mousePressEvent(QMouseEvent* event) override;
    
    /**
     * @brief Handle mouse move events for seek preview
     * @param event Mouse move event
     */
    void mouseMoveEvent(QMouseEvent* event) override;
    
    /**
     * @brief Handle mouse release events
     * @param event Mouse release event
     */
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    /**
     * @brief Handle play/pause button click
     */
    void onPlayPauseClicked();
    
    /**
     * @brief Handle stop button click
     */
    void onStopClicked();
    
    /**
     * @brief Handle skip previous button click
     */
    void onSkipPreviousClicked();
    
    /**
     * @brief Handle skip next button click
     */
    void onSkipNextClicked();
    
    /**
     * @brief Handle seek slider value change
     * @param value New slider value
     */
    void onSeekSliderChanged(int value);
    
    /**
     * @brief Handle seek slider press
     */
    void onSeekSliderPressed();
    
    /**
     * @brief Handle seek slider release
     */
    void onSeekSliderReleased();
    
    /**
     * @brief Handle volume slider change
     * @param value New volume value
     */
    void onVolumeSliderChanged(int value);
    
    /**
     * @brief Handle mute button click
     */
    void onMuteClicked();
    
    /**
     * @brief Handle playback speed selection
     * @param index Speed selection index
     */
    void onPlaybackSpeedChanged(int index);
    
    /**
     * @brief Update time display labels
     */
    void updateTimeDisplay();
    
    /**
     * @brief Show seek preview tooltip
     * @param position Mouse position for preview
     */
    void showSeekPreview(const QPoint& position);
    
    /**
     * @brief Hide seek preview tooltip
     */
    void hideSeekPreview();

private:
    /**
     * @brief Initialize the user interface
     */
    void initializeUI();
    
    /**
     * @brief Create main control buttons
     */
    void createMainControls();
    
    /**
     * @brief Create seek controls
     */
    void createSeekControls();
    
    /**
     * @brief Create volume controls
     */
    void createVolumeControls();
    
    /**
     * @brief Create time display
     */
    void createTimeDisplay();
    
    /**
     * @brief Create playback speed controls
     */
    void createSpeedControls();
    
    /**
     * @brief Apply futuristic styling
     */
    void applyFuturisticStyling();
    
    /**
     * @brief Format time for display
     * @param milliseconds Time in milliseconds
     * @return Formatted time string (MM:SS or HH:MM:SS)
     */
    QString formatTime(qint64 milliseconds) const;
    
    /**
     * @brief Update play/pause button state
     */
    void updatePlayPauseButton();
    
    /**
     * @brief Update mute button state
     */
    void updateMuteButton();
    
    /**
     * @brief Calculate seek position from mouse position
     * @param mouseX Mouse X coordinate
     * @return Calculated position in milliseconds
     */
    qint64 calculateSeekPosition(int mouseX) const;
    
    // Component manager
    ComponentManager* m_componentManager;
    PlaybackController* m_playbackController;
    
    // Main layout
    QHBoxLayout* m_mainLayout;
    QVBoxLayout* m_compactLayout;
    
    // Control buttons
    QPushButton* m_skipPreviousButton;
    QPushButton* m_playPauseButton;
    QPushButton* m_stopButton;
    QPushButton* m_skipNextButton;
    
    // Seek controls
    QSlider* m_seekSlider;
    QLabel* m_seekPreviewLabel;
    
    // Volume controls
    QSlider* m_volumeSlider;
    QPushButton* m_muteButton;
    QLabel* m_volumeLabel;
    
    // Time display
    QLabel* m_currentTimeLabel;
    QLabel* m_durationLabel;
    QLabel* m_timeSeparatorLabel;
    
    // Speed controls
    QComboBox* m_speedComboBox;
    QLabel* m_speedLabel;
    
    // State variables
    qint64 m_currentPosition;
    qint64 m_duration;
    int m_volume;
    int m_previousVolume; // For mute functionality
    double m_playbackSpeed;
    bool m_isPlaying;
    bool m_isMuted;
    bool m_isSeekSliderPressed;
    bool m_compactMode;
    bool m_controlsEnabled;
    
    // Seek preview
    bool m_seekPreviewVisible;
    QPoint m_lastMousePosition;
    
    // Predefined playback speeds
    static const QList<double> PlaybackSpeeds;
    static const QStringList SpeedLabels;
};