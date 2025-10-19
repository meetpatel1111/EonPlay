#include <QtTest/QtTest>
#include <QSignalSpy>
#include "media/PlaybackController.h"
#include "media/VLCBackend.h"
#include <memory>

class TestPlaybackController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testInitialization();
    void testComponentInterface();
    void testMediaEngineIntegration();
    void testPlaybackControls();
    void testSeekingOperations();
    void testVolumeControl();
    void testMuteControl();
    void testPlaybackSpeed();
    void testPositionReporting();
    void testErrorHandling();
    void testAdvancedPlaybackFeatures();
    void testFastSeekOperations();
    void testPlaybackModes();
    void testCrossfadeSettings();
    void testResumePositions();
    void testSeekThumbnails();

private:
    PlaybackController* m_controller;
    std::shared_ptr<VLCBackend> m_vlcBackend;
};

void TestPlaybackController::initTestCase()
{
    m_controller = new PlaybackController(this);
    
    // Create VLC backend if available
    if (VLCBackend::isVLCAvailable()) {
        m_vlcBackend = std::make_shared<VLCBackend>();
        m_vlcBackend->initialize();
    }
}

void TestPlaybackController::cleanupTestCase()
{
    if (m_vlcBackend) {
        m_vlcBackend->shutdown();
        m_vlcBackend.reset();
    }
    
    delete m_controller;
    m_controller = nullptr;
}

void TestPlaybackController::testInitialization()
{
    // Test initial state
    QVERIFY(!m_controller->isInitialized());
    QCOMPARE(m_controller->componentName(), QString("PlaybackController"));
    
    // Test initialization
    QVERIFY(m_controller->initialize());
    QVERIFY(m_controller->isInitialized());
    
    // Test double initialization
    QVERIFY(m_controller->initialize());
    QVERIFY(m_controller->isInitialized());
}

void TestPlaybackController::testComponentInterface()
{
    // Test IComponent interface
    QCOMPARE(m_controller->componentName(), QString("PlaybackController"));
    
    // Test shutdown
    m_controller->shutdown();
    QVERIFY(!m_controller->isInitialized());
    
    // Re-initialize for other tests
    QVERIFY(m_controller->initialize());
}

void TestPlaybackController::testMediaEngineIntegration()
{
    // Test without media engine
    QVERIFY(m_controller->mediaEngine() == nullptr);
    QCOMPARE(m_controller->state(), PlaybackState::Stopped);
    QCOMPARE(m_controller->position(), 0LL);
    QCOMPARE(m_controller->duration(), 0LL);
    QCOMPARE(m_controller->volume(), 0);
    QVERIFY(!m_controller->hasMedia());
    
    if (!m_vlcBackend) {
        QSKIP("VLC not available, skipping media engine integration tests");
    }
    
    // Set media engine
    m_controller->setMediaEngine(m_vlcBackend);
    QVERIFY(m_controller->mediaEngine() == m_vlcBackend);
    
    // Test state delegation
    QCOMPARE(m_controller->state(), m_vlcBackend->state());
    QCOMPARE(m_controller->position(), m_vlcBackend->position());
    QCOMPARE(m_controller->duration(), m_vlcBackend->duration());
    QCOMPARE(m_controller->volume(), m_vlcBackend->volume());
    QCOMPARE(m_controller->hasMedia(), m_vlcBackend->hasMedia());
}

void TestPlaybackController::testPlaybackControls()
{
    if (!m_vlcBackend) {
        QSKIP("VLC not available, skipping playback control tests");
    }
    
    m_controller->setMediaEngine(m_vlcBackend);
    
    // Test play without media (should emit error)
    QSignalSpy errorSpy(m_controller, &PlaybackController::errorOccurred);
    m_controller->play();
    
    // Test pause/stop without media (should not crash)
    m_controller->pause();
    m_controller->stop();
    
    // Test toggle play/pause
    m_controller->togglePlayPause();
    
    // These operations should work without crashing even without media
    QVERIFY(true);
}

void TestPlaybackController::testSeekingOperations()
{
    if (!m_vlcBackend) {
        QSKIP("VLC not available, skipping seeking tests");
    }
    
    m_controller->setMediaEngine(m_vlcBackend);
    
    // Test seeking operations without media
    m_controller->seek(5000);
    m_controller->seekForward(1000);
    m_controller->seekBackward(1000);
    m_controller->seekToPercentage(50.0);
    
    // Test percentage clamping
    m_controller->seekToPercentage(-10.0);  // Should clamp to 0
    m_controller->seekToPercentage(150.0);  // Should clamp to 100
    
    // These should not crash
    QVERIFY(true);
}

void TestPlaybackController::testVolumeControl()
{
    if (!m_vlcBackend) {
        QSKIP("VLC not available, skipping volume control tests");
    }
    
    m_controller->setMediaEngine(m_vlcBackend);
    
    // Test volume setting
    QSignalSpy volumeSpy(m_controller, &PlaybackController::volumeChanged);
    
    m_controller->setVolume(50);
    QCOMPARE(m_controller->volume(), 50);
    
    // Test volume bounds
    m_controller->setVolume(-10);
    QCOMPARE(m_controller->volume(), 0);
    
    m_controller->setVolume(150);
    QCOMPARE(m_controller->volume(), 100);
    
    // Test volume up/down
    m_controller->setVolume(50);
    m_controller->volumeUp(10);
    QCOMPARE(m_controller->volume(), 60);
    
    m_controller->volumeDown(20);
    QCOMPARE(m_controller->volume(), 40);
}

void TestPlaybackController::testMuteControl()
{
    if (!m_vlcBackend) {
        QSKIP("VLC not available, skipping mute control tests");
    }
    
    m_controller->setMediaEngine(m_vlcBackend);
    
    // Set initial volume
    m_controller->setVolume(75);
    
    // Test mute
    QSignalSpy muteSpy(m_controller, &PlaybackController::muteChanged);
    
    QVERIFY(!m_controller->isMuted());
    
    m_controller->setMuted(true);
    QVERIFY(m_controller->isMuted());
    QCOMPARE(m_controller->volume(), 0);
    
    // Test unmute
    m_controller->setMuted(false);
    QVERIFY(!m_controller->isMuted());
    QCOMPARE(m_controller->volume(), 75);  // Should restore previous volume
    
    // Test toggle mute
    m_controller->toggleMute();
    QVERIFY(m_controller->isMuted());
    
    m_controller->toggleMute();
    QVERIFY(!m_controller->isMuted());
}

void TestPlaybackController::testPlaybackSpeed()
{
    // Test playback speed control
    QSignalSpy speedSpy(m_controller, &PlaybackController::playbackSpeedChanged);
    
    // Test initial speed
    QCOMPARE(m_controller->playbackSpeed(), 100);
    
    // Test setting speed
    m_controller->setPlaybackSpeed(200);
    QCOMPARE(m_controller->playbackSpeed(), 200);
    QCOMPARE(speedSpy.count(), 1);
    
    // Test speed bounds
    m_controller->setPlaybackSpeed(5);  // Below minimum
    QCOMPARE(m_controller->playbackSpeed(), 10);  // Should clamp to minimum
    
    m_controller->setPlaybackSpeed(2000);  // Above maximum
    QCOMPARE(m_controller->playbackSpeed(), 1000);  // Should clamp to maximum
    
    // Test enum speeds
    m_controller->setPlaybackSpeed(PlaybackSpeed::Half);
    QCOMPARE(m_controller->playbackSpeed(), 50);
    
    m_controller->setPlaybackSpeed(PlaybackSpeed::Double);
    QCOMPARE(m_controller->playbackSpeed(), 200);
    
    // Test reset
    m_controller->resetPlaybackSpeed();
    QCOMPARE(m_controller->playbackSpeed(), 100);
}

void TestPlaybackController::testPositionReporting()
{
    if (!m_vlcBackend) {
        QSKIP("VLC not available, skipping position reporting tests");
    }
    
    m_controller->setMediaEngine(m_vlcBackend);
    
    // Test position percentage calculation
    double percentage = m_controller->positionPercentage();
    QVERIFY(percentage >= 0.0 && percentage <= 100.0);
    
    // Without media, should return 0
    QCOMPARE(percentage, 0.0);
}

void TestPlaybackController::testErrorHandling()
{
    // Test operations without media engine
    QSignalSpy errorSpy(m_controller, &PlaybackController::errorOccurred);
    
    m_controller->setMediaEngine(nullptr);
    
    // These should generate warnings but not crash
    m_controller->play();
    m_controller->pause();
    m_controller->stop();
    m_controller->seek(1000);
    m_controller->seekForward();
    m_controller->seekBackward();
    m_controller->setVolume(50);
    m_controller->volumeUp();
    m_controller->volumeDown();
    m_controller->setMuted(true);
    
    // Should have at least one error for the play() call
    QVERIFY(errorSpy.count() >= 1);
}

void TestPlaybackController::testAdvancedPlaybackFeatures()
{
    // Test frame stepping
    m_controller->stepForward();
    m_controller->stepBackward();
    
    // These should not crash even without media
    QVERIFY(true);
}

void TestPlaybackController::testFastSeekOperations()
{
    // Test fast seek without media
    QVERIFY(!m_controller->isFastSeeking());
    
    QSignalSpy fastSeekSpy(m_controller, &PlaybackController::fastSeekChanged);
    
    m_controller->startFastForward(SeekSpeed::Normal);
    m_controller->startRewind(SeekSpeed::Fast);
    m_controller->stopFastSeek();
    
    // Should not crash without media
    QVERIFY(true);
}

void TestPlaybackController::testPlaybackModes()
{
    QSignalSpy modeSpy(m_controller, &PlaybackController::playbackModeChanged);
    
    // Test initial mode
    QCOMPARE(m_controller->playbackMode(), PlaybackMode::Normal);
    
    // Test setting modes
    m_controller->setPlaybackMode(PlaybackMode::RepeatOne);
    QCOMPARE(m_controller->playbackMode(), PlaybackMode::RepeatOne);
    QCOMPARE(modeSpy.count(), 1);
    
    m_controller->setPlaybackMode(PlaybackMode::RepeatAll);
    QCOMPARE(m_controller->playbackMode(), PlaybackMode::RepeatAll);
    QCOMPARE(modeSpy.count(), 2);
    
    m_controller->setPlaybackMode(PlaybackMode::Shuffle);
    QCOMPARE(m_controller->playbackMode(), PlaybackMode::Shuffle);
    QCOMPARE(modeSpy.count(), 3);
    
    // Test setting same mode (should not emit signal)
    m_controller->setPlaybackMode(PlaybackMode::Shuffle);
    QCOMPARE(modeSpy.count(), 3);
}

void TestPlaybackController::testCrossfadeSettings()
{
    QSignalSpy crossfadeSpy(m_controller, &PlaybackController::crossfadeChanged);
    
    // Test initial state
    QVERIFY(!m_controller->isCrossfadeEnabled());
    QCOMPARE(m_controller->crossfadeDuration(), 3000);
    
    // Test enabling crossfade
    m_controller->setCrossfadeEnabled(true, 5000);
    QVERIFY(m_controller->isCrossfadeEnabled());
    QCOMPARE(m_controller->crossfadeDuration(), 5000);
    QCOMPARE(crossfadeSpy.count(), 1);
    
    // Test duration bounds (should clamp to 500-10000ms)
    m_controller->setCrossfadeEnabled(true, 100);  // Too short
    QCOMPARE(m_controller->crossfadeDuration(), 500);
    
    m_controller->setCrossfadeEnabled(true, 15000);  // Too long
    QCOMPARE(m_controller->crossfadeDuration(), 10000);
    
    // Test gapless playback
    QSignalSpy gaplessSpy(m_controller, &PlaybackController::gaplessPlaybackChanged);
    
    QVERIFY(!m_controller->isGaplessPlayback());
    
    m_controller->setGaplessPlayback(true);
    QVERIFY(m_controller->isGaplessPlayback());
    QCOMPARE(gaplessSpy.count(), 1);
}

void TestPlaybackController::testResumePositions()
{
    QString testFile = "/test/media.mp4";
    
    // Test saving and loading resume position
    m_controller->saveResumePosition(testFile);
    
    // Without media, should not save anything
    QVERIFY(!m_controller->loadResumePosition(testFile));
    
    // Test clearing resume position
    m_controller->clearResumePosition(testFile);
    
    // Should not crash
    QVERIFY(true);
}

void TestPlaybackController::testSeekThumbnails()
{
    QSignalSpy thumbnailSpy(m_controller, &PlaybackController::seekThumbnailGenerated);
    
    // Test initial state
    QVERIFY(m_controller->isSeekThumbnailEnabled());
    
    // Test generating thumbnail without media
    QString thumbnail = m_controller->generateSeekThumbnail(5000);
    QVERIFY(thumbnail.isEmpty());  // Should be empty without media
    
    // Test disabling thumbnails
    m_controller->setSeekThumbnailEnabled(false);
    QVERIFY(!m_controller->isSeekThumbnailEnabled());
    
    thumbnail = m_controller->generateSeekThumbnail(5000);
    QVERIFY(thumbnail.isEmpty());  // Should be empty when disabled
    
    // Re-enable for other tests
    m_controller->setSeekThumbnailEnabled(true);
    QVERIFY(m_controller->isSeekThumbnailEnabled());
}

QTEST_MAIN(TestPlaybackController)
#include "test_playback_controller.moc"