/**
 * @file SearchBar.h
 * @brief 浮动搜索栏 - 类似 Chrome/VS Code 的查找界面
 * @version 1.1 - 添加正则表达式支持
 */

#ifndef SEARCHBAR_H
#define SEARCHBAR_H

#include <QWidget>

class QLineEdit;
class QLabel;
class QToolButton;

class SearchBar : public QWidget {
  Q_OBJECT

public:
  enum SearchDirection { Next, Previous };

  explicit SearchBar(QWidget *parent = nullptr);
  ~SearchBar() override;

  /**
   * @brief 显示搜索栏并聚焦输入框
   */
  void show();

  /**
   * @brief 更新搜索结果信息
   * @param current 当前结果索引（从1开始，0表示未选中）
   * @param total 总结果数
   */
  void updateResultInfo(int current, int total);

  /**
   * @brief 获取当前搜索文本
   */
  QString searchText() const;

  /**
   * @brief 是否启用正则表达式模式
   */
  bool isRegexMode() const;

signals:
  /**
   * @brief 搜索请求信号
   * @param text 搜索文本
   * @param direction 搜索方向
   * @param useRegex 是否使用正则表达式
   */
  void searchRequested(const QString &text, SearchDirection direction, bool useRegex);

  /**
   * @brief 关闭信号
   */
  void closed();

protected:
  /**
   * @brief 按键事件处理（ESC 关闭）
   */
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void onSearchTextChanged();
  void onPreviousClicked();
  void onNextClicked();
  void onCloseClicked();
  void onRegexToggled(bool checked);

private:
  void setupUi();
  void applyStyle();

private:
  QLineEdit *m_searchInput = nullptr;
  QLabel *m_resultLabel = nullptr;
  QToolButton *m_prevButton = nullptr;
  QToolButton *m_nextButton = nullptr;
  QToolButton *m_closeButton = nullptr;
  QToolButton *m_regexButton = nullptr;  ///< 正则表达式切换按钮
};

#endif // SEARCHBAR_H
