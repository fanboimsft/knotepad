#include "documenttab.h"
#include "rtfhandler.h"

#include <QFile>
#include <QFileInfo>
#include <QTextCursor>
#include <QTextList>
#include <QTextListFormat>
#include <QTextStream>
#include <QVBoxLayout>

DocumentTab::DocumentTab(QWidget *parent)
    : QWidget(parent), m_editor(new QTextEdit(this)),
      m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_editor);

  m_editor->setAcceptRichText(true);
  m_editor->setTabStopDistance(40);

  // Set a sensible default font
  QFont defaultFont(QStringLiteral("Sans Serif"), 13);
  m_editor->setFont(defaultFont);

  connect(m_editor->document(), &QTextDocument::contentsChanged, this,
          &DocumentTab::onContentsChanged);
  connect(m_editor, &QTextEdit::cursorPositionChanged, this,
          &DocumentTab::onCursorPositionChanged);
}

DocumentTab::~DocumentTab() = default;

bool DocumentTab::loadFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  m_loading = true;

  QByteArray data = file.readAll();
  file.close();

  // Detect content type
  QByteArray trimmed = data.trimmed();
  if (trimmed.startsWith("{\\rtf")) {
    // RTF content — use our RTF parser
    RtfHandler::readRtf(data, m_editor->document());
  } else {
    QString text = QString::fromUtf8(data);
    if (text.trimmed().startsWith(QLatin1String("<!DOCTYPE html"),
                                  Qt::CaseInsensitive) ||
        text.trimmed().startsWith(QLatin1String("<html"), Qt::CaseInsensitive)) {
      m_editor->setHtml(text);
    } else {
      m_editor->setPlainText(text);
    }
  }

  m_filePath = path;
  m_tabTitle = QFileInfo(path).fileName();
  m_modified = false;
  m_loading = false;

  return true;
}

bool DocumentTab::saveFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }

  if (path.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive)) {
    // Plain text
    QTextStream stream(&file);
    stream << m_editor->toPlainText();
  } else if (path.endsWith(QLatin1String(".rtf"), Qt::CaseInsensitive)) {
    // Real RTF format
    QByteArray rtfData = RtfHandler::writeRtf(m_editor->document());
    file.write(rtfData);
  } else {
    // HTML (default for .html and other extensions)
    QTextStream stream(&file);
    stream << m_editor->toHtml();
  }

  file.close();

  m_filePath = path;
  m_tabTitle = QFileInfo(path).fileName();
  setModified(false);

  return true;
}

void DocumentTab::setModified(bool modified) {
  if (m_modified != modified) {
    m_modified = modified;
    Q_EMIT modifiedChanged(modified);
  }
}

QString DocumentTab::toHtml() const { return m_editor->toHtml(); }

void DocumentTab::setFromHtml(const QString &html) {
  m_loading = true;
  m_editor->setHtml(html);
  m_loading = false;
}

void DocumentTab::mergeFormat(const QTextCharFormat &fmt) {
  QTextCursor cursor = m_editor->textCursor();
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::WordUnderCursor);
  }
  cursor.mergeCharFormat(fmt);
  m_editor->mergeCurrentCharFormat(fmt);
}

QTextCharFormat DocumentTab::currentCharFormat() const {
  return m_editor->currentCharFormat();
}

void DocumentTab::toggleBulletList(bool enable) {
  QTextCursor cursor = m_editor->textCursor();

  if (enable) {
    QTextListFormat listFmt;
    listFmt.setStyle(QTextListFormat::ListDisc);
    listFmt.setIndent(1);
    cursor.createList(listFmt);
  } else {
    QTextList *list = cursor.currentList();
    if (list) {
      // Remove block from list
      QTextBlock block = cursor.block();
      list->remove(block);
      // Reset indent
      QTextBlockFormat bfmt = block.blockFormat();
      bfmt.setIndent(0);
      cursor.setBlockFormat(bfmt);
    }
  }
}

bool DocumentTab::isInBulletList() const {
  QTextCursor cursor = m_editor->textCursor();
  return cursor.currentList() != nullptr;
}

void DocumentTab::onContentsChanged() {
  if (!m_loading) {
    setModified(true);
  }
}

void DocumentTab::onCursorPositionChanged() { Q_EMIT cursorFormatChanged(); }
