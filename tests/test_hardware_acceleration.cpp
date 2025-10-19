#include <QtTest/QtTest>
#include <QSignalSpy>
#include "media/HardwareAcceleration.h"

class TestHardwareAcceleration : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testInitialization();
    void testPlatformSupport();
    void testAccelerationDetection();
    void testAccelerationTypeConversion();
    void testPreferredAcceleration();
    void testVLCArguments();

private:
    HardwareAcceleration* m_hwAccel;
};

void TestHardwareAcceleration::initTestCase()
{
    m_hwAccel = new HardwareAcceleration(this);
}

void TestHardwareAcceleration::cleanupTestCase()
{
    delete m_hwAccel;
    m_hwAccel = nullptr;
}

void TestHardwareAcceleration::testInitialization()
{
    // Test initialization
    bool initResult = m_hwAccel->initialize();
    QVERIFY(initResult);
    
    // Test default state
    QVERIFY(m_hwAccel->isHardwareAccelerationEnabled());
    QCOMPARE(m_hwAccel->getPreferredAcceleration(), HardwareAccelerationType::Auto);
}

void TestHardwareAcceleration::testPlatformSupport()
{
    // Test platform support detection
    bool platformSupported = HardwareAcceleration::isPlatformSupported();
    
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    QVERIFY(platformSupported);
#else
    QVERIFY(!platformSupported);
#endif
    
    qDebug() << "Platform supported:" << platformSupported;
}

void TestHardwareAcceleration::testAccelerationDetection()
{
    // Initialize if not already done
    if (!m_hwAccel->initialize()) {
        QSKIP("Hardware acceleration initialization failed");
    }
    
    // Test acceleration detection
    QList<HardwareAccelerationInfo> accelerations = m_hwAccel->getAvailableAccelerations();
    QVERIFY(!accelerations.isEmpty());
    
    // Should always have software decoding as fallback
    bool hasSoftware = false;
    for (const auto& accel : accelerations) {
        if (accel.type == HardwareAccelerationType::Software) {
            hasSoftware = true;
            QVERIFY(accel.available);
            QCOMPARE(accel.name, QString("Software Decoding"));
        }
        
        qDebug() << "Acceleration:" << accel.name << "Available:" << accel.available;
        if (accel.available && !accel.supportedCodecs.isEmpty()) {
            qDebug() << "  Supported codecs:" << accel.supportedCodecs;
        }
    }
    
    QVERIFY(hasSoftware);
    
    // Test best available acceleration
    HardwareAccelerationType best = m_hwAccel->getBestAvailableAcceleration();
    QVERIFY(best != HardwareAccelerationType::Auto); // Should resolve to a concrete type
    
    qDebug() << "Best available acceleration:" << HardwareAcceleration::getAccelerationTypeName(best);
}

void TestHardwareAcceleration::testAccelerationTypeConversion()
{
    // Test string to enum conversion
    QCOMPARE(HardwareAcceleration::stringToAccelerationType("auto"), HardwareAccelerationType::Auto);
    QCOMPARE(HardwareAcceleration::stringToAccelerationType("none"), HardwareAccelerationType::Software);
    QCOMPARE(HardwareAcceleration::stringToAccelerationType("dxva"), HardwareAccelerationType::DXVA);
    QCOMPARE(HardwareAcceleration::stringToAccelerationType("vaapi"), HardwareAccelerationType::VAAPI);
    QCOMPARE(HardwareAcceleration::stringToAccelerationType("vdpau"), HardwareAccelerationType::VDPAU);
    
    // Test case insensitive
    QCOMPARE(HardwareAcceleration::stringToAccelerationType("AUTO"), HardwareAccelerationType::Auto);
    QCOMPARE(HardwareAcceleration::stringToAccelerationType("DXVA"), HardwareAccelerationType::DXVA);
    
    // Test invalid input
    QCOMPARE(HardwareAcceleration::stringToAccelerationType("invalid"), HardwareAccelerationType::Auto);
    QCOMPARE(HardwareAcceleration::stringToAccelerationType(""), HardwareAccelerationType::Auto);
    
    // Test enum to string conversion
    QCOMPARE(HardwareAcceleration::accelerationTypeToString(HardwareAccelerationType::Auto), QString("auto"));
    QCOMPARE(HardwareAcceleration::accelerationTypeToString(HardwareAccelerationType::Software), QString("none"));
    QCOMPARE(HardwareAcceleration::accelerationTypeToString(HardwareAccelerationType::DXVA), QString("dxva"));
    QCOMPARE(HardwareAcceleration::accelerationTypeToString(HardwareAccelerationType::VAAPI), QString("vaapi"));
    QCOMPARE(HardwareAcceleration::accelerationTypeToString(HardwareAccelerationType::VDPAU), QString("vdpau"));
}

void TestHardwareAcceleration::testPreferredAcceleration()
{
    // Initialize if not already done
    if (!m_hwAccel->initialize()) {
        QSKIP("Hardware acceleration initialization failed");
    }
    
    // Test setting preferred acceleration
    QSignalSpy preferredSpy(m_hwAccel, &HardwareAcceleration::preferredAccelerationChanged);
    
    // Test setting to Software (should always be available)
    bool result = m_hwAccel->setPreferredAcceleration(HardwareAccelerationType::Software);
    QVERIFY(result);
    QCOMPARE(m_hwAccel->getPreferredAcceleration(), HardwareAccelerationType::Software);
    
    // Test setting back to Auto
    result = m_hwAccel->setPreferredAcceleration(HardwareAccelerationType::Auto);
    QVERIFY(result);
    QCOMPARE(m_hwAccel->getPreferredAcceleration(), HardwareAccelerationType::Auto);
    
    // Verify signals were emitted
    QCOMPARE(preferredSpy.count(), 2);
}

void TestHardwareAcceleration::testVLCArguments()
{
    // Initialize if not already done
    if (!m_hwAccel->initialize()) {
        QSKIP("Hardware acceleration initialization failed");
    }
    
    // Test VLC arguments with hardware acceleration enabled
    m_hwAccel->setHardwareAccelerationEnabled(true);
    QStringList args = m_hwAccel->getVLCArguments();
    QVERIFY(!args.isEmpty());
    
    // Should not contain --no-hw-dec when enabled
    QVERIFY(!args.contains("--no-hw-dec"));
    
    qDebug() << "VLC args (HW enabled):" << args;
    
    // Test VLC arguments with hardware acceleration disabled
    m_hwAccel->setHardwareAccelerationEnabled(false);
    args = m_hwAccel->getVLCArguments();
    QVERIFY(!args.isEmpty());
    
    // Should contain --no-hw-dec when disabled
    QVERIFY(args.contains("--no-hw-dec"));
    
    qDebug() << "VLC args (HW disabled):" << args;
    
    // Test with specific acceleration type
    m_hwAccel->setHardwareAccelerationEnabled(true);
    m_hwAccel->setPreferredAcceleration(HardwareAccelerationType::Software);
    args = m_hwAccel->getVLCArguments();
    QVERIFY(args.contains("--no-hw-dec"));
    
    qDebug() << "VLC args (Software only):" << args;
}

QTEST_MAIN(TestHardwareAcceleration)
#include "test_hardware_acceleration.moc"