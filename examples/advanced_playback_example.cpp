#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include "media/PlaybackController.h"
#include "media/VLCBackend.h"
#include <memory>

/**
 * @brief EonPlay - Example demonstrating advanced playback features
 * 
 * This example shows how to use EonPlay's timeless advanced playback features:
 * - Fast forward/rewind with speed control
 * - Playback modes (repeat, shuffle)
 * - Frame-by-frame stepping
 * - Crossfade settings
 * - Resume position functionality
 * - Seek thumbnail generation
 */
class AdvancedPlaybackExample : public QObject
{
    Q_OBJECT

public:
    AdvancedPlaybackExample(QObject* parent = nullptr)
        : QObject(parent)
        , m_controller(new PlaybackController(this))
        , m_vlcBackend(std::make_shared<VLCBackend>())
    {
        // Initialize components
        m_controller->initialize();
        m_vlcBackend->initialize();
        m_controller->setMediaEngine(m_vlcBackend);
        
        // Connect signals to demonstrate functionality
        connectSignals();
        
        // Start demonstration
        QTimer::singleShot(100, this, &AdvancedPlaybackExample::runDemo);
    }

private slots:
    void runDemo()
    {
        qDebug() << "=== Advanced Playback Features Demo ===";
        
        // Demo 1: Playback modes
        demonstratePlaybackModes();
        
        // Demo 2: Fast seek operations
        QTimer::singleShot(1000, this, &AdvancedPlaybackExample::demonstrateFastSeek);
        
        // Demo 3: Frame stepping
        QTimer::singleShot(2000, this, &AdvancedPlaybackExample::demonstrateFrameStepping);
        
        // Demo 4: Crossfade settings
        QTimer::singleShot(3000, this, &AdvancedPlaybackExample::demonstrateCrossfade);
        
        // Demo 5: Resume positions
        QTimer::singleShot(4000, this, &AdvancedPlaybackExample::demonstrateResumePositions);
        
        // Demo 6: Seek thumbnails
        QTimer::singleShot(5000, this, &AdvancedPlaybackExample::demonstrateSeekThumbnails);
        
        // Exit after demo
        QTimer::singleShot(6000, qApp, &QCoreApplication::quit);
    }
    
    void demonstratePlaybackModes()
    {
        qDebug() << "\n--- Playback Modes Demo ---";
        
        qDebug() << "Current mode:" << static_cast<int>(m_controller->playbackMode());
        
        m_controller->setPlaybackMode(PlaybackMode::RepeatOne);
        qDebug() << "Set to RepeatOne mode";
        
        m_controller->setPlaybackMode(PlaybackMode::RepeatAll);
        qDebug() << "Set to RepeatAll mode";
        
        m_controller->setPlaybackMode(PlaybackMode::Shuffle);
        qDebug() << "Set to Shuffle mode";
        
        m_controller->setPlaybackMode(PlaybackMode::Normal);
        qDebug() << "Reset to Normal mode";
    }
    
    void demonstrateFastSeek()
    {
        qDebug() << "\n--- Fast Seek Demo ---";
        
        qDebug() << "Starting fast forward at normal speed";
        m_controller->startFastForward(SeekSpeed::Normal);
        
        QTimer::singleShot(200, this, [this]() {
            qDebug() << "Is fast seeking:" << m_controller->isFastSeeking();
            
            qDebug() << "Starting rewind at fast speed";
            m_controller->startRewind(SeekSpeed::Fast);
        });
        
        QTimer::singleShot(400, this, [this]() {
            qDebug() << "Stopping fast seek";
            m_controller->stopFastSeek();
            qDebug() << "Is fast seeking:" << m_controller->isFastSeeking();
        });
    }
    
    void demonstrateFrameStepping()
    {
        qDebug() << "\n--- Frame Stepping Demo ---";
        
        qDebug() << "Stepping forward one frame";
        m_controller->stepForward();
        
        QTimer::singleShot(100, this, [this]() {
            qDebug() << "Stepping backward one frame";
            m_controller->stepBackward();
        });
    }
    
    void demonstrateCrossfade()
    {
        qDebug() << "\n--- Crossfade Demo ---";
        
        qDebug() << "Crossfade enabled:" << m_controller->isCrossfadeEnabled();
        qDebug() << "Crossfade duration:" << m_controller->crossfadeDuration() << "ms";
        
        m_controller->setCrossfadeEnabled(true, 5000);
        qDebug() << "Enabled crossfade with 5 second duration";
        
        m_controller->setGaplessPlayback(true);
        qDebug() << "Enabled gapless playback:" << m_controller->isGaplessPlayback();
    }
    
    void demonstrateResumePositions()
    {
        qDebug() << "\n--- Resume Positions Demo ---";
        
        QString testFile = "/path/to/test/media.mp4";
        
        qDebug() << "Saving resume position for:" << testFile;
        m_controller->saveResumePosition(testFile);
        
        qDebug() << "Loading resume position for:" << testFile;
        bool loaded = m_controller->loadResumePosition(testFile);
        qDebug() << "Resume position loaded:" << loaded;
        
        qDebug() << "Clearing resume position for:" << testFile;
        m_controller->clearResumePosition(testFile);
    }
    
    void demonstrateSeekThumbnails()
    {
        qDebug() << "\n--- Seek Thumbnails Demo ---";
        
        qDebug() << "Thumbnails enabled:" << m_controller->isSeekThumbnailEnabled();
        
        qDebug() << "Generating thumbnail for position 30000ms";
        QString thumbnail = m_controller->generateSeekThumbnail(30000);
        qDebug() << "Thumbnail generated (length):" << thumbnail.length();
        
        qDebug() << "Disabling thumbnails";
        m_controller->setSeekThumbnailEnabled(false);
        
        thumbnail = m_controller->generateSeekThumbnail(60000);
        qDebug() << "Thumbnail when disabled (should be empty):" << thumbnail.isEmpty();
        
        // Re-enable for cleanup
        m_controller->setSeekThumbnailEnabled(true);
    }
    
    void onPlaybackModeChanged(PlaybackMode mode)
    {
        qDebug() << "Playback mode changed to:" << static_cast<int>(mode);
    }
    
    void onFastSeekChanged(bool active, int speed)
    {
        qDebug() << "Fast seek changed - Active:" << active << "Speed:" << speed << "x";
    }
    
    void onCrossfadeChanged(bool enabled, int duration)
    {
        qDebug() << "Crossfade changed - Enabled:" << enabled << "Duration:" << duration << "ms";
    }
    
    void onGaplessPlaybackChanged(bool enabled)
    {
        qDebug() << "Gapless playback changed - Enabled:" << enabled;
    }
    
    void onSeekThumbnailGenerated(qint64 position, const QString& thumbnail)
    {
        qDebug() << "Seek thumbnail generated for position:" << position 
                 << "Thumbnail size:" << thumbnail.length() << "bytes";
    }

private:
    void connectSignals()
    {
        connect(m_controller, &PlaybackController::playbackModeChanged,
                this, &AdvancedPlaybackExample::onPlaybackModeChanged);
        
        connect(m_controller, &PlaybackController::fastSeekChanged,
                this, &AdvancedPlaybackExample::onFastSeekChanged);
        
        connect(m_controller, &PlaybackController::crossfadeChanged,
                this, &AdvancedPlaybackExample::onCrossfadeChanged);
        
        connect(m_controller, &PlaybackController::gaplessPlaybackChanged,
                this, &AdvancedPlaybackExample::onGaplessPlaybackChanged);
        
        connect(m_controller, &PlaybackController::seekThumbnailGenerated,
                this, &AdvancedPlaybackExample::onSeekThumbnailGenerated);
    }

private:
    PlaybackController* m_controller;
    std::shared_ptr<VLCBackend> m_vlcBackend;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "EonPlay - Advanced Playback Features Example";
    qDebug() << "This example demonstrates EonPlay's timeless advanced playback features.";
    
    AdvancedPlaybackExample example;
    
    return app.exec();
}

#include "advanced_playback_example.moc"