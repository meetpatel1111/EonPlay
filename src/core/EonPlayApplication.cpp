#include "EonPlayApplication.h"
#include "ComponentManager.h"
#include "EventBus.h"
#include <QDir>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(eonPlayApp, "eonplay.application")

EonPlayApplication::EonPlayApplication(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_componentManager(std::make_unique<ComponentManager>(this))
    , m_initialized(false)
{
    setupApplicationProperties();
    setupLogging();
    
    qCDebug(eonPlayApp) << "EonPlay Application created - Timeless, futuristic; play for eons";
    qCDebug(eonPlayApp) << "Version:" << version();
    qCDebug(eonPlayApp) << "Qt Version:" << qVersion();
    
    // Connect component manager signals
    connect(m_componentManager.get(), &ComponentManager::allComponentsInitialized,
            this, &EonPlayApplication::onAllComponentsInitialized);
    connect(m_componentManager.get(), &ComponentManager::componentInitializationFailed,
            this, &EonPlayApplication::onComponentInitializationFailed);
    
    // Handle application quit
    connect(this, &QApplication::aboutToQuit, this, &EonPlayApplication::shutdown);
}

EonPlayApplication::~EonPlayApplication()
{
    qCDebug(eonPlayApp) << "EonPlay Application destroyed";
}

bool EonPlayApplication::initialize()
{
    if (m_initialized) {
        qCWarning(eonPlayApp) << "EonPlay Application already initialized";
        return true;
    }
    
    qCDebug(eonPlayApp) << "Initializing EonPlay Application";
    
    // Register all components
    registerComponents();
    
    // Initialize all components
    if (!m_componentManager->initializeAll()) {
        qCCritical(eonPlayApp) << "Failed to initialize EonPlay components";
        return false;
    }
    
    m_initialized = true;
    qCDebug(eonPlayApp) << "EonPlay Application initialized successfully";
    
    return true;
}

void EonPlayApplication::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    qCDebug(eonPlayApp) << "Shutting down EonPlay Application";
    emit applicationShuttingDown();
    
    // Shutdown all components
    if (m_componentManager) {
        m_componentManager->shutdownAll();
    }
    
    // Clear event bus
    EventBus::instance().clear();
    
    m_initialized = false;
    qCDebug(eonPlayApp) << "EonPlay Application shutdown complete";
}

bool EonPlayApplication::notify(QObject* receiver, QEvent* event)
{
    try {
        return QApplication::notify(receiver, event);
    } catch (const std::exception& e) {
        qCCritical(eonPlayApp) << "Exception in event handling:" << e.what();
        return false;
    } catch (...) {
        qCCritical(eonPlayApp) << "Unknown exception in event handling";
        return false;
    }
}

QString EonPlayApplication::applicationName()
{
    return "EonPlay";
}

QString EonPlayApplication::version()
{
    return "1.0.0";
}

QString EonPlayApplication::description()
{
    return "Timeless, futuristic media player - play for eons";
}

void EonPlayApplication::onAllComponentsInitialized()
{
    qCDebug(eonPlayApp) << "All EonPlay components initialized, application ready";
    emit applicationReady();
}

void EonPlayApplication::onComponentInitializationFailed(const QString& componentName, const QString& error)
{
    qCCritical(eonPlayApp) << "EonPlay component initialization failed:" << componentName << "-" << error;
    
    // For now, we'll continue even if some components fail
    // In a production app, you might want to show an error dialog or exit
    QTimer::singleShot(0, this, [this]() {
        emit applicationReady();
    });
}

void EonPlayApplication::setupLogging()
{
    // Set up logging rules for EonPlay
    QLoggingCategory::setFilterRules(
        "eonplay.*.debug=true\n"
        "qt.*.debug=false"
    );
    
    qCDebug(eonPlayApp) << "EonPlay logging configured";
}

void EonPlayApplication::registerComponents()
{
    qCDebug(eonPlayApp) << "Registering EonPlay components";
    
    // Components will be registered here as they are implemented
    // For now, we'll just log that registration is ready
    
    qCDebug(eonPlayApp) << "EonPlay component registration complete";
}

void EonPlayApplication::setupApplicationProperties()
{
    setApplicationName(applicationName());
    setApplicationVersion(version());
    setOrganizationName("EonPlay Team");
    setOrganizationDomain("eonplay.com");
    
    // Set application icon (will be set when icon is available)
    // setWindowIcon(QIcon(":/icons/eonplay_icon.png"));
    
    qCDebug(eonPlayApp) << "EonPlay application properties configured";
}