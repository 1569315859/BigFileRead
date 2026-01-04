/**
 * @file MainWindow.cpp
 * @brief 主窗口类实现
 * @version 1.5 - VS Code 风格暗色主题 + 异步加载支持
 */

#include "MainWindow.h"
#include "BigFileModel.h"
#include "CompactLineDelegate.h"
#include "SearchBar.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QResizeEvent>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>  // for std::sort

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
}

MainWindow::~MainWindow() {}

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

  // 使用 ScrollPerItem 而非 ScrollPerPixel，对大文件更流畅
  m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
  m_listView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

  m_listView->setWordWrap(false);
  m_listView->setTextElideMode(Qt::ElideNone);
  m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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
  
  // === 创建主从布局分隔器 ===
  m_splitter = new QSplitter(Qt::Vertical, this);
  m_splitter->addWidget(m_listView);         // 上：主列表视图
  m_splitter->addWidget(m_detailTextEdit);   // 下：详细文本视图
  
  // 设置默认比例：80% 列表，20% 详细视图
  m_splitter->setStretchFactor(0, 4);  // 列表视图权重 4
  m_splitter->setStretchFactor(1, 1);  // 详细视图权重 1
  
  // 设置分隔条样式（VS Code 主题）
  m_splitter->setHandleWidth(1);
  m_splitter->setStyleSheet(
      "QSplitter::handle {"
      "   background-color: #3c3c3c;"  /* 分隔线颜色 */
      "}"
  );
  
  setCentralWidget(m_splitter);

  // 创建模型并绑定视图
  m_model = new BigFileModel(this);
  m_listView->setModel(m_model);
  
  // 连接选择变化信号 - 更新详细视图
  connect(m_listView->selectionModel(), &QItemSelectionModel::currentRowChanged,
          this, [this](const QModelIndex &current, const QModelIndex &previous) {
              Q_UNUSED(previous)
              if (current.isValid()) {
                  QString lineText = current.data().toString();
                  m_detailTextEdit->setPlainText(lineText);
              } else {
                  m_detailTextEdit->clear();
              }
          });

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

  // 连接搜索栏信号
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

  fileMenu->addSeparator();

  QAction *exitAction = fileMenu->addAction(tr("Exit(&X)"));
  exitAction->setShortcut(QKeySequence::Quit);
  connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

  // 编辑菜单
  QMenu *editMenu = menuBar()->addMenu(tr("Edit(&E)"));

  QAction *gotoAction = editMenu->addAction(tr("Go to Line(&G)..."));
  gotoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
  connect(gotoAction, &QAction::triggered, this, &MainWindow::onGoToLine);

  // 帮助菜单
  QMenu *helpMenu = menuBar()->addMenu(tr("Help(&H)"));

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

  setLoadingState(true);
  m_statusLabel->setText(tr("Loading: %1").arg(filePath));
  m_lineCountLabel->setText(tr("Lines: 1")); // 第一行立即可用
  m_progressBar->setValue(0);

  m_model->loadFile(filePath);
  updateWindowTitle();
}

void MainWindow::onFileLoaded(bool success, const QString &message) {
  setLoadingState(false);

  if (success) {
    m_statusLabel->setText(message);
    m_lineCountLabel->setText(tr("Lines: %1").arg(m_model->lineCount()));
  } else {
    m_statusLabel->setText(tr("Load failed"));
    m_lineCountLabel->clear();
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

// ========== 搜索功能实现 ==========

void MainWindow::onShowSearch() {
  if (!m_model || m_model->lineCount() == 0) {
    QMessageBox::information(this, tr("Info"), tr("Please open a file first"));
    return;
  }

  m_searchBar->show();
  m_searchBar->raise();
}

void MainWindow::onSearchRequested(const QString &text, int direction) {
  if (text.isEmpty()) {
    m_currentSearchIndex = -1;
    m_searchBar->updateResultInfo(0, 0);
    return;
  }

  // 如果是新搜索，启动异步搜索
  const auto &currentResults = m_model->searchResults();
  if (currentResults.empty() || m_searchBar->searchText() != text) {
    // 开始新搜索
    m_model->search(text);
    m_currentSearchIndex = -1;

    // 更新委托高亮
    auto delegate =
        static_cast<CompactLineDelegate *>(m_listView->itemDelegate());
    delegate->setHighlightTerm(text, Qt::CaseInsensitive);
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
    m_statusLabel->setText(tr("Found %1 matches").arg(totalResults));
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
    
    // 显示菜单
    contextMenu.exec(m_listView->viewport()->mapToGlobal(pos));
}

