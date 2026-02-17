#include "mainwindow.h"
#include "documenttab.h"
#include "sessionmanager.h"

#include <QAction>
#include <QApplication>
#include <QAudioOutput>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QMediaPlayer>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTextCharFormat>
#include <QTextList>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <KActionCollection>
#include <KLocalizedString>

MainWindow::MainWindow(QWidget *parent) : KXmlGuiWindow(parent) {
  // Tab widget as central widget
  m_tabWidget = new QTabWidget(this);
  m_tabWidget->setTabsClosable(true);
  m_tabWidget->setMovable(true);
  m_tabWidget->setDocumentMode(true);
  setCentralWidget(m_tabWidget);

  connect(m_tabWidget, &QTabWidget::tabCloseRequested, this,
          &MainWindow::closeTab);
  connect(m_tabWidget, &QTabWidget::currentChanged, this,
          &MainWindow::onTabChanged);

  m_sessionManager = new SessionManager(this);

  setupActions();
  setupFormatToolbar();

  // Setup window state (shortcuts, statusbar, save/restore size)
  // No Create or ToolBar — menus are built programmatically in setupActions()
  setupGUI(QSize(800, 600), KXmlGuiWindow::Keys | KXmlGuiWindow::StatusBar |
                                KXmlGuiWindow::Save);

  // Restore previous session or create a new tab
  restoreSession();
  if (m_tabWidget->count() == 0) {
    newTab();
  }

  updateWindowTitle();
  statusBar()->showMessage(i18n("Ready"));
}

MainWindow::~MainWindow() = default;

void MainWindow::setupActions() {
  KActionCollection *ac = actionCollection();

  // --- File menu ---
  QMenu *fileMenu = menuBar()->addMenu(i18n("&File"));

  QAction *newAction = new QAction(
      QIcon::fromTheme(QStringLiteral("document-new")), i18n("New Tab"), this);
  ac->addAction(QStringLiteral("file_new"), newAction);
  ac->setDefaultShortcut(newAction, QKeySequence::New);
  connect(newAction, &QAction::triggered, this, &MainWindow::newTab);
  fileMenu->addAction(newAction);

  QAction *openAction = new QAction(
      QIcon::fromTheme(QStringLiteral("document-open")), i18n("Open..."), this);
  ac->addAction(QStringLiteral("file_open"), openAction);
  ac->setDefaultShortcut(openAction, QKeySequence::Open);
  connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
  fileMenu->addAction(openAction);

  fileMenu->addSeparator();

  QAction *saveAction = new QAction(
      QIcon::fromTheme(QStringLiteral("document-save")), i18n("Save"), this);
  ac->addAction(QStringLiteral("file_save"), saveAction);
  ac->setDefaultShortcut(saveAction, QKeySequence::Save);
  connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);
  fileMenu->addAction(saveAction);

  QAction *saveAsAction =
      new QAction(QIcon::fromTheme(QStringLiteral("document-save-as")),
                  i18n("Save As..."), this);
  ac->addAction(QStringLiteral("file_save_as"), saveAsAction);
  ac->setDefaultShortcut(saveAsAction, QKeySequence::SaveAs);
  connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);
  fileMenu->addAction(saveAsAction);

  fileMenu->addSeparator();

  QAction *quitAction = new QAction(
      QIcon::fromTheme(QStringLiteral("application-exit")), i18n("Quit"), this);
  ac->addAction(QStringLiteral("file_quit"), quitAction);
  ac->setDefaultShortcut(quitAction, QKeySequence::Quit);
  connect(quitAction, &QAction::triggered, this, &MainWindow::close);
  fileMenu->addAction(quitAction);

  // --- Format menu ---
  QMenu *formatMenu = menuBar()->addMenu(i18n("F&ormat"));

  m_boldAction = new QAction(
      QIcon::fromTheme(QStringLiteral("format-text-bold")), i18n("Bold"), this);
  ac->addAction(QStringLiteral("format_bold"), m_boldAction);
  ac->setDefaultShortcut(m_boldAction, QKeySequence::Bold);
  m_boldAction->setCheckable(true);
  connect(m_boldAction, &QAction::triggered, this, &MainWindow::toggleBold);
  formatMenu->addAction(m_boldAction);

  m_italicAction =
      new QAction(QIcon::fromTheme(QStringLiteral("format-text-italic")),
                  i18n("Italic"), this);
  ac->addAction(QStringLiteral("format_italic"), m_italicAction);
  ac->setDefaultShortcut(m_italicAction, QKeySequence::Italic);
  m_italicAction->setCheckable(true);
  connect(m_italicAction, &QAction::triggered, this, &MainWindow::toggleItalic);
  formatMenu->addAction(m_italicAction);

  m_underlineAction =
      new QAction(QIcon::fromTheme(QStringLiteral("format-text-underline")),
                  i18n("Underline"), this);
  ac->addAction(QStringLiteral("format_underline"), m_underlineAction);
  ac->setDefaultShortcut(m_underlineAction, QKeySequence::Underline);
  m_underlineAction->setCheckable(true);
  connect(m_underlineAction, &QAction::triggered, this,
          &MainWindow::toggleUnderline);
  formatMenu->addAction(m_underlineAction);

  m_strikethroughAction =
      new QAction(QIcon::fromTheme(QStringLiteral("format-text-strikethrough")),
                  i18n("Strikethrough"), this);
  ac->addAction(QStringLiteral("format_strikethrough"), m_strikethroughAction);
  m_strikethroughAction->setCheckable(true);
  connect(m_strikethroughAction, &QAction::triggered, this,
          &MainWindow::toggleStrikethrough);
  formatMenu->addAction(m_strikethroughAction);

  formatMenu->addSeparator();

  m_bulletAction =
      new QAction(QIcon::fromTheme(QStringLiteral("format-list-unordered")),
                  i18n("Bullet List"), this);
  ac->addAction(QStringLiteral("format_bullet"), m_bulletAction);
  m_bulletAction->setCheckable(true);
  connect(m_bulletAction, &QAction::triggered, this,
          &MainWindow::toggleBulletList);
  formatMenu->addAction(m_bulletAction);

  formatMenu->addSeparator();

  m_colorAction =
      new QAction(QIcon::fromTheme(QStringLiteral("format-text-color")),
                  i18n("Text Color..."), this);
  ac->addAction(QStringLiteral("format_color"), m_colorAction);
  connect(m_colorAction, &QAction::triggered, this,
          &MainWindow::selectTextColor);
  formatMenu->addAction(m_colorAction);
}

void MainWindow::setupFormatToolbar() {
  QToolBar *formatBar = new QToolBar(i18n("Format"), this);
  formatBar->setObjectName(QStringLiteral("formatToolbar"));
  addToolBar(Qt::TopToolBarArea, formatBar);

  // Font family combo
  m_fontCombo = new QFontComboBox(formatBar);
  m_fontCombo->setMaximumWidth(200);
  formatBar->addWidget(m_fontCombo);
  connect(m_fontCombo, &QFontComboBox::currentFontChanged, this,
          &MainWindow::onFontFamilyChanged);

  // Font size spinner
  m_fontSizeSpinBox = new QSpinBox(formatBar);
  m_fontSizeSpinBox->setRange(6, 72);
  m_fontSizeSpinBox->setValue(13);
  m_fontSizeSpinBox->setMaximumWidth(60);
  formatBar->addWidget(m_fontSizeSpinBox);
  connect(m_fontSizeSpinBox, &QSpinBox::valueChanged, this,
          &MainWindow::onFontSizeChanged);

  formatBar->addSeparator();

  // Add format actions to toolbar
  formatBar->addAction(m_boldAction);
  formatBar->addAction(m_italicAction);
  formatBar->addAction(m_underlineAction);
  formatBar->addAction(m_strikethroughAction);
  formatBar->addSeparator();
  formatBar->addAction(m_bulletAction);
  formatBar->addSeparator();
  formatBar->addAction(m_colorAction);

  // Timer section (after text color)
  setupTimerWidgets(formatBar);
}

void MainWindow::setupTimerWidgets(QToolBar *formatBar) {
  formatBar->addSeparator();

  // Timer duration combo box
  m_timerCombo = new QComboBox(formatBar);
  for (int i = 1; i <= 20; ++i) {
    m_timerCombo->addItem(i18np("%1 min", "%1 min", i), i);
  }
  m_timerCombo->setMaximumWidth(90);
  m_timerCombo->setToolTip(i18n("Timer duration"));
  formatBar->addWidget(m_timerCombo);

  // Timer start/stop button
  m_timerButton = new QToolButton(formatBar);
  m_timerButton->setIcon(QIcon::fromTheme(QStringLiteral("chronometer")));
  m_timerButton->setToolTip(i18n("Start Timer"));
  m_timerButton->setCheckable(true);
  formatBar->addWidget(m_timerButton);
  connect(m_timerButton, &QToolButton::toggled, this, [this](bool checked) {
    if (checked) {
      startCountdown();
    } else {
      stopCountdown();
    }
  });

  // Countdown label
  m_timerLabel = new QLabel(QStringLiteral("00:00"), formatBar);
  m_timerLabel->setToolTip(i18n("Time remaining"));
  QFont monoFont(QStringLiteral("monospace"));
  monoFont.setStyleHint(QFont::Monospace);
  monoFont.setPointSize(11);
  m_timerLabel->setFont(monoFont);
  m_timerLabel->setMinimumWidth(50);
  m_timerLabel->setAlignment(Qt::AlignCenter);
  formatBar->addWidget(m_timerLabel);

  // Internal QTimer for tick updates
  m_countdownTimer = new QTimer(this);
  m_countdownTimer->setInterval(1000);
  connect(m_countdownTimer, &QTimer::timeout, this, &MainWindow::onTimerTick);

  // Media player for alarm sound
  m_mediaPlayer = new QMediaPlayer(this);
  m_audioOutput = new QAudioOutput(this);
  m_audioOutput->setVolume(1.0);
  m_mediaPlayer->setAudioOutput(m_audioOutput);
  m_mediaPlayer->setSource(QUrl(QStringLiteral("qrc:/assets/alarm.mp3")));
}

void MainWindow::newTab() {
  m_untitledCounter++;
  DocumentTab *tab = new DocumentTab(this);
  tab->setTabTitle(i18n("Untitled %1", m_untitledCounter));
  int idx = m_tabWidget->addTab(tab, tab->tabTitle());
  m_tabWidget->setCurrentIndex(idx);

  connect(tab, &DocumentTab::modifiedChanged, this,
          &MainWindow::onCurrentDocModified);
  connect(tab, &DocumentTab::cursorFormatChanged, this,
          &MainWindow::updateFormatActions);

  updateWindowTitle();
}

void MainWindow::openFile() {
  QStringList filePaths = QFileDialog::getOpenFileNames(
      this, i18n("Open File"), QString(),
      i18n("All Supported Files (*.html *.rtf *.txt);;Rich Text (*.rtf);;HTML "
           "Files (*.html);;Text Files (*.txt);;All Files (*)"));

  for (const QString &filePath : filePaths) {
    // Check if already open
    bool alreadyOpen = false;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
      DocumentTab *tab = tabAt(i);
      if (tab && tab->filePath() == filePath) {
        m_tabWidget->setCurrentIndex(i);
        alreadyOpen = true;
        break;
      }
    }
    if (alreadyOpen)
      continue;

    DocumentTab *tab = new DocumentTab(this);
    if (tab->loadFile(filePath)) {
      int idx = m_tabWidget->addTab(tab, tab->tabTitle());
      m_tabWidget->setCurrentIndex(idx);
      connect(tab, &DocumentTab::modifiedChanged, this,
              &MainWindow::onCurrentDocModified);
      connect(tab, &DocumentTab::cursorFormatChanged, this,
              &MainWindow::updateFormatActions);
    } else {
      delete tab;
      QMessageBox::warning(this, i18n("Error"),
                           i18n("Could not open file: %1", filePath));
    }
  }
  updateWindowTitle();
}

void MainWindow::saveFile() {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;

  if (tab->filePath().isEmpty()) {
    saveFileAs();
  } else {
    if (!tab->saveFile(tab->filePath())) {
      QMessageBox::warning(this, i18n("Error"), i18n("Could not save file."));
    }
    updateWindowTitle();
  }
}

void MainWindow::saveFileAs() {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;

  QString filePath = QFileDialog::getSaveFileName(
      this, i18n("Save File"), QString(),
      i18n("HTML Files (*.html);;Rich Text (*.rtf);;Text Files (*.txt);;All "
           "Files (*)"));

  if (!filePath.isEmpty()) {
    if (!tab->saveFile(filePath)) {
      QMessageBox::warning(this, i18n("Error"), i18n("Could not save file."));
    } else {
      m_tabWidget->setTabText(m_tabWidget->currentIndex(), tab->tabTitle());
    }
    updateWindowTitle();
  }
}

void MainWindow::closeTab(int index) {
  DocumentTab *tab = tabAt(index);
  if (!tab)
    return;

  // Remove the session backup for this tab
  m_sessionManager->removeTabBackup(tab->sessionId());

  m_tabWidget->removeTab(index);
  delete tab;

  if (m_tabWidget->count() == 0) {
    newTab();
  }
  updateWindowTitle();
}

void MainWindow::onTabChanged(int index) {
  Q_UNUSED(index);
  updateWindowTitle();
  updateFormatActions();
}

void MainWindow::onCurrentDocModified() {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;

  int idx = m_tabWidget->currentIndex();
  QString title = tab->tabTitle();
  if (tab->isModified()) {
    title += QStringLiteral(" *");
  }
  m_tabWidget->setTabText(idx, title);
  updateWindowTitle();

  // Auto-save to session backup
  m_sessionManager->backupTab(tab);
}

// --- Formatting slots ---

void MainWindow::toggleBold() {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;

  QTextCharFormat fmt;
  fmt.setFontWeight(m_boldAction->isChecked() ? QFont::Bold : QFont::Normal);
  tab->mergeFormat(fmt);
}

void MainWindow::toggleItalic() {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;

  QTextCharFormat fmt;
  fmt.setFontItalic(m_italicAction->isChecked());
  tab->mergeFormat(fmt);
}

void MainWindow::toggleUnderline() {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;

  QTextCharFormat fmt;
  fmt.setFontUnderline(m_underlineAction->isChecked());
  tab->mergeFormat(fmt);
}

void MainWindow::toggleStrikethrough() {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;

  QTextCharFormat fmt;
  fmt.setFontStrikeOut(m_strikethroughAction->isChecked());
  tab->mergeFormat(fmt);
}

void MainWindow::toggleBulletList() {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;
  tab->toggleBulletList(m_bulletAction->isChecked());
}

void MainWindow::selectTextColor() {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;

  QColor color =
      QColorDialog::getColor(tab->currentCharFormat().foreground().color(),
                             this, i18n("Select Text Color"));
  if (color.isValid()) {
    QTextCharFormat fmt;
    fmt.setForeground(color);
    tab->mergeFormat(fmt);
  }
}

void MainWindow::onFontFamilyChanged(const QFont &font) {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;

  QTextCharFormat fmt;
  fmt.setFontFamilies({font.family()});
  tab->mergeFormat(fmt);
}

void MainWindow::onFontSizeChanged(int size) {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;

  QTextCharFormat fmt;
  fmt.setFontPointSize(size);
  tab->mergeFormat(fmt);
}

void MainWindow::updateFormatActions() {
  DocumentTab *tab = currentTab();
  if (!tab)
    return;

  QTextCharFormat fmt = tab->currentCharFormat();

  // Block signals to prevent recursive updates
  m_boldAction->blockSignals(true);
  m_italicAction->blockSignals(true);
  m_underlineAction->blockSignals(true);
  m_strikethroughAction->blockSignals(true);
  m_fontCombo->blockSignals(true);
  m_fontSizeSpinBox->blockSignals(true);
  m_bulletAction->blockSignals(true);

  m_boldAction->setChecked(fmt.fontWeight() >= QFont::Bold);
  m_italicAction->setChecked(fmt.fontItalic());
  m_underlineAction->setChecked(fmt.fontUnderline());
  m_strikethroughAction->setChecked(fmt.fontStrikeOut());

  QStringList families = fmt.fontFamilies().toStringList();
  QFont f(families.isEmpty() ? QStringLiteral("Sans Serif") : families.first());
  m_fontCombo->setCurrentFont(f);
  m_fontSizeSpinBox->setValue(
      fmt.fontPointSize() > 0 ? static_cast<int>(fmt.fontPointSize()) : 13);

  m_bulletAction->setChecked(tab->isInBulletList());

  m_boldAction->blockSignals(false);
  m_italicAction->blockSignals(false);
  m_underlineAction->blockSignals(false);
  m_strikethroughAction->blockSignals(false);
  m_fontCombo->blockSignals(false);
  m_fontSizeSpinBox->blockSignals(false);
  m_bulletAction->blockSignals(false);
}

// --- Helpers ---

DocumentTab *MainWindow::currentTab() {
  return qobject_cast<DocumentTab *>(m_tabWidget->currentWidget());
}

DocumentTab *MainWindow::tabAt(int index) {
  return qobject_cast<DocumentTab *>(m_tabWidget->widget(index));
}

void MainWindow::updateWindowTitle() {
  DocumentTab *tab = currentTab();
  if (tab) {
    QString title = tab->tabTitle();
    if (tab->isModified()) {
      title += QStringLiteral(" *");
    }
    setWindowTitle(title + QStringLiteral(" — KNotepad"));
  } else {
    setWindowTitle(QStringLiteral("KNotepad"));
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
  saveSession();
  event->accept();
}

void MainWindow::saveSession() {
  QStringList tabSessionData;
  for (int i = 0; i < m_tabWidget->count(); ++i) {
    DocumentTab *tab = tabAt(i);
    if (tab) {
      m_sessionManager->backupTab(tab);
      tabSessionData.append(tab->sessionId());
    }
  }
  m_sessionManager->saveSessionIndex(tabSessionData,
                                     m_tabWidget->currentIndex());
}

void MainWindow::restoreSession() {
  int activeIndex = 0;
  QStringList tabIds = m_sessionManager->loadSessionIndex(activeIndex);

  for (const QString &sessionId : tabIds) {
    DocumentTab *tab = new DocumentTab(this);
    if (m_sessionManager->restoreTab(tab, sessionId)) {
      int idx = m_tabWidget->addTab(tab, tab->tabTitle());
      Q_UNUSED(idx);
      connect(tab, &DocumentTab::modifiedChanged, this,
              &MainWindow::onCurrentDocModified);
      connect(tab, &DocumentTab::cursorFormatChanged, this,
              &MainWindow::updateFormatActions);
    } else {
      delete tab;
    }
  }

  if (activeIndex >= 0 && activeIndex < m_tabWidget->count()) {
    m_tabWidget->setCurrentIndex(activeIndex);
  }
}

// --- Timer slots ---

void MainWindow::startCountdown() {
  int minutes = m_timerCombo->currentData().toInt();
  m_remainingSeconds = minutes * 60;

  // Update label immediately
  int mins = m_remainingSeconds / 60;
  int secs = m_remainingSeconds % 60;
  m_timerLabel->setText(QStringLiteral("%1:%2")
                            .arg(mins, 2, 10, QLatin1Char('0'))
                            .arg(secs, 2, 10, QLatin1Char('0')));

  // Disable the combo while timer is running
  m_timerCombo->setEnabled(false);
  m_timerButton->setToolTip(i18n("Stop Timer"));
  m_timerButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-stop")));

  m_countdownTimer->start();
  statusBar()->showMessage(i18n("Timer started: %1 minute(s)", minutes));
}

void MainWindow::stopCountdown() {
  m_countdownTimer->stop();
  m_remainingSeconds = 0;
  m_timerLabel->setText(QStringLiteral("00:00"));
  m_timerCombo->setEnabled(true);
  m_timerButton->setToolTip(i18n("Start Timer"));
  m_timerButton->setIcon(QIcon::fromTheme(QStringLiteral("chronometer")));

  // Uncheck without triggering the toggled signal
  m_timerButton->blockSignals(true);
  m_timerButton->setChecked(false);
  m_timerButton->blockSignals(false);

  // Stop alarm if playing
  m_mediaPlayer->stop();

  statusBar()->showMessage(i18n("Timer stopped"));
}

void MainWindow::onTimerTick() {
  if (m_remainingSeconds <= 0) {
    onTimerFinished();
    return;
  }

  m_remainingSeconds--;

  int mins = m_remainingSeconds / 60;
  int secs = m_remainingSeconds % 60;
  m_timerLabel->setText(QStringLiteral("%1:%2")
                            .arg(mins, 2, 10, QLatin1Char('0'))
                            .arg(secs, 2, 10, QLatin1Char('0')));

  if (m_remainingSeconds <= 0) {
    onTimerFinished();
  }
}

void MainWindow::onTimerFinished() {
  m_countdownTimer->stop();
  m_timerLabel->setText(QStringLiteral("00:00"));

  // Play alarm sound
  m_mediaPlayer->setPosition(0);
  m_mediaPlayer->play();

  // Reset the button and combo state
  m_timerCombo->setEnabled(true);
  m_timerButton->blockSignals(true);
  m_timerButton->setChecked(false);
  m_timerButton->setIcon(QIcon::fromTheme(QStringLiteral("chronometer")));
  m_timerButton->setToolTip(i18n("Start Timer"));
  m_timerButton->blockSignals(false);

  // Show notification dialog
  QMessageBox msgBox(this);
  msgBox.setWindowTitle(i18n("Timer"));
  msgBox.setText(i18n("Time is up!"));
  msgBox.setIcon(QMessageBox::Information);
  msgBox.setStandardButtons(QMessageBox::Ok);
  msgBox.setDefaultButton(QMessageBox::Ok);
  msgBox.exec();

  // Stop alarm after user dismisses dialog
  m_mediaPlayer->stop();

  statusBar()->showMessage(i18n("Timer finished"));
}
