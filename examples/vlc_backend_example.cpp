#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "media/VLCBackend.h"

/**
 * @brief Simple example demonstrating VLCBackend usage
 * 
 * This example shows how to:
 * 1. Initialize the VLCBackend
 * 2. Check VLC availability
 * 3. Load and play media (if a file path is provided)
 * 4. Handle basic events
 */
class VLCExample : public QObject
{
    Q_OBJECT

public:
    VLCExample(QObject* parent = nullptr) : QObject(parent), m_backend(new VLCBackend(this))
    {
        // Connect to VLC backend signals
        connect(m_backend, &IMediaEngine::stateChanged, this, &VLCExample::onStateChanged);
        connect(m_backend, &IMediaEngine::positionChanged, this, &VLCExample::onPositionChanged);
        connect(m_backend, &IMediaEngine::durationChanged, this, &VLCExample::onDurationChanged);
        connect(m_backend, &IMediaEngine::errorOccurred, this, &VLCExample::onErrorOccurred);
        connect(m_backend, &IMediaEngine::mediaLoaded, this, &VLCExample::onMediaLoaded);
    }
    
    bool initialize()
    {
        qDebug() << "Checking VLC availability...";
        
        if (!VLCBackend::isVLCAvailable()) {
            qCritical() << "libVLC is not available on this system";
            qCritical() << "Please install VLC media player to use this application";
            return false;
        }
        
        qDebug() << "VLC Version:" << VLCBackend::vlcVersion();
        
        qDebug() << "Initializing VLC backend...";
        if (!m_backend->initialize()) {
            qCritical() << "Failed to initialize VLC backend";
            return false;
        }
        
        qDebug() << "VLC backend initialized successfully";
        return true;
    }
    
    void loadAndPlay(const QString& mediaPath)
    {
        if (mediaPath.isEmpty()) {
            qDebug() << "No media path provided. Example will just demonstrate initialization.";
            return;
        }
        
        qDebug() << "Loading media:" << mediaPath;
        
        if (m_backend->loadMedia(mediaPath)) {
            qDebug() << "Media loaded successfully, starting playback...";
            m_backend->play();
            
            // Stop after 10 seconds for demonstration
            QTimer::singleShot(10000, this, [this]() {
                qDebug() << "Stopping playback (demo timeout)";
                m_backend->stop();
                QCoreApplication::quit();
            });
        } else {
            qWarning() << "Failed to load media";
            QCoreApplication::quit();
        }
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
        // Only log every second to avoid spam
        static qint64 lastLoggedSecond = -1;
        qint64 currentSecond = position / 1000;
        
        if (currentSecond != lastLoggedSecond) {
            qDebug() << "Position:" << position << "ms (" << currentSecond << "s)";
            lastLoggedSecond = currentSecond;
        }
    }
    
    void onDurationChanged(qint64 duration)
    {
        qDebug() << "Duration:" << duration << "ms (" << (duration / 1000) << "s)";
    }
    
    void onErrorOccurred(const QString& error)
    {
        qCritical() << "Playback error:" << error;
        QCoreApplication::quit();
    }
    
    void onMediaLoaded(bool success)
    {
        qDebug() << "Media loaded:" << (success ? "Success" : "Failed");
    }

private:
    VLCBackend* m_backend;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "VLC Backend Example";
    qDebug() << "==================";
    
    VLCExample example;
    
    if (!example.initialize()) {
        return 1;
    }
    
    // Check if media path was provided
    QString mediaPath;
    if (argc > 1) {
        mediaPath = QString::fromLocal8Bit(argv[1]);
    }
    
    example.loadAndPlay(mediaPath);
    
    // If no media was provided, just exit after showing initialization
    if (mediaPath.isEmpty()) {
        qDebug() << "VLC backend initialization completed successfully";
        qDebug() << "Usage:" << argv[0] << "<media_file_path>";
        return 0;
    }
    
    return app.exec();
}

#include "vlc_backend_example.moc"