#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "media/HardwareAcceleration.h"
#include "media/VLCBackend.h"
#include "UserPreferences.h"

/**
 * @brief Example demonstrating hardware acceleration detection and configuration
 */
class HardwareAccelerationExample : public QObject
{
    Q_OBJECT

public:
    HardwareAccelerationExample(QObject* parent = nullptr)
        : QObject(parent)
        , m_hwAccel(new HardwareAcceleration(this))
        , m_vlcBackend(new VLCBackend(this))
    {
    }

public slots:
    void run()
    {
        qDebug() << "=== Hardware Acceleration Detection Example ===";
        
        // Check platform support
        bool platformSupported = HardwareAcceleration::isPlatformSupported();
        qDebug() << "Platform supports hardware acceleration:" << platformSupported;
        
        if (!platformSupported) {
            qDebug() << "Hardware acceleration not supported on this platform";
            QCoreApplication::quit();
            return;
        }
        
        // Initialize hardware acceleration detection
        if (!m_hwAccel->initialize()) {
            qDebug() << "Failed to initialize hardware acceleration detection";
            QCoreApplication::quit();
            return;
        }
        
        // Detect available acceleration methods
        qDebug() << "\n=== Available Hardware Acceleration Methods ===";
        QList<HardwareAccelerationInfo> accelerations = m_hwAccel->getAvailableAccelerations();
        
        for (const auto& accel : accelerations) {
            qDebug() << "Name:" << accel.name;
            qDebug() << "  Type:" << HardwareAcceleration::getAccelerationTypeName(accel.type);
            qDebug() << "  Available:" << (accel.available ? "Yes" : "No");
            qDebug() << "  Description:" << accel.description;
            
            if (accel.available) {
                if (!accel.deviceName.isEmpty()) {
                    qDebug() << "  Device:" << accel.deviceName;
                }
                if (!accel.driverVersion.isEmpty()) {
                    qDebug() << "  Driver Version:" << accel.driverVersion;
                }
                if (!accel.supportedCodecs.isEmpty()) {
                    qDebug() << "  Supported Codecs:" << accel.supportedCodecs.join(", ");
                }
            }
            qDebug() << "";
        }
        
        // Get best available acceleration
        HardwareAccelerationType best = m_hwAccel->getBestAvailableAcceleration();
        qDebug() << "Best available acceleration:" << HardwareAcceleration::getAccelerationTypeName(best);
        
        // Test VLC arguments generation
        qDebug() << "\n=== VLC Arguments for Different Configurations ===";
        
        // Hardware acceleration enabled with auto-detection
        m_hwAccel->setHardwareAccelerationEnabled(true);
        m_hwAccel->setPreferredAcceleration(HardwareAccelerationType::Auto);
        QStringList args = m_hwAccel->getVLCArguments();
        qDebug() << "Hardware acceleration (Auto):" << args;
        
        // Hardware acceleration disabled
        m_hwAccel->setHardwareAccelerationEnabled(false);
        args = m_hwAccel->getVLCArguments();
        qDebug() << "Hardware acceleration (Disabled):" << args;
        
        // Software decoding only
        m_hwAccel->setHardwareAccelerationEnabled(true);
        m_hwAccel->setPreferredAcceleration(HardwareAccelerationType::Software);
        args = m_hwAccel->getVLCArguments();
        qDebug() << "Software decoding only:" << args;
        
        // Test integration with VLCBackend
        qDebug() << "\n=== VLCBackend Integration ===";
        
        if (!m_vlcBackend->initialize()) {
            qDebug() << "Failed to initialize VLC backend";
        } else {
            qDebug() << "VLC backend initialized successfully";
            
            // Test hardware acceleration settings
            UserPreferences prefs;
            prefs.hardwareAcceleration = true;
            prefs.preferredAccelerationType = "auto";
            
            m_vlcBackend->updateHardwareAccelerationFromPreferences(prefs);
            qDebug() << "Hardware acceleration settings applied to VLC backend";
            
            // Test different acceleration types
            prefs.preferredAccelerationType = "none";
            m_vlcBackend->updateHardwareAccelerationFromPreferences(prefs);
            qDebug() << "Software decoding settings applied to VLC backend";
            
            // Test disabling hardware acceleration
            prefs.hardwareAcceleration = false;
            m_vlcBackend->updateHardwareAccelerationFromPreferences(prefs);
            qDebug() << "Hardware acceleration disabled in VLC backend";
        }
        
        // Test string conversion functions
        qDebug() << "\n=== String Conversion Tests ===";
        
        QStringList testStrings = {"auto", "none", "dxva", "vaapi", "vdpau", "invalid"};
        for (const QString& str : testStrings) {
            HardwareAccelerationType type = HardwareAcceleration::stringToAccelerationType(str);
            QString converted = HardwareAcceleration::accelerationTypeToString(type);
            qDebug() << "String:" << str << "-> Type:" << (int)type << "-> String:" << converted;
        }
        
        qDebug() << "\n=== Example Complete ===";
        
        // Exit after a short delay
        QTimer::singleShot(100, QCoreApplication::quit);
    }

private:
    HardwareAcceleration* m_hwAccel;
    VLCBackend* m_vlcBackend;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    HardwareAccelerationExample example;
    
    // Run the example after the event loop starts
    QTimer::singleShot(0, &example, &HardwareAccelerationExample::run);
    
    return app.exec();
}

#include "hardware_acceleration_example.moc"