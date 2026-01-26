/**
 * @file WindowsEventLogReader.h
 * @brief Windows Event Log Reader for BigFileViewer
 * 
 * Reads Windows Event Logs using the Windows Event Log API (Evt*)
 * Features:
 * - Read System, Application, Security, and custom event logs
 * - Filter by level, provider, time range
 * - Convert events to log entries for display
 * - Real-time event monitoring
 */

#ifndef WINDOWSEVENTLOGREADER_H
#define WINDOWSEVENTLOGREADER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QDateTime>
#include <QTimer>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winevt.h>
#endif

/**
 * @class WindowsEventLogReader
 * @brief Reads and monitors Windows Event Logs
 * 
 * Platform: Windows only (Q_OS_WIN)
 * Uses Windows Event Log API (EvtQuery, EvtNext, EvtSubscribe)
 */
class WindowsEventLogReader : public QObject
{
    Q_OBJECT
    
    // QML Properties
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(bool isReading READ isReading NOTIFY readingChanged)
    Q_PROPERTY(bool isSubscribed READ isSubscribed NOTIFY subscriptionChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)
    Q_PROPERTY(QStringList availableChannels READ availableChannels NOTIFY channelsChanged)
    Q_PROPERTY(QString currentChannel READ currentChannel NOTIFY currentChannelChanged)
    Q_PROPERTY(int eventCount READ eventCount NOTIFY eventCountChanged)

public:
    /**
     * @brief Singleton instance accessor
     */
    static WindowsEventLogReader& instance();
    
    /**
     * @brief Check if Windows Event Log is available
     */
    bool isAvailable() const;
    
    /**
     * @brief Check if currently reading events
     */
    bool isReading() const { return m_isReading; }
    
    /**
     * @brief Check if subscribed to real-time events
     */
    bool isSubscribed() const { return m_isSubscribed; }
    
    /**
     * @brief Get last error message
     */
    QString lastError() const { return m_lastError; }
    
    /**
     * @brief Get list of available event log channels
     */
    QStringList availableChannels() const { return m_availableChannels; }
    
    /**
     * @brief Get current channel being read
     */
    QString currentChannel() const { return m_currentChannel; }
    
    /**
     * @brief Get count of loaded events
     */
    int eventCount() const { return m_eventCount; }
    
    // ===== Q_INVOKABLE Methods for QML =====
    
    /**
     * @brief Refresh the list of available channels
     */
    Q_INVOKABLE void refreshChannels();
    
    /**
     * @brief Read events from a channel
     * @param channel Channel name (e.g., "System", "Application", "Security")
     * @param query Optional XPath query filter
     * @param maxEvents Maximum number of events to read (0 = no limit)
     */
    Q_INVOKABLE void readEvents(const QString& channel, 
                                 const QString& query = QString(),
                                 int maxEvents = 1000);
    
    /**
     * @brief Read events with time filter
     * @param channel Channel name
     * @param startTime Start time filter
     * @param endTime End time filter (empty = now)
     * @param maxEvents Maximum events
     */
    Q_INVOKABLE void readEventsByTime(const QString& channel,
                                       const QDateTime& startTime,
                                       const QDateTime& endTime = QDateTime(),
                                       int maxEvents = 1000);
    
    /**
     * @brief Read events by level
     * @param channel Channel name
     * @param levels List of levels (1=Critical, 2=Error, 3=Warning, 4=Information, 5=Verbose)
     * @param maxEvents Maximum events
     */
    Q_INVOKABLE void readEventsByLevel(const QString& channel,
                                        const QList<int>& levels,
                                        int maxEvents = 1000);
    
    /**
     * @brief Read events by provider
     * @param channel Channel name
     * @param providerName Event provider name
     * @param maxEvents Maximum events
     */
    Q_INVOKABLE void readEventsByProvider(const QString& channel,
                                           const QString& providerName,
                                           int maxEvents = 1000);
    
    /**
     * @brief Cancel current read operation
     */
    Q_INVOKABLE void cancelRead();
    
    /**
     * @brief Subscribe to real-time events
     * @param channel Channel name
     * @param query Optional XPath query filter
     */
    Q_INVOKABLE void subscribeToEvents(const QString& channel,
                                        const QString& query = QString());
    
    /**
     * @brief Unsubscribe from real-time events
     */
    Q_INVOKABLE void unsubscribe();
    
    /**
     * @brief Get event details by record ID
     * @param channel Channel name
     * @param recordId Event record ID
     * @return Event details as QVariantMap
     */
    Q_INVOKABLE QVariantMap getEventDetails(const QString& channel, qint64 recordId);
    
    /**
     * @brief Get list of providers for a channel
     * @param channel Channel name
     * @return List of provider names
     */
    Q_INVOKABLE QStringList getProviders(const QString& channel);
    
    /**
     * @brief Export events to file (for BigFileModel to load)
     * @param filePath Output file path
     * @return true if successful
     */
    Q_INVOKABLE bool exportToFile(const QString& filePath);
    
    /**
     * @brief Convert level number to string
     */
    Q_INVOKABLE static QString levelToString(int level);
    
    /**
     * @brief Get standard channels
     */
    Q_INVOKABLE QStringList getStandardChannels() const;

signals:
    void readingChanged();
    void subscriptionChanged();
    void errorOccurred(const QString& error);
    void channelsChanged();
    void currentChannelChanged();
    void eventCountChanged();
    
    /**
     * @brief Emitted when events are loaded
     * @param events List of event objects
     */
    void eventsLoaded(const QVariantList& events);
    
    /**
     * @brief Emitted for each batch of events during reading
     * @param events Batch of event objects
     * @param progress Progress percentage (0-100)
     */
    void eventsBatch(const QVariantList& events, int progress);
    
    /**
     * @brief Emitted when a new real-time event arrives
     * @param event Event object
     */
    void newEvent(const QVariantMap& event);
    
    /**
     * @brief Emitted when reading is complete
     * @param totalEvents Total number of events read
     */
    void readComplete(int totalEvents);

private:
    explicit WindowsEventLogReader(QObject* parent = nullptr);
    ~WindowsEventLogReader();
    
    // Disable copy
    WindowsEventLogReader(const WindowsEventLogReader&) = delete;
    WindowsEventLogReader& operator=(const WindowsEventLogReader&) = delete;
    
    void setError(const QString& error);
    void setReading(bool reading);
    
#ifdef Q_OS_WIN
    /**
     * @brief Build XPath query from parameters
     */
    QString buildQuery(const QString& baseQuery,
                       const QDateTime& startTime = QDateTime(),
                       const QDateTime& endTime = QDateTime(),
                       const QList<int>& levels = QList<int>(),
                       const QString& provider = QString());
    
    /**
     * @brief Parse event from EVT_HANDLE
     */
    QVariantMap parseEvent(EVT_HANDLE hEvent);
    
    /**
     * @brief Get event property as QString
     */
    QString getEventProperty(EVT_HANDLE hEvent, EVT_SYSTEM_PROPERTY_ID propId);
    
    /**
     * @brief Get event message
     */
    QString getEventMessage(EVT_HANDLE hEvent, EVT_HANDLE hPublisher);
    
    /**
     * @brief Get event XML
     */
    QString getEventXml(EVT_HANDLE hEvent);
    
    /**
     * @brief Enumerate channels
     */
    void enumerateChannels();
    
    /**
     * @brief Subscription callback
     */
    static DWORD WINAPI subscriptionCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action,
                                              PVOID pContext,
                                              EVT_HANDLE hEvent);
    
    // Windows handles
    EVT_HANDLE m_hSubscription;
    EVT_HANDLE m_hQuery;
#endif
    
    // State
    bool m_isReading;
    bool m_isSubscribed;
    bool m_cancelRequested;
    QString m_lastError;
    QString m_currentChannel;
    int m_eventCount;
    
    // Cached data
    QStringList m_availableChannels;
    QVariantList m_loadedEvents;
    
    // Worker thread for reading
    QThread* m_workerThread;
};

#endif // WINDOWSEVENTLOGREADER_H
