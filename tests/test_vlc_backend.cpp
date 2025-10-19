#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include "media/VLCBackend.h"

class TestVLCBackend : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testInitialization();
    void testVLCAvailability();
    void testComponentInterface();
    void testMediaEngineInterface();
    void testInvalidMediaHandling();

private:
    VLCBackend* m_backend;
};

void TestVLCBackend::initTestCase()
{
    m_backend = new VLCBackend(this);
}

void TestVLCBackend::cleanupTestCase()
{
    delete m_backend;
    m_backend = nullptr;
}

void TestVLCBackend::testInitialization()
{
    // Test initial state
    QVERIFY(!m_backend->isInitialized());
    QCOMPARE(m_backend->componentName(), QString("VLCBackend"));
    
    // Test initialization
    bool initResult = m_backend->initialize();
    
    // Only test if VLC is available on the system
    if (VLCBackend::isVLCAvailable()) {
        QVERIFY(initResult);
        QVERIFY(m_backend->isInitialized());
    } else {
        QVERIFY(!initResult);
        QVERIFY(!m_backend->isInitialized());
        QSKIP("libVLC not available on this system");
    }
}

void TestVLCBackend::testVLCAvailability()
{
    // Test static methods
    bool available = VLCBackend::isVLCAvailable();
    
    if (available) {
        QString version = VLCBackend::vlcVersion();
        QVERIFY(!version.isEmpty());
        qDebug() << "VLC Version:" << version;
    } else {
        qDebug() << "VLC not available on this system";
    }
}

void TestVLCBackend::testComponentInterface()
{
    // Test IComponent interface
    QCOMPARE(m_backend->componentName(), QString("VLCBackend"));
    
    // Test shutdown
    m_backend->shutdown();
    QVERIFY(!m_backend->isInitialized());
}

void TestVLCBackend::testMediaEngineInterface()
{
    if (!VLCBackend::isVLCAvailable()) {
        QSKIP("libVLC not available on this system");
    }
    
    // Initialize backend
    QVERIFY(m_backend->initialize());
    
    // Test initial state
    QCOMPARE(m_backend->state(), PlaybackState::Stopped);
    QCOMPARE(m_backend->position(), 0LL);
    QCOMPARE(m_backend->duration(), 0LL);
    QCOMPARE(m_backend->volume(), 100);
    QVERIFY(!m_backend->hasMedia());
    
    // Test volume control
    QSignalSpy volumeSpy(m_backend, &IMediaEngine::volumeChanged);
    m_backend->setVolume(50);
    QCOMPARE(m_backend->volume(), 50);
    
    // Test invalid volume values
    m_backend->setVolume(-10);
    QCOMPARE(m_backend->volume(), 0);
    
    m_backend->setVolume(150);
    QCOMPARE(m_backend->volume(), 100);
}

void TestVLCBackend::testInvalidMediaHandling()
{
    if (!VLCBackend::isVLCAvailable()) {
        QSKIP("libVLC not available on this system");
    }
    
    // Initialize backend
    QVERIFY(m_backend->initialize());
    
    // Test empty path
    QSignalSpy errorSpy(m_backend, &IMediaEngine::errorOccurred);
    bool result = m_backend->loadMedia("");
    QVERIFY(!result);
    
    // Test non-existent file
    result = m_backend->loadMedia("/non/existent/file.mp4");
    QVERIFY(!result);
    
    // Test invalid extension
    QTemporaryFile tempFile;
    tempFile.setFileTemplate("test_XXXXXX.invalid");
    if (tempFile.open()) {
        result = m_backend->loadMedia(tempFile.fileName());
        QVERIFY(!result);
    }
}

QTEST_MAIN(TestVLCBackend)
#include "test_vlc_backend.moc"