/**
 * @file AdvancedFilterWidget.cpp
 * @brief Advanced Log Filtering UI Widget Implementation
 */

#include "AdvancedFilterWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QDateTimeEdit>
#include <QComboBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QButtonGroup>
#include <QGroupBox>
#include <QFrame>

// ============================================================================
// Constructor
// ============================================================================

AdvancedFilterWidget::AdvancedFilterWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    applyDarkTheme();
    connectSignals();
}

// ============================================================================
// UI Setup
// ============================================================================

void AdvancedFilterWidget::setupUi()
{
    // Main horizontal layout
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(16);

    // ========== Time Range Section ==========
    QHBoxLayout *timeLayout = new QHBoxLayout();
    timeLayout->setSpacing(6);
    
    m_timeRangeCheckBox = new QCheckBox(tr("Time:"), this);
    m_timeRangeCheckBox->setToolTip(tr("Enable time range filtering"));
    timeLayout->addWidget(m_timeRangeCheckBox);
    
    m_startTimeEdit = new QDateTimeEdit(this);
    m_startTimeEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_startTimeEdit->setDateTime(QDateTime::currentDateTime().addDays(-1));
    m_startTimeEdit->setCalendarPopup(true);
    m_startTimeEdit->setEnabled(false);
    m_startTimeEdit->setMinimumWidth(150);
    timeLayout->addWidget(m_startTimeEdit);
    
    m_timeToLabel = new QLabel(tr("to"), this);
    timeLayout->addWidget(m_timeToLabel);
    
    m_endTimeEdit = new QDateTimeEdit(this);
    m_endTimeEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_endTimeEdit->setDateTime(QDateTime::currentDateTime());
    m_endTimeEdit->setCalendarPopup(true);
    m_endTimeEdit->setEnabled(false);
    m_endTimeEdit->setMinimumWidth(150);
    timeLayout->addWidget(m_endTimeEdit);
    
    mainLayout->addLayout(timeLayout);

    // Separator
    QFrame *sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(sep1);

    // ========== Level Section ==========
    QHBoxLayout *levelLayout = new QHBoxLayout();
    levelLayout->setSpacing(6);
    
    m_levelLabel = new QLabel(tr("Level:"), this);
    levelLayout->addWidget(m_levelLabel);
    
    m_levelCombo = new QComboBox(this);
    m_levelCombo->addItem(tr("ALL"), static_cast<int>(LogLevel::All));
    m_levelCombo->addItem(tr("TRACE"), static_cast<int>(LogLevel::Trace));
    m_levelCombo->addItem(tr("DEBUG"), static_cast<int>(LogLevel::Debug));
    m_levelCombo->addItem(tr("INFO"), static_cast<int>(LogLevel::Info));
    m_levelCombo->addItem(tr("WARN"), static_cast<int>(LogLevel::Warn));
    m_levelCombo->addItem(tr("ERROR"), static_cast<int>(LogLevel::Error));
    m_levelCombo->addItem(tr("FATAL"), static_cast<int>(LogLevel::Fatal));
    m_levelCombo->setCurrentIndex(0);
    m_levelCombo->setMinimumWidth(80);
    levelLayout->addWidget(m_levelCombo);
    
    mainLayout->addLayout(levelLayout);

    // Separator
    QFrame *sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(sep2);

    // ========== Keyword Section ==========
    QHBoxLayout *keywordLayout = new QHBoxLayout();
    keywordLayout->setSpacing(6);
    
    m_keywordLabel = new QLabel(tr("Keywords:"), this);
    keywordLayout->addWidget(m_keywordLabel);
    
    m_keywordEdit = new QLineEdit(this);
    m_keywordEdit->setPlaceholderText(tr("Space-separated keywords..."));
    m_keywordEdit->setMinimumWidth(200);
    m_keywordEdit->setClearButtonEnabled(true);
    keywordLayout->addWidget(m_keywordEdit);
    
    // AND/OR radio buttons
    m_andRadio = new QRadioButton(tr("AND"), this);
    m_andRadio->setToolTip(tr("All keywords must match"));
    m_andRadio->setChecked(true);
    keywordLayout->addWidget(m_andRadio);
    
    m_orRadio = new QRadioButton(tr("OR"), this);
    m_orRadio->setToolTip(tr("Any keyword can match"));
    keywordLayout->addWidget(m_orRadio);
    
    QButtonGroup *logicGroup = new QButtonGroup(this);
    logicGroup->addButton(m_andRadio);
    logicGroup->addButton(m_orRadio);
    
    mainLayout->addLayout(keywordLayout);

    // ========== Options Section ==========
    QHBoxLayout *optionsLayout = new QHBoxLayout();
    optionsLayout->setSpacing(8);
    
    m_regexCheckBox = new QCheckBox(tr("Regex"), this);
    m_regexCheckBox->setToolTip(tr("Use regular expressions for keyword matching"));
    optionsLayout->addWidget(m_regexCheckBox);
    
    m_caseSensitiveCheckBox = new QCheckBox(tr("Case"), this);
    m_caseSensitiveCheckBox->setToolTip(tr("Case-sensitive matching"));
    optionsLayout->addWidget(m_caseSensitiveCheckBox);
    
    mainLayout->addLayout(optionsLayout);

    // ========== Action Buttons ==========
    mainLayout->addStretch();
    
    m_applyBtn = new QPushButton(tr("Apply"), this);
    m_applyBtn->setToolTip(tr("Apply filters (Enter)"));
    m_applyBtn->setDefault(true);
    m_applyBtn->setMinimumWidth(70);
    mainLayout->addWidget(m_applyBtn);
    
    m_clearBtn = new QPushButton(tr("Clear"), this);
    m_clearBtn->setToolTip(tr("Clear all filters"));
    m_clearBtn->setMinimumWidth(70);
    mainLayout->addWidget(m_clearBtn);

    // Set fixed height for toolbar-like appearance
    setFixedHeight(42);
}

void AdvancedFilterWidget::applyDarkTheme()
{
    setStyleSheet(R"(
        AdvancedFilterWidget {
            background-color: #2d2d30;
            border-bottom: 1px solid #3c3c3c;
        }
        
        QLabel {
            color: #d4d4d4;
            font-weight: 500;
        }
        
        QCheckBox {
            color: #d4d4d4;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
        }
        QCheckBox::indicator:unchecked {
            border: 1px solid #555555;
            background-color: #3c3c3c;
        }
        QCheckBox::indicator:checked {
            border: 1px solid #007acc;
            background-color: #007acc;
        }
        
        QRadioButton {
            color: #d4d4d4;
        }
        QRadioButton::indicator {
            width: 14px;
            height: 14px;
        }
        
        QLineEdit {
            background-color: #3c3c3c;
            color: #d4d4d4;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 4px 8px;
        }
        QLineEdit:focus {
            border-color: #007acc;
        }
        
        QComboBox {
            background-color: #3c3c3c;
            color: #d4d4d4;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 4px 8px;
        }
        QComboBox:hover {
            border-color: #007acc;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #252526;
            color: #d4d4d4;
            border: 1px solid #555555;
            selection-background-color: #094771;
        }
        
        QDateTimeEdit {
            background-color: #3c3c3c;
            color: #d4d4d4;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 4px 8px;
        }
        QDateTimeEdit:focus {
            border-color: #007acc;
        }
        QDateTimeEdit:disabled {
            background-color: #2d2d30;
            color: #808080;
        }
        
        QPushButton {
            background-color: #3c3c3c;
            color: #d4d4d4;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px 12px;
        }
        QPushButton:hover {
            background-color: #505050;
            border-color: #007acc;
        }
        QPushButton:pressed {
            background-color: #2d2d30;
        }
        
        QPushButton#applyBtn {
            background-color: #0e639c;
            color: white;
            border: none;
            font-weight: bold;
        }
        QPushButton#applyBtn:hover {
            background-color: #1177bb;
        }
        
        QFrame[frameShape="5"] {  /* VLine */
            background-color: #555555;
            max-width: 1px;
        }
    )");
    
    m_applyBtn->setObjectName(QStringLiteral("applyBtn"));
}

void AdvancedFilterWidget::connectSignals()
{
    // Time range checkbox enables/disables time edits
    connect(m_timeRangeCheckBox, &QCheckBox::toggled, m_startTimeEdit, &QWidget::setEnabled);
    connect(m_timeRangeCheckBox, &QCheckBox::toggled, m_endTimeEdit, &QWidget::setEnabled);
    connect(m_timeRangeCheckBox, &QCheckBox::toggled, this, &AdvancedFilterWidget::onFilterValueChanged);
    
    // Time edits
    connect(m_startTimeEdit, &QDateTimeEdit::dateTimeChanged, this, &AdvancedFilterWidget::onFilterValueChanged);
    connect(m_endTimeEdit, &QDateTimeEdit::dateTimeChanged, this, &AdvancedFilterWidget::onFilterValueChanged);
    
    // Level combo
    connect(m_levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AdvancedFilterWidget::onFilterValueChanged);
    
    // Keywords - apply on Enter
    connect(m_keywordEdit, &QLineEdit::returnPressed, this, &AdvancedFilterWidget::onApplyClicked);
    connect(m_keywordEdit, &QLineEdit::textChanged, this, &AdvancedFilterWidget::onFilterValueChanged);
    
    // Radio buttons
    connect(m_andRadio, &QRadioButton::toggled, this, &AdvancedFilterWidget::onFilterValueChanged);
    
    // Checkboxes
    connect(m_regexCheckBox, &QCheckBox::toggled, this, &AdvancedFilterWidget::onFilterValueChanged);
    connect(m_caseSensitiveCheckBox, &QCheckBox::toggled, this, &AdvancedFilterWidget::onFilterValueChanged);
    
    // Buttons
    connect(m_applyBtn, &QPushButton::clicked, this, &AdvancedFilterWidget::onApplyClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &AdvancedFilterWidget::onClearClicked);
}

// ============================================================================
// Slots
// ============================================================================

void AdvancedFilterWidget::onApplyClicked()
{
    emit filterRequested();
}

void AdvancedFilterWidget::onClearClicked()
{
    clearFilters();
}

void AdvancedFilterWidget::onFilterValueChanged()
{
    emit filterChanged();
    
    // If live filtering is enabled, also emit filterRequested
    if (m_liveFilterEnabled) {
        emit filterRequested();
    }
}

// ============================================================================
// Getters
// ============================================================================

bool AdvancedFilterWidget::isTimeRangeEnabled() const
{
    return m_timeRangeCheckBox->isChecked();
}

QDateTime AdvancedFilterWidget::startTime() const
{
    return m_startTimeEdit->dateTime();
}

QDateTime AdvancedFilterWidget::endTime() const
{
    return m_endTimeEdit->dateTime();
}

AdvancedFilterWidget::LogLevel AdvancedFilterWidget::selectedLevel() const
{
    return static_cast<LogLevel>(m_levelCombo->currentData().toInt());
}

QString AdvancedFilterWidget::selectedLevelString() const
{
    LogLevel level = selectedLevel();
    switch (level) {
    case LogLevel::Trace: return QStringLiteral("TRACE");
    case LogLevel::Debug: return QStringLiteral("DEBUG");
    case LogLevel::Info:  return QStringLiteral("INFO");
    case LogLevel::Warn:  return QStringLiteral("WARN");
    case LogLevel::Error: return QStringLiteral("ERROR");
    case LogLevel::Fatal: return QStringLiteral("FATAL");
    default:              return QString();  // ALL = no filter
    }
}

QStringList AdvancedFilterWidget::keywords() const
{
    QString text = m_keywordEdit->text().trimmed();
    if (text.isEmpty()) {
        return QStringList();
    }
    
    // Split by whitespace but preserve quoted strings
    QStringList result;
    QRegularExpression regex(QStringLiteral("\"([^\"]+)\"|'([^']+)'|(\\S+)"));
    QRegularExpressionMatchIterator it = regex.globalMatch(text);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        if (match.captured(1).length() > 0) {
            result.append(match.captured(1));  // Double-quoted
        } else if (match.captured(2).length() > 0) {
            result.append(match.captured(2));  // Single-quoted
        } else {
            result.append(match.captured(3));  // Unquoted word
        }
    }
    
    return result;
}

AdvancedFilterWidget::KeywordLogic AdvancedFilterWidget::keywordLogic() const
{
    return m_andRadio->isChecked() ? KeywordLogic::And : KeywordLogic::Or;
}

bool AdvancedFilterWidget::isRegexEnabled() const
{
    return m_regexCheckBox->isChecked();
}

bool AdvancedFilterWidget::isCaseSensitive() const
{
    return m_caseSensitiveCheckBox->isChecked();
}

// ============================================================================
// Setters
// ============================================================================

void AdvancedFilterWidget::setTimeRange(const QDateTime &start, const QDateTime &end)
{
    m_startTimeEdit->setDateTime(start);
    m_endTimeEdit->setDateTime(end);
    m_timeRangeCheckBox->setChecked(true);
}

void AdvancedFilterWidget::setLevel(LogLevel level)
{
    int index = m_levelCombo->findData(static_cast<int>(level));
    if (index >= 0) {
        m_levelCombo->setCurrentIndex(index);
    }
}

void AdvancedFilterWidget::setKeywords(const QString &keywords)
{
    m_keywordEdit->setText(keywords);
}

void AdvancedFilterWidget::clearFilters()
{
    m_timeRangeCheckBox->setChecked(false);
    m_startTimeEdit->setDateTime(QDateTime::currentDateTime().addDays(-1));
    m_endTimeEdit->setDateTime(QDateTime::currentDateTime());
    m_levelCombo->setCurrentIndex(0);  // ALL
    m_keywordEdit->clear();
    m_andRadio->setChecked(true);
    m_regexCheckBox->setChecked(false);
    m_caseSensitiveCheckBox->setChecked(false);
    
    emit filterCleared();
}
