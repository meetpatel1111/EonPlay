#include "media/HardwareAcceleration.h"
#include <QLoggingCategory>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>

// libVLC includes
#include <vlc/vlc.h>

#ifdef Q_OS_WIN
#include <windows.h>
#include <d3d9.h>
#include <dxva2api.h>
#endif

#ifdef Q_OS_LINUX
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#ifdef Q_OS_LINUX
// Only include platform interface on Linux where it's commonly available
#if __has_include(<qpa/qplatformnativeinterface.h>)
#include <qpa/qplatformnativeinterface.h>
#define HAS_PLATFORM_INTERFACE
#endif
#endif
#endif
#ifdef HAVE_LIBVA
#include <va/va.h>
#include <va/va_x11.h>
#endif
#ifdef HAVE_LIBVDPAU
#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>
#endif
#endif

// libVLC includes
#include <vlc/vlc.h>

Q_LOGGING_CATEGORY(hwAccel, "mediaplayer.hwaccel")

HardwareAcceleration::HardwareAcceleration(QObject* parent)
    : QObject(parent)
    , m_preferredAcceleration(HardwareAccelerationType::Auto)
    , m_hardwareAccelerationEnabled(true)
    , m_initialized(false)
{
    qCDebug(hwAccel) << "HardwareAcceleration created";
}

bool HardwareAcceleration::initialize()
{
    if (m_initialized) {
        qCWarning(hwAccel) << "HardwareAcceleration already initialized";
        return true;
    }
    
    qCDebug(hwAccel) << "Initializing hardware acceleration detection";
    
    if (!isPlatformSupported()) {
        qCWarning(hwAccel) << "Hardware acceleration not supported on this platform";
        m_hardwareAccelerationEnabled = false;
        m_initialized = true;
        return true;
    }
    
    // Detect available acceleration methods
    m_availableAccelerations = detectAvailableAcceleration();
    
    // Log detected accelerations
    qCDebug(hwAccel) << "Detected" << m_availableAccelerations.size() << "hardware acceleration methods:";
    for (const auto& accel : m_availableAccelerations) {
        qCDebug(hwAccel) << " -" << accel.name << ":" << (accel.available ? "Available" : "Not available");
    }
    
    // Set default preferred acceleration if auto is selected
    if (m_preferredAcceleration == HardwareAccelerationType::Auto) {
        HardwareAccelerationType best = getBestAvailableAcceleration();
        if (best != HardwareAccelerationType::Software) {
            m_preferredAcceleration = best;
            qCDebug(hwAccel) << "Auto-selected acceleration:" << getAccelerationTypeName(best);
        } else {
            qCDebug(hwAccel) << "No hardware acceleration available, using software decoding";
            m_hardwareAccelerationEnabled = false;
        }
    }
    
    m_initialized = true;
    qCDebug(hwAccel) << "Hardware acceleration initialization complete";
    
    return true;
}

QList<HardwareAccelerationInfo> HardwareAcceleration::detectAvailableAcceleration()
{
    QList<HardwareAccelerationInfo> accelerations;
    
    qCDebug(hwAccel) << "Detecting available hardware acceleration methods";
    
#ifdef Q_OS_WIN
    // Detect DXVA on Windows
    HardwareAccelerationInfo dxva = detectDXVA();
    accelerations.append(dxva);
#endif
    
#ifdef Q_OS_LINUX
    // Detect VA-API on Linux
    HardwareAccelerationInfo vaapi = detectVAAPI();
    accelerations.append(vaapi);
    
    // Detect VDPAU on Linux
    HardwareAccelerationInfo vdpau = detectVDPAU();
    accelerations.append(vdpau);
#endif
    
    // Always add software decoding as fallback
    HardwareAccelerationInfo software;
    software.type = HardwareAccelerationType::Software;
    software.name = "Software Decoding";
    software.description = "CPU-based software decoding (fallback)";
    software.available = true;
    software.supportedCodecs << "All supported codecs";
    accelerations.append(software);
    
    return accelerations;
}

HardwareAccelerationType HardwareAcceleration::getBestAvailableAcceleration() const
{
    // Priority order: DXVA (Windows) > VA-API (Linux) > VDPAU (Linux) > None
    
    for (const auto& accel : m_availableAccelerations) {
        if (accel.available) {
            switch (accel.type) {
#ifdef Q_OS_WIN
                case HardwareAccelerationType::DXVA:
                    return HardwareAccelerationType::DXVA;
#endif
#ifdef Q_OS_LINUX
                case HardwareAccelerationType::VAAPI:
                    return HardwareAccelerationType::VAAPI;
                case HardwareAccelerationType::VDPAU:
                    return HardwareAccelerationType::VDPAU;
#endif
                default:
                    break;
            }
        }
    }
    
    return HardwareAccelerationType::Software;
}

bool HardwareAcceleration::isAccelerationAvailable(HardwareAccelerationType type) const
{
    for (const auto& accel : m_availableAccelerations) {
        if (accel.type == type) {
            return accel.available;
        }
    }
    return false;
}

HardwareAccelerationInfo HardwareAcceleration::getAccelerationInfo(HardwareAccelerationType type) const
{
    for (const auto& accel : m_availableAccelerations) {
        if (accel.type == type) {
            return accel;
        }
    }
    
    // Return empty info if not found
    HardwareAccelerationInfo empty;
    empty.type = type;
    return empty;
}

QList<HardwareAccelerationInfo> HardwareAcceleration::getAvailableAccelerations() const
{
    return m_availableAccelerations;
}

bool HardwareAcceleration::setPreferredAcceleration(HardwareAccelerationType type)
{
    if (type == HardwareAccelerationType::Auto) {
        m_preferredAcceleration = type;
        emit preferredAccelerationChanged(type);
        return true;
    }
    
    if (type == HardwareAccelerationType::Software || isAccelerationAvailable(type)) {
        m_preferredAcceleration = type;
        qCDebug(hwAccel) << "Preferred acceleration set to:" << getAccelerationTypeName(type);
        emit preferredAccelerationChanged(type);
        return true;
    }
    
    qCWarning(hwAccel) << "Cannot set preferred acceleration to unavailable type:" << getAccelerationTypeName(type);
    return false;
}

HardwareAccelerationType HardwareAcceleration::getPreferredAcceleration() const
{
    return m_preferredAcceleration;
}

void HardwareAcceleration::setHardwareAccelerationEnabled(bool enabled)
{
    if (m_hardwareAccelerationEnabled != enabled) {
        m_hardwareAccelerationEnabled = enabled;
        qCDebug(hwAccel) << "Hardware acceleration" << (enabled ? "enabled" : "disabled");
        emit accelerationEnabledChanged(enabled);
    }
}

bool HardwareAcceleration::isHardwareAccelerationEnabled() const
{
    return m_hardwareAccelerationEnabled;
}

QStringList HardwareAcceleration::getVLCArguments() const
{
    QStringList args;
    
    if (!m_hardwareAccelerationEnabled) {
        // Force software decoding
        args << "--no-hw-dec";
        qCDebug(hwAccel) << "Using software decoding (hardware acceleration disabled)";
        return args;
    }
    
    HardwareAccelerationType activeType = m_preferredAcceleration;
    if (activeType == HardwareAccelerationType::Auto) {
        activeType = getBestAvailableAcceleration();
    }
    
    switch (activeType) {
#ifdef Q_OS_WIN
        case HardwareAccelerationType::DXVA:
            args << "--avcodec-hw=dxva2";
            qCDebug(hwAccel) << "Using DXVA hardware acceleration";
            break;
#endif
            
#ifdef Q_OS_LINUX
        case HardwareAccelerationType::VAAPI:
            args << "--avcodec-hw=vaapi";
            qCDebug(hwAccel) << "Using VA-API hardware acceleration";
            break;
            
        case HardwareAccelerationType::VDPAU:
            args << "--avcodec-hw=vdpau";
            qCDebug(hwAccel) << "Using VDPAU hardware acceleration";
            break;
#endif
            
        case HardwareAccelerationType::Software:
        default:
            args << "--no-hw-dec";
            qCDebug(hwAccel) << "Using software decoding (no hardware acceleration available)";
            break;
    }
    
    return args;
}

bool HardwareAcceleration::testHardwareAcceleration(const QString& testVideoPath)
{
    if (!m_hardwareAccelerationEnabled) {
        return false;
    }
    
    qCDebug(hwAccel) << "Testing hardware acceleration";
    
    // Create a test libVLC instance with hardware acceleration
    QStringList vlcArgs = getVLCArguments();
    vlcArgs << "--intf=dummy" << "--no-video" << "--no-audio";
    
    // Convert to char* array
    QList<QByteArray> argBytes;
    QList<const char*> argPointers;
    
    for (const QString& arg : vlcArgs) {
        argBytes.append(arg.toUtf8());
        argPointers.append(argBytes.last().constData());
    }
    
    libvlc_instance_t* testInstance = libvlc_new(argPointers.size(), argPointers.data());
    if (!testInstance) {
        qCWarning(hwAccel) << "Failed to create test libVLC instance";
        return false;
    }
    
    // Create test media player
    libvlc_media_player_t* testPlayer = libvlc_media_player_new(testInstance);
    if (!testPlayer) {
        qCWarning(hwAccel) << "Failed to create test media player";
        libvlc_release(testInstance);
        return false;
    }
    
    bool testPassed = true;
    
    // If a test video is provided, try to load it
    if (!testVideoPath.isEmpty() && QFileInfo::exists(testVideoPath)) {
        libvlc_media_t* testMedia = libvlc_media_new_path(testInstance, testVideoPath.toUtf8().constData());
        if (testMedia) {
            libvlc_media_player_set_media(testPlayer, testMedia);
            
            // Try to start playback briefly
            if (libvlc_media_player_play(testPlayer) == 0) {
                // Wait a bit to see if it starts
                QThread::msleep(500);
                
                libvlc_state_t state = libvlc_media_player_get_state(testPlayer);
                if (state == libvlc_Error) {
                    testPassed = false;
                    qCWarning(hwAccel) << "Hardware acceleration test failed - playback error";
                }
                
                libvlc_media_player_stop(testPlayer);
            } else {
                testPassed = false;
                qCWarning(hwAccel) << "Hardware acceleration test failed - could not start playback";
            }
            
            libvlc_media_release(testMedia);
        } else {
            qCWarning(hwAccel) << "Could not load test video for hardware acceleration test";
        }
    }
    
    // Clean up
    libvlc_media_player_release(testPlayer);
    libvlc_release(testInstance);
    
    qCDebug(hwAccel) << "Hardware acceleration test" << (testPassed ? "passed" : "failed");
    return testPassed;
}

QString HardwareAcceleration::getAccelerationTypeName(HardwareAccelerationType type)
{
    switch (type) {
        case HardwareAccelerationType::Software:
            return "Software Decoding";
        case HardwareAccelerationType::DXVA:
            return "DXVA";
        case HardwareAccelerationType::VAAPI:
            return "VA-API";
        case HardwareAccelerationType::VDPAU:
            return "VDPAU";
        case HardwareAccelerationType::Auto:
            return "Auto-detect";
        default:
            return "Unknown";
    }
}

QString HardwareAcceleration::getAccelerationTypeDescription(HardwareAccelerationType type)
{
    switch (type) {
        case HardwareAccelerationType::Software:
            return "CPU-based software decoding. Compatible with all systems but uses more CPU.";
        case HardwareAccelerationType::DXVA:
            return "DirectX Video Acceleration. Hardware-accelerated decoding on Windows systems with compatible graphics cards.";
        case HardwareAccelerationType::VAAPI:
            return "Video Acceleration API. Hardware-accelerated decoding on Linux systems with Intel or AMD graphics.";
        case HardwareAccelerationType::VDPAU:
            return "Video Decode and Presentation API for Unix. Hardware-accelerated decoding on Linux systems with NVIDIA graphics.";
        case HardwareAccelerationType::Auto:
            return "Automatically detect and use the best available hardware acceleration method.";
        default:
            return "Unknown acceleration method.";
    }
}

bool HardwareAcceleration::isPlatformSupported()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

QString HardwareAcceleration::accelerationTypeToString(HardwareAccelerationType type)
{
    switch (type) {
        case HardwareAccelerationType::Software:
            return "none";
        case HardwareAccelerationType::DXVA:
            return "dxva";
        case HardwareAccelerationType::VAAPI:
            return "vaapi";
        case HardwareAccelerationType::VDPAU:
            return "vdpau";
        case HardwareAccelerationType::Auto:
            return "auto";
        default:
            return "auto";
    }
}

HardwareAccelerationType HardwareAcceleration::stringToAccelerationType(const QString& typeString)
{
    QString lower = typeString.toLower();
    
    if (lower == "none") {
        return HardwareAccelerationType::Software;
    } else if (lower == "dxva") {
        return HardwareAccelerationType::DXVA;
    } else if (lower == "vaapi") {
        return HardwareAccelerationType::VAAPI;
    } else if (lower == "vdpau") {
        return HardwareAccelerationType::VDPAU;
    } else if (lower == "auto") {
        return HardwareAccelerationType::Auto;
    } else {
        return HardwareAccelerationType::Auto;
    }
}

#ifdef Q_OS_WIN
HardwareAccelerationInfo HardwareAcceleration::detectDXVA() const
{
    HardwareAccelerationInfo info;
    info.type = HardwareAccelerationType::DXVA;
    info.name = "DXVA";
    info.description = "DirectX Video Acceleration";
    info.available = false;
    
    qCDebug(hwAccel) << "Detecting DXVA support";
    
    try {
        // Initialize Direct3D
        IDirect3D9* d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d9) {
            qCDebug(hwAccel) << "Failed to create Direct3D9 interface";
            return info;
        }
        
        // Get adapter information
        D3DADAPTER_IDENTIFIER9 adapterInfo;
        HRESULT hr = d3d9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapterInfo);
        if (SUCCEEDED(hr)) {
            info.deviceName = QString::fromLatin1(adapterInfo.Description);
            info.driverVersion = QString("%1.%2.%3.%4")
                .arg(HIWORD(adapterInfo.DriverVersion.HighPart))
                .arg(LOWORD(adapterInfo.DriverVersion.HighPart))
                .arg(HIWORD(adapterInfo.DriverVersion.LowPart))
                .arg(LOWORD(adapterInfo.DriverVersion.LowPart));
        }
        
        // Check for DXVA2 support
        D3DPRESENT_PARAMETERS pp = {};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = GetDesktopWindow();
        
        IDirect3DDevice9* device = nullptr;
        hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, GetDesktopWindow(),
                               D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
        
        if (SUCCEEDED(hr) && device) {
            // DXVA2 is available if we can create a device
            info.available = true;
            info.supportedCodecs << "H.264" << "MPEG-2" << "VC-1" << "HEVC";
            
            device->Release();
        }
        
        d3d9->Release();
        
    } catch (...) {
        qCWarning(hwAccel) << "Exception while detecting DXVA support";
    }
    
    // Also check if libVLC supports DXVA
    if (info.available) {
        info.available = isVLCAccelerationSupported(HardwareAccelerationType::DXVA);
    }
    
    qCDebug(hwAccel) << "DXVA detection result:" << (info.available ? "Available" : "Not available");
    if (info.available) {
        qCDebug(hwAccel) << "Device:" << info.deviceName;
        qCDebug(hwAccel) << "Driver version:" << info.driverVersion;
    }
    
    return info;
}
#endif

#ifdef Q_OS_LINUX
HardwareAccelerationInfo HardwareAcceleration::detectVAAPI() const
{
    HardwareAccelerationInfo info;
    info.type = HardwareAccelerationType::VAAPI;
    info.name = "VA-API";
    info.description = "Video Acceleration API";
    info.available = false;
    
    qCDebug(hwAccel) << "Detecting VA-API support";
    
#ifdef HAVE_LIBVA
    try {
        // Try to initialize VA-API
        Display* x11Display = QX11Info::display();
        if (!x11Display) {
            qCDebug(hwAccel) << "No X11 display available for VA-API";
            return info;
        }
        
        VADisplay vaDisplay = vaGetDisplay(x11Display);
        if (!vaDisplay) {
            qCDebug(hwAccel) << "Failed to get VA display";
            return info;
        }
        
        int majorVersion, minorVersion;
        VAStatus status = vaInitialize(vaDisplay, &majorVersion, &minorVersion);
        if (status == VA_STATUS_SUCCESS) {
            info.available = true;
            info.driverVersion = QString("VA-API %1.%2").arg(majorVersion).arg(minorVersion);
            
            // Get driver info
            const char* vendorString = vaQueryVendorString(vaDisplay);
            if (vendorString) {
                info.deviceName = QString::fromLatin1(vendorString);
            }
            
            // Query supported profiles
            int numProfiles = vaMaxNumProfiles(vaDisplay);
            if (numProfiles > 0) {
                QVector<VAProfile> profiles(numProfiles);
                int actualProfiles;
                status = vaQueryConfigProfiles(vaDisplay, profiles.data(), &actualProfiles);
                if (status == VA_STATUS_SUCCESS) {
                    for (int i = 0; i < actualProfiles; ++i) {
                        switch (profiles[i]) {
                            case VAProfileH264Main:
                            case VAProfileH264High:
                                if (!info.supportedCodecs.contains("H.264")) {
                                    info.supportedCodecs << "H.264";
                                }
                                break;
                            case VAProfileMPEG2Simple:
                            case VAProfileMPEG2Main:
                                if (!info.supportedCodecs.contains("MPEG-2")) {
                                    info.supportedCodecs << "MPEG-2";
                                }
                                break;
                            case VAProfileHEVCMain:
                                if (!info.supportedCodecs.contains("HEVC")) {
                                    info.supportedCodecs << "HEVC";
                                }
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
            
            vaTerminate(vaDisplay);
        }
        
    } catch (...) {
        qCWarning(hwAccel) << "Exception while detecting VA-API support";
    }
    
    // Also check if libVLC supports VA-API
    if (info.available) {
        info.available = isVLCAccelerationSupported(HardwareAccelerationType::VAAPI);
    }
#else
    qCDebug(hwAccel) << "VA-API support not compiled in";
#endif
    
    qCDebug(hwAccel) << "VA-API detection result:" << (info.available ? "Available" : "Not available");
    if (info.available) {
        qCDebug(hwAccel) << "Device:" << info.deviceName;
        qCDebug(hwAccel) << "Version:" << info.driverVersion;
        qCDebug(hwAccel) << "Supported codecs:" << info.supportedCodecs;
    }
    
    return info;
}

HardwareAccelerationInfo HardwareAcceleration::detectVDPAU() const
{
    HardwareAccelerationInfo info;
    info.type = HardwareAccelerationType::VDPAU;
    info.name = "VDPAU";
    info.description = "Video Decode and Presentation API for Unix";
    info.available = false;
    
    qCDebug(hwAccel) << "Detecting VDPAU support";
    
#ifdef HAVE_LIBVDPAU
    try {
        // Try to initialize VDPAU
        Display* x11Display = QX11Info::display();
        if (!x11Display) {
            qCDebug(hwAccel) << "No X11 display available for VDPAU";
            return info;
        }
        
        VdpDevice device;
        VdpGetProcAddress* getProcAddress;
        
        VdpStatus status = vdp_device_create_x11(x11Display, DefaultScreen(x11Display),
                                                &device, &getProcAddress);
        if (status == VDP_STATUS_OK) {
            info.available = true;
            
            // Get device information
            VdpGetInformationString* getInfoString;
            status = getProcAddress(device, VDP_FUNC_ID_GET_INFORMATION_STRING,
                                   (void**)&getInfoString);
            if (status == VDP_STATUS_OK) {
                char const* infoString;
                status = getInfoString(&infoString);
                if (status == VDP_STATUS_OK && infoString) {
                    info.deviceName = QString::fromLatin1(infoString);
                }
            }
            
            // Query supported decoders
            VdpDecoderQueryCapabilities* queryCapabilities;
            status = getProcAddress(device, VDP_FUNC_ID_DECODER_QUERY_CAPABILITIES,
                                   (void**)&queryCapabilities);
            if (status == VDP_STATUS_OK) {
                VdpBool supported;
                uint32_t maxLevel, maxMacroblocks, maxWidth, maxHeight;
                
                // Check H.264 support
                status = queryCapabilities(device, VDP_DECODER_PROFILE_H264_HIGH,
                                         &supported, &maxLevel, &maxMacroblocks, &maxWidth, &maxHeight);
                if (status == VDP_STATUS_OK && supported) {
                    info.supportedCodecs << "H.264";
                }
                
                // Check MPEG-2 support
                status = queryCapabilities(device, VDP_DECODER_PROFILE_MPEG2_MAIN,
                                         &supported, &maxLevel, &maxMacroblocks, &maxWidth, &maxHeight);
                if (status == VDP_STATUS_OK && supported) {
                    info.supportedCodecs << "MPEG-2";
                }
                
                // Check HEVC support (if available)
                status = queryCapabilities(device, VDP_DECODER_PROFILE_HEVC_MAIN,
                                         &supported, &maxLevel, &maxMacroblocks, &maxWidth, &maxHeight);
                if (status == VDP_STATUS_OK && supported) {
                    info.supportedCodecs << "HEVC";
                }
            }
            
            // Clean up VDPAU device
            VdpDeviceDestroy* deviceDestroy;
            status = getProcAddress(device, VDP_FUNC_ID_DEVICE_DESTROY,
                                   (void**)&deviceDestroy);
            if (status == VDP_STATUS_OK) {
                deviceDestroy(device);
            }
        }
        
    } catch (...) {
        qCWarning(hwAccel) << "Exception while detecting VDPAU support";
    }
    
    // Also check if libVLC supports VDPAU
    if (info.available) {
        info.available = isVLCAccelerationSupported(HardwareAccelerationType::VDPAU);
    }
#else
    qCDebug(hwAccel) << "VDPAU support not compiled in";
#endif
    
    qCDebug(hwAccel) << "VDPAU detection result:" << (info.available ? "Available" : "Not available");
    if (info.available) {
        qCDebug(hwAccel) << "Device:" << info.deviceName;
        qCDebug(hwAccel) << "Supported codecs:" << info.supportedCodecs;
    }
    
    return info;
}
#endif

QString HardwareAcceleration::getDriverInfo() const
{
#ifdef Q_OS_WIN
    // Get Windows graphics driver info
    // This could be expanded to query WMI or registry for detailed driver info
    return "Windows Graphics Driver";
#endif
    
#ifdef Q_OS_LINUX
    // Get Linux graphics driver info
    QProcess process;
    process.start("lspci", QStringList() << "-k");
    if (process.waitForFinished(3000)) {
        QString output = process.readAllStandardOutput();
        // Parse lspci output for graphics driver info
        QStringList lines = output.split('\n');
        for (const QString& line : lines) {
            if (line.contains("VGA", Qt::CaseInsensitive) || 
                line.contains("Display", Qt::CaseInsensitive)) {
                return line.trimmed();
            }
        }
    }
    return "Linux Graphics Driver";
#endif
    
    return "Unknown Driver";
}

QString HardwareAcceleration::getDeviceInfo() const
{
#ifdef Q_OS_WIN
    // Get Windows graphics device info
    return "Windows Graphics Device";
#endif
    
#ifdef Q_OS_LINUX
    // Get Linux graphics device info
    QProcess process;
    process.start("lspci", QStringList() << "-nn");
    if (process.waitForFinished(3000)) {
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n');
        for (const QString& line : lines) {
            if (line.contains("VGA", Qt::CaseInsensitive) || 
                line.contains("Display", Qt::CaseInsensitive)) {
                return line.trimmed();
            }
        }
    }
    return "Linux Graphics Device";
#endif
    
    return "Unknown Device";
}

bool HardwareAcceleration::isVLCAccelerationSupported(HardwareAccelerationType type) const
{
    // Create a temporary libVLC instance to check for acceleration support
    QStringList testArgs;
    testArgs << "--intf=dummy" << "--no-video" << "--no-audio";
    
    switch (type) {
        case HardwareAccelerationType::DXVA:
            testArgs << "--avcodec-hw=dxva2";
            break;
        case HardwareAccelerationType::VAAPI:
            testArgs << "--avcodec-hw=vaapi";
            break;
        case HardwareAccelerationType::VDPAU:
            testArgs << "--avcodec-hw=vdpau";
            break;
        default:
            return true; // Software decoding is always supported
    }
    
    // Convert to char* array
    QList<QByteArray> argBytes;
    QList<const char*> argPointers;
    
    for (const QString& arg : testArgs) {
        argBytes.append(arg.toUtf8());
        argPointers.append(argBytes.last().constData());
    }
    
    // Try to create libVLC instance with hardware acceleration
    libvlc_instance_t* testInstance = libvlc_new(argPointers.size(), argPointers.data());
    if (testInstance) {
        libvlc_release(testInstance);
        return true;
    }
    
    return false;
}

