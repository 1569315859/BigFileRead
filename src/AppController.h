#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QSettings>
#include <QUrl>

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isRegistered READ isRegistered NOTIFY licenseChanged)
    Q_PROPERTY(int trialDaysRemaining READ trialDaysRemaining CONSTANT)
    Q_PROPERTY(bool isTrialExpired READ isTrialExpired CONSTANT)
    Q_PROPERTY(QString machineId READ machineId CONSTANT)
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    bool isRegistered() const;
    int trialDaysRemaining() const;
    bool isTrialExpired() const;
    QString machineId() const;

    Q_INVOKABLE bool activateLicense(const QString &key);
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void openUrl(const QString &url);
    
    // File dialog helper for macOS compatibility
    Q_INVOKABLE QString urlToLocalPath(const QUrl &url) const;
    
    // Recent files management
    QStringList recentFiles() const;
    Q_INVOKABLE void addRecentFile(const QString &filePath);
    Q_INVOKABLE void clearRecentFiles();
    Q_INVOKABLE QString getFileName(const QString &filePath) const;

signals:
    void licenseChanged();
    void recentFilesChanged();
    
private:
    static constexpr int MAX_RECENT_FILES = 10;
    QSettings m_settings;
    QStringList m_recentFiles;
};

#endif // APPCONTROLLER_H
