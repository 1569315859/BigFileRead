/**
 * @file SystemTraceReader.cpp
 * @brief Cross-platform System Trace Reader Implementation
 */

#include "SystemTraceReader.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QTimeZone>

#ifdef Q_OS_WIN
#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "advapi32.lib")
SystemTraceReader* SystemTraceReader::s_instance = nullptr;
#endif

SystemTraceReader& SystemTraceReader::instance()
{
    static SystemTraceReader instance;
    return instance;
}

SystemTraceReader::SystemTraceReader(QObject* parent)
    : QObject(parent)
    , m_isReading(false)
    , m_cancelRequested(false)
    , m_eventCount(0)
    , m_maxEvents(0)
    , m_workerThread(nullptr)
#ifdef Q_OS_WIN
    , m_traceHandle(INVALID_PROCESSTRACE_HANDLE)
    , m_sessionHandle(0)
#endif
#ifdef Q_OS_LINUX
    , m_perfProcess(nullptr)
#endif
{
#ifdef Q_OS_WIN
    s_instance = this;
#endif
}

SystemTraceReader::~SystemTraceReader()
{
    cancelRead();
    
#ifdef Q_OS_WIN
    if (m_sessionHandle) {
        stopEtwSession();
    }
#endif
    
#ifdef Q_OS_LINUX
    if (m_perfProcess) {
        m_perfProcess->kill();
        delete m_perfProcess;
    }
#endif
    
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
    }
}

bool SystemTraceReader::isAvailable() const
{
#ifdef Q_OS_WIN
    return true;
#elif defined(Q_OS_LINUX)
    // Check if perf is available
    QProcess proc;
    proc.start("perf", QStringList() << "--version");
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
#elif defined(Q_OS_MACOS)
    // Check if dtrace is available (requires root)
    return QFile::exists("/usr/sbin/dtrace");
#else
    return false;
#endif
}

QString SystemTraceReader::platformType() const
{
#ifdef Q_OS_WIN
    return "etw";
#elif defined(Q_OS_LINUX)
    return "perf";
#elif defined(Q_OS_MACOS)
    return "dtrace";
#else
    return "unsupported";
#endif
}

QStringList SystemTraceReader::supportedExtensions() const
{
#ifdef Q_OS_WIN
    return QStringList() << "*.etl" << "*.ETL";
#elif defined(Q_OS_LINUX)
    return QStringList() << "perf.data" << "*.perf";
#elif defined(Q_OS_MACOS)
    return QStringList() << "*.dtrace" << "*.d";
#else
    return QStringList();
#endif
}

void SystemTraceReader::setError(const QString& error)
{
    m_lastError = error;
    emit errorOccurred(error);
    qWarning() << "SystemTraceReader error:" << error;
}

void SystemTraceReader::setReading(bool reading)
{
    if (m_isReading != reading) {
        m_isReading = reading;
        emit readingChanged();
    }
}

void SystemTraceReader::openTraceFile(const QString& filePath, int maxEvents)
{
    if (m_isReading) {
        setError("Already reading a trace file");
        return;
    }
    
    if (!QFile::exists(filePath)) {
        setError(QString("File not found: %1").arg(filePath));
        return;
    }
    
    setReading(true);
    m_cancelRequested = false;
    m_currentFile = filePath;
    m_maxEvents = maxEvents;
    m_events.clear();
    m_eventsVariant.clear();
    m_providers.clear();
    m_eventCount = 0;
    emit currentFileChanged();
    emit eventCountChanged();
    
    bool success = false;
    
#ifdef Q_OS_WIN
    success = readEtlFile(filePath, maxEvents);
#elif defined(Q_OS_LINUX)
    success = readPerfData(filePath, maxEvents);
#elif defined(Q_OS_MACOS)
    success = readDTraceOutput(filePath, maxEvents);
#else
    setError("Trace reading not supported on this platform");
#endif
    
    if (success) {
        // Convert events to QVariantList
        for (const auto& event : m_events) {
            QVariantMap map;
            map["timestamp"] = event.timestamp;
            map["timestampStr"] = QDateTime::fromMSecsSinceEpoch(event.timestamp / 1000000)
                                    .toString("yyyy-MM-dd hh:mm:ss.zzz");
            map["provider"] = event.provider;
            map["eventName"] = event.eventName;
            map["processId"] = event.processId;
            map["threadId"] = event.threadId;
            map["message"] = event.message;
            map["properties"] = event.properties;
            m_eventsVariant.append(map);
            
            if (!m_providers.contains(event.provider)) {
                m_providers.append(event.provider);
            }
        }
        m_providers.sort();
        
        emit eventsLoaded(m_eventsVariant);
        emit readComplete(m_eventCount);
    }
    
    setReading(false);
}

void SystemTraceReader::cancelRead()
{
    m_cancelRequested = true;
    
#ifdef Q_OS_WIN
    if (m_traceHandle != INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(m_traceHandle);
        m_traceHandle = INVALID_PROCESSTRACE_HANDLE;
    }
#endif
}

bool SystemTraceReader::exportToFile(const QString& outputPath)
{
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(QString("Failed to open file for writing: %1").arg(outputPath));
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    for (const auto& event : m_events) {
        // Format: timestamp [provider] eventName (pid:tid): message
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(event.timestamp / 1000000, QTimeZone::utc());
        QString line = QString("%1 [%2] %3 (%4:%5): %6")
            .arg(dt.toLocalTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(event.provider)
            .arg(event.eventName)
            .arg(event.processId)
            .arg(event.threadId)
            .arg(event.message);
        
        out << line << "\n";
    }
    
    file.close();
    return true;
}

QVariantMap SystemTraceReader::getEventDetails(int index)
{
    if (index >= 0 && index < m_eventsVariant.size()) {
        return m_eventsVariant[index].toMap();
    }
    return QVariantMap();
}

QVariantList SystemTraceReader::getEvents() const
{
    return m_eventsVariant;
}

QStringList SystemTraceReader::getProviders() const
{
    return m_providers;
}

void SystemTraceReader::filterByProvider(const QString& provider)
{
    if (provider.isEmpty()) {
        // Restore all events
        m_eventsVariant.clear();
        for (const auto& event : m_events) {
            QVariantMap map;
            map["timestamp"] = event.timestamp;
            map["provider"] = event.provider;
            map["eventName"] = event.eventName;
            map["message"] = event.message;
            m_eventsVariant.append(map);
        }
    } else {
        m_eventsVariant.clear();
        for (const auto& event : m_events) {
            if (event.provider == provider) {
                QVariantMap map;
                map["timestamp"] = event.timestamp;
                map["provider"] = event.provider;
                map["eventName"] = event.eventName;
                map["message"] = event.message;
                m_eventsVariant.append(map);
            }
        }
    }
    
    emit eventsLoaded(m_eventsVariant);
}

void SystemTraceReader::filterByTimeRange(const QDateTime& start, const QDateTime& end)
{
    qint64 startNs = start.toMSecsSinceEpoch() * 1000000;
    qint64 endNs = end.toMSecsSinceEpoch() * 1000000;
    
    m_eventsVariant.clear();
    for (const auto& event : m_events) {
        if (event.timestamp >= startNs && event.timestamp <= endNs) {
            QVariantMap map;
            map["timestamp"] = event.timestamp;
            map["provider"] = event.provider;
            map["eventName"] = event.eventName;
            map["message"] = event.message;
            m_eventsVariant.append(map);
        }
    }
    
    emit eventsLoaded(m_eventsVariant);
}

// ==================== Windows ETW Implementation ====================
#ifdef Q_OS_WIN

bool SystemTraceReader::readEtlFile(const QString& filePath, int maxEvents)
{
    EVENT_TRACE_LOGFILEW logFile = {0};
    logFile.LogFileName = (LPWSTR)filePath.utf16();
    logFile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.EventRecordCallback = eventRecordCallback;
    logFile.Context = this;
    
    m_traceHandle = OpenTraceW(&logFile);
    if (m_traceHandle == INVALID_PROCESSTRACE_HANDLE) {
        setError(QString("Failed to open ETL file: %1").arg(GetLastError()));
        return false;
    }
    
    // Process trace
    ULONG status = ProcessTrace(&m_traceHandle, 1, nullptr, nullptr);
    
    CloseTrace(m_traceHandle);
    m_traceHandle = INVALID_PROCESSTRACE_HANDLE;
    
    if (status != ERROR_SUCCESS && status != ERROR_CANCELLED) {
        setError(QString("Failed to process trace: %1").arg(status));
        return false;
    }
    
    return true;
}

void WINAPI SystemTraceReader::eventRecordCallback(PEVENT_RECORD pEvent)
{
    if (s_instance) {
        s_instance->processEtwEvent(pEvent);
    }
}

void SystemTraceReader::processEtwEvent(PEVENT_RECORD pEvent)
{
    if (m_cancelRequested) {
        return;
    }
    
    if (m_maxEvents > 0 && m_eventCount >= m_maxEvents) {
        m_cancelRequested = true;
        return;
    }
    
    SystemTraceEvent traceEvt;
    
    // Get timestamp (100-nanosecond intervals since 1601)
    LARGE_INTEGER timestamp;
    timestamp.QuadPart = pEvent->EventHeader.TimeStamp.QuadPart;
    // Convert to Unix nanoseconds
    traceEvt.timestamp = (timestamp.QuadPart - 116444736000000000LL) * 100;
    
    traceEvt.processId = pEvent->EventHeader.ProcessId;
    traceEvt.threadId = pEvent->EventHeader.ThreadId;
    
    // Get event information
    DWORD bufferSize = 0;
    PTRACE_EVENT_INFO pInfo = nullptr;
    
    ULONG status = TdhGetEventInformation(pEvent, 0, nullptr, pInfo, &bufferSize);
    if (status == ERROR_INSUFFICIENT_BUFFER) {
        pInfo = (PTRACE_EVENT_INFO)malloc(bufferSize);
        if (pInfo) {
            status = TdhGetEventInformation(pEvent, 0, nullptr, pInfo, &bufferSize);
        }
    }
    
    if (status == ERROR_SUCCESS && pInfo) {
        // Provider name
        if (pInfo->ProviderNameOffset) {
            traceEvt.provider = QString::fromWCharArray(
                (LPWSTR)((PBYTE)pInfo + pInfo->ProviderNameOffset));
        }
        
        // Event name (task + opcode)
        if (pInfo->TaskNameOffset) {
            traceEvt.eventName = QString::fromWCharArray(
                (LPWSTR)((PBYTE)pInfo + pInfo->TaskNameOffset));
        }
        if (pInfo->OpcodeNameOffset) {
            QString opcode = QString::fromWCharArray(
                (LPWSTR)((PBYTE)pInfo + pInfo->OpcodeNameOffset));
            if (!opcode.isEmpty()) {
                traceEvt.eventName += "/" + opcode;
            }
        }
        
        // Format message
        traceEvt.message = formatEtwEvent(pEvent, pInfo);
        
        free(pInfo);
    } else {
        // Fallback: use GUID as provider
        WCHAR guidStr[40];
        StringFromGUID2(pEvent->EventHeader.ProviderId, guidStr, 40);
        traceEvt.provider = QString::fromWCharArray(guidStr);
        traceEvt.eventName = QString("Event %1").arg(pEvent->EventHeader.EventDescriptor.Id);
        traceEvt.message = QString("(Raw event data, %1 bytes)").arg(pEvent->UserDataLength);
    }
    
    m_events.append(traceEvt);
    m_eventCount++;
    emit eventCountChanged();
    
    if (m_eventCount % 1000 == 0) {
        emit readProgress(m_eventCount, 0);
    }
}

QString SystemTraceReader::formatEtwEvent(PEVENT_RECORD pEvent, PTRACE_EVENT_INFO pInfo)
{
    QStringList parts;
    
    // Parse event properties
    PBYTE userData = (PBYTE)pEvent->UserData;
    PBYTE endOfData = (PBYTE)pEvent->UserData + pEvent->UserDataLength;
    
    for (ULONG i = 0; i < pInfo->TopLevelPropertyCount && userData < endOfData; i++) {
        EVENT_PROPERTY_INFO& propInfo = pInfo->EventPropertyInfoArray[i];
        
        QString propName;
        if (propInfo.NameOffset) {
            propName = QString::fromWCharArray((LPWSTR)((PBYTE)pInfo + propInfo.NameOffset));
        }
        
        // Get property value (simplified - handle common types)
        PROPERTY_DATA_DESCRIPTOR desc = {0};
        desc.PropertyName = (ULONGLONG)((PBYTE)pInfo + propInfo.NameOffset);
        desc.ArrayIndex = 0;
        
        DWORD propSize = 0;
        TdhGetPropertySize(pEvent, 0, nullptr, 1, &desc, &propSize);
        
        if (propSize > 0 && propSize < 4096) {
            QByteArray propData(propSize, 0);
            if (TdhGetProperty(pEvent, 0, nullptr, 1, &desc, propSize, 
                              (PBYTE)propData.data()) == ERROR_SUCCESS) {
                // Try to format as string
                QString value;
                switch (propInfo.nonStructType.InType) {
                    case TDH_INTYPE_UNICODESTRING:
                        value = QString::fromWCharArray((LPCWSTR)propData.constData());
                        break;
                    case TDH_INTYPE_ANSISTRING:
                        value = QString::fromLocal8Bit(propData.constData());
                        break;
                    case TDH_INTYPE_INT32:
                    case TDH_INTYPE_UINT32:
                        value = QString::number(*(PULONG)propData.constData());
                        break;
                    case TDH_INTYPE_INT64:
                    case TDH_INTYPE_UINT64:
                        value = QString::number(*(PULONGLONG)propData.constData());
                        break;
                    default:
                        value = propData.toHex();
                        break;
                }
                
                if (!propName.isEmpty()) {
                    parts.append(QString("%1=%2").arg(propName, value));
                } else {
                    parts.append(value);
                }
            }
        }
    }
    
    return parts.join(", ");
}

void SystemTraceReader::startEtwSession(const QString& sessionName, 
                                         const QStringList& providers)
{
    // Real-time ETW session implementation
    // This is a simplified version - full implementation would need more work
    Q_UNUSED(sessionName)
    Q_UNUSED(providers)
    setError("Real-time ETW session not yet implemented");
}

void SystemTraceReader::stopEtwSession()
{
    if (m_sessionHandle) {
        // Stop session
        m_sessionHandle = 0;
    }
}

QVariantList SystemTraceReader::getEtwProviders()
{
    QVariantList providers;
    
    // Enumerate registered providers using TdhEnumerateProviders
    DWORD bufferSize = 0;
    TdhEnumerateProviders(nullptr, &bufferSize);
    
    if (bufferSize > 0) {
        PPROVIDER_ENUMERATION_INFO pInfo = (PPROVIDER_ENUMERATION_INFO)malloc(bufferSize);
        if (pInfo && TdhEnumerateProviders(pInfo, &bufferSize) == ERROR_SUCCESS) {
            for (DWORD i = 0; i < pInfo->NumberOfProviders; i++) {
                PTRACE_PROVIDER_INFO provInfo = &pInfo->TraceProviderInfoArray[i];
                
                QVariantMap provider;
                WCHAR guidStr[40];
                StringFromGUID2(provInfo->ProviderGuid, guidStr, 40);
                provider["guid"] = QString::fromWCharArray(guidStr);
                
                if (provInfo->ProviderNameOffset) {
                    provider["name"] = QString::fromWCharArray(
                        (LPWSTR)((PBYTE)pInfo + provInfo->ProviderNameOffset));
                }
                
                providers.append(provider);
            }
            free(pInfo);
        }
    }
    
    return providers;
}

#endif // Q_OS_WIN

// ==================== Linux perf Implementation ====================
#ifdef Q_OS_LINUX

bool SystemTraceReader::readPerfData(const QString& filePath, int maxEvents)
{
    Q_UNUSED(maxEvents)
    
    // Use perf script to convert perf.data to readable format
    QProcess process;
    process.start("perf", QStringList() << "script" << "-i" << filePath);
    
    if (!process.waitForFinished(60000)) {
        setError("Timeout reading perf data");
        return false;
    }
    
    if (process.exitCode() != 0) {
        setError(QString("perf script failed: %1").arg(QString::fromLocal8Bit(process.readAllStandardError())));
        return false;
    }
    
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    parsePerfScriptOutput(output);
    
    return true;
}

void SystemTraceReader::parsePerfScriptOutput(const QString& output)
{
    // Parse perf script output format:
    // comm pid [tid] timestamp: event: ...
    QRegularExpression re(R"((\S+)\s+(\d+)(?:\s+\[(\d+)\])?\s+([\d.]+):\s+(\S+):\s*(.*))");
    
    for (const QString& line : output.split('\n')) {
        if (m_cancelRequested) break;
        
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            SystemTraceEvent event;
            event.provider = match.captured(1); // comm (process name)
            event.processId = match.captured(2).toInt();
            event.threadId = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
            
            // Parse timestamp (seconds.microseconds)
            double ts = match.captured(4).toDouble();
            event.timestamp = (qint64)(ts * 1e9); // Convert to nanoseconds
            
            event.eventName = match.captured(5);
            event.message = match.captured(6);
            
            m_events.append(event);
            m_eventCount++;
            
            if (m_maxEvents > 0 && m_eventCount >= m_maxEvents) {
                break;
            }
        }
    }
    
    emit eventCountChanged();
}

void SystemTraceReader::startPerfRecord(const QString& outputPath,
                                         const QStringList& events,
                                         int pid)
{
    if (m_perfProcess) {
        setError("Recording already in progress");
        return;
    }
    
    QStringList args;
    args << "record" << "-o" << outputPath;
    
    for (const QString& event : events) {
        args << "-e" << event;
    }
    
    if (pid > 0) {
        args << "-p" << QString::number(pid);
    } else {
        args << "-a"; // System-wide
    }
    
    m_perfProcess = new QProcess(this);
    m_perfProcess->start("perf", args);
    
    if (!m_perfProcess->waitForStarted()) {
        setError("Failed to start perf record");
        delete m_perfProcess;
        m_perfProcess = nullptr;
    }
}

void SystemTraceReader::stopPerfRecord()
{
    if (m_perfProcess) {
        m_perfProcess->terminate();
        m_perfProcess->waitForFinished(5000);
        delete m_perfProcess;
        m_perfProcess = nullptr;
    }
}

#endif // Q_OS_LINUX

// ==================== macOS DTrace Implementation ====================
#ifdef Q_OS_MACOS

bool SystemTraceReader::readDTraceOutput(const QString& filePath, int maxEvents)
{
    Q_UNUSED(maxEvents)
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(QString("Failed to open DTrace output: %1").arg(filePath));
        return false;
    }
    
    QString output = QString::fromLocal8Bit(file.readAll());
    file.close();
    
    parseDTraceOutput(output);
    return true;
}

void SystemTraceReader::parseDTraceOutput(const QString& output)
{
    // DTrace output format varies by script, this is a basic parser
    // Typical format: timestamp probe_name args...
    
    int lineNum = 0;
    for (const QString& line : output.split('\n')) {
        if (m_cancelRequested) break;
        if (line.trimmed().isEmpty()) continue;
        
        SystemTraceEvent event;
        event.timestamp = QDateTime::currentMSecsSinceEpoch() * 1000000 + lineNum;
        event.provider = "dtrace";
        
        // Simple parsing - first word is probe, rest is message
        int spaceIdx = line.indexOf(' ');
        if (spaceIdx > 0) {
            event.eventName = line.left(spaceIdx);
            event.message = line.mid(spaceIdx + 1);
        } else {
            event.eventName = line;
        }
        
        m_events.append(event);
        m_eventCount++;
        lineNum++;
        
        if (m_maxEvents > 0 && m_eventCount >= m_maxEvents) {
            break;
        }
    }
    
    emit eventCountChanged();
}

#endif // Q_OS_MACOS

// ==================== Fallback for unsupported platforms ====================
#if !defined(Q_OS_WIN) && !defined(Q_OS_LINUX) && !defined(Q_OS_MACOS)

// Stub implementations for unsupported platforms

#endif
