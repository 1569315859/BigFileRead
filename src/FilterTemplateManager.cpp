/**
 * @file FilterTemplateManager.cpp
 * @brief 过滤模板管理器实现
 */

#include "FilterTemplateManager.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

FilterTemplateManager::FilterTemplateManager(QObject *parent)
    : QObject(parent),
      m_settings("BigFileRead", "FilterTemplates")
{
  loadTemplates();
}

FilterTemplateManager::~FilterTemplateManager() {
  saveTemplates();
}

QVariantList FilterTemplateManager::templates() const {
  QVariantList result;
  for (const FilterTemplate &tmpl : m_templates) {
    result.append(templateToVariant(tmpl));
  }
  return result;
}

QVariantList FilterTemplateManager::recentTemplates() const {
  // 按最后使用时间排序，返回最近使用的模板
  QList<FilterTemplate> sorted = m_templates;
  std::sort(sorted.begin(), sorted.end(), [](const FilterTemplate &a, const FilterTemplate &b) {
    return a.lastUsed > b.lastUsed;
  });
  
  QVariantList result;
  int count = std::min(MAX_RECENT, static_cast<int>(sorted.size()));
  for (int i = 0; i < count; ++i) {
    if (sorted[i].lastUsed.isValid()) {
      result.append(templateToVariant(sorted[i]));
    }
  }
  return result;
}

bool FilterTemplateManager::saveTemplate(const QString &name, const QString &keyword,
                                          bool caseSensitive, bool useRegex,
                                          bool includeContext, int contextLines,
                                          const QString &logLevel) {
  if (name.isEmpty() || keyword.isEmpty()) {
    qWarning() << "Cannot save template: name or keyword is empty";
    return false;
  }
  
  // 检查是否已存在
  for (FilterTemplate &existing : m_templates) {
    if (existing.name == name) {
      // 更新现有模板
      existing.keyword = keyword;
      existing.caseSensitive = caseSensitive;
      existing.useRegex = useRegex;
      existing.includeContext = includeContext;
      existing.contextLines = contextLines;
      existing.logLevel = logLevel;
      saveTemplates();
      emit templatesChanged();
      qDebug() << "Template updated:" << name;
      return true;
    }
  }
  
  // 创建新模板
  FilterTemplate tmpl;
  tmpl.name = name;
  tmpl.keyword = keyword;
  tmpl.caseSensitive = caseSensitive;
  tmpl.useRegex = useRegex;
  tmpl.includeContext = includeContext;
  tmpl.contextLines = contextLines;
  tmpl.logLevel = logLevel;
  tmpl.createdAt = QDateTime::currentDateTime();
  tmpl.useCount = 0;
  
  m_templates.append(tmpl);
  saveTemplates();
  emit templatesChanged();
  
  qDebug() << "Template saved:" << name;
  return true;
}

bool FilterTemplateManager::updateTemplate(const QString &name, const QString &keyword,
                                            bool caseSensitive, bool useRegex,
                                            bool includeContext, int contextLines,
                                            const QString &logLevel) {
  for (FilterTemplate &tmpl : m_templates) {
    if (tmpl.name == name) {
      tmpl.keyword = keyword;
      tmpl.caseSensitive = caseSensitive;
      tmpl.useRegex = useRegex;
      tmpl.includeContext = includeContext;
      tmpl.contextLines = contextLines;
      tmpl.logLevel = logLevel;
      saveTemplates();
      emit templatesChanged();
      return true;
    }
  }
  return false;
}

bool FilterTemplateManager::deleteTemplate(const QString &name) {
  for (int i = 0; i < m_templates.size(); ++i) {
    if (m_templates[i].name == name) {
      m_templates.removeAt(i);
      saveTemplates();
      emit templatesChanged();
      qDebug() << "Template deleted:" << name;
      return true;
    }
  }
  return false;
}

QVariantMap FilterTemplateManager::getTemplate(const QString &name) const {
  for (const FilterTemplate &tmpl : m_templates) {
    if (tmpl.name == name) {
      return templateToVariant(tmpl);
    }
  }
  return QVariantMap();
}

bool FilterTemplateManager::hasTemplate(const QString &name) const {
  for (const FilterTemplate &tmpl : m_templates) {
    if (tmpl.name == name) {
      return true;
    }
  }
  return false;
}

QVariantMap FilterTemplateManager::applyTemplate(const QString &name) {
  for (FilterTemplate &tmpl : m_templates) {
    if (tmpl.name == name) {
      tmpl.lastUsed = QDateTime::currentDateTime();
      tmpl.useCount++;
      saveTemplates();
      emit templatesChanged();
      emit templateApplied(name, tmpl.keyword);
      qDebug() << "Template applied:" << name << "use count:" << tmpl.useCount;
      return templateToVariant(tmpl);
    }
  }
  return QVariantMap();
}

bool FilterTemplateManager::renameTemplate(const QString &oldName, const QString &newName) {
  if (newName.isEmpty() || hasTemplate(newName)) {
    return false;
  }
  
  for (FilterTemplate &tmpl : m_templates) {
    if (tmpl.name == oldName) {
      tmpl.name = newName;
      saveTemplates();
      emit templatesChanged();
      return true;
    }
  }
  return false;
}

bool FilterTemplateManager::exportTemplates(const QString &path) {
  if (path.isEmpty()) {
    return false;
  }
  
  QJsonArray array;
  for (const FilterTemplate &tmpl : m_templates) {
    QJsonObject obj;
    obj["name"] = tmpl.name;
    obj["keyword"] = tmpl.keyword;
    obj["caseSensitive"] = tmpl.caseSensitive;
    obj["useRegex"] = tmpl.useRegex;
    obj["includeContext"] = tmpl.includeContext;
    obj["contextLines"] = tmpl.contextLines;
    obj["logLevel"] = tmpl.logLevel;
    obj["createdAt"] = tmpl.createdAt.toString(Qt::ISODate);
    array.append(obj);
  }
  
  QJsonObject root;
  root["version"] = 1;
  root["templates"] = array;
  
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Cannot open file for writing:" << path;
    return false;
  }
  
  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  file.close();
  
  qDebug() << "Templates exported to:" << path;
  return true;
}

bool FilterTemplateManager::importTemplates(const QString &path, bool overwrite) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "Cannot open file for reading:" << path;
    return false;
  }
  
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
  file.close();
  
  if (error.error != QJsonParseError::NoError) {
    qWarning() << "JSON parse error:" << error.errorString();
    return false;
  }
  
  QJsonObject root = doc.object();
  QJsonArray array = root["templates"].toArray();
  
  int imported = 0;
  for (const QJsonValue &val : array) {
    QJsonObject obj = val.toObject();
    QString name = obj["name"].toString();
    
    if (name.isEmpty()) continue;
    
    if (!overwrite && hasTemplate(name)) {
      continue;
    }
    
    FilterTemplate tmpl;
    tmpl.name = name;
    tmpl.keyword = obj["keyword"].toString();
    tmpl.caseSensitive = obj["caseSensitive"].toBool();
    tmpl.useRegex = obj["useRegex"].toBool();
    tmpl.includeContext = obj["includeContext"].toBool();
    tmpl.contextLines = obj["contextLines"].toInt();
    tmpl.logLevel = obj["logLevel"].toString();
    tmpl.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
    if (!tmpl.createdAt.isValid()) {
      tmpl.createdAt = QDateTime::currentDateTime();
    }
    tmpl.useCount = 0;
    
    // 如果覆盖模式，先删除现有的
    if (overwrite) {
      deleteTemplate(name);
    }
    
    m_templates.append(tmpl);
    imported++;
  }
  
  if (imported > 0) {
    saveTemplates();
    emit templatesChanged();
  }
  
  qDebug() << "Imported" << imported << "templates from:" << path;
  return imported > 0;
}

void FilterTemplateManager::clearAllTemplates() {
  m_templates.clear();
  saveTemplates();
  emit templatesChanged();
}

void FilterTemplateManager::loadTemplates() {
  m_templates.clear();
  
  int count = m_settings.beginReadArray("templates");
  for (int i = 0; i < count; ++i) {
    m_settings.setArrayIndex(i);
    
    FilterTemplate tmpl;
    tmpl.name = m_settings.value("name").toString();
    tmpl.keyword = m_settings.value("keyword").toString();
    tmpl.caseSensitive = m_settings.value("caseSensitive", false).toBool();
    tmpl.useRegex = m_settings.value("useRegex", false).toBool();
    tmpl.includeContext = m_settings.value("includeContext", false).toBool();
    tmpl.contextLines = m_settings.value("contextLines", 0).toInt();
    tmpl.logLevel = m_settings.value("logLevel").toString();
    tmpl.createdAt = m_settings.value("createdAt").toDateTime();
    tmpl.lastUsed = m_settings.value("lastUsed").toDateTime();
    tmpl.useCount = m_settings.value("useCount", 0).toInt();
    
    if (!tmpl.name.isEmpty()) {
      m_templates.append(tmpl);
    }
  }
  m_settings.endArray();
  
  qDebug() << "Loaded" << m_templates.size() << "filter templates";
}

void FilterTemplateManager::saveTemplates() {
  m_settings.beginWriteArray("templates", m_templates.size());
  for (int i = 0; i < m_templates.size(); ++i) {
    m_settings.setArrayIndex(i);
    const FilterTemplate &tmpl = m_templates[i];
    
    m_settings.setValue("name", tmpl.name);
    m_settings.setValue("keyword", tmpl.keyword);
    m_settings.setValue("caseSensitive", tmpl.caseSensitive);
    m_settings.setValue("useRegex", tmpl.useRegex);
    m_settings.setValue("includeContext", tmpl.includeContext);
    m_settings.setValue("contextLines", tmpl.contextLines);
    m_settings.setValue("logLevel", tmpl.logLevel);
    m_settings.setValue("createdAt", tmpl.createdAt);
    m_settings.setValue("lastUsed", tmpl.lastUsed);
    m_settings.setValue("useCount", tmpl.useCount);
  }
  m_settings.endArray();
  m_settings.sync();
}

QVariantMap FilterTemplateManager::templateToVariant(const FilterTemplate &tmpl) const {
  QVariantMap map;
  map["name"] = tmpl.name;
  map["keyword"] = tmpl.keyword;
  map["caseSensitive"] = tmpl.caseSensitive;
  map["useRegex"] = tmpl.useRegex;
  map["includeContext"] = tmpl.includeContext;
  map["contextLines"] = tmpl.contextLines;
  map["logLevel"] = tmpl.logLevel;
  map["createdAt"] = tmpl.createdAt.toString(Qt::ISODate);
  map["lastUsed"] = tmpl.lastUsed.toString(Qt::ISODate);
  map["useCount"] = tmpl.useCount;
  return map;
}

FilterTemplate FilterTemplateManager::variantToTemplate(const QVariantMap &map) const {
  FilterTemplate tmpl;
  tmpl.name = map["name"].toString();
  tmpl.keyword = map["keyword"].toString();
  tmpl.caseSensitive = map["caseSensitive"].toBool();
  tmpl.useRegex = map["useRegex"].toBool();
  tmpl.includeContext = map["includeContext"].toBool();
  tmpl.contextLines = map["contextLines"].toInt();
  tmpl.logLevel = map["logLevel"].toString();
  tmpl.createdAt = QDateTime::fromString(map["createdAt"].toString(), Qt::ISODate);
  tmpl.lastUsed = QDateTime::fromString(map["lastUsed"].toString(), Qt::ISODate);
  tmpl.useCount = map["useCount"].toInt();
  return tmpl;
}
