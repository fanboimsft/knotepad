#ifndef DOCUMENTTAB_H
#define DOCUMENTTAB_H

#include <QTextCharFormat>
#include <QTextEdit>
#include <QUuid>
#include <QWidget>

class DocumentTab : public QWidget {
  Q_OBJECT

public:
  explicit DocumentTab(QWidget *parent = nullptr);
  ~DocumentTab() override;

  // File operations
  bool loadFile(const QString &path);
  bool saveFile(const QString &path);

  // Properties
  QString filePath() const { return m_filePath; }
  QString tabTitle() const { return m_tabTitle; }
  void setTabTitle(const QString &title) { m_tabTitle = title; }
  bool isModified() const { return m_modified; }
  void setModified(bool modified);

  // Session management
  QString sessionId() const { return m_sessionId; }
  void setSessionId(const QString &id) { m_sessionId = id; }
  QString toHtml() const;
  void setFromHtml(const QString &html);

  // Formatting
  void mergeFormat(const QTextCharFormat &fmt);
  QTextCharFormat currentCharFormat() const;
  void toggleBulletList(bool enable);
  bool isInBulletList() const;

  // Access to editor
  QTextEdit *editor() const { return m_editor; }

Q_SIGNALS:
  void modifiedChanged(bool modified);
  void cursorFormatChanged();

private Q_SLOTS:
  void onContentsChanged();
  void onCursorPositionChanged();

private:
  QTextEdit *m_editor;
  QString m_filePath;
  QString m_tabTitle;
  QString m_sessionId;
  bool m_modified = false;
  bool m_loading = false;
};

#endif // DOCUMENTTAB_H
