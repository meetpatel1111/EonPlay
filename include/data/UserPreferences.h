#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QSettings>

namespace EonPlay {
namespace Data {

class UserPreferences : public QObject
{
    Q_OBJECT

public:
    explicit UserPreferences(QObject* parent = nullptr);
    ~UserPreferences();

    // General preferences
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void setValue(const QString& key, const QVariant& value);
    
    // Media preferences
    QString getDefaultAudioLanguage() const;
    void setDefaultAudioLanguage(const QString& language);
    
    QString getDefaultSubtitleLanguage() const;
    void setDefaultSubtitleLanguage(const QString& language);
    
    // Playback preferences
    int getDefaultVolume() const;
    void setDefaultVolume(int volume);
    
    bool getAutoPlayNext() const;
    void setAutoPlayNext(bool autoPlay);
    
    // Hardware acceleration preferences
    bool getHardwareAccelerationEnabled() const;
    void setHardwareAccelerationEnabled(bool enabled);
    
    QString getPreferredHardwareAcceleration() const;
    void setPreferredHardwareAcceleration(const QString& type);

signals:
    void preferenceChanged(const QString& key, const QVariant& value);

private:
    QSettings* m_settings;
    
    void initializeDefaults();
};

} // namespace Data
} // namespace EonPlay