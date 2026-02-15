#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KXmlGuiWindow>
#include <QFontComboBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolBar>

class DocumentTab;
class SessionManager;

class MainWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

protected:
  void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
  void newTab();
  void openFile();
  void saveFile();
  void saveFileAs();
  void closeTab(int index);
  void onTabChanged(int index);
  void onCurrentDocModified();

  // Formatting
  void toggleBold();
  void toggleItalic();
  void toggleUnderline();
  void toggleBulletList();
  void selectTextColor();
  void onFontFamilyChanged(const QFont &font);
  void onFontSizeChanged(int size);
  void updateFormatActions();

private:
  void setupActions();
  void setupFormatToolbar();
  DocumentTab *currentTab();
  DocumentTab *tabAt(int index);
  void updateWindowTitle();
  void restoreSession();
  void saveSession();

  QTabWidget *m_tabWidget;
  SessionManager *m_sessionManager;

  // Format toolbar widgets
  QFontComboBox *m_fontCombo;
  QSpinBox *m_fontSizeSpinBox;

  // Format actions
  QAction *m_boldAction;
  QAction *m_italicAction;
  QAction *m_underlineAction;
  QAction *m_bulletAction;
  QAction *m_colorAction;

  int m_untitledCounter = 0;
};

#endif // MAINWINDOW_H
