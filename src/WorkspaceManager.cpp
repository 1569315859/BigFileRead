/**
 * @file WorkspaceManager.cpp
 * @brief 工作区管理器实现
 */

#include "WorkspaceManager.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

WorkspaceManager::WorkspaceManager(QObject *parent)
    : QObject(parent),
      m_settings("BigFileRead", "Workspaces")
{
  loadAllWorkspaces();
}

WorkspaceManager::~WorkspaceManager() {
  saveAllWorkspaces();
}

QVariantList WorkspaceManager::workspaces() const {
  QVariantList result;
  for (const Workspace &ws : m_workspaces) {
    result.append(workspaceToVariant(ws));
  }
  return result;
}

bool WorkspaceManager::saveWorkspace(const QString &name, const QString &description) {
  if (name.isEmpty()) {
    qWarning() << "Cannot save workspace: name is empty";
    return false;
  }
  
  // 检查是否已存在
  for (Workspace &existing : m_workspaces) {
    if (existing.name == name) {
      // 更新现有工作区
      existing.description = description;
      existing.lastUsed = QDateTime::currentDateTime();
      
      // 从当前状态更新
      existing.filePath = m_currentState.value("filePath").toString();
      existing.scrollPosition = m_currentState.value("scrollPosition").toLongLong();
      existing.filterKeyword = m_currentState.value("filterKeyword").toString();
      existing.caseSensitive = m_currentState.value("caseSensitive").toBool();
      existing.useRegex = m_currentState.value("useRegex").toBool();
      existing.logLevel = m_currentState.value("logLevel").toString();
      existing.bookmarks = m_currentState.value("bookmarks").toList();
      existing.followMode = m_currentState.value("followMode").toBool();
      existing.lineNumberWidth = m_currentState.value("lineNumberWidth", 80).toDouble();
      existing.contentWidth = m_currentState.value("contentWidth", 800).toDouble();
      
      saveAllWorkspaces();
      m_currentWorkspace = name;
      emit workspacesChanged();
      emit currentWorkspaceChanged();
      emit workspaceSaved(name);
      return true;
    }
  }
  
  // 检查数量限制
  if (m_workspaces.size() >= MAX_WORKSPACES) {
    qWarning() << "Maximum workspace count reached:" << MAX_WORKSPACES;
    return false;
  }
  
  // 创建新工作区
  Workspace ws;
  ws.name = name;
  ws.description = description;
  ws.createdAt = QDateTime::currentDateTime();
  ws.lastUsed = QDateTime::currentDateTime();
  
  // 从当前状态保存
  ws.filePath = m_currentState.value("filePath").toString();
  ws.scrollPosition = m_currentState.value("scrollPosition").toLongLong();
  ws.filterKeyword = m_currentState.value("filterKeyword").toString();
  ws.caseSensitive = m_currentState.value("caseSensitive").toBool();
  ws.useRegex = m_currentState.value("useRegex").toBool();
  ws.logLevel = m_currentState.value("logLevel").toString();
  ws.bookmarks = m_currentState.value("bookmarks").toList();
  ws.followMode = m_currentState.value("followMode").toBool();
  ws.lineNumberWidth = m_currentState.value("lineNumberWidth", 80).toDouble();
  ws.contentWidth = m_currentState.value("contentWidth", 800).toDouble();
  
  m_workspaces.append(ws);
  saveAllWorkspaces();
  
  m_currentWorkspace = name;
  emit workspacesChanged();
  emit currentWorkspaceChanged();
  emit workspaceSaved(name);
  
  qDebug() << "Workspace saved:" << name;
  return true;
}

bool WorkspaceManager::updateWorkspace(const QString &name, const QVariantMap &state) {
  for (Workspace &ws : m_workspaces) {
    if (ws.name == name) {
      ws.lastUsed = QDateTime::currentDateTime();
      ws.filePath = state.value("filePath", ws.filePath).toString();
      ws.scrollPosition = state.value("scrollPosition", ws.scrollPosition).toLongLong();
      ws.filterKeyword = state.value("filterKeyword", ws.filterKeyword).toString();
      ws.caseSensitive = state.value("caseSensitive", ws.caseSensitive).toBool();
      ws.useRegex = state.value("useRegex", ws.useRegex).toBool();
      ws.logLevel = state.value("logLevel", ws.logLevel).toString();
      if (state.contains("bookmarks")) {
        ws.bookmarks = state.value("bookmarks").toList();
      }
      ws.followMode = state.value("followMode", ws.followMode).toBool();
      ws.lineNumberWidth = state.value("lineNumberWidth", ws.lineNumberWidth).toDouble();
      ws.contentWidth = state.value("contentWidth", ws.contentWidth).toDouble();
      
      saveAllWorkspaces();
      emit workspacesChanged();
      return true;
    }
  }
  return false;
}

QVariantMap WorkspaceManager::loadWorkspace(const QString &name) {
  for (Workspace &ws : m_workspaces) {
    if (ws.name == name) {
      ws.lastUsed = QDateTime::currentDateTime();
      saveAllWorkspaces();
      
      m_currentWorkspace = name;
      emit currentWorkspaceChanged();
      emit workspaceLoaded(name);
      
      qDebug() << "Workspace loaded:" << name;
      return workspaceToVariant(ws);
    }
  }
  return QVariantMap();
}

bool WorkspaceManager::deleteWorkspace(const QString &name) {
  for (int i = 0; i < m_workspaces.size(); ++i) {
    if (m_workspaces[i].name == name) {
      m_workspaces.removeAt(i);
      saveAllWorkspaces();
      
      if (m_currentWorkspace == name) {
        m_currentWorkspace.clear();
        emit currentWorkspaceChanged();
      }
      
      emit workspacesChanged();
      qDebug() << "Workspace deleted:" << name;
      return true;
    }
  }
  return false;
}

bool WorkspaceManager::renameWorkspace(const QString &oldName, const QString &newName) {
  if (newName.isEmpty() || hasWorkspace(newName)) {
    return false;
  }
  
  for (Workspace &ws : m_workspaces) {
    if (ws.name == oldName) {
      ws.name = newName;
      saveAllWorkspaces();
      
      if (m_currentWorkspace == oldName) {
        m_currentWorkspace = newName;
        emit currentWorkspaceChanged();
      }
      
      emit workspacesChanged();
      return true;
    }
  }
  return false;
}

bool WorkspaceManager::hasWorkspace(const QString &name) const {
  for (const Workspace &ws : m_workspaces) {
    if (ws.name == name) {
      return true;
    }
  }
  return false;
}

QVariantMap WorkspaceManager::getWorkspace(const QString &name) const {
  for (const Workspace &ws : m_workspaces) {
    if (ws.name == name) {
      return workspaceToVariant(ws);
    }
  }
  return QVariantMap();
}

bool WorkspaceManager::exportWorkspace(const QString &name, const QString &path) {
  if (path.isEmpty()) return false;
  
  Workspace *target = nullptr;
  for (Workspace &ws : m_workspaces) {
    if (ws.name == name) {
      target = &ws;
      break;
    }
  }
  
  if (!target) return false;
  
  QJsonObject obj;
  obj["version"] = 1;
  obj["name"] = target->name;
  obj["description"] = target->description;
  obj["createdAt"] = target->createdAt.toString(Qt::ISODate);
  obj["filePath"] = target->filePath;
  obj["scrollPosition"] = target->scrollPosition;
  obj["filterKeyword"] = target->filterKeyword;
  obj["caseSensitive"] = target->caseSensitive;
  obj["useRegex"] = target->useRegex;
  obj["logLevel"] = target->logLevel;
  obj["bookmarks"] = QJsonArray::fromVariantList(target->bookmarks);
  obj["followMode"] = target->followMode;
  obj["lineNumberWidth"] = target->lineNumberWidth;
  obj["contentWidth"] = target->contentWidth;
  
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  
  file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
  file.close();
  
  qDebug() << "Workspace exported:" << name << "to" << path;
  return true;
}

bool WorkspaceManager::importWorkspace(const QString &path, bool overwrite) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
  file.close();
  
  if (error.error != QJsonParseError::NoError) {
    return false;
  }
  
  QJsonObject obj = doc.object();
  QString name = obj["name"].toString();
  
  if (name.isEmpty()) return false;
  
  if (!overwrite && hasWorkspace(name)) {
    return false;
  }
  
  // 如果覆盖模式，先删除
  if (overwrite) {
    deleteWorkspace(name);
  }
  
  Workspace ws;
  ws.name = name;
  ws.description = obj["description"].toString();
  ws.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
  if (!ws.createdAt.isValid()) {
    ws.createdAt = QDateTime::currentDateTime();
  }
  ws.lastUsed = QDateTime::currentDateTime();
  ws.filePath = obj["filePath"].toString();
  ws.scrollPosition = obj["scrollPosition"].toVariant().toLongLong();
  ws.filterKeyword = obj["filterKeyword"].toString();
  ws.caseSensitive = obj["caseSensitive"].toBool();
  ws.useRegex = obj["useRegex"].toBool();
  ws.logLevel = obj["logLevel"].toString();
  ws.bookmarks = obj["bookmarks"].toArray().toVariantList();
  ws.followMode = obj["followMode"].toBool();
  ws.lineNumberWidth = obj["lineNumberWidth"].toDouble(80);
  ws.contentWidth = obj["contentWidth"].toDouble(800);
  
  m_workspaces.append(ws);
  saveAllWorkspaces();
  emit workspacesChanged();
  
  qDebug() << "Workspace imported:" << name;
  return true;
}

void WorkspaceManager::setCurrentState(const QVariantMap &state) {
  m_currentState = state;
}

bool WorkspaceManager::quickSave() {
  if (m_currentWorkspace.isEmpty()) {
    return false;
  }
  return saveWorkspace(m_currentWorkspace);
}

QVariantList WorkspaceManager::recentWorkspaces(int count) const {
  QList<Workspace> sorted = m_workspaces;
  std::sort(sorted.begin(), sorted.end(), [](const Workspace &a, const Workspace &b) {
    return a.lastUsed > b.lastUsed;
  });
  
  QVariantList result;
  int n = std::min(count, static_cast<int>(sorted.size()));
  for (int i = 0; i < n; ++i) {
    result.append(workspaceToVariant(sorted[i]));
  }
  return result;
}

void WorkspaceManager::loadAllWorkspaces() {
  m_workspaces.clear();
  
  int count = m_settings.beginReadArray("workspaces");
  for (int i = 0; i < count; ++i) {
    m_settings.setArrayIndex(i);
    
    Workspace ws;
    ws.name = m_settings.value("name").toString();
    ws.description = m_settings.value("description").toString();
    ws.createdAt = m_settings.value("createdAt").toDateTime();
    ws.lastUsed = m_settings.value("lastUsed").toDateTime();
    ws.filePath = m_settings.value("filePath").toString();
    ws.scrollPosition = m_settings.value("scrollPosition").toLongLong();
    ws.filterKeyword = m_settings.value("filterKeyword").toString();
    ws.caseSensitive = m_settings.value("caseSensitive").toBool();
    ws.useRegex = m_settings.value("useRegex").toBool();
    ws.logLevel = m_settings.value("logLevel").toString();
    ws.bookmarks = m_settings.value("bookmarks").toList();
    ws.followMode = m_settings.value("followMode").toBool();
    ws.lineNumberWidth = m_settings.value("lineNumberWidth", 80).toDouble();
    ws.contentWidth = m_settings.value("contentWidth", 800).toDouble();
    
    if (!ws.name.isEmpty()) {
      m_workspaces.append(ws);
    }
  }
  m_settings.endArray();
  
  m_currentWorkspace = m_settings.value("currentWorkspace").toString();
  
  qDebug() << "Loaded" << m_workspaces.size() << "workspaces";
}

void WorkspaceManager::saveAllWorkspaces() {
  m_settings.beginWriteArray("workspaces", m_workspaces.size());
  for (int i = 0; i < m_workspaces.size(); ++i) {
    m_settings.setArrayIndex(i);
    const Workspace &ws = m_workspaces[i];
    
    m_settings.setValue("name", ws.name);
    m_settings.setValue("description", ws.description);
    m_settings.setValue("createdAt", ws.createdAt);
    m_settings.setValue("lastUsed", ws.lastUsed);
    m_settings.setValue("filePath", ws.filePath);
    m_settings.setValue("scrollPosition", ws.scrollPosition);
    m_settings.setValue("filterKeyword", ws.filterKeyword);
    m_settings.setValue("caseSensitive", ws.caseSensitive);
    m_settings.setValue("useRegex", ws.useRegex);
    m_settings.setValue("logLevel", ws.logLevel);
    m_settings.setValue("bookmarks", ws.bookmarks);
    m_settings.setValue("followMode", ws.followMode);
    m_settings.setValue("lineNumberWidth", ws.lineNumberWidth);
    m_settings.setValue("contentWidth", ws.contentWidth);
  }
  m_settings.endArray();
  
  m_settings.setValue("currentWorkspace", m_currentWorkspace);
  m_settings.sync();
}

QVariantMap WorkspaceManager::workspaceToVariant(const Workspace &ws) const {
  QVariantMap map;
  map["name"] = ws.name;
  map["description"] = ws.description;
  map["createdAt"] = ws.createdAt.toString(Qt::ISODate);
  map["lastUsed"] = ws.lastUsed.toString(Qt::ISODate);
  map["filePath"] = ws.filePath;
  map["scrollPosition"] = ws.scrollPosition;
  map["filterKeyword"] = ws.filterKeyword;
  map["caseSensitive"] = ws.caseSensitive;
  map["useRegex"] = ws.useRegex;
  map["logLevel"] = ws.logLevel;
  map["bookmarks"] = ws.bookmarks;
  map["followMode"] = ws.followMode;
  map["lineNumberWidth"] = ws.lineNumberWidth;
  map["contentWidth"] = ws.contentWidth;
  return map;
}

Workspace WorkspaceManager::variantToWorkspace(const QVariantMap &map) const {
  Workspace ws;
  ws.name = map["name"].toString();
  ws.description = map["description"].toString();
  ws.createdAt = QDateTime::fromString(map["createdAt"].toString(), Qt::ISODate);
  ws.lastUsed = QDateTime::fromString(map["lastUsed"].toString(), Qt::ISODate);
  ws.filePath = map["filePath"].toString();
  ws.scrollPosition = map["scrollPosition"].toLongLong();
  ws.filterKeyword = map["filterKeyword"].toString();
  ws.caseSensitive = map["caseSensitive"].toBool();
  ws.useRegex = map["useRegex"].toBool();
  ws.logLevel = map["logLevel"].toString();
  ws.bookmarks = map["bookmarks"].toList();
  ws.followMode = map["followMode"].toBool();
  ws.lineNumberWidth = map["lineNumberWidth"].toDouble();
  ws.contentWidth = map["contentWidth"].toDouble();
  return ws;
}
