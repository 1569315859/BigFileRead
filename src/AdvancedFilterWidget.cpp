/**
 * @file AdvancedFilterWidget.cpp
 * @brief Advanced Log Filtering UI Widget Implementation
 * @version 2.0 - 优化布局和统一高度
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
    // ========== 布局设置 ==========
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 5, 10, 5);
    mainLayout->setSpacing(8);
    mainLayout->setAlignment(Qt::AlignVCenter);  // 垂直居中对齐

    // ========== 固定高度常量 ==========
    const int FIXED_HEIGHT = 30;

    // ========== 筛选标签 ==========
    QLabel *filterLabel = new QLabel(tr("筛选:"), this);
    filterLabel->setAlignment(Qt::AlignVCenter);
    mainLayout->addWidget(filterLabel);

    // ========== Level Section ==========
    m_levelLabel = new QLabel(tr("级别:"), this);
    m_levelLabel->setAlignment(Qt::AlignVCenter);
    mainLayout->addWidget(m_levelLabel);
    
    m_levelCombo = new QComboBox(this);
    m_levelCombo->addItem(tr("ALL"), static_cast<int>(LogLevel::All));
    m_levelCombo->addItem(tr("TRACE"), static_cast<int>(LogLevel::Trace));
    m_levelCombo->addItem(tr("DEBUG"), static_cast<int>(LogLevel::Debug));
    m_levelCombo->addItem(tr("INFO"), static_cast<int>(LogLevel::Info));
    m_levelCombo->addItem(tr("WARN"), static_cast<int>(LogLevel::Warn));
    m_levelCombo->addItem(tr("ERROR"), static_cast<int>(LogLevel::Error));
    m_levelCombo->addItem(tr("FATAL"), static_cast<int>(LogLevel::Fatal));
    m_levelCombo->setCurrentIndex(0);
    mainLayout->addWidget(m_levelCombo);

    // ========== Keyword Section ==========
    m_keywordLabel = new QLabel(tr("关键词:"), this);
    m_keywordLabel->setAlignment(Qt::AlignVCenter);
    mainLayout->addWidget(m_keywordLabel);
    
    m_keywordEdit = new QLineEdit(this);
    m_keywordEdit->setPlaceholderText(tr("空格分隔，\"引号\"保留短语"));
    m_keywordEdit->setClearButtonEnabled(true);
    m_keywordEdit->setMinimumWidth(200);
    mainLayout->addWidget(m_keywordEdit, 1);  // 伸展因子=1

    // ========== Mode ComboBox ==========
    m_modeLabel = new QLabel(tr("模式:"), this);
    m_modeLabel->setAlignment(Qt::AlignVCenter);
    mainLayout->addWidget(m_modeLabel);
    
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("全部 (AND)"), true);
    m_modeCombo->addItem(tr("任意 (OR)"), false);
    m_modeCombo->setCurrentIndex(0);
    m_modeCombo->setToolTip(tr("AND = 所有关键词匹配, OR = 任意关键词匹配"));
    mainLayout->addWidget(m_modeCombo);

    // ========== Options ==========
    m_regexCheckBox = new QCheckBox(tr("正则"), this);
    m_regexCheckBox->setToolTip(tr("使用正则表达式"));
    mainLayout->addWidget(m_regexCheckBox);

    // ========== Action Buttons ==========
    m_applyBtn = new QPushButton(tr("应用"), this);
    m_applyBtn->setToolTip(tr("应用筛选 (Enter)"));
    m_applyBtn->setDefault(true);
    m_applyBtn->setMinimumWidth(60);
    mainLayout->addWidget(m_applyBtn);
    
    m_clearBtn = new QPushButton(tr("清除"), this);
    m_clearBtn->setToolTip(tr("清除筛选条件"));
    m_clearBtn->setMinimumWidth(60);
    mainLayout->addWidget(m_clearBtn);

    // ========== *** 关键：C++ 硬编码强制高度 *** ==========
    // 遍历所有需要固定高度的控件
    QList<QWidget*> fixedHeightWidgets = {
        m_levelCombo,
        m_keywordEdit,
        m_modeCombo,
        m_applyBtn,
        m_clearBtn
    };
    
    for (QWidget* w : fixedHeightWidgets) {
        if (w) {
            w->setFixedHeight(FIXED_HEIGHT);  // 强制精确高度
            w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);  // 水平可伸展，垂直固定
        }
    }
    
    // CheckBox 也需要固定高度
    m_regexCheckBox->setFixedHeight(FIXED_HEIGHT);
    m_regexCheckBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // 设置工具栏总高度 (30px + 5*2 边距)
    setFixedHeight(42);
    
    // ========== 保留隐藏的旧控件 ==========
    m_timeRangeCheckBox = new QCheckBox(this);
    m_timeRangeCheckBox->hide();
    m_startTimeEdit = new QDateTimeEdit(this);
    m_startTimeEdit->hide();
    m_endTimeEdit = new QDateTimeEdit(this);
    m_endTimeEdit->hide();
    m_timeToLabel = new QLabel(this);
    m_timeToLabel->hide();
    m_andRadio = new QRadioButton(this);
    m_andRadio->setChecked(true);
    m_andRadio->hide();
    m_orRadio = new QRadioButton(this);
    m_orRadio->hide();
    m_caseSensitiveCheckBox = new QCheckBox(this);
    m_caseSensitiveCheckBox->hide();
}

void AdvancedFilterWidget::applyDarkTheme()
{
    // *** 扁平化样式：移除原生边框，高度由 C++ 控制 ***
    setStyleSheet(
        // Widget 容器背景
        "AdvancedFilterWidget {"
        "   background-color: #2b2b2b;"
        "   border-bottom: 1px solid #3c3c3c;"
        "}"
        
        // 标签样式
        "AdvancedFilterWidget QLabel {"
        "   color: #e0e0e0;"
        "   font-size: 13px;"
        "   font-weight: 500;"
        "   padding: 0px;"
        "}"
        
        // *** 关键：扁平化控件，移除原生边框 ***
        "AdvancedFilterWidget QComboBox,"
        "AdvancedFilterWidget QLineEdit,"
        "AdvancedFilterWidget QPushButton,"
        "AdvancedFilterWidget QDateTimeEdit {"
        "   border: 1px solid #555;"   /* 替换原生 3D 边框 */
        "   border-radius: 4px;"
        "   background-color: #333;"
        "   color: white;"
        "   padding-left: 5px;"
        "   margin: 0px;"              /* 移除外边距，由布局控制 */
        "}"

        // 修正 ComboBox 文本对齐
        "AdvancedFilterWidget QComboBox {"
        "   padding-right: 20px;"      /* 为箭头预留空间 */
        "}"

        // LineEdit 文本垂直居中微调
        "AdvancedFilterWidget QLineEdit {"
        "   padding-bottom: 2px;"
        "}"
        
        // Hover 效果 (蓝色边框)
        "AdvancedFilterWidget QPushButton:hover, "
        "AdvancedFilterWidget QLineEdit:hover, "
        "AdvancedFilterWidget QComboBox:hover {"
        "   border: 1px solid #3a86ff;"
        "}"
        
        // Focus 效果
        "AdvancedFilterWidget QLineEdit:focus {"
        "   border: 1px solid #007acc;"
        "}"
        
        // ComboBox 下拉箭头区域
        "AdvancedFilterWidget QComboBox::drop-down {"
        "   border: none;"
        "   width: 20px;"
        "   subcontrol-origin: padding;"
        "   subcontrol-position: right center;"
        "}"
        "AdvancedFilterWidget QComboBox::down-arrow {"
        "   image: none;"
        "   border-left: 5px solid transparent;"
        "   border-right: 5px solid transparent;"
        "   border-top: 6px solid #888888;"
        "   margin-right: 6px;"
        "}"
        "AdvancedFilterWidget QComboBox::down-arrow:hover {"
        "   border-top-color: #e0e0e0;"
        "}"
        
        // ComboBox 下拉列表
        "AdvancedFilterWidget QComboBox QAbstractItemView {"
        "   background-color: #252526;"
        "   color: #e0e0e0;"
        "   border: 1px solid #555555;"
        "   selection-background-color: #094771;"
        "   outline: none;"
        "}"
        
        // 按钮特定样式
        "AdvancedFilterWidget QPushButton {"
        "   background-color: #444444;"
        "   font-weight: bold;"
        "}"
        "AdvancedFilterWidget QPushButton:pressed {"
        "   background-color: #222222;"
        "}"
        
        // 应用按钮（蓝色高亮）
        "AdvancedFilterWidget QPushButton#applyBtn {"
        "   background-color: #0e639c;"
        "   color: white;"
        "   border: none;"
        "}"
        "AdvancedFilterWidget QPushButton#applyBtn:hover {"
        "   background-color: #1177bb;"
        "}"
        "AdvancedFilterWidget QPushButton#applyBtn:pressed {"
        "   background-color: #094771;"
        "}"
        
        // CheckBox 样式
        "AdvancedFilterWidget QCheckBox {"
        "   color: #e0e0e0;"
        "   font-size: 13px;"
        "   spacing: 6px;"
        "}"
        "AdvancedFilterWidget QCheckBox::indicator {"
        "   width: 16px;"
        "   height: 16px;"
        "   border: 1px solid #555555;"
        "   border-radius: 3px;"
        "   background-color: #3c3c3c;"
        "}"
        "AdvancedFilterWidget QCheckBox::indicator:checked {"
        "   background-color: #007acc;"
        "   border-color: #007acc;"
        "}"
        "AdvancedFilterWidget QCheckBox::indicator:hover {"
        "   border-color: #3a86ff;"
        "}"
    );
    
    m_applyBtn->setObjectName(QStringLiteral("applyBtn"));
}

void AdvancedFilterWidget::connectSignals()
{
    // Level combo
    connect(m_levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AdvancedFilterWidget::onFilterValueChanged);
    
    // Mode combo
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AdvancedFilterWidget::onFilterValueChanged);
    
    // Keywords - apply on Enter
    connect(m_keywordEdit, &QLineEdit::returnPressed, this, &AdvancedFilterWidget::onApplyClicked);
    connect(m_keywordEdit, &QLineEdit::textChanged, this, &AdvancedFilterWidget::onFilterValueChanged);
    
    // Checkboxes
    connect(m_regexCheckBox, &QCheckBox::toggled, this, &AdvancedFilterWidget::onFilterValueChanged);
    
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
    // 使用新的 ComboBox 来获取逻辑模式
    if (m_modeCombo) {
        bool isAnd = m_modeCombo->currentData().toBool();
        return isAnd ? KeywordLogic::And : KeywordLogic::Or;
    }
    // 回退到旧的 RadioButton（如果存在）
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
    if (m_modeCombo) {
        m_modeCombo->setCurrentIndex(0);  // AND
    }
    m_andRadio->setChecked(true);
    m_regexCheckBox->setChecked(false);
    m_caseSensitiveCheckBox->setChecked(false);
    
    emit filterCleared();
}
