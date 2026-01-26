/**
 * @file SystemTraceReader.h
 * @brief Cross-platform System Trace Reader for BigFileViewer
 * 
 * Provides unified interface for reading system traces:
 * - Windows: ETW (Event Tracing for Windows) .etl files
 * - Linux: perf (perf.data files)
 * - macOS: DTrace output files
 */

#ifndef SYSTEMTRACEREADER_H
#define SYSTEMTRACEREADER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QDateTime>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <evntrace.h>
#include <tdh.h>
#endif

/**
 * @brief Trace event data structure
 */
struct SystemTraceEvent {
    qint64 timestamp;       // Nanoseconds since epoch
    QString provider;       // Provider/source name
    QString eventName;      // Event type name
    int processId;
    int threadId;
    QString message;        // Formatted message
    QVariantMap properties; // Event-specific properties
};

/**
 * @class SystemTraceReader
 * @brief Reads system traces from various platforms
 * 
 * Platform-specific implementations:
 * - Windows: Uses TDH (Trace Data Helper) API for ETW
 * - Linux: Parses perf.data using perf script output
 * - macOS: Parses DTrace output files
 */
class SystemTraceReader : public QObject
{
    Q_OBJECT
    
    // QML Properties
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(QString platformType READ platformType CONSTANT)
    Q_PROPERTY(bool isReading READ isReading NOTIFY readingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)
    Q_PROPERTY(int eventCount READ eventCount NOTIFY eventCountChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)

public:
    /**
     * @brief Singleton instance accessor
     */
    static SystemTraceReader& instance();
    
    /**
     * @brief Check if trace reading is available on this platform
     */
    bool isAvailable() const;
    
    /**
     * @brief Get platform type string
     * @return "etw", "perf", "dtrace", or "unsupported"
     */
    QString platformType() const;
    
    bool isReading() const { return m_isReading; }
    QString lastError() const { return m_lastError; }
    int eventCount() const { return m_eventCount; }
    QString currentFile() const { return m_currentFile; }
    
    // ===== Q_INVOKABLE Methods for QML =====
    
    /**
     * @brief Get supported file extensions for current platform
     */
    Q_INVOKABLE QStringList supportedExtensions() const;
    
    /**
     * @brief Open and read a trace file
     * @param filePath Path to trace file (.etl, perf.data, .dtrace)
     * @param maxEvents Maximum events to read (0 = no limit)
     */
    Q_INVOKABLE void openTraceFile(const QString& filePath, int maxEvents = 0);
    
    /**
     * @brief Cancel current read operation
     */
    Q_INVOKABLE void cancelRead();
    
    /**
     * @brief Export loaded events to text file for BigFileModel
     * @param outputPath Output file path
     * @return true if successful
     */
    Q_INVOKABLE bool exportToFile(const QString& outputPath);
    
    /**
     * @brief Get event details by index
     */
    Q_INVOKABLE QVariantMap getEventDetails(int index);
    
    /**
     * @brief Get loaded events
     */
    Q_INVOKABLE QVariantList getEvents() const;
    
    /**
     * @brief Get list of providers in loaded trace
     */
    Q_INVOKABLE QStringList getProviders() const;
    
    /**
     * @brief Filter events by provider
     * @param provider Provider name (empty = all)
     */
    Q_INVOKABLE void filterByProvider(const QString& provider);
    
    /**
     * @brief Filter events by time range
     */
    Q_INVOKABLE void filterByTimeRange(const QDateTime& start, const QDateTime& end);
    
    // ===== Windows ETW specific =====
#ifdef Q_OS_WIN
    /**
     * @brief Start real-time ETW session
     * @param sessionName Session name
     * @param providers List of provider GUIDs
     */
    Q_INVOKABLE void startEtwSession(const QString& sessionName, 
                                      const QStringList& providers);
    
    /**
     * @brief Stop ETW session
     */
    Q_INVOKABLE void stopEtwSession();
    
    /**
     * @brief Get list of registered ETW providers
     */
    Q_INVOKABLE QVariantList getEtwProviders();
#endif
    
    // ===== Linux perf specific =====
#ifdef Q_OS_LINUX
    /**
     * @brief Start perf recording
     * @param outputPath Output file path
     * @param events Events to record (e.g., "cycles", "cache-misses")
     * @param pid Process ID to trace (0 = system-wide)
     */
    Q_INVOKABLE void startPerfRecord(const QString& outputPath,
                                      const QStringList& events,
                                      int pid = 0);
    
    /**
     * @brief Stop perf recording
     */
    Q_INVOKABLE void stopPerfRecord();
#endif

signals:
    void readingChanged();
    void errorOccurred(const QString& error);
    void eventCountChanged();
    void currentFileChanged();
    
    /**
     * @brief Emitted when events are loaded
     */
    void eventsLoaded(const QVariantList& events);
    
    /**
     * @brief Emitted for progress during reading
     */
    void readProgress(int current, int total);
    
    /**
     * @brief Emitted when a new real-time event arrives
     */
    void newEvent(const QVariantMap& event);
    
    /**
     * @brief Emitted when reading is complete
     */
    void readComplete(int totalEvents);

private:
    explicit SystemTraceReader(QObject* parent = nullptr);
    ~SystemTraceReader();
    
    // Disable copy
    SystemTraceReader(const SystemTraceReader&) = delete;
    SystemTraceReader& operator=(const SystemTraceReader&) = delete;
    
    void setError(const QString& error);
    void setReading(bool reading);
    
    // Platform-specific implementations
#ifdef Q_OS_WIN
    bool readEtlFile(const QString& filePath, int maxEvents);
    static void WINAPI eventRecordCallback(PEVENT_RECORD pEvent);
    void processEtwEvent(PEVENT_RECORD pEvent);
    QString formatEtwEvent(PEVENT_RECORD pEvent, PTRACE_EVENT_INFO pInfo);
    
    TRACEHANDLE m_traceHandle;
    TRACEHANDLE m_sessionHandle;
    static SystemTraceReader* s_instance;
#endif
    
#ifdef Q_OS_LINUX
    bool readPerfData(const QString& filePath, int maxEvents);
    void parsePerfScriptOutput(const QString& output);
    
    QProcess* m_perfProcess;
#endif
    
#ifdef Q_OS_MACOS
    bool readDTraceOutput(const QString& filePath, int maxEvents);
    void parseDTraceOutput(const QString& output);
#endif
    
    // Common data
    bool m_isReading;
    bool m_cancelRequested;
    QString m_lastError;
    QString m_currentFile;
    int m_eventCount;
    int m_maxEvents;
    
    // Loaded events
    QList<SystemTraceEvent> m_events;
    QVariantList m_eventsVariant;
    QStringList m_providers;
    
    // Worker thread
    QThread* m_workerThread;
};

#endif // SYSTEMTRACEREADER_H
