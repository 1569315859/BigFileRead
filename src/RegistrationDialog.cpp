/**
 * @file RegistrationDialog.cpp
 * @brief License Registration Dialog Implementation
 */

#include "RegistrationDialog.h"
#include "LicenseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QDateTime>
#include <QStyle>
#include <QTimer>

// ============================================================================
// Constructor
// ============================================================================

RegistrationDialog::RegistrationDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    applyDarkTheme();
    
    // Set dialog properties
    setWindowTitle(tr("License Registration"));
    setMinimumWidth(520);
    setMinimumHeight(350);
    setModal(true);
    
    // Remove help button from title bar
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

// ============================================================================
// UI Setup
// ============================================================================

void RegistrationDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ========== Group 1: Machine Info ==========
    QGroupBox *machineGroup = new QGroupBox(tr("Machine Information"), this);
    machineGroup->setObjectName("machineGroup");
    QVBoxLayout *machineLayout = new QVBoxLayout(machineGroup);
    
    QLabel *machineLabel = new QLabel(tr("Send this Machine ID to the developer to get your license:"), machineGroup);
    machineLabel->setWordWrap(true);
    machineLayout->addWidget(machineLabel);
    
    // Machine ID display (read-only)
    QHBoxLayout *machineIdLayout = new QHBoxLayout();
    
    m_machineIdEdit = new QLineEdit(machineGroup);
    m_machineIdEdit->setText(LicenseManager::instance().getMachineId());
    m_machineIdEdit->setReadOnly(true);
    m_machineIdEdit->setObjectName("machineIdEdit");
    machineIdLayout->addWidget(m_machineIdEdit);
    
    m_copyBtn = new QPushButton(tr("Copy"), machineGroup);
    m_copyBtn->setObjectName("copyBtn");
    m_copyBtn->setToolTip(tr("Copy Machine ID to clipboard"));
    m_copyBtn->setMaximumWidth(80);
    connect(m_copyBtn, &QPushButton::clicked, this, &RegistrationDialog::onCopyMachineId);
    machineIdLayout->addWidget(m_copyBtn);
    
    machineLayout->addLayout(machineIdLayout);
    mainLayout->addWidget(machineGroup);

    // ========== Group 2: Activation ==========
    QGroupBox *activationGroup = new QGroupBox(tr("Activation"), this);
    activationGroup->setObjectName("activationGroup");
    QVBoxLayout *activationLayout = new QVBoxLayout(activationGroup);
    
    QLabel *keyLabel = new QLabel(tr("Enter your License Key:"), activationGroup);
    activationLayout->addWidget(keyLabel);
    
    m_licenseKeyEdit = new QLineEdit(activationGroup);
    m_licenseKeyEdit->setPlaceholderText(tr("Paste your license key here..."));
    m_licenseKeyEdit->setObjectName("licenseKeyEdit");
    activationLayout->addWidget(m_licenseKeyEdit);
    
    // Status label for feedback
    m_statusLabel = new QLabel(activationGroup);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setVisible(false);
    activationLayout->addWidget(m_statusLabel);
    
    mainLayout->addWidget(activationGroup);

    // ========== Spacer ==========
    mainLayout->addStretch();

    // ========== Buttons ==========
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    // Get License Button (opens web page)
    m_getLicenseBtn = new QPushButton(tr("🌐 Get License Key (Web)"), this);
    m_getLicenseBtn->setObjectName("getLicenseBtn");
    m_getLicenseBtn->setToolTip(tr("Open the registration website"));
    connect(m_getLicenseBtn, &QPushButton::clicked, this, &RegistrationDialog::onGetLicenseClicked);
    buttonLayout->addWidget(m_getLicenseBtn);
    
    buttonLayout->addStretch();
    
    // Cancel Button
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setObjectName("cancelBtn");
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelBtn);
    
    // Activate Button
    m_activateBtn = new QPushButton(tr("✓ Activate"), this);
    m_activateBtn->setObjectName("activateBtn");
    m_activateBtn->setDefault(true);
    connect(m_activateBtn, &QPushButton::clicked, this, &RegistrationDialog::onActivateClicked);
    buttonLayout->addWidget(m_activateBtn);
    
    mainLayout->addLayout(buttonLayout);
}

// ============================================================================
// Dark Theme Styling
// ============================================================================

void RegistrationDialog::applyDarkTheme()
{
    setStyleSheet(R"(
        QDialog {
            background-color: #252526;
        }
        
        QGroupBox {
            color: #d4d4d4;
            font-weight: bold;
            border: 1px solid #3c3c3c;
            border-radius: 4px;
            margin-top: 12px;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 3px 8px;
            background-color: #252526;
        }
        
        QLabel {
            color: #d4d4d4;
        }
        
        QLineEdit {
            background-color: #3c3c3c;
            color: #d4d4d4;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 8px 12px;
            font-family: Consolas, "Courier New", monospace;
            font-size: 12px;
        }
        QLineEdit:focus {
            border-color: #007acc;
        }
        QLineEdit:read-only {
            background-color: #2d2d30;
            color: #9cdcfe;
        }
        QLineEdit::placeholder {
            color: #808080;
        }
        
        QPushButton {
            background-color: #3c3c3c;
            color: #d4d4d4;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 8px 16px;
            min-width: 80px;
        }
        QPushButton:hover {
            background-color: #505050;
            border-color: #007acc;
        }
        QPushButton:pressed {
            background-color: #2d2d30;
        }
        
        QPushButton#activateBtn {
            background-color: #0e639c;
            color: white;
            border: none;
            font-weight: bold;
        }
        QPushButton#activateBtn:hover {
            background-color: #1177bb;
        }
        
        QPushButton#getLicenseBtn {
            background-color: #2d7d32;
            color: white;
            border: none;
        }
        QPushButton#getLicenseBtn:hover {
            background-color: #388e3c;
        }
        
        QPushButton#copyBtn {
            min-width: 60px;
        }
        
        QLabel#statusLabel {
            color: #f44336;
            font-weight: bold;
        }
    )");
}

// ============================================================================
// Slots
// ============================================================================

void RegistrationDialog::onActivateClicked()
{
    QString licenseKey = m_licenseKeyEdit->text().trimmed();
    
    if (licenseKey.isEmpty()) {
        m_statusLabel->setText(tr("⚠ Please enter a license key."));
        m_statusLabel->setStyleSheet("color: #ff9800; font-weight: bold;");
        m_statusLabel->setVisible(true);
        m_licenseKeyEdit->setFocus();
        return;
    }
    
    // Show processing status
    m_statusLabel->setText(tr("🔄 Verifying license..."));
    m_statusLabel->setStyleSheet("color: #2196f3; font-weight: bold;");
    m_statusLabel->setVisible(true);
    m_activateBtn->setEnabled(false);
    QApplication::processEvents();
    
    // Verify the license
    if (LicenseManager::instance().verifyLicense(licenseKey)) {
        // Save the valid license key
        QSettings settings;
        settings.setValue("license/key", licenseKey);
        settings.setValue("license/activationDate", QDateTime::currentDateTime());
        settings.setValue("license/machineId", LicenseManager::instance().getMachineId());
        settings.sync();
        
        QMessageBox::information(this, tr("Success"),
            tr("🎉 License activated successfully!\n\n"
               "Thank you for registering BigFileViewer.\n"
               "All features are now unlocked."));
        
        accept();  // Close dialog with success
    } else {
        QString error = LicenseManager::instance().lastError();
        m_statusLabel->setText(tr("❌ Invalid license key."));
        m_statusLabel->setStyleSheet("color: #f44336; font-weight: bold;");
        m_statusLabel->setVisible(true);
        m_activateBtn->setEnabled(true);
        
        QMessageBox::warning(this, tr("Activation Failed"),
            tr("The license key is not valid for this machine.\n\n"
               "Please make sure you entered the correct key.\n\n"
               "Error: %1").arg(error));
    }
}

void RegistrationDialog::onGetLicenseClicked()
{
    // CUSTOMIZE: Replace with your actual registration URL
    // You can include the machine ID as a query parameter
    QString machineId = LicenseManager::instance().getMachineId();
    QString url = QString("https://your-website.com/register?mid=%1").arg(machineId);
    
    // For now, just show a message since URL is placeholder
    QMessageBox::information(this, tr("Get License"),
        tr("To purchase a license:\n\n"
           "1. Copy your Machine ID using the 'Copy' button\n"
           "2. Visit: your-website.com/register\n"
           "3. Complete the purchase\n"
           "4. You will receive a license key via email\n\n"
           "Machine ID: %1").arg(machineId));
    
    // Uncomment to actually open the URL:
    // QDesktopServices::openUrl(QUrl(url));
}

void RegistrationDialog::onCopyMachineId()
{
    QString machineId = m_machineIdEdit->text();
    QApplication::clipboard()->setText(machineId);
    
    // Visual feedback
    m_copyBtn->setText(tr("Copied!"));
    m_copyBtn->setEnabled(false);
    
    // Reset button after 2 seconds
    QTimer::singleShot(2000, this, [this]() {
        m_copyBtn->setText(tr("Copy"));
        m_copyBtn->setEnabled(true);
    });
}

QString RegistrationDialog::getLicenseKey() const
{
    return m_licenseKeyEdit->text().trimmed();
}
