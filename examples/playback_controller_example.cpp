#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QThread>
#include "media/PlaybackController.h"
#include "media/VLCBackend.h"
#include <memory>

/**
 * @brief Example demonstrating PlaybackController usage
 * 
 * This example shows how to:
 * 1. Create and initialize PlaybackController and VLCBackend
 * 2. Connect them together
 * 3. Use high-level playback controls
 * 4. Handle volume and mute operations
 * 5. Demonstrate seeking functionality
 */
class PlaybackExample : public QObject
{
    Q_OBJECT

public:
    PlaybackExample(QObject* parent = nullptr) 
        : QObject(parent)
        , m_vlcBackend(std::make_shared<VLCBackend>())
        , m_controller(std::make_shared<PlaybackController>())
    {
        // Connect to controller signals
        connect(m_controller.get(), &PlaybackController::stateChanged, 
                this, &PlaybackExample::onStateChanged);
        connect(m_controller.get(), &PlaybackController::positionChanged, 
                this, &PlaybackExample::onPositionChanged);
        connect(m_controller.get(), &PlaybackController::durationChanged, 
                this, &PlaybackExample::onDurationChanged);
        connect(m_controller.get(), &PlaybackController::volumeChanged, 
                this, &PlaybackExample::onVolumeChanged);
        connect(m_controller.get(), &PlaybackController::muteChanged, 
                this, &PlaybackExample::onMuteChanged);
        connect(m_controller.get(), &PlaybackController::playbackSpeedChanged, 
                this, &PlaybackExample::onSpeedChanged);
        connect(m_controller.get(), &PlaybackController::mediaLoaded, 
                this, &PlaybackExample::onMediaLoaded);
        connect(m_controller.get(), &PlaybackController::errorOccurred, 
                this, &PlaybackExample::onErrorOccurred);
    }
    
    bool initialize()
    {
        qDebug() << "Initializing PlaybackController Example";
        qDebug() << "======================================";
        
        if (!VLCBackend::isVLCAvailable()) {
            qCritical() << "libVLC is not available on this system";
            return false;
        }
        
        qDebug() << "VLC Version:" << VLCBackend::vlcVersion();
        
        // Initialize components
        if (!m_vlcBackend->initialize()) {
            qCritical() << "Failed to initialize VLC backend";
            return false;
        }
        
        if (!m_controller->initialize()) {
            qCritical() << "Failed to initialize PlaybackController";
            return false;
        }
        
        // Connect controller to VLC backend
        m_controller->setMediaEngine(m_vlcBackend);
        
        qDebug() << "Components initialized and connected successfully";
        return true;
    }
    
    void demonstrateControls(const QString& mediaPath)
    {
        if (mediaPath.isEmpty()) {
            qDebug() << "No media path provided. Demonstrating controls without media.";
            demonstrateControlsWithoutMedia();
            return;
        }
        
        qDebug() << "\nLoading media:" << mediaPath;
        m_controller->loadMedia(mediaPath);
        
        // The rest of the demonstration will continue in onMediaLoaded()
    }
    
    void demonstrateControlsWithoutMedia()
    {
        qDebug() << "\nDemonstrating PlaybackController features:";
        
        // Volume control
        qDebug() << "\n--- Volume Control ---";
        qDebug() << "Initial volume:" << m_controller->volume();
        
        m_controller->setVolume(75);
        qDebug() << "Set volume to 75:" << m_controller->volume();
        
        m_controller->volumeUp(10);
        qDebug() << "Volume up by 10:" << m_controller->volume();
        
        m_controller->volumeDown(20);
        qDebug() << "Volume down by 20:" << m_controller->volume();
        
        // Mute control
        qDebug() << "\n--- Mute Control ---";
        qDebug() << "Is muted:" << m_controller->isMuted();
        
        m_controller->toggleMute();
        qDebug() << "After toggle mute:" << m_controller->isMuted() << "Volume:" << m_controller->volume();
        
        m_controller->toggleMute();
        qDebug() << "After toggle mute again:" << m_controller->isMuted() << "Volume:" << m_controller->volume();
        
        // Playback speed
        qDebug() << "\n--- Playback Speed ---";
        qDebug() << "Initial speed:" << m_controller->playbackSpeed() << "%";
        
        m_controller->setPlaybackSpeed(PlaybackSpeed::Half);
        qDebug() << "Set to half speed:" << m_controller->playbackSpeed() << "%";
        
        m_controller->setPlaybackSpeed(PlaybackSpeed::Double);
        qDebug() << "Set to double speed:" << m_controller->playbackSpeed() << "%";
        
        m_controller->resetPlaybackSpeed();
        qDebug() << "Reset to normal speed:" << m_controller->playbackSpeed() << "%";
        
        // State information
        qDebug() << "\n--- State Information ---";
        qDebug() << "Current state:" << static_cast<int>(m_controller->state());
        qDebug() << "Has media:" << m_controller->hasMedia();
        qDebug() << "Position:" << m_controller->position() << "ms";
        qDebug() << "Duration:" << m_controller->duration() << "ms";
        qDebug() << "Position percentage:" << m_controller->positionPercentage() << "%";
        
        qDebug() << "\nPlaybackController demonstration completed";
        QCoreApplication::quit();
    }

private slots:
    void onStateChanged(PlaybackState state)
    {
        QString stateStr;
        switch (state) {
            case PlaybackState::Stopped: stateStr = "Stopped"; break;
            case PlaybackState::Playing: stateStr = "Playing"; break;
            case PlaybackState::Paused: stateStr = "Paused"; break;
            case PlaybackState::Buffering: stateStr = "Buffering"; break;
            case PlaybackState::Error: stateStr = "Error"; break;
        }
        qDebug() << "State changed to:" << stateStr;
    }
    
    void onPositionChanged(qint64 position)
    {
        // Only log every 2 seconds to avoid spam
        static qint64 lastLoggedSecond = -1;
        qint64 currentSecond = position / 1000;
        
        if (currentSecond != lastLoggedSecond && currentSecond % 2 == 0) {
            double percentage = m_controller->positionPercentage();
            qDebug() << QString("Position: %1s (%2%)").arg(currentSecond).arg(percentage, 0, 'f', 1);
            lastLoggedSecond = currentSecond;
        }
    }
    
    void onDurationChanged(qint64 duration)
    {
        qDebug() << "Duration:" << (duration / 1000) << "seconds";
    }
    
    void onVolumeChanged(int volume)
    {
        qDebug() << "Volume changed to:" << volume;
    }
    
    void onMuteChanged(bool muted)
    {
        qDebug() << "Mute changed to:" << (muted ? "ON" : "OFF");
    }
    
    void onSpeedChanged(int speed)
    {
        qDebug() << "Playback speed changed to:" << speed << "%";
    }
    
    void onMediaLoaded(bool success, const QString& path)
    {
        qDebug() << "Media loaded:" << (success ? "SUCCESS" : "FAILED") << "Path:" << path;
        
        if (success) {
            // Demonstrate playback controls with loaded media
            demonstratePlaybackWithMedia();
        } else {
            qWarning() << "Failed to load media, demonstrating without media";
            demonstrateControlsWithoutMedia();
        }
    }
    
    void onErrorOccurred(const QString& error)
    {
        qCritical() << "Playback error:" << error;
    }
    
    void demonstratePlaybackWithMedia()
    {
        qDebug() << "\n--- Playback Control Demonstration ---";
        
        // Start playback
        qDebug() << "Starting playback...";
        m_controller->play();
        
        // Schedule various operations
        QTimer::singleShot(2000, this, [this]() {
            qDebug() << "Pausing playback...";
            m_controller->pause();
        });
        
        QTimer::singleShot(4000, this, [this]() {
            qDebug() << "Resuming playback...";
            m_controller->play();
        });
        
        QTimer::singleShot(6000, this, [this]() {
            qDebug() << "Seeking forward 10 seconds...";
            m_controller->seekForward(10000);
        });
        
        QTimer::singleShot(8000, this, [this]() {
            qDebug() << "Seeking to 25% of duration...";
            m_controller->seekToPercentage(25.0);
        });
        
        QTimer::singleShot(10000, this, [this]() {
            qDebug() << "Setting playback speed to 1.5x...";
            m_controller->setPlaybackSpeed(PlaybackSpeed::OneAndHalf);
        });
        
        QTimer::singleShot(12000, this, [this]() {
            qDebug() << "Stopping playback and exiting...";
            m_controller->stop();
            QCoreApplication::quit();
        });
    }

private:
    std::shared_ptr<VLCBackend> m_vlcBackend;
    std::shared_ptr<PlaybackController> m_controller;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    PlaybackExample example;
    
    if (!example.initialize()) {
        return 1;
    }
    
    // Check if media path was provided
    QString mediaPath;
    if (argc > 1) {
        mediaPath = QString::fromLocal8Bit(argv[1]);
    }
    
    example.demonstrateControls(mediaPath);
    
    return app.exec();
}

#include "playback_controller_example.moc"