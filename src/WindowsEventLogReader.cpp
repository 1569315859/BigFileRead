/**
 * @file WindowsEventLogReader.cpp
 * @brief Windows Event Log Reader Implementation
 */

#include "WindowsEventLogReader.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QTimeZone>

#ifdef Q_OS_WIN
#pragma comment(lib, "wevtapi.lib")
#endif

WindowsEventLogReader& WindowsEventLogReader::instance()
{
    static WindowsEventLogReader instance;
    return instance;
}

WindowsEventLogReader::WindowsEventLogReader(QObject* parent)
    : QObject(parent)
    , m_isReading(false)
    , m_isSubscribed(false)
    , m_cancelRequested(false)
    , m_eventCount(0)
    , m_workerThread(nullptr)
#ifdef Q_OS_WIN
    , m_hSubscription(nullptr)
    , m_hQuery(nullptr)
#endif
{
    // Initialize available channels
    refreshChannels();
}

WindowsEventLogReader::~WindowsEventLogReader()
{
    unsubscribe();
    cancelRead();
    
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
    }
}

bool WindowsEventLogReader::isAvailable() const
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

void WindowsEventLogReader::setError(const QString& error)
{
    m_lastError = error;
    emit errorOccurred(error);
    qWarning() << "WindowsEventLogReader error:" << error;
}

void WindowsEventLogReader::setReading(bool reading)
{
    if (m_isReading != reading) {
        m_isReading = reading;
        emit readingChanged();
    }
}

QStringList WindowsEventLogReader::getStandardChannels() const
{
    return QStringList{
        "System",
        "Application", 
        "Security",
        "Setup",
        "Microsoft-Windows-PowerShell/Operational",
        "Microsoft-Windows-TaskScheduler/Operational",
        "Microsoft-Windows-WindowsUpdateClient/Operational"
    };
}

QString WindowsEventLogReader::levelToString(int level)
{
    switch (level) {
        case 1: return "Critical";
        case 2: return "Error";
        case 3: return "Warning";
        case 4: return "Information";
        case 5: return "Verbose";
        default: return QString("Level %1").arg(level);
    }
}

#ifdef Q_OS_WIN

void WindowsEventLogReader::refreshChannels()
{
    m_availableChannels.clear();
    enumerateChannels();
    emit channelsChanged();
}

void WindowsEventLogReader::enumerateChannels()
{
    EVT_HANDLE hChannels = EvtOpenChannelEnum(nullptr, 0);
    if (!hChannels) {
        setError(QString("Failed to enumerate channels: %1").arg(GetLastError()));
        return;
    }
    
    DWORD bufferSize = 0;
    DWORD bufferUsed = 0;
    LPWSTR buffer = nullptr;
    
    while (true) {
        if (!EvtNextChannelPath(hChannels, bufferSize, buffer, &bufferUsed)) {
            DWORD status = GetLastError();
            if (status == ERROR_NO_MORE_ITEMS) {
                break;
            } else if (status == ERROR_INSUFFICIENT_BUFFER) {
                bufferSize = bufferUsed;
                buffer = new WCHAR[bufferSize];
                continue;
            } else {
                break;
            }
        }
        
        if (buffer) {
            QString channelName = QString::fromWCharArray(buffer);
            m_availableChannels.append(channelName);
        }
    }
    
    if (buffer) {
        delete[] buffer;
    }
    
    EvtClose(hChannels);
    
    // Sort channels, put standard ones first
    QStringList standard = getStandardChannels();
    QStringList sorted;
    
    for (const QString& s : standard) {
        if (m_availableChannels.contains(s)) {
            sorted.append(s);
        }
    }
    
    m_availableChannels.sort();
    for (const QString& ch : m_availableChannels) {
        if (!standard.contains(ch)) {
            sorted.append(ch);
        }
    }
    
    m_availableChannels = sorted;
}

QString WindowsEventLogReader::buildQuery(const QString& baseQuery,
                                           const QDateTime& startTime,
                                           const QDateTime& endTime,
                                           const QList<int>& levels,
                                           const QString& provider)
{
    if (!baseQuery.isEmpty()) {
        return baseQuery;
    }
    
    QStringList conditions;
    
    // Time filter
    if (startTime.isValid()) {
        // Windows Event Log uses FILETIME (100-nanosecond intervals since 1601)
        qint64 startMs = startTime.toMSecsSinceEpoch();
        QString timeStr = startTime.toUTC().toString(Qt::ISODate);
        conditions.append(QString("TimeCreated[@SystemTime>='%1']").arg(timeStr));
    }
    
    if (endTime.isValid()) {
        QString timeStr = endTime.toUTC().toString(Qt::ISODate);
        conditions.append(QString("TimeCreated[@SystemTime<='%1']").arg(timeStr));
    }
    
    // Level filter
    if (!levels.isEmpty()) {
        QStringList levelConditions;
        for (int level : levels) {
            levelConditions.append(QString("Level=%1").arg(level));
        }
        conditions.append(QString("(%1)").arg(levelConditions.join(" or ")));
    }
    
    // Provider filter
    if (!provider.isEmpty()) {
        conditions.append(QString("Provider[@Name='%1']").arg(provider));
    }
    
    if (conditions.isEmpty()) {
        return "*";
    }
    
    return QString("*[System[%1]]").arg(conditions.join(" and "));
}

void WindowsEventLogReader::readEvents(const QString& channel, 
                                        const QString& query,
                                        int maxEvents)
{
    if (m_isReading) {
        setError("Already reading events");
        return;
    }
    
    setReading(true);
    m_cancelRequested = false;
    m_currentChannel = channel;
    m_loadedEvents.clear();
    m_eventCount = 0;
    emit currentChannelChanged();
    emit eventCountChanged();
    
    QString xpathQuery = query.isEmpty() ? "*" : query;
    
    EVT_HANDLE hResults = EvtQuery(nullptr, 
                                    reinterpret_cast<LPCWSTR>(channel.utf16()),
                                    reinterpret_cast<LPCWSTR>(xpathQuery.utf16()),
                                    EvtQueryChannelPath | EvtQueryReverseDirection);
    
    if (!hResults) {
        setError(QString("Failed to query channel '%1': %2").arg(channel).arg(GetLastError()));
        setReading(false);
        return;
    }
    
    const int BATCH_SIZE = 100;
    EVT_HANDLE hEvents[BATCH_SIZE];
    DWORD dwReturned = 0;
    int totalRead = 0;
    QVariantList batch;
    
    while (!m_cancelRequested) {
        if (!EvtNext(hResults, BATCH_SIZE, hEvents, INFINITE, 0, &dwReturned)) {
            DWORD status = GetLastError();
            if (status == ERROR_NO_MORE_ITEMS) {
                break;
            } else {
                setError(QString("Failed to read events: %1").arg(status));
                break;
            }
        }
        
        for (DWORD i = 0; i < dwReturned; i++) {
            QVariantMap event = parseEvent(hEvents[i]);
            m_loadedEvents.append(event);
            batch.append(event);
            EvtClose(hEvents[i]);
            totalRead++;
            
            if (maxEvents > 0 && totalRead >= maxEvents) {
                m_cancelRequested = true;
                break;
            }
        }
        
        // Emit batch progress
        if (batch.size() >= BATCH_SIZE) {
            int progress = maxEvents > 0 ? (totalRead * 100 / maxEvents) : 0;
            emit eventsBatch(batch, progress);
            batch.clear();
        }
        
        m_eventCount = totalRead;
        emit eventCountChanged();
    }
    
    // Emit remaining batch
    if (!batch.isEmpty()) {
        emit eventsBatch(batch, 100);
    }
    
    EvtClose(hResults);
    
    emit eventsLoaded(m_loadedEvents);
    emit readComplete(totalRead);
    setReading(false);
}

void WindowsEventLogReader::readEventsByTime(const QString& channel,
                                              const QDateTime& startTime,
                                              const QDateTime& endTime,
                                              int maxEvents)
{
    QString query = buildQuery(QString(), startTime, endTime, QList<int>(), QString());
    readEvents(channel, query, maxEvents);
}

void WindowsEventLogReader::readEventsByLevel(const QString& channel,
                                               const QList<int>& levels,
                                               int maxEvents)
{
    QString query = buildQuery(QString(), QDateTime(), QDateTime(), levels, QString());
    readEvents(channel, query, maxEvents);
}

void WindowsEventLogReader::readEventsByProvider(const QString& channel,
                                                  const QString& providerName,
                                                  int maxEvents)
{
    QString query = buildQuery(QString(), QDateTime(), QDateTime(), QList<int>(), providerName);
    readEvents(channel, query, maxEvents);
}

void WindowsEventLogReader::cancelRead()
{
    m_cancelRequested = true;
}

QVariantMap WindowsEventLogReader::parseEvent(EVT_HANDLE hEvent)
{
    QVariantMap event;
    
    // Get system properties
    DWORD bufferSize = 0;
    DWORD propertyCount = 0;
    
    if (!EvtRender(nullptr, hEvent, EvtRenderEventValues, bufferSize, nullptr, &bufferSize, &propertyCount)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return event;
        }
    }
    
    PEVT_VARIANT pRenderedValues = (PEVT_VARIANT)malloc(bufferSize);
    if (!pRenderedValues) {
        return event;
    }
    
    if (EvtRender(nullptr, hEvent, EvtRenderEventValues, bufferSize, pRenderedValues, &bufferSize, &propertyCount)) {
        // EvtSystemProviderName
        if (pRenderedValues[EvtSystemProviderName].Type == EvtVarTypeString) {
            event["provider"] = QString::fromWCharArray(pRenderedValues[EvtSystemProviderName].StringVal);
        }
        
        // EvtSystemEventID
        if (pRenderedValues[EvtSystemEventID].Type == EvtVarTypeUInt16) {
            event["eventId"] = pRenderedValues[EvtSystemEventID].UInt16Val;
        }
        
        // EvtSystemLevel
        if (pRenderedValues[EvtSystemLevel].Type == EvtVarTypeByte) {
            int level = pRenderedValues[EvtSystemLevel].ByteVal;
            event["level"] = level;
            event["levelStr"] = levelToString(level);
        }
        
        // EvtSystemTask
        if (pRenderedValues[EvtSystemTask].Type == EvtVarTypeUInt16) {
            event["task"] = pRenderedValues[EvtSystemTask].UInt16Val;
        }
        
        // EvtSystemKeywords
        if (pRenderedValues[EvtSystemKeywords].Type == EvtVarTypeUInt64) {
            event["keywords"] = (qint64)pRenderedValues[EvtSystemKeywords].UInt64Val;
        }
        
        // EvtSystemTimeCreated
        if (pRenderedValues[EvtSystemTimeCreated].Type == EvtVarTypeFileTime) {
            ULONGLONG ull = pRenderedValues[EvtSystemTimeCreated].FileTimeVal;
            FILETIME ft;
            ft.dwLowDateTime = (DWORD)ull;
            ft.dwHighDateTime = (DWORD)(ull >> 32);
            SYSTEMTIME st;
            FileTimeToSystemTime(&ft, &st);
            
            QDateTime dt(QDate(st.wYear, st.wMonth, st.wDay),
                         QTime(st.wHour, st.wMinute, st.wSecond, st.wMilliseconds),
                         QTimeZone::utc());
            event["timestamp"] = dt.toLocalTime();
            event["timestampStr"] = dt.toLocalTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        }
        
        // EvtSystemEventRecordId
        if (pRenderedValues[EvtSystemEventRecordId].Type == EvtVarTypeUInt64) {
            event["recordId"] = (qint64)pRenderedValues[EvtSystemEventRecordId].UInt64Val;
        }
        
        // EvtSystemComputer
        if (pRenderedValues[EvtSystemComputer].Type == EvtVarTypeString) {
            event["computer"] = QString::fromWCharArray(pRenderedValues[EvtSystemComputer].StringVal);
        }
        
        // EvtSystemProcessID
        if (pRenderedValues[EvtSystemProcessID].Type == EvtVarTypeUInt32) {
            event["processId"] = pRenderedValues[EvtSystemProcessID].UInt32Val;
        }
        
        // EvtSystemThreadID
        if (pRenderedValues[EvtSystemThreadID].Type == EvtVarTypeUInt32) {
            event["threadId"] = pRenderedValues[EvtSystemThreadID].UInt32Val;
        }
        
        // EvtSystemChannel
        if (pRenderedValues[EvtSystemChannel].Type == EvtVarTypeString) {
            event["channel"] = QString::fromWCharArray(pRenderedValues[EvtSystemChannel].StringVal);
        }
    }
    
    free(pRenderedValues);
    
    // Get event message
    QString providerName = event["provider"].toString();
    if (!providerName.isEmpty()) {
        EVT_HANDLE hPublisher = EvtOpenPublisherMetadata(nullptr, 
                                                          reinterpret_cast<LPCWSTR>(providerName.utf16()),
                                                          nullptr, 0, 0);
        if (hPublisher) {
            event["message"] = getEventMessage(hEvent, hPublisher);
            EvtClose(hPublisher);
        }
    }
    
    // Get XML if message is empty
    if (event["message"].toString().isEmpty()) {
        event["message"] = getEventXml(hEvent);
    }
    
    return event;
}

QString WindowsEventLogReader::getEventMessage(EVT_HANDLE hEvent, EVT_HANDLE hPublisher)
{
    DWORD bufferSize = 0;
    DWORD bufferUsed = 0;
    
    if (!EvtFormatMessage(hPublisher, hEvent, 0, 0, nullptr, EvtFormatMessageEvent,
                          bufferSize, nullptr, &bufferUsed)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return QString();
        }
    }
    
    bufferSize = bufferUsed;
    LPWSTR buffer = new WCHAR[bufferSize];
    
    QString message;
    if (EvtFormatMessage(hPublisher, hEvent, 0, 0, nullptr, EvtFormatMessageEvent,
                         bufferSize, buffer, &bufferUsed)) {
        message = QString::fromWCharArray(buffer);
    }
    
    delete[] buffer;
    return message;
}

QString WindowsEventLogReader::getEventXml(EVT_HANDLE hEvent)
{
    DWORD bufferSize = 0;
    DWORD bufferUsed = 0;
    DWORD propertyCount = 0;
    
    if (!EvtRender(nullptr, hEvent, EvtRenderEventXml, bufferSize, nullptr, &bufferUsed, &propertyCount)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return QString();
        }
    }
    
    bufferSize = bufferUsed;
    LPWSTR buffer = new WCHAR[bufferSize / sizeof(WCHAR)];
    
    QString xml;
    if (EvtRender(nullptr, hEvent, EvtRenderEventXml, bufferSize, buffer, &bufferUsed, &propertyCount)) {
        xml = QString::fromWCharArray(buffer);
    }
    
    delete[] buffer;
    return xml;
}

QVariantMap WindowsEventLogReader::getEventDetails(const QString& channel, qint64 recordId)
{
    QString query = QString("*[System[EventRecordID=%1]]").arg(recordId);
    
    EVT_HANDLE hResults = EvtQuery(nullptr,
                                    reinterpret_cast<LPCWSTR>(channel.utf16()),
                                    reinterpret_cast<LPCWSTR>(query.utf16()),
                                    EvtQueryChannelPath);
    
    if (!hResults) {
        setError(QString("Failed to query event: %1").arg(GetLastError()));
        return QVariantMap();
    }
    
    EVT_HANDLE hEvent;
    DWORD dwReturned = 0;
    QVariantMap event;
    
    if (EvtNext(hResults, 1, &hEvent, INFINITE, 0, &dwReturned) && dwReturned > 0) {
        event = parseEvent(hEvent);
        event["xml"] = getEventXml(hEvent);
        EvtClose(hEvent);
    }
    
    EvtClose(hResults);
    return event;
}

QStringList WindowsEventLogReader::getProviders(const QString& channel)
{
    QStringList providers;
    
    EVT_HANDLE hPublishers = EvtOpenPublisherEnum(nullptr, 0);
    if (!hPublishers) {
        return providers;
    }
    
    DWORD bufferSize = 0;
    DWORD bufferUsed = 0;
    LPWSTR buffer = nullptr;
    
    while (true) {
        if (!EvtNextPublisherId(hPublishers, bufferSize, buffer, &bufferUsed)) {
            DWORD status = GetLastError();
            if (status == ERROR_NO_MORE_ITEMS) {
                break;
            } else if (status == ERROR_INSUFFICIENT_BUFFER) {
                bufferSize = bufferUsed;
                buffer = new WCHAR[bufferSize];
                continue;
            } else {
                break;
            }
        }
        
        if (buffer) {
            providers.append(QString::fromWCharArray(buffer));
        }
    }
    
    if (buffer) {
        delete[] buffer;
    }
    
    EvtClose(hPublishers);
    providers.sort();
    return providers;
}

DWORD WINAPI WindowsEventLogReader::subscriptionCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action,
                                                          PVOID pContext,
                                                          EVT_HANDLE hEvent)
{
    WindowsEventLogReader* reader = static_cast<WindowsEventLogReader*>(pContext);
    
    if (action == EvtSubscribeActionDeliver) {
        QVariantMap event = reader->parseEvent(hEvent);
        
        // Use Qt's signal/slot mechanism for thread safety
        QMetaObject::invokeMethod(reader, [reader, event]() {
            reader->m_loadedEvents.append(event);
            reader->m_eventCount++;
            emit reader->eventCountChanged();
            emit reader->newEvent(event);
        }, Qt::QueuedConnection);
    }
    
    return ERROR_SUCCESS;
}

void WindowsEventLogReader::subscribeToEvents(const QString& channel, const QString& query)
{
    if (m_isSubscribed) {
        unsubscribe();
    }
    
    QString xpathQuery = query.isEmpty() ? "*" : query;
    
    m_hSubscription = EvtSubscribe(nullptr,
                                    nullptr,
                                    reinterpret_cast<LPCWSTR>(channel.utf16()),
                                    reinterpret_cast<LPCWSTR>(xpathQuery.utf16()),
                                    nullptr,
                                    this,
                                    subscriptionCallback,
                                    EvtSubscribeToFutureEvents);
    
    if (!m_hSubscription) {
        setError(QString("Failed to subscribe to channel '%1': %2").arg(channel).arg(GetLastError()));
        return;
    }
    
    m_isSubscribed = true;
    m_currentChannel = channel;
    emit subscriptionChanged();
    emit currentChannelChanged();
}

void WindowsEventLogReader::unsubscribe()
{
    if (m_hSubscription) {
        EvtClose(m_hSubscription);
        m_hSubscription = nullptr;
    }
    
    if (m_isSubscribed) {
        m_isSubscribed = false;
        emit subscriptionChanged();
    }
}

bool WindowsEventLogReader::exportToFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(QString("Failed to open file for writing: %1").arg(filePath));
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    for (const QVariant& v : m_loadedEvents) {
        QVariantMap event = v.toMap();
        
        // Format: timestamp [level] provider (eventId): message
        QString line = QString("%1 [%2] %3 (%4): %5")
            .arg(event["timestampStr"].toString())
            .arg(event["levelStr"].toString())
            .arg(event["provider"].toString())
            .arg(event["eventId"].toInt())
            .arg(event["message"].toString().replace("\r\n", " ").replace("\n", " "));
        
        out << line << "\n";
    }
    
    file.close();
    return true;
}

#else // Non-Windows platforms

void WindowsEventLogReader::refreshChannels()
{
    setError("Windows Event Log is not available on this platform");
}

void WindowsEventLogReader::readEvents(const QString& channel, const QString& query, int maxEvents)
{
    Q_UNUSED(channel)
    Q_UNUSED(query)
    Q_UNUSED(maxEvents)
    setError("Windows Event Log is not available on this platform");
}

void WindowsEventLogReader::readEventsByTime(const QString& channel, const QDateTime& startTime, const QDateTime& endTime, int maxEvents)
{
    Q_UNUSED(channel)
    Q_UNUSED(startTime)
    Q_UNUSED(endTime)
    Q_UNUSED(maxEvents)
    setError("Windows Event Log is not available on this platform");
}

void WindowsEventLogReader::readEventsByLevel(const QString& channel, const QList<int>& levels, int maxEvents)
{
    Q_UNUSED(channel)
    Q_UNUSED(levels)
    Q_UNUSED(maxEvents)
    setError("Windows Event Log is not available on this platform");
}

void WindowsEventLogReader::readEventsByProvider(const QString& channel, const QString& providerName, int maxEvents)
{
    Q_UNUSED(channel)
    Q_UNUSED(providerName)
    Q_UNUSED(maxEvents)
    setError("Windows Event Log is not available on this platform");
}

void WindowsEventLogReader::cancelRead()
{
    // No-op on non-Windows
}

void WindowsEventLogReader::subscribeToEvents(const QString& channel, const QString& query)
{
    Q_UNUSED(channel)
    Q_UNUSED(query)
    setError("Windows Event Log is not available on this platform");
}

void WindowsEventLogReader::unsubscribe()
{
    // No-op on non-Windows
}

QVariantMap WindowsEventLogReader::getEventDetails(const QString& channel, qint64 recordId)
{
    Q_UNUSED(channel)
    Q_UNUSED(recordId)
    return QVariantMap();
}

QStringList WindowsEventLogReader::getProviders(const QString& channel)
{
    Q_UNUSED(channel)
    return QStringList();
}

bool WindowsEventLogReader::exportToFile(const QString& filePath)
{
    Q_UNUSED(filePath)
    return false;
}

#endif // Q_OS_WIN

