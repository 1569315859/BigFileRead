/**
 * @file SearchBar.cpp
 * @brief 浮动搜索栏实现 - VS Code 风格
 * @version 1.1 - 添加正则表达式支持
 */

#include "SearchBar.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

SearchBar::SearchBar(QWidget *parent) : QWidget(parent) {
  setupUi();
  applyStyle();

  // 连接信号
  connect(m_searchInput, &QLineEdit::textChanged, this,
          &SearchBar::onSearchTextChanged);
  connect(m_searchInput, &QLineEdit::returnPressed, this,
          &SearchBar::onNextClicked);
  connect(m_prevButton, &QToolButton::clicked, this,
          &SearchBar::onPreviousClicked);
  connect(m_nextButton, &QToolButton::clicked, this, &SearchBar::onNextClicked);
  connect(m_closeButton, &QToolButton::clicked, this,
          &SearchBar::onCloseClicked);
  connect(m_regexButton, &QToolButton::toggled, this,
          &SearchBar::onRegexToggled);
}

SearchBar::~SearchBar() {}

void SearchBar::setupUi() {
  // 水平布局 - VS Code 风格
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setContentsMargins(10, 8, 10, 8);
  layout->setSpacing(6);

  // 搜索输入框 - 更宽，更易用
  m_searchInput = new QLineEdit(this);
  m_searchInput->setPlaceholderText(tr("Find"));
  m_searchInput->setMinimumWidth(200);
  m_searchInput->setFixedHeight(26);
  layout->addWidget(m_searchInput);

  // 正则表达式切换按钮 [.*]
  m_regexButton = new QToolButton(this);
  m_regexButton->setText(".*");
  m_regexButton->setToolTip(tr("Use Regular Expression"));
  m_regexButton->setCheckable(true);
  m_regexButton->setChecked(false);
  m_regexButton->setFixedSize(28, 24);
  layout->addWidget(m_regexButton);

  // Previous 按钮
  m_prevButton = new QToolButton(this);
  m_prevButton->setText("▲");
  m_prevButton->setToolTip(tr("Previous (Shift+Enter)"));
  m_prevButton->setFixedSize(24, 24);
  layout->addWidget(m_prevButton);

  // Next 按钮
  m_nextButton = new QToolButton(this);
  m_nextButton->setText("▼");
  m_nextButton->setToolTip(tr("Next (Enter)"));
  m_nextButton->setFixedSize(24, 24);
  layout->addWidget(m_nextButton);

  // 结果信息标签 - 紧凑显示
  m_resultLabel = new QLabel(tr("No results"), this);
  m_resultLabel->setMinimumWidth(70);
  m_resultLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(m_resultLabel);

  // 关闭按钮
  m_closeButton = new QToolButton(this);
  m_closeButton->setText("✕");
  m_closeButton->setToolTip(tr("Close (Esc)"));
  m_closeButton->setFixedSize(24, 24);
  layout->addWidget(m_closeButton);

  setLayout(layout);

  // 固定大小 - 类似 VS Code
  setFixedSize(480, 42);
}

void SearchBar::applyStyle() {
  // VS Code 风格搜索栏 - 简洁专业
  QString styleSheet = QStringLiteral(R"(
        SearchBar {
            background-color: #252526;
            border: 1px solid #454545;
            border-radius: 3px;
        }
        
        QLineEdit {
            background-color: #3c3c3c;
            color: #cccccc;
            border: 1px solid #3c3c3c;
            border-radius: 2px;
            padding: 3px 8px;
            selection-background-color: #264f78;
            font-size: 13px;
        }
        QLineEdit:focus {
            border: 1px solid #007acc;
            background-color: #3c3c3c;
        }
        
        QLabel {
            color: #969696;
            font-size: 11px;
            background-color: transparent;
            padding: 0px 4px;
        }
        
        QToolButton {
            background-color: transparent;
            color: #cccccc;
            border: none;
            border-radius: 2px;
            font-size: 14px;
            padding: 2px;
        }
        QToolButton:hover {
            background-color: #4e4e4e;
        }
        QToolButton:pressed {
            background-color: #007acc;
        }
        QToolButton:checked {
            background-color: #094771;
            border: 1px solid #007acc;
            color: #ffffff;
        }
        QToolButton:disabled {
            color: #6d6d6d;
            background-color: transparent;
        }
    )");

  setStyleSheet(styleSheet);

  // 清晰的背景渲染
  setAutoFillBackground(true);
  setAttribute(Qt::WA_StyledBackground, true);
}

void SearchBar::show() {
  QWidget::show();
  m_searchInput->setFocus();
  m_searchInput->selectAll();
}

void SearchBar::updateResultInfo(int current, int total) {
  if (total == 0) {
    m_resultLabel->setText(tr("0/0"));
    m_prevButton->setEnabled(false);
    m_nextButton->setEnabled(false);
  } else {
    m_resultLabel->setText(tr("%1/%2").arg(current).arg(total));
    m_prevButton->setEnabled(total > 0);
    m_nextButton->setEnabled(total > 0);
  }
}

QString SearchBar::searchText() const { return m_searchInput->text(); }

bool SearchBar::isRegexMode() const { 
  return m_regexButton && m_regexButton->isChecked(); 
}

void SearchBar::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    onCloseClicked();
    event->accept();
  } else {
    QWidget::keyPressEvent(event);
  }
}

void SearchBar::onSearchTextChanged() {
  QString text = m_searchInput->text();
  if (!text.isEmpty()) {
    emit searchRequested(text, Next, isRegexMode());
  } else {
    updateResultInfo(0, 0);
  }
}

void SearchBar::onPreviousClicked() {
  QString text = m_searchInput->text();
  if (!text.isEmpty()) {
    emit searchRequested(text, Previous, isRegexMode());
  }
}

void SearchBar::onNextClicked() {
  QString text = m_searchInput->text();
  if (!text.isEmpty()) {
    emit searchRequested(text, Next, isRegexMode());
  }
}

void SearchBar::onCloseClicked() {
  hide();
  emit closed();
}

void SearchBar::onRegexToggled(bool checked) {
  Q_UNUSED(checked)
  // 切换正则模式时重新触发搜索
  QString text = m_searchInput->text();
  if (!text.isEmpty()) {
    emit searchRequested(text, Next, isRegexMode());
  }
}
