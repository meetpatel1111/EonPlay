#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

/**
 * @brief Hardware acceleration types supported by the system
 */
enum class HardwareAccelerationType
{
    Software,       // Software decoding only
    DXVA,          // DirectX Video Acceleration (Windows)
    VAAPI,         // Video Acceleration API (Linux)
    VDPAU,         // Video Decode and Presentation API for Unix (Linux)
    Auto           // Automatically detect best available
};

/**
 * @brief Hardware acceleration capabilities and status
 */
struct HardwareAccelerationInfo
{
    HardwareAccelerationType type = HardwareAccelerationType::Software;
    QString name;                    // Human-readable name
    QString description;             // Detailed description
    bool available = false;          // Whether this acceleration is available
    bool enabled = false;            // Whether this acceleration is currently enabled
    QStringList supportedCodecs;     // List of supported codecs
    QString driverVersion;           // Graphics driver version (if available)
    QString deviceName;              // Graphics device name (if available)
};

/**
 * @brief Manages hardware acceleration detection and configuration
 * 
 * Detects available hardware acceleration methods on the current platform,
 * provides configuration options, and manages fallback to software decoding.
 */
class HardwareAcceleration : public QObject
{
    Q_OBJECT

public:
    explicit HardwareAcceleration(QObject* parent = nullptr);
    ~HardwareAcceleration() = default;
    
    /**
     * @brief Initialize hardware acceleration detection
     * @return true if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Detect available hardware acceleration methods
     * @return List of available acceleration methods
     */
    QList<HardwareAccelerationInfo> detectAvailableAcceleration();
    
    /**
     * @brief Get the best available hardware acceleration method
     * @return Best acceleration method, or None if none available
     */
    HardwareAccelerationType getBestAvailableAcceleration() const;
    
    /**
     * @brief Check if a specific acceleration type is available
     * @param type Acceleration type to check
     * @return true if the acceleration type is available
     */
    bool isAccelerationAvailable(HardwareAccelerationType type) const;
    
    /**
     * @brief Get information about a specific acceleration type
     * @param type Acceleration type
     * @return Information about the acceleration type
     */
    HardwareAccelerationInfo getAccelerationInfo(HardwareAccelerationType type) const;
    
    /**
     * @brief Get all available acceleration methods
     * @return List of all available acceleration methods
     */
    QList<HardwareAccelerationInfo> getAvailableAccelerations() const;
    
    /**
     * @brief Set the preferred acceleration method
     * @param type Preferred acceleration type
     * @return true if the acceleration was set successfully
     */
    bool setPreferredAcceleration(HardwareAccelerationType type);
    
    /**
     * @brief Get the currently preferred acceleration method
     * @return Currently preferred acceleration type
     */
    HardwareAccelerationType getPreferredAcceleration() const;
    
    /**
     * @brief Enable or disable hardware acceleration
     * @param enabled Whether to enable hardware acceleration
     */
    void setHardwareAccelerationEnabled(bool enabled);
    
    /**
     * @brief Check if hardware acceleration is currently enabled
     * @return true if hardware acceleration is enabled
     */
    bool isHardwareAccelerationEnabled() const;
    
    /**
     * @brief Get libVLC arguments for the current acceleration configuration
     * @return List of libVLC arguments to enable hardware acceleration
     */
    QStringList getVLCArguments() const;
    
    /**
     * @brief Test if hardware acceleration is working with a sample video
     * @param testVideoPath Path to test video file (optional)
     * @return true if hardware acceleration test passed
     */
    bool testHardwareAcceleration(const QString& testVideoPath = QString());
    
    /**
     * @brief Get platform-specific acceleration type name
     * @param type Acceleration type
     * @return Platform-specific name for the acceleration type
     */
    static QString getAccelerationTypeName(HardwareAccelerationType type);
    
    /**
     * @brief Get platform-specific acceleration type description
     * @param type Acceleration type
     * @return Detailed description of the acceleration type
     */
    static QString getAccelerationTypeDescription(HardwareAccelerationType type);
    
    /**
     * @brief Check if the current platform supports hardware acceleration
     * @return true if the platform supports hardware acceleration
     */
    static bool isPlatformSupported();
    
    /**
     * @brief Convert acceleration type to string
     * @param type Acceleration type
     * @return String representation of the acceleration type
     */
    static QString accelerationTypeToString(HardwareAccelerationType type);
    
    /**
     * @brief Convert string to acceleration type
     * @param typeString String representation of acceleration type
     * @return Acceleration type enum value
     */
    static HardwareAccelerationType stringToAccelerationType(const QString& typeString);

signals:
    /**
     * @brief Emitted when hardware acceleration availability changes
     * @param available Whether hardware acceleration is available
     */
    void accelerationAvailabilityChanged(bool available);
    
    /**
     * @brief Emitted when hardware acceleration is enabled/disabled
     * @param enabled Whether hardware acceleration is enabled
     */
    void accelerationEnabledChanged(bool enabled);
    
    /**
     * @brief Emitted when preferred acceleration method changes
     * @param type New preferred acceleration type
     */
    void preferredAccelerationChanged(HardwareAccelerationType type);

private:
    /**
     * @brief Detect DXVA availability on Windows
     * @return DXVA acceleration info
     */
    HardwareAccelerationInfo detectDXVA() const;
    
    /**
     * @brief Detect VA-API availability on Linux
     * @return VA-API acceleration info
     */
    HardwareAccelerationInfo detectVAAPI() const;
    
    /**
     * @brief Detect VDPAU availability on Linux
     * @return VDPAU acceleration info
     */
    HardwareAccelerationInfo detectVDPAU() const;
    
    /**
     * @brief Get graphics driver information
     * @return Driver information string
     */
    QString getDriverInfo() const;
    
    /**
     * @brief Get graphics device information
     * @return Device information string
     */
    QString getDeviceInfo() const;
    
    /**
     * @brief Check if libVLC supports a specific acceleration method
     * @param type Acceleration type to check
     * @return true if libVLC supports the acceleration method
     */
    bool isVLCAccelerationSupported(HardwareAccelerationType type) const;
    
    QList<HardwareAccelerationInfo> m_availableAccelerations;
    HardwareAccelerationType m_preferredAcceleration;
    bool m_hardwareAccelerationEnabled;
    bool m_initialized;
};