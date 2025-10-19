#include "MediaPlayerApplication.h"
#include "ComponentManager.h"
#include "EventBus.h"
#include <QDir>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(mediaPlayerApp, "mediaplayer.application")

MediaPlayerApplication::MediaPlayerApplication(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_componentManager(std::make_unique<ComponentManager>(this))
    , m_initialized(false)
{
    setupApplicationProperties();
    setupLogging();
    
    qCDebug(mediaPlayerApp) << "MediaPlayerApplication created";
    qCDebug(mediaPlayerApp) << "Version:" << version();
    qCDebug(mediaPlayerApp) << "Qt Version:" << qVersion();
    
    // Connect component manager signals
    connect(m_componentManager.get(), &ComponentManager::allComponentsInitialized,
            this, &MediaPlayerApplication::onAllComponentsInitialized);
    connect(m_componentManager.get(), &ComponentManager::componentInitializationFailed,
            this, &MediaPlayerApplication::onComponentInitializationFailed);
    
    // Handle application quit
    connect(this, &QApplication::aboutToQuit, this, &MediaPlayerApplication::shutdown);
}

MediaPlayerApplication::~MediaPlayerApplication()
{
    qCDebug(mediaPlayerApp) << "MediaPlayerApplication destroyed";
}

bool MediaPlayerApplication::initialize()
{
    if (m_initialized) {
        qCWarning(mediaPlayerApp) << "Application already initialized";
        return true;
    }
    
    qCDebug(mediaPlayerApp) << "Initializing MediaPlayerApplication";
    
    // Register all components
    registerComponents();
    
    // Initialize all components
    if (!m_componentManager->initializeAll()) {
        qCCritical(mediaPlayerApp) << "Failed to initialize application components";
        return false;
    }
    
    m_initialized = true;
    qCDebug(mediaPlayerApp) << "MediaPlayerApplication initialized successfully";
    
    return true;
}

void MediaPlayerApplication::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    qCDebug(mediaPlayerApp) << "Shutting down MediaPlayerApplication";
    emit applicationShuttingDown();
    
    // Shutdown all components
    if (m_componentManager) {
        m_componentManager->shutdownAll();
    }
    
    // Clear event bus
    EventBus::instance().clear();
    
    m_initialized = false;
    qCDebug(mediaPlayerApp) << "MediaPlayerApplication shutdown complete";
}

bool MediaPlayerApplication::notify(QObject* receiver, QEvent* event)
{
    try {
        return QApplication::notify(receiver, event);
    } catch (const std::exception& e) {
        qCCritical(mediaPlayerApp) << "Exception in event handling:" << e.what();
        return false;
    } catch (...) {
        qCCritical(mediaPlayerApp) << "Unknown exception in event handling";
        return false;
    }
}

void MediaPlayerApplication::onAllComponentsInitialized()
{
    qCDebug(mediaPlayerApp) << "All components initialized, application ready";
    emit applicationReady();
}

void MediaPlayerApplication::onComponentInitializationFailed(const QString& componentName, const QString& error)
{
    qCCritical(mediaPlayerApp) << "Component initialization failed:" << componentName << "-" << error;
    
    // For now, we'll continue even if some components fail
    // In a production app, you might want to show an error dialog or exit
    QTimer::singleShot(0, this, [this]() {
        emit applicationReady();
    });
}

void MediaPlayerApplication::setupLogging()
{
    // Set up logging rules
    QLoggingCategory::setFilterRules(
        "mediaplayer.*.debug=true\n"
        "qt.*.debug=false"
    );
    
    qCDebug(mediaPlayerApp) << "Logging configured";
}

void MediaPlayerApplication::registerComponents()
{
    qCDebug(mediaPlayerApp) << "Registering application components";
    
    // Components will be registered here as they are implemented
    // For now, we'll just log that registration is ready
    
    qCDebug(mediaPlayerApp) << "Component registration complete";
}

void MediaPlayerApplication::setupApplicationProperties()
{
    setApplicationName(applicationName());
    setApplicationVersion(version());
    setOrganizationName("MediaPlayer Team");
    setOrganizationDomain("mediaplayer.com");
    
    // Set application icon (will be set when icon is available)
    // setWindowIcon(QIcon(":/icons/app_icon.png"));
    
    qCDebug(mediaPlayerApp) << "Application properties configured";
}