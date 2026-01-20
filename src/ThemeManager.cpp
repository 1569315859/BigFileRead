#include "ThemeManager.h"

ThemeManager::ThemeManager(QObject *parent) 
    : QObject(parent)
    , m_settings("BigFileViewer", "BigFileViewer")
{
    loadSettings();
}

ThemeManager::~ThemeManager()
{
    saveSettings();
}

void ThemeManager::loadSettings()
{
    QString savedTheme = m_settings.value("theme", "Dark").toString();
    setTheme(savedTheme);
}

void ThemeManager::saveSettings()
{
    m_settings.setValue("theme", m_currentTheme);
    m_settings.sync();
}

void ThemeManager::setTheme(const QString &themeName)
{
    if (m_currentTheme == themeName) return;

    m_currentTheme = themeName;
    applyTheme(themeName);
    saveSettings();  // 自动保存
    emit themeChanged();
}

void ThemeManager::applyTheme(const QString &themeName)
{
    if (themeName == "Light") {
        m_backgroundColor = QColor("#ffffff");
        m_panelBackground = QColor("#f3f3f3");
        m_textColor = QColor("#333333");
        m_accentColor = QColor("#007acc");
        m_borderColor = QColor("#e5e5e5");
        m_isDarkTheme = false;
    } else if (themeName == "Warm") {
        m_backgroundColor = QColor("#fdf6e3");
        m_panelBackground = QColor("#eee8d5");
        m_textColor = QColor("#657b83");
        m_accentColor = QColor("#b58900");
        m_borderColor = QColor("#d2b48c");
        m_isDarkTheme = false;  // Warm 也是浅色主题
    } else { // Dark (Default)
        m_backgroundColor = QColor("#1e1e1e");
        m_panelBackground = QColor("#252526");
        m_textColor = QColor("#d4d4d4");
        m_accentColor = QColor("#0e639c");
        m_borderColor = QColor("#3c3c3c");
        m_isDarkTheme = true;
    }
}
