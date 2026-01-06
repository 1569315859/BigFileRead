/**
 * @file RegistrationDialog.h
 * @brief License Registration Dialog - Programmatic UI
 */

#ifndef REGISTRATIONDIALOG_H
#define REGISTRATIONDIALOG_H

#include <QDialog>

class QLineEdit;
class QPushButton;
class QLabel;

/**
 * @class RegistrationDialog
 * @brief Dialog for license registration and activation
 * 
 * Features:
 * - Displays machine ID for license request
 * - Accepts license key input
 * - Verifies and activates license
 * - Link to web registration
 */
class RegistrationDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit RegistrationDialog(QWidget *parent = nullptr);
    ~RegistrationDialog() override = default;

    /**
     * @brief Get the entered license key
     * @return The license key entered by user
     */
    QString getLicenseKey() const;

private slots:
    /**
     * @brief Handle Activate button click
     */
    void onActivateClicked();

    /**
     * @brief Handle Get License button click (open web page)
     */
    void onGetLicenseClicked();

    /**
     * @brief Copy machine ID to clipboard
     */
    void onCopyMachineId();

private:
    /**
     * @brief Setup the UI programmatically
     */
    void setupUi();

    /**
     * @brief Apply VS Code dark theme styling
     */
    void applyDarkTheme();

    // UI Elements
    QLineEdit *m_machineIdEdit = nullptr;
    QLineEdit *m_licenseKeyEdit = nullptr;
    QPushButton *m_copyBtn = nullptr;
    QPushButton *m_getLicenseBtn = nullptr;
    QPushButton *m_activateBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
};

#endif // REGISTRATIONDIALOG_H
