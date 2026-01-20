#pragma once

#include <QObject>
#include <QColor>
#include <QSettings>

class ThemeManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY themeChanged)
    Q_PROPERTY(QColor panelBackground READ panelBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor textColor READ textColor NOTIFY themeChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY themeChanged)
    Q_PROPERTY(QColor borderColor READ borderColor NOTIFY themeChanged)
    Q_PROPERTY(QString currentTheme READ currentTheme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool isDarkTheme READ isDarkTheme NOTIFY themeChanged)

public:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager();

    QColor backgroundColor() const { return m_backgroundColor; }
    QColor panelBackground() const { return m_panelBackground; }
    QColor textColor() const { return m_textColor; }
    QColor accentColor() const { return m_accentColor; }
    QColor borderColor() const { return m_borderColor; }
    QString currentTheme() const { return m_currentTheme; }
    bool isDarkTheme() const { return m_isDarkTheme; }

    Q_INVOKABLE void setTheme(const QString &themeName);
    
    // 加载/保存配置
    void loadSettings();
    void saveSettings();

signals:
    void themeChanged();

private:
    void applyTheme(const QString &themeName);

    QColor m_backgroundColor;
    QColor m_panelBackground;
    QColor m_textColor;
    QColor m_accentColor;
    QColor m_borderColor;
    QString m_currentTheme;
    bool m_isDarkTheme = true;
    
    QSettings m_settings;
};
