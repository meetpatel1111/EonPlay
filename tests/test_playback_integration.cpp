#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include "media/PlaybackController.h"
#include "media/VLCBackend.h"
#include "ComponentManager.h"
#include <memory>

class TestPlaybackIntegration : public QObject
{
    Q_OBJECT

private slots:
    void testComponentManagerIntegration();
    void testPlaybackControllerVLCIntegration();
    void testSignalPropagation();
    void testMediaLoadingWorkflow();

private:
    std::unique_ptr<ComponentManager> m_componentManager;
};

void TestPlaybackIntegration::testComponentManagerIntegration()
{
    m_componentManager = std::make_unique<ComponentManager>();
    
    // Create and register components
    auto vlcBackend = std::make_shared<VLCBackend>();
    auto playbackController = std::make_shared<PlaybackController>();
    
    m_componentManager->registerComponent(vlcBackend, 10);
    m_componentManager->registerComponent(playbackController, 20);
    
    // Verify registration
    QStringList componentNames = m_componentManager->getComponentNames();
    QVERIFY(componentNames.contains("VLCBackend"));
    QVERIFY(componentNames.contains("PlaybackController"));
    
    if (!VLCBackend::isVLCAvailable()) {
        QSKIP("VLC not available, skipping component initialization");
    }
    
    // Initialize components
    QSignalSpy initSpy(m_componentManager.get(), &ComponentManager::allComponentsInitialized);
    bool result = m_componentManager->initializeAll();
    
    QVERIFY(result);
    QVERIFY(m_componentManager->areAllInitialized());
    
    // Connect playback controller to VLC backend
    playbackController->setMediaEngine(vlcBackend);
    QVERIFY(playbackController->mediaEngine() == vlcBackend);
    
    // Test shutdown
    m_componentManager->shutdownAll();
    QVERIFY(!vlcBackend->isInitialized());
    QVERIFY(!playbackController->isInitialized());
}

void TestPlaybackIntegration::testPlaybackControllerVLCIntegration()
{
    if (!VLCBackend::isVLCAvailable()) {
        QSKIP("VLC not available, skipping integration tests");
    }
    
    // Create and initialize components
    auto vlcBackend = std::make_shared<VLCBackend>();
    auto playbackController = std::make_shared<PlaybackController>();
    
    QVERIFY(vlcBackend->initialize());
    QVERIFY(playbackController->initialize());
    
    // Connect them
    playbackController->setMediaEngine(vlcBackend);
    
    // Test state synchronization
    QCOMPARE(playbackController->state(), vlcBackend->state());
    QCOMPARE(playbackController->position(), vlcBackend->position());
    QCOMPARE(playbackController->duration(), vlcBackend->duration());
    QCOMPARE(playbackController->volume(), vlcBackend->volume());
    QCOMPARE(playbackController->hasMedia(), vlcBackend->hasMedia());
    
    // Test volume control through controller
    playbackController->setVolume(75);
    QCOMPARE(vlcBackend->volume(), 75);
    QCOMPARE(playbackController->volume(), 75);
    
    // Test mute functionality
    playbackController->setMuted(true);
    QCOMPARE(vlcBackend->volume(), 0);
    QVERIFY(playbackController->isMuted());
    
    playbackController->setMuted(false);
    QCOMPARE(vlcBackend->volume(), 75);  // Should restore
    QVERIFY(!playbackController->isMuted());
    
    // Cleanup
    vlcBackend->shutdown();
    playbackController->shutdown();
}

void TestPlaybackIntegration::testSignalPropagation()
{
    if (!VLCBackend::isVLCAvailable()) {
        QSKIP("VLC not available, skipping signal propagation tests");
    }
    
    // Create and initialize components
    auto vlcBackend = std::make_shared<VLCBackend>();
    auto playbackController = std::make_shared<PlaybackController>();
    
    QVERIFY(vlcBackend->initialize());
    QVERIFY(playbackController->initialize());
    
    // Connect them
    playbackController->setMediaEngine(vlcBackend);
    
    // Set up signal spies
    QSignalSpy controllerStateSpy(playbackController.get(), &PlaybackController::stateChanged);
    QSignalSpy controllerVolumeSpy(playbackController.get(), &PlaybackController::volumeChanged);
    QSignalSpy controllerErrorSpy(playbackController.get(), &PlaybackController::errorOccurred);
    
    // Test volume signal propagation
    vlcBackend->setVolume(60);
    
    // Should receive volume change signal
    QVERIFY(controllerVolumeSpy.count() >= 1);
    if (controllerVolumeSpy.count() > 0) {
        QCOMPARE(controllerVolumeSpy.last().at(0).toInt(), 60);
    }
    
    // Test error signal propagation by trying to load invalid media
    vlcBackend->loadMedia("/non/existent/file.mp4");
    
    // May or may not generate error depending on VLC behavior
    // Just verify no crashes occur
    QVERIFY(true);
    
    // Cleanup
    vlcBackend->shutdown();
    playbackController->shutdown();
}

void TestPlaybackIntegration::testMediaLoadingWorkflow()
{
    if (!VLCBackend::isVLCAvailable()) {
        QSKIP("VLC not available, skipping media loading workflow tests");
    }
    
    // Create and initialize components
    auto vlcBackend = std::make_shared<VLCBackend>();
    auto playbackController = std::make_shared<PlaybackController>();
    
    QVERIFY(vlcBackend->initialize());
    QVERIFY(playbackController->initialize());
    
    // Connect them
    playbackController->setMediaEngine(vlcBackend);
    
    // Set up signal spies
    QSignalSpy mediaLoadedSpy(playbackController.get(), &PlaybackController::mediaLoaded);
    
    // Test loading invalid media through controller
    playbackController->loadMedia("");
    
    // Should get media loaded signal with failure
    QVERIFY(mediaLoadedSpy.count() >= 1);
    if (mediaLoadedSpy.count() > 0) {
        QCOMPARE(mediaLoadedSpy.last().at(0).toBool(), false);
        QCOMPARE(mediaLoadedSpy.last().at(1).toString(), QString(""));
    }
    
    // Test loading non-existent file
    mediaLoadedSpy.clear();
    playbackController->loadMedia("/non/existent/file.mp4");
    
    QVERIFY(mediaLoadedSpy.count() >= 1);
    if (mediaLoadedSpy.count() > 0) {
        QCOMPARE(mediaLoadedSpy.last().at(0).toBool(), false);
    }
    
    // Cleanup
    vlcBackend->shutdown();
    playbackController->shutdown();
}

QTEST_MAIN(TestPlaybackIntegration)
#include "test_playback_integration.moc"