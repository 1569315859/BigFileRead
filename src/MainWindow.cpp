/**
 * @file MainWindow.cpp
 * @brief 主窗口类实现
 * @version 2.0 - VS Code 风格暗色主题 + 双视图模式（原始/表格）
 */

#include "MainWindow.h"
#include "BigFileModel.h"
#include "CompactLineDelegate.h"
#include "SearchBar.h"
#include "LicenseManager.h"
#include "TrialManager.h"
#include "RegistrationDialog.h"
#include "LanguageManager.h"
#include "LogParser.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QTableView>
#include <QStackedWidget>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QStringConverter>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QSettings>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QTextStream>
#include <QUrl>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QItemSelection>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPushButton>
#include <QDialog>
#include <QHBoxLayout>
#include <QDateTime>
#include <QActionGroup>

#include <algorithm>  // for std::sort
#include <cstdlib>    // for std::exit

// VS Code Dark Theme Colors
namespace VSCodeDark {
const QString Background = "#1e1e1e";
const QString EditorBg = "#1e1e1e";
const QString SidebarBg = "#252526";
const QString StatusBarBg = "#007acc";
const QString TextColor = "#d4d4d4";
const QString TextColorDim = "#808080";
const QString HoverBg = "#2a2d2e";
const QString SelectedBg = "#094771";
const QString BorderColor = "#3c3c3c";
const QString ScrollbarBg = "#1e1e1e";
const QString ScrollbarHandle = "#424242";
const QString ProgressBarBg = "#3c3c3c";
const QString ProgressBarFill = "#0e639c";
const QString MenuBg = "#252526";
const QString MenuHover = "#094771";
} // namespace VSCodeDark

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  applyDarkTheme();
  setupUi();
  createMenus();
  updateWindowTitle();

  // 连接模型信号
  connect(m_model, &BigFileModel::fileLoaded, this, &MainWindow::onFileLoaded);
  connect(m_model, &BigFileModel::indexingProgress, this,
          &MainWindow::onIndexingProgress);
  connect(m_model, &BigFileModel::indexingCancelled, this,
          &MainWindow::onIndexingCancelled);

  // 状态更新定时器 - 实时显示行数变化
  m_statusTimer = new QTimer(this);
  m_statusTimer->setInterval(100); // 每 100ms 更新一次状态栏
  connect(m_statusTimer, &QTimer::timeout, this, [this]() {
    if (m_model && m_isLoading) {
      m_lineCountLabel->setText(tr("Lines: %1").arg(m_model->lineCount()));
    }
  });

  // 加载用户设置（必须在所有 UI 初始化完成后调用）
  loadSettings();

  // 启用拖放支持
  setAcceptDrops(true);

  // 初始化最近文件菜单
  updateRecentFilesMenu();

  // ========== License/Trial Startup Check ==========
  checkLicenseStatus();
}

MainWindow::~MainWindow() {}

// ========== 设置持久化 (QSettings) ==========

void MainWindow::loadSettings() {
  QSettings settings;

  // 恢复窗口几何信息
  if (settings.contains("geometry")) {
    restoreGeometry(settings.value("geometry").toByteArray());
  }

  // 恢复窗口状态（工具栏/Dock 位置）
  if (settings.contains("windowState")) {
    restoreState(settings.value("windowState").toByteArray());
  }

  // 恢复分隔器状态
  if (settings.contains("splitterState") && m_splitter) {
    m_splitter->restoreState(settings.value("splitterState").toByteArray());
  }

  // 恢复 Follow Tail 状态
  if (settings.contains("followTail") && m_followTailAction) {
    m_followTailAction->setChecked(settings.value("followTail").toBool());
  }

  // 恢复编码选择
  if (settings.contains("encodingIndex") && m_encodingCombo) {
    int index = settings.value("encodingIndex").toInt();
    if (index >= 0 && index < m_encodingCombo->count()) {
      m_encodingCombo->setCurrentIndex(index);
    }
  }

  // 恢复过滤栏显示状态
  if (settings.contains("filterBarVisible") && m_toggleFilterAction) {
    m_toggleFilterAction->setChecked(settings.value("filterBarVisible").toBool());
  }
}

void MainWindow::saveSettings() {
  QSettings settings;

  // 保存窗口几何信息
  settings.setValue("geometry", saveGeometry());

  // 保存窗口状态（工具栏/Dock 位置）
  settings.setValue("windowState", saveState());

  // 保存分隔器状态
  if (m_splitter) {
    settings.setValue("splitterState", m_splitter->saveState());
  }

  // 保存 Follow Tail 状态
  if (m_followTailAction) {
    settings.setValue("followTail", m_followTailAction->isChecked());
  }

  // 保存编码选择
  if (m_encodingCombo) {
    settings.setValue("encodingIndex", m_encodingCombo->currentIndex());
  }

  // 保存过滤栏显示状态
  if (m_toggleFilterAction) {
    settings.setValue("filterBarVisible", m_toggleFilterAction->isChecked());
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
  // 保存用户设置
  saveSettings();

  // 调用父类实现
  QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    // 语言发生变化，更新 UI 字符串
    updateLocalizedStrings();
  }
  
  // 调用父类实现
  QMainWindow::changeEvent(event);
}

void MainWindow::updateLocalizedStrings() {
  // ========== 更新状态栏文字 ==========
  // 状态栏会在 updateStatusBar() 中自动更新，这里只更新静态文本
  if (m_model && m_model->rowCount() == 0) {
    m_statusLabel->setText(tr("Ready"));
  }
  
  // ========== 更新工具提示 ==========
  if (m_encodingCombo) {
    m_encodingCombo->setToolTip(tr("Text Encoding"));
  }
  
  if (m_followTailAction) {
    m_followTailAction->setText(tr("Follow Tail"));
    m_followTailAction->setToolTip(tr("Auto-scroll to new content (Tail -f mode)"));
  }
  
  // ========== 更新过滤栏文字 ==========
  if (m_filterInput) {
    m_filterInput->setPlaceholderText(tr("Type keyword to filter... (Enter to apply)"));
  }
  
  if (m_regexToggleBtn) {
    m_regexToggleBtn->setToolTip(tr("Use Regular Expression"));
  }
  
  // ========== 更新详细视图占位符 ==========
  if (m_detailTextEdit) {
    m_detailTextEdit->setPlaceholderText(tr("Select a line above to view details here..."));
  }
  
  // ========== 更新窗口标题 ==========
  updateWindowTitle();
  
  // ========== 更新状态栏统计信息 ==========
  updateStatusBar();
  
  // ========== 重建菜单栏（完整更新菜单文字）==========
  // 注意：菜单栏的 tr() 文字需要重新创建菜单才能生效
  // 这里采用简化方案：清除并重建菜单
  menuBar()->clear();
  createMenus();
  updateRecentFilesMenu();
}

void MainWindow::applyDarkTheme() {
  QString styleSheet = QStringLiteral(R"(
        /* Main Window */
        QMainWindow {
            background-color: %1;
        }

        /* Menu Bar */
        QMenuBar {
            background-color: %2;
            color: %3;
            border-bottom: 1px solid %4;
            padding: 2px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 4px 8px;
        }
        QMenuBar::item:selected {
            background-color: %5;
        }

        /* Menu */
        QMenu {
            background-color: %2;
            color: %3;
            border: 1px solid %4;
        }
        QMenu::item {
            padding: 6px 30px 6px 20px;
        }
        QMenu::item:selected {
            background-color: %5;
        }
        QMenu::separator {
            height: 1px;
            background: %4;
            margin: 4px 10px;
        }

        /* List View */
        QListView {
            background-color: %1;
            color: %3;
            border: none;
            outline: none;
        }
        QListView::item {
            padding: 2px 8px;
            border: none;
        }
        QListView::item:hover {
            background-color: %6;
        }
        QListView::item:selected {
            background-color: %7;
        }
        QListView::item:selected:!active {
            background-color: %6;
        }

        /* Scrollbar - Vertical (VS Code style) */
        QScrollBar:vertical {
            border: none;
            background: #1e1e1e;
            width: 14px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #424242;
            min-height: 20px;
            border-radius: 7px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: #686868;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }

        /* Scrollbar - Horizontal (VS Code style) */
        QScrollBar:horizontal {
            border: none;
            background: #1e1e1e;
            height: 14px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #424242;
            min-width: 20px;
            border-radius: 7px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #686868;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
        }

        /* Status Bar */
        QStatusBar {
            background-color: %8;
            color: white;
            border-top: none;
        }
        QStatusBar::item {
            border: none;
        }
        QStatusBar QLabel {
            color: white;
            padding: 2px 8px;
        }

        /* Progress Bar */
        QProgressBar {
            background-color: %9;
            border: none;
            border-radius: 0px;
            text-align: center;
            color: white;
            font-size: 11px;
        }
        QProgressBar::chunk {
            background-color: %10;
        }

        /* Input Dialog & Message Box */
        QDialog, QInputDialog, QMessageBox {
            background-color: %2;
            color: %3;
        }
        QDialog QLabel, QInputDialog QLabel, QMessageBox QLabel {
            color: %3;
        }
        QDialog QLineEdit, QInputDialog QLineEdit {
            background-color: %1;
            color: %3;
            border: 1px solid %4;
            padding: 4px;
        }
        QDialog QPushButton, QInputDialog QPushButton, QMessageBox QPushButton {
            background-color: %10;
            color: white;
            border: none;
            padding: 6px 14px;
            min-width: 60px;
        }
        QDialog QPushButton:hover, QInputDialog QPushButton:hover, QMessageBox QPushButton:hover {
            background-color: #1177bb;
        }
        QDialog QSpinBox, QInputDialog QSpinBox {
            background-color: %1;
            color: %3;
            border: 1px solid %4;
            padding: 4px;
        }

        /* File Dialog */
        QFileDialog {
            background-color: %2;
            color: %3;
        }

        /* ToolTip */
        QToolTip {
            background-color: %2;
            color: %3;
            border: 1px solid %4;
            padding: 4px;
        }

        /* ========== 修复 QTableView 表头白色块问题 ========== */
        
        /* 1. 修复表头容器的背景色 (Fix Header Container Background) */
        QHeaderView {
            background-color: %2;
            border: none;
        }

        /* 2. 修复表头单元格 (Ensure Sections are also dark) */
        QHeaderView::section {
            background-color: %2;
            color: %3;
            padding: 4px 8px;
            border: none;
            border-right: 1px solid %4;
            border-bottom: 1px solid %4;
            font-weight: bold;
        }
        QHeaderView::section:hover {
            background-color: %6;
        }

        /* 3. 关键：修复表格左上角那个小按钮 (Top-left Corner Button) */
        QTableCornerButton::section {
            background-color: %2;
            border: none;
            border-right: 1px solid %4;
            border-bottom: 1px solid %4;
        }

        /* 4. 修复 QTableView 样式 */
        QTableView {
            background-color: %1;
            color: %3;
            border: none;
            outline: none;
            gridline-color: %4;
            alternate-background-color: %2;
        }
        QTableView::item {
            padding: 2px 8px;
            border: none;
        }
        QTableView::item:hover {
            background-color: %6;
        }
        QTableView::item:selected {
            background-color: %7;
        }
    )")
                           .arg(VSCodeDark::Background)       // %1
                           .arg(VSCodeDark::SidebarBg)        // %2
                           .arg(VSCodeDark::TextColor)        // %3
                           .arg(VSCodeDark::BorderColor)      // %4
                           .arg(VSCodeDark::MenuHover)        // %5
                           .arg(VSCodeDark::HoverBg)          // %6
                           .arg(VSCodeDark::SelectedBg)       // %7
                           .arg(VSCodeDark::StatusBarBg)      // %8
                           .arg(VSCodeDark::ProgressBarBg)    // %9
                           .arg(VSCodeDark::ProgressBarFill); // %10

  qApp->setStyleSheet(styleSheet);
}

void MainWindow::setupUi() {
  // 设置窗口大小
  resize(1200, 800);

  // 创建中央 ListView
  m_listView = new QListView(this);
  m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);

  // *** 关键性能优化 ***
  m_listView->setUniformItemSizes(true); // 告诉 Qt 所有行高度相同
  // 2. 【删除或注释】这一行！Batch 模式会延迟布局更新，导致滚动条不随
  // insertRows 增长 m_listView->setLayoutMode(QListView::Batched);
  // m_listView->setBatchSize(200);

  // 使用 ScrollPerPixel 实现像素级滚动，防止最后一行被裁剪
  m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_listView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

  m_listView->setWordWrap(false);
  m_listView->setTextElideMode(Qt::ElideNone);
  m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  
  // 添加行间距，防止最后一行被裁剪
  m_listView->setSpacing(1);

  // 使用等宽字体
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
  QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#else
  QFont monoFont("Consolas", 10);
#endif
  monoFont.setPointSize(11);
  m_listView->setFont(monoFont);

  // 使用自定义委托实现正确行高和水平滚动
  CompactLineDelegate *delegate = new CompactLineDelegate(m_listView);
  delegate->setRowHeight(monoFont); // 根据字体计算正确行高
  m_listView->setItemDelegate(delegate);

  // === 创建详细文本视图（底部，支持部分文本选择）===
  m_detailTextEdit = new QPlainTextEdit(this);
  m_detailTextEdit->setReadOnly(true);  // 只读
  m_detailTextEdit->setFont(monoFont);  // 使用相同的等宽字体
  
  // 修复 Tab 宽度：设置为 4 个空格的宽度
  QFontMetricsF fm(m_detailTextEdit->font());
  qreal spaceWidth = fm.horizontalAdvance(' ');
  m_detailTextEdit->setTabStopDistance(spaceWidth * 4);
  
  m_detailTextEdit->setPlaceholderText(tr("Select a line above to view details here..."));
  m_detailTextEdit->setMaximumHeight(150);  // 限制最大高度
  
  // 应用 VS Code 暗色主题样式
  m_detailTextEdit->setStyleSheet(
      "QPlainTextEdit {"
      "   background-color: #1e1e1e;"           /* VS Code 背景色 */
      "   color: #d4d4d4;"                      /* VS Code 文本色 */
      "   border: none;"
      "   border-top: 1px solid #3c3c3c;"      /* 顶部分隔线 */
      "   selection-background-color: #264f78;" /* 选中背景色 */
      "   selection-color: #ffffff;"            /* 选中文本色 */
      "}"
  );
  
  // ========== 创建表格视图（结构化日志模式） ==========
  m_tableView = new QTableView(this);
  setupTableView();
  
  // ========== 创建视图堆栈（切换原始/表格模式） ==========
  m_viewStack = new QStackedWidget(this);
  m_viewStack->addWidget(m_listView);   // Index 0: 原始文本模式
  m_viewStack->addWidget(m_tableView);  // Index 1: 表格模式
  m_viewStack->setCurrentIndex(0);       // 默认原始文本模式
  
  // === 创建主从布局分隔器 ===
  m_splitter = new QSplitter(Qt::Vertical, this);
  m_splitter->addWidget(m_viewStack);       // 上：视图堆栈
  m_splitter->addWidget(m_detailTextEdit);  // 下：详细文本视图
  
  // 设置默认比例：80% 列表，20% 详细视图
  m_splitter->setStretchFactor(0, 4);  // 视图堆栈权重 4
  m_splitter->setStretchFactor(1, 1);  // 详细视图权重 1
  
  // 设置分隔条样式（VS Code 主题）
  m_splitter->setHandleWidth(1);
  m_splitter->setStyleSheet(
      "QSplitter::handle {"
      "   background-color: #3c3c3c;"  /* 分隔线颜色 */
      "}"
  );
  
  setCentralWidget(m_splitter);

  // 创建模型并绑定到两个视图
  m_model = new BigFileModel(this);
  m_listView->setModel(m_model);
  m_tableView->setModel(m_model);
  
  // 连接选择变化信号 - 更新详细视图（支持多行选择）
  connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [this](const QItemSelection &selected, const QItemSelection &deselected) {
              Q_UNUSED(selected)
              Q_UNUSED(deselected)
              
              // 获取所有选中的行
              QModelIndexList rows = m_listView->selectionModel()->selectedRows();
              
              if (rows.isEmpty()) {
                  m_detailTextEdit->clear();
                  return;
              }
              
              // 按行号排序，确保日志顺序正确
              std::sort(rows.begin(), rows.end(), [](const QModelIndex &a, const QModelIndex &b) {
                  return a.row() < b.row();
              });
              
              // 性能保护：选择过多行时显示警告
              if (rows.size() > 2000) {
                  m_detailTextEdit->setPlainText(
                      tr("--- Selected %1 lines (Too many to display in preview) ---\n"
                         "Use 'Copy' (Ctrl+C) or 'Export' to save selected lines.")
                      .arg(rows.size()));
                  return;
              }
              
              // 聚合选中行的文本
              QString fullText;
              fullText.reserve(rows.size() * 100);  // 预分配内存提升性能
              
              for (const QModelIndex &idx : rows) {
                  if (!fullText.isEmpty()) {
                      fullText.append('\n');
                  }
                  fullText.append(idx.data(Qt::DisplayRole).toString());
              }
              
              // JSON 自动美化：检测并格式化 JSON 内容
              QByteArray data = fullText.toUtf8().trimmed();
              if ((data.startsWith('{') && data.endsWith('}')) ||
                  (data.startsWith('[') && data.endsWith(']'))) {
                  
                  QJsonParseError error;
                  QJsonDocument doc = QJsonDocument::fromJson(data, &error);
                  
                  if (error.error == QJsonParseError::NoError) {
                      // 有效的 JSON，进行美化输出
                      m_detailTextEdit->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
                      return;
                  }
              }
              
              // 普通文本，直接显示
              m_detailTextEdit->setPlainText(fullText);
          });

  // ========== 状态栏统计信号连接 ==========
  // 光标移动时更新
  connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged,
          this, &MainWindow::updateStatusBar);
  
  // 选择变化时更新
  connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &MainWindow::updateStatusBar);
  
  // 模型重置时更新（过滤后行数变化）
  connect(m_model, &QAbstractItemModel::modelReset,
          this, &MainWindow::updateStatusBar);

  // 状态栏组件
  m_statusLabel = new QLabel(tr("Ready"));
  statusBar()->addWidget(m_statusLabel, 1);

  m_lineCountLabel = new QLabel();
  statusBar()->addPermanentWidget(m_lineCountLabel);

  m_progressBar = new QProgressBar();
  m_progressBar->setMinimumWidth(150);
  m_progressBar->setMaximumWidth(200);
  m_progressBar->setMaximumHeight(16);
  m_progressBar->setTextVisible(true);
  m_progressBar->setFormat("%p%");
  m_progressBar->setVisible(false);
  statusBar()->addPermanentWidget(m_progressBar);

  // 编码选择器
  m_encodingCombo = new QComboBox(this);
  m_encodingCombo->setToolTip(tr("Text Encoding"));
  m_encodingCombo->setMinimumWidth(100);
  m_encodingCombo->setMaximumWidth(120);
  
  // 添加编码选项（使用 UserRole 存储枚举值）
  m_encodingCombo->addItem("UTF-8", static_cast<int>(QStringConverter::Utf8));
  m_encodingCombo->addItem("GBK/System", static_cast<int>(QStringConverter::System));
  
  // 设置下拉框样式以匹配暗色主题
  m_encodingCombo->setStyleSheet(
      "QComboBox {"
      "   background-color: #3c3c3c;"
      "   color: #d4d4d4;"
      "   border: 1px solid #555555;"
      "   border-radius: 3px;"
      "   padding: 2px 6px;"
      "}"
      "QComboBox:hover {"
      "   border-color: #007acc;"
      "}"
      "QComboBox::drop-down {"
      "   border: none;"
      "}"
      "QComboBox QAbstractItemView {"
      "   background-color: #252526;"
      "   color: #d4d4d4;"
      "   selection-background-color: #094771;"
      "}"
  );
  
  statusBar()->addPermanentWidget(m_encodingCombo);
  
  // 连接编码切换信号
  connect(m_encodingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
              QVariant data = m_encodingCombo->itemData(index);
              auto encoding = static_cast<QStringConverter::Encoding>(data.toInt());
              m_model->setEncoding(encoding);
              
              // 更新详细视图（如果有当前选中的行）
              QModelIndex current = m_listView->currentIndex();
              if (current.isValid()) {
                  m_detailTextEdit->setPlainText(current.data().toString());
              }
          });

  // Follow Tail 按钮（实时日志跟踪）
  m_followTailAction = new QAction(tr("Follow Tail"), this);
  m_followTailAction->setCheckable(true);
  m_followTailAction->setChecked(false);
  m_followTailAction->setToolTip(tr("Auto-scroll to new content (Tail -f mode)"));
  
  // 创建一个按钮放入状态栏
  QToolButton *followButton = new QToolButton(this);
  followButton->setDefaultAction(m_followTailAction);
  followButton->setStyleSheet(
      "QToolButton {"
      "   background-color: transparent;"
      "   color: #d4d4d4;"
      "   border: 1px solid #555555;"
      "   border-radius: 3px;"
      "   padding: 2px 8px;"
      "}"
      "QToolButton:hover {"
      "   background-color: #3c3c3c;"
      "   border-color: #007acc;"
      "}"
      "QToolButton:checked {"
      "   background-color: #094771;"
      "   border-color: #007acc;"
      "}"
  );
  statusBar()->addPermanentWidget(followButton);
  
  // 连接日志追加信号
  connect(m_model, &BigFileModel::logAppended, this, &MainWindow::onLogAppended);

  // ========== 过滤工具栏 (Filter Bar) ==========
  m_filterToolBar = addToolBar(tr("Filter"));
  m_filterToolBar->setMovable(false);
  m_filterToolBar->setVisible(false);  // 默认隐藏
  m_filterToolBar->setStyleSheet(
      "QToolBar {"
      "   background-color: #252526;"
      "   border-bottom: 1px solid #3c3c3c;"
      "   spacing: 6px;"
      "   padding: 4px 8px;"
      "}"
  );

  // 过滤标签
  QLabel *filterLabel = new QLabel(tr("Filter:"), this);
  filterLabel->setStyleSheet("QLabel { color: #d4d4d4; margin-right: 4px; }");
  m_filterToolBar->addWidget(filterLabel);

  // 过滤输入框
  m_filterInput = new QLineEdit(this);
  m_filterInput->setPlaceholderText(tr("Keywords... (Space to separate, \"quotes\" for exact phrases)"));
  m_filterInput->setMinimumWidth(300);
  m_filterInput->setMaximumWidth(500);
  m_filterInput->setStyleSheet(
      "QLineEdit {"
      "   background-color: #3c3c3c;"
      "   color: #d4d4d4;"
      "   border: 1px solid #555555;"
      "   border-radius: 3px;"
      "   padding: 4px 8px;"
      "}"
      "QLineEdit:focus {"
      "   border-color: #007acc;"
      "}"
      "QLineEdit::placeholder {"
      "   color: #808080;"
      "}"
  );
  m_filterToolBar->addWidget(m_filterInput);

  // 正则表达式切换按钮 [.*]
  m_regexToggleBtn = new QToolButton(this);
  m_regexToggleBtn->setText(".*");
  m_regexToggleBtn->setToolTip(tr("Use Regular Expression"));
  m_regexToggleBtn->setCheckable(true);
  m_regexToggleBtn->setChecked(false);
  m_regexToggleBtn->setStyleSheet(
      "QToolButton {"
      "   min-width: 32px;"
      "   background-color: #3e3e42;"
      "   color: #d4d4d4;"
      "   border: 1px solid #555555;"
      "   border-radius: 3px;"
      "   padding: 4px 8px;"
      "   font-family: Consolas, monospace;"
      "   font-weight: bold;"
      "}"
      "QToolButton:hover {"
      "   background-color: #505050;"
      "   border-color: #007acc;"
      "}"
      "QToolButton:checked {"
      "   background-color: #094771;"
      "   border-color: #007acc;"
      "   color: #ffffff;"
      "}"
  );
  m_filterToolBar->addWidget(m_regexToggleBtn);

  // 统一按钮样式 (Apply 和 Clear 保持一致)
  QString filterBtnStyle = 
      "QToolButton {"
      "   min-width: 60px;"
      "   background-color: #3e3e42;"
      "   color: #d4d4d4;"
      "   border: 1px solid #555555;"
      "   border-radius: 3px;"
      "   padding: 4px 12px;"
      "}"
      "QToolButton:hover {"
      "   background-color: #505050;"
      "   border-color: #007acc;"
      "}"
      "QToolButton:pressed {"
      "   background-color: #2d2d30;"
      "}";

  // ========== AND/OR 逻辑选择（多关键词匹配）==========
  m_logicModeCombo = new QComboBox(this);
  m_logicModeCombo->addItem(tr("ALL (AND)"), true);   // userData = true for AND logic
  m_logicModeCombo->addItem(tr("ANY (OR)"), false);   // userData = false for OR logic
  m_logicModeCombo->setCurrentIndex(0);  // 默认 AND
  m_logicModeCombo->setToolTip(tr("Match mode: ALL = all keywords must match, ANY = any keyword matches"));
  m_logicModeCombo->setMinimumWidth(90);
  m_logicModeCombo->setStyleSheet(
      "QComboBox {"
      "   background-color: #3c3c3c;"
      "   color: #d4d4d4;"
      "   border: 1px solid #555555;"
      "   border-radius: 3px;"
      "   padding: 3px 6px;"
      "}"
      "QComboBox:hover {"
      "   border-color: #007acc;"
      "}"
      "QComboBox::drop-down {"
      "   border: none;"
      "   width: 18px;"
      "}"
      "QComboBox QAbstractItemView {"
      "   background-color: #252526;"
      "   color: #d4d4d4;"
      "   selection-background-color: #094771;"
      "}"
  );
  m_filterToolBar->addWidget(m_logicModeCombo);

  // 应用过滤按钮
  QToolButton *applyFilterBtn = new QToolButton(this);
  applyFilterBtn->setText(tr("Apply"));
  applyFilterBtn->setToolTip(tr("Apply filter (Enter)"));
  applyFilterBtn->setStyleSheet(filterBtnStyle);
  m_filterToolBar->addWidget(applyFilterBtn);

  // 清除过滤按钮
  QToolButton *clearFilterBtn = new QToolButton(this);
  clearFilterBtn->setText(tr("Clear"));
  clearFilterBtn->setToolTip(tr("Clear filter and show all lines"));
  clearFilterBtn->setStyleSheet(filterBtnStyle);
  m_filterToolBar->addWidget(clearFilterBtn);

  // 过滤状态标签
  m_filterStatusLabel = new QLabel(this);
  m_filterStatusLabel->setStyleSheet("QLabel { color: #808080; margin-left: 12px; }");
  m_filterToolBar->addWidget(m_filterStatusLabel);

  // 连接过滤信号
  connect(m_filterInput, &QLineEdit::returnPressed, this, &MainWindow::onFilterRequested);
  connect(applyFilterBtn, &QToolButton::clicked, this, &MainWindow::onFilterRequested);
  connect(clearFilterBtn, &QToolButton::clicked, this, &MainWindow::onClearFilter);
  connect(m_model, &BigFileModel::filterFinished, this, &MainWindow::onFilterFinished);
  connect(m_model, &BigFileModel::filterProgress, this, [this](int percent) {
    m_filterStatusLabel->setText(tr("Filtering... %1%").arg(percent));
  });

  // 快捷键
  QShortcut *openShortcut = new QShortcut(QKeySequence::Open, this);
  connect(openShortcut, &QShortcut::activated, this, &MainWindow::onOpenFile);

  QShortcut *gotoShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);
  connect(gotoShortcut, &QShortcut::activated, this, &MainWindow::onGoToLine);

  // Ctrl+F 搜索快捷键
  QShortcut *searchShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
  connect(searchShortcut, &QShortcut::activated, this,
          &MainWindow::onShowSearch);

  // ESC 取消加载
  QShortcut *cancelShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(cancelShortcut, &QShortcut::activated, this, [this]() {
    if (m_model->isIndexing()) {
      m_model->cancelIndexing();
    }
  });

  // Ctrl+C 复制
  QShortcut *copyShortcut = new QShortcut(QKeySequence::Copy, this);
  connect(copyShortcut, &QShortcut::activated, this, &MainWindow::onCopySelected);

  // 启用右键菜单
  m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_listView, &QListView::customContextMenuRequested,
          this, &MainWindow::onCustomContextMenu);

  // 创建浮动搜索栏（初始隐藏）
  m_searchBar = new SearchBar(this);
  m_searchBar->hide();

  // 连接搜索栏信号（新版本带 useRegex 参数）
  connect(m_searchBar, &SearchBar::searchRequested, this,
          &MainWindow::onSearchRequested);
  connect(m_searchBar, &SearchBar::closed, this, [this]() {
    // 清除高亮
    auto delegate =
        static_cast<CompactLineDelegate *>(m_listView->itemDelegate());
    delegate->setHighlightTerm("", Qt::CaseInsensitive);
    m_listView->viewport()->update();

    m_searchBar->hide();
    m_listView->setFocus();
  });

  // 连接模型搜索信号
  connect(m_model, &BigFileModel::searchFinished, this,
          &MainWindow::onSearchFinished);
}

void MainWindow::createMenus() {
  // 文件菜单
  QMenu *fileMenu = menuBar()->addMenu(tr("File(&F)"));

  QAction *openAction = fileMenu->addAction(tr("Open(&O)..."));
  openAction->setShortcut(QKeySequence::Open);
  connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);

  // 最近文件子菜单
  m_recentFilesMenu = fileMenu->addMenu(tr("Open Recent"));

  fileMenu->addSeparator();

  QAction *exitAction = fileMenu->addAction(tr("Exit(&X)"));
  exitAction->setShortcut(QKeySequence::Quit);
  connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

  // 编辑菜单
  QMenu *editMenu = menuBar()->addMenu(tr("Edit(&E)"));

  QAction *findAction = editMenu->addAction(tr("Find(&F)..."));
  findAction->setShortcut(QKeySequence::Find);
  connect(findAction, &QAction::triggered, this, [this]() {
    m_searchBar->show();
    m_searchBar->raise();
    m_searchBar->setFocus();
    // 触发 resize 事件以正确定位搜索栏
    QResizeEvent event(size(), size());
    resizeEvent(&event);
  });
  
  editMenu->addSeparator();

  QAction *gotoAction = editMenu->addAction(tr("Go to Line(&G)..."));
  gotoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
  connect(gotoAction, &QAction::triggered, this, &MainWindow::onGoToLine);

  editMenu->addSeparator();

  // 书签功能
  m_toggleBookmarkAction = editMenu->addAction(tr("Toggle Bookmark(&B)"));
  m_toggleBookmarkAction->setShortcut(QKeySequence(Qt::Key_F2));
  connect(m_toggleBookmarkAction, &QAction::triggered, this, &MainWindow::onToggleBookmark);

  m_nextBookmarkAction = editMenu->addAction(tr("Next Bookmark(&N)"));
  m_nextBookmarkAction->setShortcut(QKeySequence(Qt::Key_F3));
  connect(m_nextBookmarkAction, &QAction::triggered, this, &MainWindow::onNextBookmark);

  // 视图菜单
  QMenu *viewMenu = menuBar()->addMenu(tr("View(&V)"));

  m_toggleFilterAction = viewMenu->addAction(tr("Toggle Filter(&T)"));
  m_toggleFilterAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
  m_toggleFilterAction->setCheckable(true);
  m_toggleFilterAction->setChecked(false);
  connect(m_toggleFilterAction, &QAction::toggled, this, [this](bool checked) {
    m_filterToolBar->setVisible(checked);
    if (checked) {
      m_filterInput->setFocus();
      m_filterInput->selectAll();
    }
  });

  viewMenu->addSeparator();

  // ========== 视图切换（原始文本 / 表格模式）==========
  m_toggleViewAction = viewMenu->addAction(tr("Grid View"));
  m_toggleViewAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
  m_toggleViewAction->setToolTip(tr("Switch to Grid View (structured columns)"));
  connect(m_toggleViewAction, &QAction::triggered, this, &MainWindow::toggleViewMode);

  // ========== 设置菜单 ==========
  QMenu *settingsMenu = menuBar()->addMenu(tr("Settings(&S)"));

  // 语言子菜单
  m_languageMenu = settingsMenu->addMenu(tr("Language(&L)"));
  m_languageActionGroup = new QActionGroup(this);
  m_languageActionGroup->setExclusive(true);  // 单选

  // 获取可用语言列表
  QVariantList languages = LanguageManager::instance().availableLanguages();
  QString currentLang = LanguageManager::instance().currentLanguage();

  for (const QVariant& langVar : languages) {
    QVariantMap langMap = langVar.toMap();
    QString code = langMap["code"].toString();
    QString displayName = langMap["name"].toString();

    QAction *langAction = m_languageMenu->addAction(displayName);
    langAction->setCheckable(true);
    langAction->setData(code);  // 存储语言代码
    langAction->setChecked(code == currentLang);
    m_languageActionGroup->addAction(langAction);

    connect(langAction, &QAction::triggered, this, [this, code]() {
      LanguageManager::instance().loadLanguage(code);
    });
  }

  // 帮助菜单
  QMenu *helpMenu = menuBar()->addMenu(tr("Help(&H)"));

  // Register License
  QAction *registerAction = helpMenu->addAction(tr("Register License(&R)..."));
  registerAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
  connect(registerAction, &QAction::triggered, this, &MainWindow::showRegisterDialog);

  // License Status
  QAction *licenseStatusAction = helpMenu->addAction(tr("License Status(&L)"));
  connect(licenseStatusAction, &QAction::triggered, this, [this]() {
    bool isRegistered = LicenseManager::instance().isRegistered();
    QString status;
    
    if (isRegistered) {
      status = tr("✓ Licensed Version\n\nThank you for registering BigFileViewer!");
    } else {
      int daysLeft = TrialManager::instance().daysRemaining();
      if (daysLeft > 0) {
        status = tr("Trial Version\n\n%1 days remaining in your trial.\n\n"
                   "Click Help > Register License to activate.").arg(daysLeft);
      } else {
        status = tr("⚠ Trial Expired\n\nPlease register to continue using BigFileViewer.");
      }
    }
    
    QMessageBox::information(this, tr("License Status"), status);
  });

  helpMenu->addSeparator();

  QAction *aboutAction = helpMenu->addAction(tr("About(&A)"));
  connect(aboutAction, &QAction::triggered, this, [this]() {
    QMessageBox::about(this, tr("About BigFileViewer"),
                       tr("BigFileViewer v1.5\n\n"
                          "High-performance large file viewer\n"
                          "Features:\n"
                          "  - Memory-mapped file access\n"
                          "  - Async indexing with progress\n"
                          "  - Supports 10GB+ text files\n"
                          "  - VS Code Dark Theme\n\n"
                          "Built with Qt %1")
                           .arg(qVersion()));
  });
}

void MainWindow::setLoadingState(bool loading) {
  m_isLoading = loading;
  m_progressBar->setVisible(loading);

  // 控制状态更新定时器
  if (loading) {
    m_statusTimer->start();
  } else {
    m_statusTimer->stop();
  }

  // *** 加载时保持菜单可用，允许用户取消或打开其他文件 ***
  // menuBar()->setEnabled(!loading);  // 不再禁用菜单
}

void MainWindow::onOpenFile() {
  if (m_isLoading) {
    return;
  }

  QString filePath = QFileDialog::getOpenFileName(
      this, tr("Open File"), QString(),
      tr("All Files (*);;Text Files (*.txt *.log *.csv);;Log Files (*.log)"));

  if (!filePath.isEmpty()) {
    openFile(filePath);
  }
}

void MainWindow::openFile(const QString &filePath) {
  // *** 允许在加载过程中打开新文件（会自动取消当前加载）***

  // 获取文件大小
  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists()) {
    QMessageBox::warning(this, tr("Error"), tr("File does not exist: %1").arg(filePath));
    return;
  }
  
  qint64 fileSize = fileInfo.size();
  const qint64 SMALL_FILE_THRESHOLD = 10 * 1024 * 1024;  // 10 MB
  
  setLoadingState(true);
  m_statusLabel->setText(tr("Loading: %1").arg(filePath));
  m_lineCountLabel->setText(tr("Lines: 0"));
  m_progressBar->setValue(0);

  // ========== 混合加载策略 ==========
  if (fileSize < SMALL_FILE_THRESHOLD) {
    // *** 小文件 (<10MB)：同步加载，立即完成 ***
    qDebug() << "[MainWindow] Small file detected (" << fileSize / 1024 << "KB), using synchronous load";
    
    // 加载文件
    bool success = m_model->loadFile(filePath);
    
    // 立即更新 UI 状态
    setLoadingState(false);
    m_progressBar->setVisible(false);
    m_progressBar->setValue(0);
    
    if (success) {
      m_statusLabel->setText(tr("Ready"));
      m_lineCountLabel->setText(tr("Lines: %1").arg(m_model->lineCount()));
      updateStatusBar();
      qDebug() << "[MainWindow] Small file loaded successfully:" << m_model->lineCount() << "lines";
    } else {
      m_statusLabel->setText(tr("Load failed"));
      m_lineCountLabel->clear();
    }
    
  } else {
    // *** 大文件 (>=10MB)：异步加载 ***
    qDebug() << "[MainWindow] Large file detected (" << fileSize / (1024 * 1024) << "MB), using async load";
    
    // 设置安全超时（30秒，大文件需要更长时间）
    QTimer::singleShot(30000, this, [this]() {
      if (m_isLoading) {
        qWarning() << "[MainWindow] Safety timeout triggered - forcing load complete";
        setLoadingState(false);
        m_progressBar->setVisible(false);
        m_statusLabel->setText(tr("Ready (timeout)"));
        m_lineCountLabel->setText(tr("Lines: %1").arg(m_model->lineCount()));
        updateStatusBar();
      }
    });
    
    // 启动异步加载
    m_model->loadFile(filePath);
  }

  updateWindowTitle();

  // 添加到最近文件列表
  addToRecentFiles(filePath);
}


void MainWindow::onFileLoaded(bool success, const QString &message) {
  // *** 确保加载状态被正确重置 ***
  setLoadingState(false);
  
  // *** 强制隐藏进度条 ***
  m_progressBar->setVisible(false);
  m_progressBar->setValue(0);

  if (success) {
    // *** 显式设置状态消息 ***
    m_statusLabel->setText(tr("Ready - %1").arg(message));
    m_lineCountLabel->setText(tr("Lines: %1").arg(m_model->lineCount()));
    
    // 更新状态栏统计信息
    updateStatusBar();
    
    // *** 关键：更新窗口标题，移除 [Indexing...] ***
    updateWindowTitle();
    
    qDebug() << "[MainWindow] File loaded successfully:" << message;
  } else {
    m_statusLabel->setText(tr("Load failed"));
    m_lineCountLabel->clear();
    updateWindowTitle();  // 同样更新标题
    QMessageBox::critical(this, tr("Error"), message);
  }
}

void MainWindow::onIndexingProgress(int percent) {
  m_progressBar->setValue(percent);
  m_lineCountLabel->setText(tr("Lines: %1").arg(m_model->lineCount()));
}

void MainWindow::onIndexingCancelled() {
  setLoadingState(false);
  m_statusLabel->setText(tr("Loading cancelled"));
}

void MainWindow::onGoToLine() {
  if (m_model->lineCount() == 0) {
    QMessageBox::information(this, tr("Info"), tr("Please open a file first"));
    return;
  }

  bool ok;
  int lineNumber = QInputDialog::getInt(
      this, tr("Go to Line"),
      tr("Line number (1 - %1):").arg(m_model->lineCount()), 1, 1,
      m_model->lineCount(), 1, &ok);

  if (ok) {
    QModelIndex index = m_model->index(lineNumber - 1, 0);
    m_listView->scrollTo(index, QAbstractItemView::PositionAtCenter);
    m_listView->setCurrentIndex(index);
  }
}

void MainWindow::updateWindowTitle() {
  QString title = QStringLiteral("BigFileViewer");
  if (!m_model->filePath().isEmpty()) {
    QFileInfo fi(m_model->filePath());
    title += QString(" - %1").arg(fi.fileName());
  }
  if (m_model->isIndexing()) {
    title += tr(" [Indexing...]");
  }
  setWindowTitle(title);
}

void MainWindow::updateStatusBar() {
  // 安全检查：防止在析构期间访问已销毁的对象
  if (!m_listView || !m_model || !m_statusLabel) {
    return;
  }
  
  QItemSelectionModel *selModel = m_listView->selectionModel();
  if (!selModel) {
    return;
  }

  // 获取当前光标位置
  QModelIndex current = m_listView->currentIndex();
  int currentRow = current.isValid() ? current.row() + 1 : 0;

  // 获取总行数
  int totalRows = m_model->rowCount();

  // 获取选中行数
  int selectedCount = selModel->selectedRows().count();

  // 获取文件大小并格式化
  qint64 fileSize = m_model->fileSize();
  QString sizeStr;
  if (fileSize >= 1024 * 1024 * 1024) {
    // GB
    sizeStr = QString::number(static_cast<double>(fileSize) / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
  } else if (fileSize >= 1024 * 1024) {
    // MB
    sizeStr = QString::number(static_cast<double>(fileSize) / (1024.0 * 1024.0), 'f', 2) + " MB";
  } else if (fileSize >= 1024) {
    // KB
    sizeStr = QString::number(static_cast<double>(fileSize) / 1024.0, 'f', 2) + " KB";
  } else {
    // Bytes
    sizeStr = QString::number(fileSize) + " B";
  }

  // 获取编码
  QString encoding = m_encodingCombo ? m_encodingCombo->currentText() : "UTF-8";

  // 格式化状态栏文本
  QString info;
  if (totalRows == 0) {
    info = tr("Ready");
  } else {
    info = QString("Line: %1 / %2 | Selected: %3 | Size: %4 | %5")
               .arg(currentRow)
               .arg(totalRows)
               .arg(selectedCount)
               .arg(sizeStr)
               .arg(encoding);
  }

  m_statusLabel->setText(info);
}

// ========== 搜索功能实现 ==========

void MainWindow::onShowSearch() {
  if (!m_model || m_model->lineCount() == 0) {
    QMessageBox::information(this, tr("Info"), tr("Please open a file first"));
    return;
  }

  m_searchBar->show();
  m_searchBar->raise();
}

void MainWindow::onSearchRequested(const QString &text, int direction, bool useRegex) {
  if (text.isEmpty()) {
    m_currentSearchIndex = -1;
    m_searchBar->updateResultInfo(0, 0);
    return;
  }

  // 如果是新搜索，启动异步搜索
  const auto &currentResults = m_model->searchResults();
  if (currentResults.empty() || m_searchBar->searchText() != text) {
    // 开始新搜索（传递正则表达式标志）
    m_model->search(text, useRegex);
    m_currentSearchIndex = -1;

    // 更新委托高亮（支持正则和普通模式）
    auto delegate =
        static_cast<CompactLineDelegate *>(m_listView->itemDelegate());
    if (useRegex) {
      delegate->setHighlightRegex(text);
    } else {
      delegate->setHighlightTerm(text, Qt::CaseInsensitive);
    }
    m_listView->viewport()->update();

    return;
  }

  // 在现有结果中导航
  if (currentResults.empty()) {
    return;
  }

  int totalResults = static_cast<int>(currentResults.size());

  if (direction == SearchBar::Next) {
    // 下一个
    m_currentSearchIndex++;
    if (m_currentSearchIndex >= totalResults) {
      m_currentSearchIndex = 0;
    }
  } else {
    // 上一个
    m_currentSearchIndex--;
    if (m_currentSearchIndex < 0) {
      m_currentSearchIndex = totalResults - 1;
    }
  }

  // 跳转到该行
  int rowIndex = currentResults[m_currentSearchIndex];
  QModelIndex index = m_model->index(rowIndex, 0);
  m_listView->scrollTo(index, QAbstractItemView::PositionAtCenter);
  m_listView->setCurrentIndex(index);

  // 计算该行内的局部匹配索引（向后计数同一行有多少个匹配）
  int localMatchIndex = 0;
  for (int i = m_currentSearchIndex - 1; i >= 0; --i) {
    if (currentResults[i] == rowIndex) {
      localMatchIndex++;
    } else {
      break;  // 不同行了，停止计数
    }
  }

  // 更新委托的活动匹配
  auto delegate = static_cast<CompactLineDelegate*>(m_listView->itemDelegate());
  delegate->setActiveMatch(rowIndex, localMatchIndex);
  m_listView->viewport()->update();

  // 更新信息（从1开始计数）
  m_searchBar->updateResultInfo(m_currentSearchIndex + 1, totalResults);
}

void MainWindow::onSearchFinished(const std::vector<int> &results) {
  int totalResults = static_cast<int>(results.size());

  if (totalResults == 0) {
    m_currentSearchIndex = -1;
    m_searchBar->updateResultInfo(0, 0);
    m_statusLabel->setText(tr("No matches found"));
    
    // 清除活动匹配
    auto delegate = static_cast<CompactLineDelegate*>(m_listView->itemDelegate());
    delegate->setActiveMatch(-1, -1);
    m_listView->viewport()->update();
  } else {
    // 自动跳转到第一个结果
    m_currentSearchIndex = 0;
    int rowIndex = results[0];
    QModelIndex index = m_model->index(rowIndex, 0);
    m_listView->scrollTo(index, QAbstractItemView::PositionAtCenter);
    m_listView->setCurrentIndex(index);

    // 设置活动匹配（第一个结果总是该行的第一个匹配，索引为0）
    auto delegate = static_cast<CompactLineDelegate*>(m_listView->itemDelegate());
    delegate->setActiveMatch(rowIndex, 0);
    m_listView->viewport()->update();

    m_searchBar->updateResultInfo(1, totalResults);
    
    // 显示正则/普通模式提示
    QString modeText = m_searchBar->isRegexMode() ? tr(" (Regex)") : "";
    m_statusLabel->setText(tr("Found %1 matches%2").arg(totalResults).arg(modeText));
  }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);

  // 定位浮动搜索栏在右上角（带合适的边距）
  if (m_searchBar) {
    // 使用 sizeHint 或实际尺寸
    int w = m_searchBar->width();
    int h = m_searchBar->height();

    // 右边距 25px（避免遮盖滚动条），上边距 10px
    int rightMargin = 25;
    int topMargin = 10;

    int x = this->width() - w - rightMargin;
    int y = topMargin;

    m_searchBar->setGeometry(x, y, w, h);
    m_searchBar->raise(); // 确保浮动在最上层
  }
}

// ========== 复制功能实现 ==========

void MainWindow::onCopySelected()
{
    // 获取选中的索引
    QModelIndexList indexes = m_listView->selectionModel()->selectedIndexes();
    
    if (indexes.isEmpty()) {
        return;  // 没有选中任何行
    }
    
    // 按行号排序（selectedIndexes() 不保证顺序）
    std::sort(indexes.begin(), indexes.end(), 
              [](const QModelIndex &a, const QModelIndex &b) {
                  return a.row() < b.row();
              });
    
    // 提取所有选中行的文本
    QStringList lines;
    lines.reserve(indexes.size());
    
    for (const QModelIndex &idx : indexes) {
        QString lineText = idx.data().toString();
        lines.append(lineText);
    }
    
    // 合并为一个字符串（用换行符分隔）
    QString fullText = lines.join('\n');
    
    // 复制到剪贴板
    QGuiApplication::clipboard()->setText(fullText);
    
    // 状态栏提示
    m_statusLabel->setText(tr("Copied %1 lines to clipboard").arg(indexes.size()));
    
    // 2秒后自动清除提示
    QTimer::singleShot(2000, this, [this]() {
        if (m_statusLabel->text().contains("Copied")) {
            m_statusLabel->setText(tr("Ready"));
        }
    });
}

void MainWindow::onCustomContextMenu(const QPoint &pos)
{
    // 创建右键菜单
    QMenu contextMenu(this);
    
    // 添加 "复制" 动作
    QAction *copyAction = contextMenu.addAction(tr("Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &MainWindow::onCopySelected);
    
    // 检查是否有选中内容
    bool hasSelection = !m_listView->selectionModel()->selectedIndexes().isEmpty();
    copyAction->setEnabled(hasSelection);

    contextMenu.addSeparator();

    // 书签操作
    QAction *bookmarkAction = contextMenu.addAction(tr("Toggle Bookmark"));
    bookmarkAction->setShortcut(QKeySequence(Qt::Key_F2));
    connect(bookmarkAction, &QAction::triggered, this, &MainWindow::onToggleBookmark);

    contextMenu.addSeparator();

    // 导出操作
    QAction *exportAction = contextMenu.addAction(tr("Export Visible Lines..."));
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportVisibleLines);

    // 打开文件夹操作
    QAction *openFolderAction = contextMenu.addAction(tr("Open Containing Folder"));
    connect(openFolderAction, &QAction::triggered, this, &MainWindow::openContainingFolder);

    // 如果没有加载文件，禁用部分操作
    bool hasFile = m_model && !m_model->filePath().isEmpty();
    exportAction->setEnabled(hasFile && m_model->rowCount() > 0);
    openFolderAction->setEnabled(hasFile);
    
    // 显示菜单
    contextMenu.exec(m_listView->viewport()->mapToGlobal(pos));
}

// ========== 实时日志监控实现 ==========

void MainWindow::onLogAppended()
{
    // 检查是否启用了 Follow Tail
    if (m_followTailAction && m_followTailAction->isChecked()) {
        // 滚动到底部
        m_listView->scrollToBottom();
        
        // 更新状态栏
        m_statusLabel->setText(tr("New log content detected"));
        
        // 2秒后清除提示
        QTimer::singleShot(2000, this, [this]() {
            if (m_statusLabel->text().contains("New log")) {
                m_statusLabel->setText(tr("Ready"));
            }
        });
    }
}

// ========== 日志过滤功能实现 ==========

void MainWindow::onFilterRequested()
{
    QString inputText = m_filterInput->text().trimmed();
    
    if (inputText.isEmpty()) {
        onClearFilter();
        return;
    }

    // 检查是否有文件加载
    if (!m_model || m_model->lineCount() == 0) {
        QMessageBox::information(this, tr("Info"), tr("Please open a file first"));
        return;
    }

    // 获取正则表达式开关状态
    bool useRegex = m_regexToggleBtn && m_regexToggleBtn->isChecked();
    
    // 获取 AND/OR 逻辑（从下拉框）
    bool andLogic = m_logicModeCombo ? m_logicModeCombo->currentData().toBool() : true;

    // 解析空格分隔的关键词（保留引号内的空格）
    QStringList keywords;
    QRegularExpression keywordRegex(QStringLiteral("\"([^\"]+)\"|'([^']+)'|(\\S+)"));
    QRegularExpressionMatchIterator it = keywordRegex.globalMatch(inputText);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        if (match.captured(1).length() > 0) {
            keywords.append(match.captured(1));  // Double-quoted
        } else if (match.captured(2).length() > 0) {
            keywords.append(match.captured(2));  // Single-quoted
        } else {
            keywords.append(match.captured(3));  // Unquoted word
        }
    }

    // 如果启用正则表达式，验证每个模式
    if (useRegex) {
        for (const QString &kw : keywords) {
            QRegularExpression regex(kw, QRegularExpression::CaseInsensitiveOption);
            if (!regex.isValid()) {
                QMessageBox::warning(this, tr("Invalid Regex"),
                    tr("The regular expression is invalid:\n%1\n\nPattern: %2")
                    .arg(regex.errorString())
                    .arg(kw));
                m_filterInput->setFocus();
                m_filterInput->selectAll();
                return;
            }
        }
    }

    // 更新状态（显示 AND/OR 逻辑）
    QString modeText = useRegex ? tr("(Regex)") : "";
    QString logicText = andLogic ? tr("[AND]") : tr("[OR]");
    m_filterStatusLabel->setText(tr("Filtering... %1").arg(logicText));
    m_statusLabel->setText(tr("Applying filter %1 %2: %3 keywords")
                                .arg(modeText)
                                .arg(logicText)
                                .arg(keywords.size()));

    // 应用高级过滤（支持多关键词 + AND/OR 逻辑）
    m_model->applyAdvancedFilter(QString(), keywords, andLogic, useRegex);

    // 更新委托高亮（显示第一个关键词或整个输入）
    auto delegate = static_cast<CompactLineDelegate *>(m_listView->itemDelegate());
    delegate->setHighlightTerm(keywords.isEmpty() ? inputText : keywords.first(), Qt::CaseInsensitive);
    m_listView->viewport()->update();
}

void MainWindow::onFilterFinished(int matchCount)
{
    if (m_model->isFilterMode()) {
        // 过滤模式
        int totalLines = m_model->totalLineCount();
        m_filterStatusLabel->setText(
            tr("Showing %1 of %2 lines")
            .arg(matchCount)
            .arg(totalLines)
        );
        m_lineCountLabel->setText(tr("Lines: %1 (filtered)").arg(matchCount));
        
        // 如果有结果，滚动到第一行
        if (matchCount > 0) {
            m_listView->scrollToTop();
        }
    } else {
        // 非过滤模式（显示全部）
        int totalLines = m_model->totalLineCount();
        m_filterStatusLabel->clear();
        m_lineCountLabel->setText(tr("Lines: %1").arg(totalLines));
    }
    
    // 更新状态栏统计信息
    updateStatusBar();
}

void MainWindow::onClearFilter()
{
    // 清空输入框
    m_filterInput->clear();
    
    // 清除过滤
    if (m_model) {
        m_model->clearFilter();
    }

    // 清除高亮
    auto delegate = static_cast<CompactLineDelegate *>(m_listView->itemDelegate());
    delegate->setHighlightTerm("", Qt::CaseInsensitive);
    m_listView->viewport()->update();

    // 更新 UI
    m_filterStatusLabel->clear();
    m_statusLabel->setText(tr("Filter cleared"));
    
    if (m_model) {
        m_lineCountLabel->setText(tr("Lines: %1").arg(m_model->lineCount()));
    }

    // 2秒后清除状态
    QTimer::singleShot(2000, this, [this]() {
        if (m_statusLabel->text().contains("cleared")) {
            m_statusLabel->setText(tr("Ready"));
        }
    });
}

// ========== 书签功能 ==========

void MainWindow::onToggleBookmark() {
  QModelIndex currentIndex = m_listView->currentIndex();
  if (!currentIndex.isValid()) {
    m_statusLabel->setText(tr("No line selected"));
    return;
  }

  m_model->toggleBookmark(currentIndex.row());
  
  // 更新视图以显示书签图标变化
  m_listView->viewport()->update();
}

void MainWindow::onNextBookmark() {
  if (!m_model) {
    return;
  }

  int currentRow = m_listView->currentIndex().isValid() ? m_listView->currentIndex().row() : -1;
  int nextRow = m_model->getNextBookmark(currentRow);

  if (nextRow >= 0) {
    QModelIndex nextIndex = m_model->index(nextRow, 0);
    m_listView->setCurrentIndex(nextIndex);
    m_listView->scrollTo(nextIndex, QAbstractItemView::PositionAtCenter);
    m_statusLabel->setText(tr("Jumped to bookmark at line %1").arg(m_model->toRealRow(nextRow) + 1));
  } else {
    m_statusLabel->setText(tr("No bookmarks found"));
  }
}


void MainWindow::exportVisibleLines() {
  if (!m_model || m_model->rowCount() == 0) {
    QMessageBox::warning(this, tr("Export"), tr("No lines to export."));
    return;
  }

  // 构建默认文件名
  QString defaultFileName;
  if (!m_model->filePath().isEmpty()) {
    QFileInfo fi(m_model->filePath());
    QString suffix = m_model->isFilterMode() ? "_filtered" : "_export";
    defaultFileName = fi.absolutePath() + "/" + fi.baseName() + suffix + ".txt";
  } else {
    defaultFileName = "export.txt";
  }

  // 打开保存文件对话框
  QString filePath = QFileDialog::getSaveFileName(
      this,
      tr("Export Visible Lines"),
      defaultFileName,
      tr("Text Files (*.txt);;Log Files (*.log);;All Files (*)")
  );

  if (filePath.isEmpty()) {
    return;  // 用户取消
  }

  // 打开文件写入
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::critical(this, tr("Export Error"),
                          tr("Failed to open file for writing:\n%1").arg(file.errorString()));
    return;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  // 导出所有可见行（模型已经处理了过滤逻辑）
  int totalRows = m_model->rowCount();
  for (int row = 0; row < totalRows; ++row) {
    QModelIndex index = m_model->index(row, 0);
    QString lineText = m_model->data(index, Qt::DisplayRole).toString();
    stream << lineText << "\n";
  }

  file.close();

  // 显示成功消息
  QString message = m_model->isFilterMode()
      ? tr("Successfully exported %1 filtered lines to:\n%2").arg(totalRows).arg(filePath)
      : tr("Successfully exported %1 lines to:\n%2").arg(totalRows).arg(filePath);

  QMessageBox::information(this, tr("Export Successful"), message);
  m_statusLabel->setText(tr("Exported %1 lines").arg(totalRows));
}

void MainWindow::openContainingFolder() {
  if (!m_model || m_model->filePath().isEmpty()) {
    QMessageBox::warning(this, tr("Open Folder"), tr("No file is currently loaded."));
    return;
  }

  QString filePath = m_model->filePath();
  QFileInfo fi(filePath);
  QString folderPath = fi.absolutePath();

  // 打开文件所在目录
  bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
  
  if (success) {
    m_statusLabel->setText(tr("Opened folder: %1").arg(folderPath));
  } else {
    QMessageBox::warning(this, tr("Open Folder"),
                         tr("Failed to open folder:\n%1").arg(folderPath));
  }
}

// ========== 拖放支持 ==========

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
  // 检查是否包含文件 URL
  if (event->mimeData()->hasUrls()) {
    // 检查是否有有效的本地文件
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
      if (url.isLocalFile()) {
        event->acceptProposedAction();
        return;
      }
    }
  }
}

void MainWindow::dropEvent(QDropEvent *event) {
  const QList<QUrl> urls = event->mimeData()->urls();
  
  if (!urls.isEmpty()) {
    // 取第一个文件
    QString filePath = urls.first().toLocalFile();
    
    if (!filePath.isEmpty() && QFileInfo::exists(filePath)) {
      openFile(filePath);
      m_statusLabel->setText(tr("Opened via drag & drop: %1").arg(QFileInfo(filePath).fileName()));
    }
  }
}

// ========== 最近文件功能 ==========

void MainWindow::addToRecentFiles(const QString &filePath) {
  if (filePath.isEmpty()) {
    return;
  }

  QSettings settings;
  QStringList recentFiles = settings.value("recentFileList").toStringList();

  // 移除已存在的路径（如果有的话）
  recentFiles.removeAll(filePath);

  // 添加到列表开头
  recentFiles.prepend(filePath);

  // 限制最大数量
  while (recentFiles.size() > MAX_RECENT_FILES) {
    recentFiles.removeLast();
  }

  // 保存到设置
  settings.setValue("recentFileList", recentFiles);

  // 更新菜单
  updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu() {
  if (!m_recentFilesMenu) {
    return;
  }

  // 清空现有菜单项
  m_recentFilesMenu->clear();

  QSettings settings;
  QStringList recentFiles = settings.value("recentFileList").toStringList();

  if (recentFiles.isEmpty()) {
    // 如果没有最近文件，显示禁用的提示
    QAction *emptyAction = m_recentFilesMenu->addAction(tr("(No recent files)"));
    emptyAction->setEnabled(false);
  } else {
    // 添加最近文件项
    int index = 1;
    for (const QString &filePath : recentFiles) {
      // 获取文件名用于显示
      QFileInfo fi(filePath);
      QString displayName = QString("%1. %2").arg(index).arg(fi.fileName());
      
      QAction *action = m_recentFilesMenu->addAction(displayName);
      action->setToolTip(filePath);  // 完整路径作为提示
      action->setData(filePath);     // 存储完整路径
      
      // 连接信号 - 点击时打开文件
      connect(action, &QAction::triggered, this, [this, filePath]() {
        if (QFileInfo::exists(filePath)) {
          openFile(filePath);
        } else {
          QMessageBox::warning(this, tr("File Not Found"),
                              tr("The file no longer exists:\n%1").arg(filePath));
          // 从列表中移除不存在的文件
          QSettings settings;
          QStringList recentFiles = settings.value("recentFileList").toStringList();
          recentFiles.removeAll(filePath);
          settings.setValue("recentFileList", recentFiles);
          updateRecentFilesMenu();
        }
      });
      
      ++index;
    }

    // 添加分隔线和清除选项
    m_recentFilesMenu->addSeparator();
    
    QAction *clearAction = m_recentFilesMenu->addAction(tr("Clear Recent List"));
    connect(clearAction, &QAction::triggered, this, &MainWindow::clearRecentFiles);
  }
}

void MainWindow::clearRecentFiles() {
  QSettings settings;
  settings.remove("recentFileList");
  
  updateRecentFilesMenu();
  m_statusLabel->setText(tr("Recent files list cleared"));
}

// ============================================================================
// License Registration Dialog
// ============================================================================

void MainWindow::showRegisterDialog() {
  // Get Machine ID
  QString machineId = LicenseManager::instance().getMachineId();
  
  // Create a custom dialog with machine ID display and license key input
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Register License"));
  dialog.setMinimumWidth(500);
  
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  
  // Machine ID section
  QLabel *machineLabel = new QLabel(tr("Your Machine ID (send this to the vendor):"), &dialog);
  layout->addWidget(machineLabel);
  
  QLineEdit *machineIdEdit = new QLineEdit(&dialog);
  machineIdEdit->setText(machineId);
  machineIdEdit->setReadOnly(true);
  machineIdEdit->setStyleSheet(
      "QLineEdit { "
      "   background-color: #2d2d30; "
      "   color: #9cdcfe; "
      "   font-family: Consolas, monospace; "
      "   padding: 8px; "
      "   border: 1px solid #3c3c3c; "
      "}"
  );
  layout->addWidget(machineIdEdit);
  
  // Copy button for machine ID
  QPushButton *copyBtn = new QPushButton(tr("Copy Machine ID"), &dialog);
  connect(copyBtn, &QPushButton::clicked, [machineId]() {
      QApplication::clipboard()->setText(machineId);
  });
  layout->addWidget(copyBtn);
  
  layout->addSpacing(20);
  
  // License key input section
  QLabel *keyLabel = new QLabel(tr("Enter License Key:"), &dialog);
  layout->addWidget(keyLabel);
  
  QLineEdit *keyEdit = new QLineEdit(&dialog);
  keyEdit->setPlaceholderText(tr("Paste your license key here..."));
  keyEdit->setStyleSheet(
      "QLineEdit { "
      "   background-color: #3c3c3c; "
      "   color: #d4d4d4; "
      "   padding: 8px; "
      "   border: 1px solid #555555; "
      "}"
      "QLineEdit:focus { border-color: #007acc; }"
  );
  layout->addWidget(keyEdit);
  
  layout->addSpacing(10);
  
  // Buttons
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  
  QPushButton *verifyBtn = new QPushButton(tr("Verify && Activate"), &dialog);
  verifyBtn->setDefault(true);
  verifyBtn->setStyleSheet(
      "QPushButton { "
      "   background-color: #0e639c; "
      "   color: white; "
      "   padding: 8px 20px; "
      "   border: none; "
      "   border-radius: 3px; "
      "}"
      "QPushButton:hover { background-color: #1177bb; }"
  );
  
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), &dialog);
  cancelBtn->setStyleSheet(
      "QPushButton { "
      "   background-color: #3c3c3c; "
      "   color: #d4d4d4; "
      "   padding: 8px 20px; "
      "   border: 1px solid #555555; "
      "   border-radius: 3px; "
      "}"
      "QPushButton:hover { background-color: #505050; }"
  );
  
  buttonLayout->addStretch();
  buttonLayout->addWidget(cancelBtn);
  buttonLayout->addWidget(verifyBtn);
  layout->addLayout(buttonLayout);
  
  // Connect buttons
  connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
  connect(verifyBtn, &QPushButton::clicked, [&]() {
      QString licenseKey = keyEdit->text().trimmed();
      
      if (licenseKey.isEmpty()) {
          QMessageBox::warning(&dialog, tr("Error"), 
              tr("Please enter a license key."));
          return;
      }
      
      // Verify the license
      if (LicenseManager::instance().verifyLicense(licenseKey)) {
          // Save valid license to settings
          QSettings settings;
          settings.setValue("license/key", licenseKey);
          settings.setValue("license/activationDate", QDateTime::currentDateTime());
          
          QMessageBox::information(&dialog, tr("Success"), 
              tr("License activated successfully!\n\nThank you for registering."));
          dialog.accept();
      } else {
          QString error = LicenseManager::instance().lastError();
          QMessageBox::critical(&dialog, tr("Invalid License"), 
              tr("The license key is not valid for this machine.\n\nError: %1").arg(error));
      }
  });
  
  // Apply dark theme to dialog
  dialog.setStyleSheet(
      "QDialog { background-color: #252526; }"
      "QLabel { color: #d4d4d4; }"
  );
  
  dialog.exec();
}

// ============================================================================
// License/Trial Status Check (Startup)
// ============================================================================

void MainWindow::checkLicenseStatus() {
  // Step 1: Check if already registered
  if (LicenseManager::instance().isRegistered()) {
    // User is registered - unlock full features
    setWindowTitle(tr("BigFileViewer (Pro) - Licensed"));
    m_statusLabel->setText(tr("Licensed version - All features unlocked"));
    return;
  }

  // Step 2: Not registered - check trial status
  TrialManager &trial = TrialManager::instance();
  
  if (trial.isTrialExpired()) {
    // Trial has expired - MUST register
    QMessageBox::critical(this, tr("Trial Expired"),
        tr("Your 30-day trial period has expired.\n\n"
           "Please register to continue using BigFileViewer.\n"
           "Click OK to open the registration dialog."));
    
    // Force registration - strict exit if cancelled
    RegistrationDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
      // User cancelled or closed dialog - EXIT APPLICATION
      QMessageBox::warning(this, tr("Registration Required"),
          tr("BigFileViewer requires a valid license to continue.\n\n"
             "The application will now close."));
      
      // Schedule exit after event loop processes
      QTimer::singleShot(0, []() {
        std::exit(0);
      });
      return;
    }
    
    // Registration successful - update title
    setWindowTitle(tr("BigFileViewer (Pro) - Licensed"));
    m_statusLabel->setText(tr("Licensed version - Thank you for registering!"));
    
  } else {
    // Trial still active - show remaining days
    int daysLeft = trial.daysRemaining();
    
    setWindowTitle(tr("BigFileViewer (Trial) - %1 days remaining").arg(daysLeft));
    
    // Show trial notice in status bar
    QString trialMsg = tr("Trial Mode: %1 days remaining | Click Help > Register to unlock").arg(daysLeft);
    m_statusLabel->setText(trialMsg);
    
    // Optional: Show reminder when trial is almost over (< 7 days)
    if (daysLeft <= 7 && daysLeft > 0) {
      QMessageBox::information(this, tr("Trial Ending Soon"),
          tr("Your trial will expire in %1 days.\n\n"
             "Please consider registering to continue using BigFileViewer.\n"
             "Go to Help > Register License to activate.").arg(daysLeft));
    }
  }
}

// ========== 表格视图设置（性能优化关键！！！） ==========

void MainWindow::setupTableView() {
  if (!m_tableView) return;
  
  // *** 关键性能优化：禁用所有自动调整 ***
  // ResizeToContents 会扫描所有行来计算宽度，导致 UI 卡死！
  m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  
  // 设置固定行高（避免每行计算高度）
  m_tableView->verticalHeader()->setDefaultSectionSize(22);
  
  // 设置默认列宽
  m_tableView->horizontalHeader()->setDefaultSectionSize(150);
  
  // 最后一列自动伸展
  m_tableView->horizontalHeader()->setStretchLastSection(true);
  
  // 使用等宽字体
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
  QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#else
  QFont monoFont("Consolas", 10);
#endif
  monoFont.setPointSize(11);
  m_tableView->setFont(monoFont);
  
  // 选择模式
  m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  
  // 隐藏垂直表头（行号由模型提供）
  m_tableView->verticalHeader()->setVisible(true);
  m_tableView->verticalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_tableView->verticalHeader()->setFixedWidth(60);
  
  // 禁用网格线，使用交替行颜色
  m_tableView->setShowGrid(false);
  m_tableView->setAlternatingRowColors(true);
  
  // 像素级滚动
  m_tableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  
  // 应用 VS Code 暗色主题（包含表头白色块修复）
  m_tableView->setStyleSheet(R"(
      QTableView {
          background-color: #1e1e1e;
          color: #d4d4d4;
          border: none;
          outline: none;
          gridline-color: #3c3c3c;
      }
      QTableView::item {
          padding: 2px 8px;
          border: none;
      }
      QTableView::item:hover {
          background-color: #2a2d2e;
      }
      QTableView::item:selected {
          background-color: #094771;
      }
      QTableView::item:alternate {
          background-color: #252526;
      }
      /* 修复表头容器背景色 */
      QHeaderView {
          background-color: #252526;
          border: none;
      }
      QHeaderView::section {
          background-color: #252526;
          color: #d4d4d4;
          border: none;
          border-right: 1px solid #3c3c3c;
          border-bottom: 1px solid #3c3c3c;
          padding: 4px 8px;
          font-weight: bold;
      }
      QHeaderView::section:hover {
          background-color: #2a2d2e;
      }
      /* 关键：修复表格左上角白色块 */
      QTableCornerButton::section {
          background-color: #252526;
          border: none;
          border-right: 1px solid #3c3c3c;
          border-bottom: 1px solid #3c3c3c;
      }
  )");
  
  qDebug() << "[MainWindow] TableView setup complete with performance optimizations";
}

// ========== 视图切换（原始文本 / 表格视图） ==========

void MainWindow::toggleViewMode() {
  if (!m_viewStack) return;
  
  m_isTableViewMode = !m_isTableViewMode;
  
  if (m_isTableViewMode) {
    // 切换到表格模式
    m_viewStack->setCurrentIndex(1);
    m_model->setTableModeEnabled(true);
    
    // 更新切换按钮文本
    if (m_toggleViewAction) {
      m_toggleViewAction->setText(tr("Raw Text"));
      m_toggleViewAction->setToolTip(tr("Switch to Raw Text view (high performance)"));
    }
    
    m_statusLabel->setText(tr("Grid View - Structured Log Mode"));
    
    qDebug() << "[MainWindow] Switched to TABLE view mode";
  } else {
    // 切换到原始文本模式
    m_viewStack->setCurrentIndex(0);
    m_model->setTableModeEnabled(false);
    
    // 更新切换按钮文本
    if (m_toggleViewAction) {
      m_toggleViewAction->setText(tr("Grid View"));
      m_toggleViewAction->setToolTip(tr("Switch to Grid View (structured columns)"));
    }
    
    m_statusLabel->setText(tr("Raw Text - High Performance Mode"));
    
    qDebug() << "[MainWindow] Switched to RAW TEXT view mode";
  }
}

