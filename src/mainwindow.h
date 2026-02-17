#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KXmlGuiWindow>
#include <QComboBox>
#include <QFontComboBox>
#include <QLabel>
#include <QMediaPlayer>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

class DocumentTab;
class SessionManager;
class QAudioOutput;

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
  void toggleStrikethrough();
  void toggleBulletList();
  void selectTextColor();
  void onFontFamilyChanged(const QFont &font);
  void onFontSizeChanged(int size);
  void updateFormatActions();

  // Timer
  void startCountdown();
  void stopCountdown();
  void onTimerTick();
  void onTimerFinished();

private:
  void setupActions();
  void setupFormatToolbar();
  void setupTimerWidgets(QToolBar *formatBar);
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
  QAction *m_strikethroughAction;
  QAction *m_bulletAction;
  QAction *m_colorAction;

  // Timer widgets
  QToolButton *m_timerButton;
  QComboBox *m_timerCombo;
  QLabel *m_timerLabel;
  QTimer *m_countdownTimer;
  QMediaPlayer *m_mediaPlayer;
  QAudioOutput *m_audioOutput;
  int m_remainingSeconds = 0;

  int m_untitledCounter = 0;
};

#endif // MAINWINDOW_H
