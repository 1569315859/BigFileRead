/**
 * @file MainWindow.cpp
 * @brief 主窗口类实现
 * @version 1.5 - VS Code 风格暗色主题 + 异步加载支持
 */

#include "MainWindow.h"
#include "BigFileModel.h"
#include "CompactLineDelegate.h"

#include <QApplication>
#include <QFileDialog>
#include <QInputDialog>
#include <QListView>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QStatusBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QShortcut>
#include <QFont>
#include <QFontDatabase>
#include <QStyle>
#include <QTimer>
#include <QFileInfo>

// VS Code Dark Theme Colors
namespace VSCodeDark {
    const QString Background      = "#1e1e1e";
    const QString EditorBg        = "#1e1e1e";
    const QString SidebarBg       = "#252526";
    const QString StatusBarBg     = "#007acc";
    const QString TextColor       = "#d4d4d4";
    const QString TextColorDim    = "#808080";
    const QString HoverBg         = "#2a2d2e";
    const QString SelectedBg      = "#094771";
    const QString BorderColor     = "#3c3c3c";
    const QString ScrollbarBg     = "#1e1e1e";
    const QString ScrollbarHandle = "#424242";
    const QString ProgressBarBg   = "#3c3c3c";
    const QString ProgressBarFill = "#0e639c";
    const QString MenuBg          = "#252526";
    const QString MenuHover       = "#094771";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    applyDarkTheme();
    setupUi();
    createMenus();
    updateWindowTitle();

    // 连接模型信号
    connect(m_model, &BigFileModel::fileLoaded,
            this, &MainWindow::onFileLoaded);
    connect(m_model, &BigFileModel::indexingProgress,
            this, &MainWindow::onIndexingProgress);
    connect(m_model, &BigFileModel::indexingCancelled,
            this, &MainWindow::onIndexingCancelled);

    // 状态更新定时器 - 实时显示行数变化
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(100);  // 每 100ms 更新一次状态栏
    connect(m_statusTimer, &QTimer::timeout, this, [this]() {
        if (m_model && m_isLoading) {
            m_lineCountLabel->setText(tr("Lines: %1").arg(m_model->lineCount()));
        }
    });
}

MainWindow::~MainWindow()
{
}

void MainWindow::applyDarkTheme()
{
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
    .arg(VSCodeDark::Background)      // %1
    .arg(VSCodeDark::SidebarBg)       // %2
    .arg(VSCodeDark::TextColor)       // %3
    .arg(VSCodeDark::BorderColor)     // %4
    .arg(VSCodeDark::MenuHover)       // %5
    .arg(VSCodeDark::HoverBg)         // %6
    .arg(VSCodeDark::SelectedBg)      // %7
    .arg(VSCodeDark::StatusBarBg)     // %8
    .arg(VSCodeDark::ProgressBarBg)   // %9
    .arg(VSCodeDark::ProgressBarFill);// %10

    qApp->setStyleSheet(styleSheet);
}

void MainWindow::setupUi()
{
    // 设置窗口大小
    resize(1200, 800);

    // 创建中央 ListView
    m_listView = new QListView(this);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    // *** 关键性能优化 ***
    m_listView->setUniformItemSizes(true);      // 告诉 Qt 所有行高度相同
   // 2. 【删除或注释】这一行！Batch 模式会延迟布局更新，导致滚动条不随 insertRows 增长
    // m_listView->setLayoutMode(QListView::Batched); 
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
    delegate->setRowHeight(monoFont);  // 根据字体计算正确行高
    m_listView->setItemDelegate(delegate);

    setCentralWidget(m_listView);

    // 创建模型并绑定视图
    m_model = new BigFileModel(this);
    m_listView->setModel(m_model);

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

    QShortcut *gotoShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);
    connect(gotoShortcut, &QShortcut::activated, this, &MainWindow::onGoToLine);

    // ESC 取消加载
    QShortcut *cancelShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(cancelShortcut, &QShortcut::activated, this, [this]() {
        if (m_model->isIndexing()) {
            m_model->cancelIndexing();
        }
    });
}

void MainWindow::createMenus()
{
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
                              "Built with Qt %1").arg(qVersion()));
    });
}

void MainWindow::setLoadingState(bool loading)
{
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

void MainWindow::onOpenFile()
{
    if (m_isLoading) {
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open File"),
        QString(),
        tr("All Files (*);;Text Files (*.txt *.log *.csv);;Log Files (*.log)")
        );

    if (!filePath.isEmpty()) {
        openFile(filePath);
    }
}

void MainWindow::openFile(const QString &filePath)
{
    // *** 允许在加载过程中打开新文件（会自动取消当前加载）***
    
    setLoadingState(true);
    m_statusLabel->setText(tr("Loading: %1").arg(filePath));
    m_lineCountLabel->setText(tr("Lines: 1"));  // 第一行立即可用
    m_progressBar->setValue(0);

    m_model->loadFile(filePath);
    updateWindowTitle();
}

void MainWindow::onFileLoaded(bool success, const QString &message)
{
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

void MainWindow::onIndexingProgress(int percent)
{
    m_progressBar->setValue(percent);
    m_lineCountLabel->setText(tr("Lines: %1").arg(m_model->lineCount()));
}

void MainWindow::onIndexingCancelled()
{
    setLoadingState(false);
    m_statusLabel->setText(tr("Loading cancelled"));
}

void MainWindow::onGoToLine()
{
    if (m_model->lineCount() == 0) {
        QMessageBox::information(this, tr("Info"), tr("Please open a file first"));
        return;
    }

    bool ok;
    int lineNumber = QInputDialog::getInt(
        this,
        tr("Go to Line"),
        tr("Line number (1 - %1):").arg(m_model->lineCount()),
        1,
        1,
        m_model->lineCount(),
        1,
        &ok
        );

    if (ok) {
        QModelIndex index = m_model->index(lineNumber - 1, 0);
        m_listView->scrollTo(index, QAbstractItemView::PositionAtCenter);
        m_listView->setCurrentIndex(index);
    }
}

void MainWindow::updateWindowTitle()
{
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
