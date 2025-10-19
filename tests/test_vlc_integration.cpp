#include <QtTest/QtTest>
#include <QSignalSpy>
#include "media/VLCBackend.h"
#include "ComponentManager.h"
#include <memory>

class TestVLCIntegration : public QObject
{
    Q_OBJECT

private slots:
    void testComponentRegistration();
    void testComponentInitialization();

private:
    std::unique_ptr<ComponentManager> m_componentManager;
};

void TestVLCIntegration::testComponentRegistration()
{
    m_componentManager = std::make_unique<ComponentManager>();
    
    // Create VLC backend
    auto vlcBackend = std::make_shared<VLCBackend>();
    
    // Register with component manager
    m_componentManager->registerComponent(vlcBackend, 10);
    
    // Verify registration
    QStringList componentNames = m_componentManager->getComponentNames();
    QVERIFY(componentNames.contains("VLCBackend"));
    
    // Get component back
    auto retrievedComponent = m_componentManager->getComponent("VLCBackend");
    QVERIFY(retrievedComponent != nullptr);
    QCOMPARE(retrievedComponent->componentName(), QString("VLCBackend"));
}

void TestVLCIntegration::testComponentInitialization()
{
    if (!VLCBackend::isVLCAvailable()) {
        QSKIP("libVLC not available on this system");
    }
    
    m_componentManager = std::make_unique<ComponentManager>();
    
    // Create and register VLC backend
    auto vlcBackend = std::make_shared<VLCBackend>();
    m_componentManager->registerComponent(vlcBackend, 10);
    
    // Set up signal spy for initialization
    QSignalSpy initSpy(m_componentManager.get(), &ComponentManager::allComponentsInitialized);
    QSignalSpy failSpy(m_componentManager.get(), &ComponentManager::componentInitializationFailed);
    
    // Initialize all components
    bool result = m_componentManager->initializeAll();
    
    QVERIFY(result);
    QCOMPARE(initSpy.count(), 1);
    QCOMPARE(failSpy.count(), 0);
    QVERIFY(m_componentManager->areAllInitialized());
    
    // Verify VLC backend is initialized
    QVERIFY(vlcBackend->isInitialized());
    
    // Test shutdown
    m_componentManager->shutdownAll();
    QVERIFY(!vlcBackend->isInitialized());
}

QTEST_MAIN(TestVLCIntegration)
#include "test_vlc_integration.moc"