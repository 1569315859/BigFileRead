#include "AppController.h"
#include "LicenseManager.h"
#include "TrialManager.h"
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QGuiApplication>
#include <QFileInfo>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings("BigFileViewer", "BigFileViewer")
{
    // 从设置中加载最近文件列表
    m_recentFiles = m_settings.value("recentFiles").toStringList();
    // 清理不存在的文件
    m_recentFiles.erase(
        std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
            [](const QString &path) { return !QFileInfo::exists(path); }),
        m_recentFiles.end());
}

bool AppController::isRegistered() const {
    return LicenseManager::instance().isRegistered();
}

int AppController::trialDaysRemaining() const {
    return TrialManager::instance().daysRemaining();
}

bool AppController::isTrialExpired() const {
    return TrialManager::instance().isTrialExpired();
}

QString AppController::machineId() const {
    return LicenseManager::instance().getMachineId();
}

bool AppController::activateLicense(const QString &key) {
    bool success = LicenseManager::instance().verifyLicense(key);
    if (success) {
        emit licenseChanged();
    }
    return success;
}

void AppController::copyToClipboard(const QString &text) {
    QGuiApplication::clipboard()->setText(text);
}

void AppController::openUrl(const QString &url) {
    QDesktopServices::openUrl(QUrl(url));
}

QStringList AppController::recentFiles() const {
    return m_recentFiles;
}

void AppController::addRecentFile(const QString &filePath) {
    // 移除已存在的相同路径（避免重复）
    m_recentFiles.removeAll(filePath);
    // 添加到列表开头
    m_recentFiles.prepend(filePath);
    // 限制最大数量
    while (m_recentFiles.size() > MAX_RECENT_FILES) {
        m_recentFiles.removeLast();
    }
    // 保存到设置
    m_settings.setValue("recentFiles", m_recentFiles);
    emit recentFilesChanged();
}

void AppController::clearRecentFiles() {
    m_recentFiles.clear();
    m_settings.setValue("recentFiles", m_recentFiles);
    emit recentFilesChanged();
}

QString AppController::getFileName(const QString &filePath) const {
    return QFileInfo(filePath).fileName();
}

QString AppController::urlToLocalPath(const QUrl &url) const {
    // 将 QUrl 转换为本地文件路径
    // 这个方法处理 macOS 上的 file:// URL 格式问题
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    // 如果不是本地文件 URL，尝试直接转换
    QString path = url.toString();
    // 移除 file:// 前缀
    if (path.startsWith("file://")) {
        path = path.mid(7);
        // macOS 可能会有 localhost
        if (path.startsWith("localhost")) {
            path = path.mid(9);
        }
    }
    return path;
}
