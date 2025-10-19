#include "EonPlayApplication.h"
#include "ui/MainWindow.h"
#include <QLoggingCategory>
#include <QDir>
#include <QStandardPaths>
#include <iostream>

/**
 * @brief Set up application directories and ensure they exist
 */
void setupApplicationDirectories()
{
    // Ensure config directory exists
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    
    // Ensure data directory exists
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    
    // Ensure cache directory exists
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);
}

/**
 * @brief Print application information
 */
void printApplicationInfo()
{
    std::cout << EonPlayApplication::applicationName().toStdString() << std::endl;
    std::cout << "Version: " << EonPlayApplication::version().toStdString() << std::endl;
    std::cout << "Qt Version: " << qVersion() << std::endl;
    std::cout << "Starting application..." << std::endl;
}

/**
 * @brief Main application entry point
 */
int main(int argc, char *argv[])
{
    // Enable high DPI support
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // Create application instance
    EonPlayApplication app(argc, argv);
    
    // Print application information
    printApplicationInfo();
    
    // Set up application directories
    setupApplicationDirectories();
    
    // Initialize the application
    if (!app.initialize()) {
        qCritical() << "Failed to initialize application";
        return 1;
    }
    
    // Show main window when application is ready
    QObject::connect(&app, &EonPlayApplication::applicationReady, [&app]() {
        qDebug() << "Application ready - showing main window";
        // TODO: Get MainWindow from component manager when component system is fully integrated
        // For now, create and show MainWindow directly
        static MainWindow* mainWindow = new MainWindow();
        mainWindow->initialize(app.componentManager());
        mainWindow->showWindow();
    });
    
    qDebug() << "Starting application event loop";
    
    // Start the application event loop
    int result = app.exec();
    
    qDebug() << "Application event loop finished with code:" << result;
    
    return result;
}