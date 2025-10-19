#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QMap>

class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QTabWidget;
class QLabel;
class QProgressBar;

/**
 * @brief Comprehensive license and legal information viewer
 * 
 * Displays software licenses, EULA, privacy policy, third-party attributions,
 * and legal compliance information for EonPlay media player.
 */
class LicenseViewer : public QDialog
{
    Q_OBJECT

public:
    enum LicenseType {
        LICENSE_MAIN_APPLICATION,
        LICENSE_THIRD_PARTY,
        LICENSE_OPEN_SOURCE,
        LICENSE_PROPRIETARY,
        LICENSE_CREATIVE_COMMONS,
        LICENSE_PUBLIC_DOMAIN
    };

    struct LicenseInfo {
        QString name;
        QString version;
        QString author;
        QString website;
        QString licenseText;
        LicenseType type;
        QString licenseUrl;
        QString description;
        bool isRequired = true;
        QStringList dependencies;
    };

    struct LegalDocument {
        QString title;
        QString content;
        QString version;
        QDateTime lastUpdated;
        QString language;
        bool requiresAcceptance = false;
        bool isAccepted = false;
    };

    explicit LicenseViewer(QWidget *parent = nullptr);
    ~LicenseViewer();

    // License management
    void addLicense(const LicenseInfo& license);
    void removeLicense(const QString& name);
    void updateLicense(const LicenseInfo& license);
    QList<LicenseInfo> getAllLicenses() const;
    LicenseInfo getLicense(const QString& name) const;

    // Legal documents
    void addLegalDocument(const QString& key, const LegalDocument& document);
    void updateLegalDocument(const QString& key, const LegalDocument& document);
    LegalDocument getLegalDocument(const QString& key) const;
    QStringList getAvailableDocuments() const;

    // Document acceptance tracking
    void setDocumentAccepted(const QString& key, bool accepted);
    bool isDocumentAccepted(const QString& key) const;
    QStringList getUnacceptedDocuments() const;
    bool areAllRequiredDocumentsAccepted() const;

    // Content loading
    void loadLicensesFromDirectory(const QString& directory);
    void loadLicenseFromFile(const QString& filePath);
    void loadEULA(const QString& filePath);
    void loadPrivacyPolicy(const QString& filePath);
    void loadThirdPartyNotices(const QString& filePath);

    // Export functionality
    void exportLicensesToFile(const QString& filePath) const;
    void exportLegalDocumentsToFile(const QString& filePath) const;
    QString generateAttributionText() const;
    QString generateComplianceReport() const;

    // Display customization
    void setShowOnlyRequired(bool showOnly);
    bool isShowOnlyRequired() const;
    void setGroupByType(bool group);
    bool isGroupByType() const;
    void setSearchFilter(const QString& filter);
    QString getSearchFilter() const;

public slots:
    void showLicense(const QString& name);
    void showEULA();
    void showPrivacyPolicy();
    void showThirdPartyNotices();
    void acceptCurrentDocument();
    void declineCurrentDocument();
    void exportAllLicenses();
    void printCurrentDocument();

signals:
    void documentAccepted(const QString& key);
    void documentDeclined(const QString& key);
    void allDocumentsAccepted();
    void licenseViewed(const QString& name);
    void exportCompleted(const QString& filePath);

private slots:
    void onLicenseSelectionChanged();
    void onDocumentSelectionChanged();
    void onSearchTextChanged(const QString& text);
    void onGroupingChanged(bool enabled);
    void onFilterChanged();
    void onTabChanged(int index);

private:
    // UI setup
    void setupUI();
    void setupLicenseTab();
    void setupDocumentsTab();
    void setupComplianceTab();
    void setupMenuBar();
    void setupToolBar();
    
    // License display
    void populateLicenseTree();
    void displayLicense(const LicenseInfo& license);
    void filterLicenses();
    void groupLicensesByType();
    
    // Document display
    void populateDocumentList();
    void displayDocument(const LegalDocument& document);
    void updateAcceptanceStatus();
    
    // Compliance display
    void updateComplianceInfo();
    void generateComplianceReport();
    void checkLegalCompliance();
    
    // Content loading helpers
    LicenseInfo parseLicenseFile(const QString& filePath) const;
    LegalDocument parseDocumentFile(const QString& filePath) const;
    QString detectLicenseType(const QString& content) const;
    
    // Export helpers
    void exportToPlainText(const QString& filePath) const;
    void exportToHTML(const QString& filePath) const;
    void exportToPDF(const QString& filePath) const;
    
    // Search and filter helpers
    bool matchesFilter(const LicenseInfo& license) const;
    bool matchesFilter(const LegalDocument& document) const;
    void highlightSearchTerms(const QString& text);
    
    // UI components
    QTabWidget* m_tabWidget;
    
    // License tab
    QTreeWidget* m_licenseTree;
    QTextEdit* m_licenseDisplay;
    QPushButton* m_exportLicensesButton;
    QPushButton* m_printLicenseButton;
    
    // Documents tab
    QTreeWidget* m_documentTree;
    QTextEdit* m_documentDisplay;
    QPushButton* m_acceptButton;
    QPushButton* m_declineButton;
    QLabel* m_acceptanceStatus;
    
    // Compliance tab
    QTextEdit* m_complianceDisplay;
    QProgressBar* m_complianceProgress;
    QLabel* m_complianceStatus;
    QPushButton* m_generateReportButton;
    
    // Search and filter
    QLineEdit* m_searchEdit;
    QCheckBox* m_showOnlyRequiredCheck;
    QCheckBox* m_groupByTypeCheck;
    
    // Data storage
    QMap<QString, LicenseInfo> m_licenses;
    QMap<QString, LegalDocument> m_legalDocuments;
    QMap<QString, bool> m_documentAcceptance;
    
    // Display settings
    bool m_showOnlyRequired;
    bool m_groupByType;
    QString m_searchFilter;
    QString m_currentLicense;
    QString m_currentDocument;
    
    // Built-in content
    void loadBuiltInLicenses();
    void loadBuiltInDocuments();
    
    // Constants
    static const QString EULA_KEY;
    static const QString PRIVACY_POLICY_KEY;
    static const QString THIRD_PARTY_NOTICES_KEY;
    static const QString OPEN_SOURCE_LICENSES_KEY;
};