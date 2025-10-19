#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextEdit>
#include <QSplitter>
#include <QGroupBox>
#include <QLoggingCategory>
#include <memory>

#include "media/FileUrlSupport.h"
#include "media/VLCBackend.h"
#include "media/PlaybackController.h"
#include "ui/DragDropWidget.h"
#include "ui/MediaInfoWidget.h"
#include "core/ComponentManager.h"
#include "core/EventBus.h"

Q_LOGGING_CATEGORY(fileUrlExample, "eonplay.example.fileurlsupport")

/**
 * @brief Example application demonstrating FileUrlSupport functionality
 * 
 * This example shows how to:
 * - Load media files and URLs
 * - Handle drag and drop operations
 * - Display media information
 * - Capture screenshots
 * - Auto-detect subtitles
 * - Validate file formats
 */
class FileUrlSupportExample : public QMainWindow
{
    Q_OBJECT

public:
    FileUrlSupportExample(QWidget* parent = nullptr)
        : QMainWindow(parent)
    {
        setupUI();
        setupComponents();
        connectSignals();
        
        setWindowTitle("EonPlay - File & URL Support Example");
        resize(1000, 700);
        
        // Show usage instructions
        m_logOutput->append("=== EonPlay File & URL Support Example ===");
        m_logOutput->append("Features demonstrated:");
        m_logOutput->append("• Drag and drop media files");
        m_logOutput->append("• Load files via file dialog");
        m_logOutput->append("• Load URLs/streams");
        m_logOutput->append("• Display detailed media information");
        m_logOutput->append("• Auto-detect subtitles");
        m_logOutput->append("• Capture screenshots");
        m_logOutput->append("• File format validation");
        m_logOutput->append("");
        m_logOutput->append("Try dragging a media file onto the drop area!");
    }

private slots:
    void openFile()
    {
        QString filter = m_fileUrlSupport->getFileFilter();
        QString filePath = QFileDialog::getOpenFileName(this, "Open Media File", QString(), filter);
        
        if (!filePath.isEmpty()) {
            m_logOutput->append(QString("Loading file: %1").arg(filePath));
            m_fileUrlSupport->loadMediaFile(filePath);
        }
    }
    
    void openUrl()
    {
        QString urlText = m_urlInput->text().trimmed();
        if (urlText.isEmpty()) {
            QMessageBox::warning(this, "Warning", "Please enter a URL");
            return;
        }
        
        QUrl url(urlText);
        if (!url.isValid()) {
            QMessageBox::warning(this, "Warning", "Invalid URL format");
            return;
        }
        
        m_logOutput->append(QString("Loading URL: %1").arg(urlText));
        m_fileUrlSupport->loadMediaUrl(url);
    }
    
    void captureScreenshot()
    {
        if (!m_fileUrlSupport->hasVideo()) {
            QMessageBox::information(this, "Info", "No video content available for screenshot");
            return;
        }
        
        m_logOutput->append("Capturing screenshot...");
        QString screenshotPath = m_fileUrlSupport->captureScreenshot();
        
        if (!screenshotPath.isEmpty()) {
            m_logOutput->append(QString("Screenshot saved: %1").arg(screenshotPath));
        }
    }
    
    void showSupportedFormats()
    {
        QStringList extensions = m_fileUrlSupport->getSupportedExtensions();
        QStringList schemes = m_fileUrlSupport->getSupportedUrlSchemes();
        
        QString message = "Supported file extensions:\n";
        message += extensions.join(", ");
        message += "\n\nSupported URL schemes:\n";
        message += schemes.join(", ");
        
        QMessageBox::information(this, "Supported Formats", message);
    }
    
    void onMediaFileLoaded(const QString& filePath, const MediaInfo& mediaInfo)
    {
        m_logOutput->append(QString("✓ Media file loaded successfully: %1").arg(filePath));
        m_logOutput->append(QString("Media info: %1").arg(mediaInfo.toString()));
        
        updateButtons();
    }
    
    void onMediaUrlLoaded(const QUrl& url, const MediaInfo& mediaInfo)
    {
        m_logOutput->append(QString("✓ Media URL loaded successfully: %1").arg(url.toString()));
        m_logOutput->append(QString("Media info: %1").arg(mediaInfo.toString()));
        
        updateButtons();
    }
    
    void onFileValidationFailed(const QString& filePath, ValidationResult result, const QString& errorMessage)
    {
        m_logOutput->append(QString("✗ File validation failed: %1").arg(filePath));
        m_logOutput->append(QString("Error: %1").arg(errorMessage));
        
        QMessageBox::warning(this, "File Validation Failed", errorMessage);
    }
    
    void onUrlValidationFailed(const QUrl& url, const QString& errorMessage)
    {
        m_logOutput->append(QString("✗ URL validation failed: %1").arg(url.toString()));
        m_logOutput->append(QString("Error: %1").arg(errorMessage));
        
        QMessageBox::warning(this, "URL Validation Failed", errorMessage);
    }
    
    void onSubtitlesDetected(const QString& mediaPath, const QStringList& subtitlePaths)
    {
        m_logOutput->append(QString("Found %1 subtitle files for: %2").arg(subtitlePaths.size()).arg(mediaPath));
        for (const QString& subtitle : subtitlePaths) {
            m_logOutput->append(QString("  - %1").arg(subtitle));
        }
    }
    
    void onScreenshotCaptured(const QString& screenshotPath, qint64 position)
    {
        m_logOutput->append(QString("✓ Screenshot captured at %1ms: %2").arg(position).arg(screenshotPath));
    }
    
    void onScreenshotCaptureFailed(const QString& errorMessage)
    {
        m_logOutput->append(QString("✗ Screenshot capture failed: %1").arg(errorMessage));
        QMessageBox::warning(this, "Screenshot Failed", errorMessage);
    }
    
    void onFilesDropped(const QStringList& processedFiles, const QStringList& failedFiles)
    {
        m_logOutput->append(QString("Drag & Drop completed:"));
        m_logOutput->append(QString("  Processed: %1 files").arg(processedFiles.size()));
        m_logOutput->append(QString("  Failed: %1 files").arg(failedFiles.size()));
        
        for (const QString& file : processedFiles) {
            m_logOutput->append(QString("  ✓ %1").arg(file));
        }
        
        for (const QString& file : failedFiles) {
            m_logOutput->append(QString("  ✗ %1").arg(file));
        }
    }

private:
    void setupUI()
    {
        auto* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        auto* mainLayout = new QVBoxLayout(centralWidget);
        
        // Create splitter for main content
        auto* splitter = new QSplitter(Qt::Horizontal, this);
        mainLayout->addWidget(splitter);
        
        // Left panel - Controls and drag/drop
        auto* leftPanel = new QWidget();
        leftPanel->setMaximumWidth(400);
        leftPanel->setMinimumWidth(300);
        
        auto* leftLayout = new QVBoxLayout(leftPanel);
        
        // File operations group
        auto* fileGroup = new QGroupBox("File Operations");
        auto* fileLayout = new QVBoxLayout(fileGroup);
        
        auto* openFileBtn = new QPushButton("Open File...");
        connect(openFileBtn, &QPushButton::clicked, this, &FileUrlSupportExample::openFile);
        fileLayout->addWidget(openFileBtn);
        
        // URL input
        auto* urlLayout = new QHBoxLayout();
        m_urlInput = new QLineEdit();
        m_urlInput->setPlaceholderText("Enter URL (http://, rtsp://, etc.)");
        auto* openUrlBtn = new QPushButton("Open URL");
        connect(openUrlBtn, &QPushButton::clicked, this, &FileUrlSupportExample::openUrl);
        
        urlLayout->addWidget(m_urlInput);
        urlLayout->addWidget(openUrlBtn);
        fileLayout->addLayout(urlLayout);
        
        leftLayout->addWidget(fileGroup);
        
        // Drag and drop area
        auto* dropGroup = new QGroupBox("Drag & Drop Area");
        auto* dropLayout = new QVBoxLayout(dropGroup);
        
        m_dragDropWidget = new DragDropWidget();
        m_dragDropWidget->setMinimumHeight(150);
        m_dragDropWidget->setStyleSheet(
            "DragDropWidget { "
            "  border: 2px dashed #aaa; "
            "  border-radius: 8px; "
            "  background-color: #f9f9f9; "
            "} "
            "DragDropWidget:hover { "
            "  border-color: #0078d4; "
            "  background-color: #f0f8ff; "
            "}"
        );
        
        auto* dropLabel = new QLabel("Drag media files here");
        dropLabel->setAlignment(Qt::AlignCenter);
        dropLabel->setStyleSheet("color: #666; font-size: 14px;");
        
        auto* dragLayout = new QVBoxLayout(m_dragDropWidget);
        dragLayout->addWidget(dropLabel);
        
        dropLayout->addWidget(m_dragDropWidget);
        leftLayout->addWidget(dropGroup);
        
        // Media operations group
        auto* mediaGroup = new QGroupBox("Media Operations");
        auto* mediaLayout = new QVBoxLayout(mediaGroup);
        
        m_screenshotBtn = new QPushButton("Capture Screenshot");
        m_screenshotBtn->setEnabled(false);
        connect(m_screenshotBtn, &QPushButton::clicked, this, &FileUrlSupportExample::captureScreenshot);
        mediaLayout->addWidget(m_screenshotBtn);
        
        auto* formatsBtn = new QPushButton("Show Supported Formats");
        connect(formatsBtn, &QPushButton::clicked, this, &FileUrlSupportExample::showSupportedFormats);
        mediaLayout->addWidget(formatsBtn);
        
        leftLayout->addWidget(mediaGroup);
        leftLayout->addStretch();
        
        splitter->addWidget(leftPanel);
        
        // Right panel - Media info and log
        auto* rightPanel = new QWidget();
        auto* rightLayout = new QVBoxLayout(rightPanel);
        
        // Media info widget
        m_mediaInfoWidget = new MediaInfoWidget();
        m_mediaInfoWidget->setShowTechnicalDetails(true);
        rightLayout->addWidget(m_mediaInfoWidget);
        
        // Log output
        auto* logGroup = new QGroupBox("Log Output");
        auto* logLayout = new QVBoxLayout(logGroup);
        
        m_logOutput = new QTextEdit();
        m_logOutput->setMaximumHeight(200);
        m_logOutput->setFont(QFont("Consolas", 9));
        logLayout->addWidget(m_logOutput);
        
        rightLayout->addWidget(logGroup);
        
        splitter->addWidget(rightPanel);
        
        // Set splitter proportions
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
    }
    
    void setupComponents()
    {
        // Create component manager
        m_componentManager = std::make_unique<ComponentManager>();
        
        // Create and initialize VLC backend
        m_vlcBackend = std::make_shared<VLCBackend>();
        if (!m_vlcBackend->initialize()) {
            QMessageBox::critical(this, "Error", "Failed to initialize VLC backend");
            return;
        }
        
        // Create and initialize playback controller
        m_playbackController = std::make_shared<PlaybackController>();
        m_playbackController->setMediaEngine(m_vlcBackend);
        if (!m_playbackController->initialize()) {
            QMessageBox::critical(this, "Error", "Failed to initialize playback controller");
            return;
        }
        
        // Create and initialize file URL support
        m_fileUrlSupport = std::make_shared<FileUrlSupport>();
        m_fileUrlSupport->setMediaEngine(m_vlcBackend);
        m_fileUrlSupport->setPlaybackController(m_playbackController);
        if (!m_fileUrlSupport->initialize()) {
            QMessageBox::critical(this, "Error", "Failed to initialize file URL support");
            return;
        }
        
        // Connect components to UI
        m_dragDropWidget->setFileUrlSupport(m_fileUrlSupport);
        m_mediaInfoWidget->setFileUrlSupport(m_fileUrlSupport);
        
        qCDebug(fileUrlExample) << "All components initialized successfully";
    }
    
    void connectSignals()
    {
        // FileUrlSupport signals
        connect(m_fileUrlSupport.get(), &FileUrlSupport::mediaFileLoaded,
                this, &FileUrlSupportExample::onMediaFileLoaded);
        connect(m_fileUrlSupport.get(), &FileUrlSupport::mediaUrlLoaded,
                this, &FileUrlSupportExample::onMediaUrlLoaded);
        connect(m_fileUrlSupport.get(), &FileUrlSupport::fileValidationFailed,
                this, &FileUrlSupportExample::onFileValidationFailed);
        connect(m_fileUrlSupport.get(), &FileUrlSupport::urlValidationFailed,
                this, &FileUrlSupportExample::onUrlValidationFailed);
        connect(m_fileUrlSupport.get(), &FileUrlSupport::subtitlesDetected,
                this, &FileUrlSupportExample::onSubtitlesDetected);
        connect(m_fileUrlSupport.get(), &FileUrlSupport::screenshotCaptured,
                this, &FileUrlSupportExample::onScreenshotCaptured);
        connect(m_fileUrlSupport.get(), &FileUrlSupport::screenshotCaptureFailed,
                this, &FileUrlSupportExample::onScreenshotCaptureFailed);
        
        // DragDropWidget signals
        connect(m_dragDropWidget, &DragDropWidget::filesDropped,
                this, &FileUrlSupportExample::onFilesDropped);
        
        // URL input enter key
        connect(m_urlInput, &QLineEdit::returnPressed, this, &FileUrlSupportExample::openUrl);
    }
    
    void updateButtons()
    {
        bool hasVideo = m_fileUrlSupport->hasVideo();
        m_screenshotBtn->setEnabled(hasVideo);
    }

private:
    std::unique_ptr<ComponentManager> m_componentManager;
    std::shared_ptr<VLCBackend> m_vlcBackend;
    std::shared_ptr<PlaybackController> m_playbackController;
    std::shared_ptr<FileUrlSupport> m_fileUrlSupport;
    
    // UI components
    DragDropWidget* m_dragDropWidget;
    MediaInfoWidget* m_mediaInfoWidget;
    QLineEdit* m_urlInput;
    QPushButton* m_screenshotBtn;
    QTextEdit* m_logOutput;
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("EonPlay File & URL Support Example");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("EonPlay");
    
    // Enable logging
    QLoggingCategory::setFilterRules("eonplay.*.debug=true");
    
    // Create and show main window
    FileUrlSupportExample window;
    window.show();
    
    return app.exec();
}

#include "file_url_support_example.moc"