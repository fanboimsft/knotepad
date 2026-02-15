#include "sessionmanager.h"
#include "documenttab.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>

SessionManager::SessionManager(QObject *parent) : QObject(parent) {
  m_sessionDir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
      QStringLiteral("/sessions");
  ensureSessionDir();
}

void SessionManager::ensureSessionDir() {
  QDir dir(m_sessionDir);
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }
}

QString SessionManager::tabBackupPath(const QString &sessionId) const {
  return m_sessionDir + QStringLiteral("/") + sessionId +
         QStringLiteral(".html");
}

QString SessionManager::tabMetaPath(const QString &sessionId) const {
  return m_sessionDir + QStringLiteral("/") + sessionId +
         QStringLiteral(".json");
}

void SessionManager::backupTab(DocumentTab *tab) {
  ensureSessionDir();

  // Save content as HTML
  QFile contentFile(tabBackupPath(tab->sessionId()));
  if (contentFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QTextStream stream(&contentFile);
    stream << tab->toHtml();
    contentFile.close();
  }

  // Save metadata (file path, title, modified state)
  QJsonObject meta;
  meta[QStringLiteral("filePath")] = tab->filePath();
  meta[QStringLiteral("tabTitle")] = tab->tabTitle();
  meta[QStringLiteral("modified")] = tab->isModified();

  QFile metaFile(tabMetaPath(tab->sessionId()));
  if (metaFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    metaFile.write(QJsonDocument(meta).toJson(QJsonDocument::Compact));
    metaFile.close();
  }
}

bool SessionManager::restoreTab(DocumentTab *tab, const QString &sessionId) {
  QString contentPath = tabBackupPath(sessionId);
  QString metaPath = tabMetaPath(sessionId);

  if (!QFile::exists(contentPath) || !QFile::exists(metaPath)) {
    return false;
  }

  // Load metadata
  QFile metaFile(metaPath);
  if (!metaFile.open(QIODevice::ReadOnly)) {
    return false;
  }
  QJsonObject meta = QJsonDocument::fromJson(metaFile.readAll()).object();
  metaFile.close();

  // Load content
  QFile contentFile(contentPath);
  if (!contentFile.open(QIODevice::ReadOnly)) {
    return false;
  }
  QString html = QString::fromUtf8(contentFile.readAll());
  contentFile.close();

  // Restore the tab
  tab->setSessionId(sessionId);
  tab->setFromHtml(html);

  QString filePath = meta[QStringLiteral("filePath")].toString();
  QString tabTitle = meta[QStringLiteral("tabTitle")].toString();
  bool modified = meta[QStringLiteral("modified")].toBool();

  if (!filePath.isEmpty()) {
    // This tab was associated with a file on disk
    // Reload from file if it still exists, otherwise use backup
    if (QFile::exists(filePath)) {
      tab->loadFile(filePath);
      // If it was modified (had unsaved changes), overlay the backup content
      if (modified) {
        tab->setFromHtml(html);
        tab->setModified(true);
      }
    } else {
      // File no longer exists, keep backup content
      tab->setTabTitle(tabTitle);
      tab->setModified(true);
    }
  } else {
    // Untitled document
    tab->setTabTitle(tabTitle);
    tab->setModified(modified);
  }

  return true;
}

void SessionManager::removeTabBackup(const QString &sessionId) {
  QFile::remove(tabBackupPath(sessionId));
  QFile::remove(tabMetaPath(sessionId));
}

void SessionManager::saveSessionIndex(const QStringList &tabIds,
                                      int activeIndex) {
  ensureSessionDir();

  QJsonObject index;
  index[QStringLiteral("activeIndex")] = activeIndex;

  QJsonArray tabs;
  for (const QString &id : tabIds) {
    tabs.append(id);
  }
  index[QStringLiteral("tabs")] = tabs;

  QFile file(m_sessionDir + QStringLiteral("/session.json"));
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    file.write(QJsonDocument(index).toJson(QJsonDocument::Compact));
    file.close();
  }

  // Clean up orphaned backup files (tabs that are no longer in the session)
  QDir dir(m_sessionDir);
  QStringList allFiles = dir.entryList(
      QStringList() << QStringLiteral("*.html") << QStringLiteral("*.json"),
      QDir::Files);
  for (const QString &fileName : allFiles) {
    if (fileName == QStringLiteral("session.json"))
      continue;

    // Extract session ID from filename
    QString id = fileName;
    id = id.remove(QStringLiteral(".html")).remove(QStringLiteral(".json"));

    if (!tabIds.contains(id)) {
      QFile::remove(m_sessionDir + QStringLiteral("/") + fileName);
    }
  }
}

QStringList SessionManager::loadSessionIndex(int &activeIndex) {
  QStringList result;
  activeIndex = 0;

  QFile file(m_sessionDir + QStringLiteral("/session.json"));
  if (!file.open(QIODevice::ReadOnly)) {
    return result;
  }

  QJsonObject index = QJsonDocument::fromJson(file.readAll()).object();
  file.close();

  activeIndex = index[QStringLiteral("activeIndex")].toInt(0);

  QJsonArray tabs = index[QStringLiteral("tabs")].toArray();
  for (const QJsonValue &val : tabs) {
    result.append(val.toString());
  }

  return result;
}
