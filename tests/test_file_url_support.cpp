#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QUrl>
#include <memory>

#include "media/FileUrlSupport.h"
#include "media/VLCBackend.h"

/**
 * @brief Test class for FileUrlSupport functionality
 */
class TestFileUrlSupport : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Initialize VLC backend
        m_vlcBackend = std::make_shared<VLCBackend>();
        QVERIFY(m_vlcBackend->initialize());
        
        // Initialize FileUrlSupport
        m_fileUrlSupport = std::make_unique<FileUrlSupport>();
        m_fileUrlSupport->setMediaEngine(m_vlcBackend);
        QVERIFY(m_fileUrlSupport->initialize());
    }
    
    void cleanupTestCase()
    {
        m_fileUrlSupport.reset();
        m_vlcBackend.reset();
    }
    
    void testSupportedExtensions()
    {
        QStringList extensions = m_fileUrlSupport->getSupportedExtensions();
        
        // Check that common formats are supported
        QVERIFY(extensions.contains("mp4"));
        QVERIFY(extensions.contains("avi"));
        QVERIFY(extensions.contains("mkv"));
        QVERIFY(extensions.contains("mp3"));
        QVERIFY(extensions.contains("flac"));
        QVERIFY(extensions.contains("wav"));
        
        // Should have a reasonable number of formats
        QVERIFY(extensions.size() > 20);
    }
    
    void testSupportedUrlSchemes()
    {
        QStringList schemes = m_fileUrlSupport->getSupportedUrlSchemes();
        
        // Check that common schemes are supported
        QVERIFY(schemes.contains("http"));
        QVERIFY(schemes.contains("https"));
        QVERIFY(schemes.contains("rtsp"));
        QVERIFY(schemes.contains("file"));
        
        // Should have a reasonable number of schemes
        QVERIFY(schemes.size() > 5);
    }
    
    void testFileFilter()
    {
        QString filter = m_fileUrlSupport->getFileFilter();
        
        // Should contain common patterns
        QVERIFY(filter.contains("*.mp4"));
        QVERIFY(filter.contains("*.mp3"));
        QVERIFY(filter.contains("All Supported Media"));
        QVERIFY(filter.contains("Video Files"));
        QVERIFY(filter.contains("Audio Files"));
    }
    
    void testUrlValidation()
    {
        // Valid URLs
        QVERIFY(m_fileUrlSupport->isValidStreamUrl(QUrl("http://example.com/stream")));
        QVERIFY(m_fileUrlSupport->isValidStreamUrl(QUrl("https://example.com/video.mp4")));
        QVERIFY(m_fileUrlSupport->isValidStreamUrl(QUrl("rtsp://example.com/stream")));
        QVERIFY(m_fileUrlSupport->isValidStreamUrl(QUrl("file:///path/to/file.mp4")));
        
        // Invalid URLs
        QVERIFY(!m_fileUrlSupport->isValidStreamUrl(QUrl("invalid://example.com")));
        QVERIFY(!m_fileUrlSupport->isValidStreamUrl(QUrl("mailto:test@example.com")));
        QVERIFY(!m_fileUrlSupport->isValidStreamUrl(QUrl()));
    }
    
    void testLocalFileUrl()
    {
        QVERIFY(m_fileUrlSupport->isLocalFileUrl(QUrl("file:///path/to/file.mp4")));
        QVERIFY(!m_fileUrlSupport->isLocalFileUrl(QUrl("http://example.com/file.mp4")));
        QVERIFY(!m_fileUrlSupport->isLocalFileUrl(QUrl("rtsp://example.com/stream")));
    }
    
    void testFileValidation()
    {
        // Test with non-existent file
        ValidationResult result = m_fileUrlSupport->validateMediaFile("/non/existent/file.mp4");
        QCOMPARE(result, ValidationResult::FileNotFound);
        
        // Test with empty path
        result = m_fileUrlSupport->validateMediaFile("");
        QCOMPARE(result, ValidationResult::FileNotFound);
        
        // Test error message generation
        QString errorMsg = m_fileUrlSupport->getValidationErrorMessage(
            ValidationResult::UnsupportedFormat, "test.xyz");
        QVERIFY(errorMsg.contains("Unsupported file format"));
        QVERIFY(errorMsg.contains("XYZ"));
    }
    
    void testSubtitleExtensions()
    {
        QStringList subtitleExts = m_fileUrlSupport->getSupportedSubtitleExtensions();
        
        // Check common subtitle formats
        QVERIFY(subtitleExts.contains("srt"));
        QVERIFY(subtitleExts.contains("ass"));
        QVERIFY(subtitleExts.contains("ssa"));
        QVERIFY(subtitleExts.contains("vtt"));
        
        // Should have multiple formats
        QVERIFY(subtitleExts.size() > 5);
    }
    
    void testSubtitleMatching()
    {
        // Test exact match
        QVERIFY(m_fileUrlSupport->isSubtitleMatch(
            "/path/movie.mp4", "/path/movie.srt"));
        
        // Test language suffix match
        QVERIFY(m_fileUrlSupport->isSubtitleMatch(
            "/path/movie.mp4", "/path/movie.en.srt"));
        
        // Test no match
        QVERIFY(!m_fileUrlSupport->isSubtitleMatch(
            "/path/movie.mp4", "/path/different.srt"));
    }
    
    void testScreenshotSettings()
    {
        // Test screenshot directory
        QString originalDir = m_fileUrlSupport->getScreenshotDirectory();
        m_fileUrlSupport->setScreenshotDirectory("/tmp/screenshots");
        QCOMPARE(m_fileUrlSupport->getScreenshotDirectory(), QString("/tmp/screenshots"));
        m_fileUrlSupport->setScreenshotDirectory(originalDir);
        
        // Test screenshot format
        QString originalFormat = m_fileUrlSupport->getScreenshotFormat();
        m_fileUrlSupport->setScreenshotFormat("JPG");
        QCOMPARE(m_fileUrlSupport->getScreenshotFormat(), QString("JPG"));
        m_fileUrlSupport->setScreenshotFormat(originalFormat);
    }
    
    void testMediaInfoStructure()
    {
        MediaInfo info;
        
        // Test initial state
        QVERIFY(!info.isValid);
        QVERIFY(!info.isComplete());
        
        // Test clear
        info.title = "Test";
        info.duration = 1000;
        info.clear();
        QVERIFY(info.title.isEmpty());
        QCOMPARE(info.duration, 0LL);
        
        // Test complete info
        info.isValid = true;
        info.filePath = "/test/file.mp4";
        info.duration = 120000;
        info.hasVideo = true;
        QVERIFY(info.isComplete());
        
        // Test toString
        QString infoStr = info.toString();
        QVERIFY(!infoStr.isEmpty());
        QVERIFY(infoStr != "Invalid media");
    }

private:
    std::shared_ptr<VLCBackend> m_vlcBackend;
    std::unique_ptr<FileUrlSupport> m_fileUrlSupport;
};

QTEST_MAIN(TestFileUrlSupport)
#include "test_file_url_support.moc"