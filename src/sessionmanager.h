#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

class DocumentTab;

class SessionManager : public QObject {
  Q_OBJECT

public:
  explicit SessionManager(QObject *parent = nullptr);

  // Backup a single tab's content to disk
  void backupTab(DocumentTab *tab);

  // Restore a tab from a session backup
  bool restoreTab(DocumentTab *tab, const QString &sessionId);

  // Remove a tab's backup (when tab is closed)
  void removeTabBackup(const QString &sessionId);

  // Save/load the session index (list of tab IDs + active tab)
  void saveSessionIndex(const QStringList &tabIds, int activeIndex);
  QStringList loadSessionIndex(int &activeIndex);

  // Get the session directory path
  QString sessionDir() const { return m_sessionDir; }

private:
  void ensureSessionDir();
  QString tabBackupPath(const QString &sessionId) const;
  QString tabMetaPath(const QString &sessionId) const;

  QString m_sessionDir;
};

#endif // SESSIONMANAGER_H
