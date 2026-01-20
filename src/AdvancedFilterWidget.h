/**
 * @file AdvancedFilterWidget.h
 * @brief Advanced Log Filtering UI Widget
 * @description Provides filtering controls for log level, time range, and keywords
 */

#ifndef ADVANCEDFILTERWIDGET_H
#define ADVANCEDFILTERWIDGET_H

#include <QWidget>
#include <QDateTime>

class QDateTimeEdit;
class QComboBox;
class QLineEdit;
class QRadioButton;
class QPushButton;
class QCheckBox;
class QLabel;

/**
 * @class AdvancedFilterWidget
 * @brief Widget providing advanced filtering controls for log analysis
 * 
 * Features:
 * - Time range filtering (Start/End datetime)
 * - Log level filtering (ALL, TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
 * - Keyword filtering (space-separated, AND/OR logic)
 * - Real-time filter application
 */
class AdvancedFilterWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Log level filter options
     */
    enum class LogLevel {
        All = 0,
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };
    Q_ENUM(LogLevel)

    /**
     * @brief Keyword matching logic
     */
    enum class KeywordLogic {
        And,  // All keywords must match
        Or    // Any keyword can match
    };
    Q_ENUM(KeywordLogic)

    explicit AdvancedFilterWidget(QWidget *parent = nullptr);
    ~AdvancedFilterWidget() override = default;

    // ===================== Filter State Getters =====================

    /**
     * @brief Check if time range filter is enabled
     */
    bool isTimeRangeEnabled() const;

    /**
     * @brief Get start time for filtering
     */
    QDateTime startTime() const;

    /**
     * @brief Get end time for filtering
     */
    QDateTime endTime() const;

    /**
     * @brief Get selected log level filter
     */
    LogLevel selectedLevel() const;

    /**
     * @brief Get the level filter as string for matching
     */
    QString selectedLevelString() const;

    /**
     * @brief Get keywords as list (space-separated input)
     */
    QStringList keywords() const;

    /**
     * @brief Get keyword matching logic
     */
    KeywordLogic keywordLogic() const;

    /**
     * @brief Check if regex mode is enabled for keywords
     */
    bool isRegexEnabled() const;

    /**
     * @brief Check if case-sensitive matching is enabled
     */
    bool isCaseSensitive() const;

    // ===================== Filter State Setters =====================

    /**
     * @brief Set time range
     */
    void setTimeRange(const QDateTime &start, const QDateTime &end);

    /**
     * @brief Set log level filter
     */
    void setLevel(LogLevel level);

    /**
     * @brief Set keyword text
     */
    void setKeywords(const QString &keywords);

    /**
     * @brief Clear all filters
     */
    Q_SLOT void clearFilters();

signals:
    /**
     * @brief Emitted when user clicks Apply or changes any filter
     */
    void filterRequested();

    /**
     * @brief Emitted when filters are cleared
     */
    void filterCleared();

    /**
     * @brief Emitted when any filter value changes (for live filtering)
     */
    void filterChanged();

private slots:
    void onApplyClicked();
    void onClearClicked();
    void onFilterValueChanged();

private:
    void setupUi();
    void applyDarkTheme();
    void connectSignals();

    // Time Range Controls
    QCheckBox *m_timeRangeCheckBox = nullptr;
    QDateTimeEdit *m_startTimeEdit = nullptr;
    QDateTimeEdit *m_endTimeEdit = nullptr;
    QLabel *m_timeToLabel = nullptr;

    // Level Controls
    QLabel *m_levelLabel = nullptr;
    QComboBox *m_levelCombo = nullptr;

    // Keyword Controls
    QLabel *m_keywordLabel = nullptr;
    QLineEdit *m_keywordEdit = nullptr;
    QRadioButton *m_andRadio = nullptr;
    QRadioButton *m_orRadio = nullptr;
    QCheckBox *m_regexCheckBox = nullptr;
    QCheckBox *m_caseSensitiveCheckBox = nullptr;

    // Mode ComboBox (替代 AND/OR 单选按钮)
    QLabel *m_modeLabel = nullptr;
    QComboBox *m_modeCombo = nullptr;

    // Action Buttons
    QPushButton *m_applyBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;


    // Settings
    bool m_liveFilterEnabled = false;  // Filter on every change vs manual apply
};

#endif // ADVANCEDFILTERWIDGET_H
