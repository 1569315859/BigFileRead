/**
 * EventLogDialog.qml
 * Windows Event Log Browser for BigFileViewer
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: qsTr("Windows Event Log")
    width: 900
    height: 650
    modal: true
    anchors.centerIn: parent
    
    property var eventReader: _eventLogReader
    property color textColor: _themeManager ? _themeManager.textColor : palette.text
    property color panelColor: _themeManager ? _themeManager.panelBackground : palette.window
    property color bgColor: _themeManager ? _themeManager.backgroundColor : palette.base
    property color accentColor: _themeManager ? _themeManager.accentColor : palette.highlight
    property color borderColor: _themeManager ? _themeManager.borderColor : palette.mid
    
    palette.text: textColor
    palette.windowText: textColor
    palette.window: panelColor
    palette.base: bgColor
    palette.highlight: accentColor
    palette.buttonText: textColor
    
    signal eventsExported(string filePath)
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    Component.onCompleted: {
        if (eventReader.available) {
            eventReader.refreshChannels()
        }
    }
    
    Connections {
        target: eventReader
        
        function onEventsLoaded(events) {
            eventModel.clear()
            for (var i = 0; i < events.length; i++) {
                eventModel.append(events[i])
            }
        }
        
        function onEventsBatch(events, progress) {
            for (var i = 0; i < events.length; i++) {
                eventModel.append(events[i])
            }
            progressBar.value = progress / 100
        }
        
        function onNewEvent(event) {
            eventModel.insert(0, event)
        }
        
        function onReadComplete(total) {
            statusLabel.text = qsTr("Loaded %1 events").arg(total)
        }
        
        function onErrorOccurred(error) {
            errorLabel.text = error
            errorLabel.visible = true
        }
    }
    
    ListModel { id: eventModel }
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        
        // Not available warning
        Rectangle {
            Layout.fillWidth: true
            height: 40
            color: "#fff3cd"
            radius: 4
            visible: !eventReader.available
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                
                Label {
                    text: "⚠️"
                    font.pixelSize: 16
                }
                
                Label {
                    text: qsTr("Windows Event Log is not available on this platform")
                    Layout.fillWidth: true
                }
            }
        }
        
        // Channel selection and filters
        GroupBox {
            title: qsTr("Event Source")
            Layout.fillWidth: true
            visible: eventReader.available
            
            GridLayout {
                columns: 4
                rowSpacing: 8
                columnSpacing: 12
                anchors.fill: parent
                
                Label { text: qsTr("Channel:") }
                ComboBox {
                    id: channelCombo
                    Layout.fillWidth: true
                    model: eventReader.availableChannels
                    editable: true
                    
                    Component.onCompleted: {
                        currentIndex = find("Application")
                        if (currentIndex < 0) currentIndex = 0
                    }
                }
                
                Label { text: qsTr("Level:") }
                ComboBox {
                    id: levelCombo
                    Layout.preferredWidth: 120
                    model: [
                        { text: qsTr("All"), value: [] },
                        { text: qsTr("Critical"), value: [1] },
                        { text: qsTr("Error"), value: [2] },
                        { text: qsTr("Warning"), value: [3] },
                        { text: qsTr("Information"), value: [4] },
                        { text: qsTr("Error & Warning"), value: [2, 3] },
                        { text: qsTr("Error & Above"), value: [1, 2] }
                    ]
                    textRole: "text"
                    valueRole: "value"
                }
                
                Label { text: qsTr("Time Range:") }
                RowLayout {
                    Layout.columnSpan: 3
                    
                    ComboBox {
                        id: timeRangeCombo
                        model: [
                            { text: qsTr("All Time"), hours: 0 },
                            { text: qsTr("Last Hour"), hours: 1 },
                            { text: qsTr("Last 6 Hours"), hours: 6 },
                            { text: qsTr("Last 24 Hours"), hours: 24 },
                            { text: qsTr("Last 7 Days"), hours: 168 },
                            { text: qsTr("Last 30 Days"), hours: 720 },
                            { text: qsTr("Custom..."), hours: -1 }
                        ]
                        textRole: "text"
                        valueRole: "hours"
                        Layout.preferredWidth: 140
                    }
                    
                    TextField {
                        id: customStartTime
                        placeholderText: "yyyy-MM-dd HH:mm"
                        visible: timeRangeCombo.currentValue === -1
                        Layout.preferredWidth: 150
                    }
                    
                    Label {
                        text: "—"
                        visible: timeRangeCombo.currentValue === -1
                    }
                    
                    TextField {
                        id: customEndTime
                        placeholderText: "yyyy-MM-dd HH:mm"
                        visible: timeRangeCombo.currentValue === -1
                        Layout.preferredWidth: 150
                    }
                }
                
                Label { text: qsTr("Max Events:") }
                SpinBox {
                    id: maxEventsSpinner
                    from: 100
                    to: 100000
                    value: 1000
                    stepSize: 100
                    editable: true
                }
                
                Item { Layout.fillWidth: true }
                
                RowLayout {
                    Button {
                        text: qsTr("Load Events")
                        highlighted: true
                        enabled: !eventReader.isReading
                        onClicked: loadEvents()
                    }
                    
                    Button {
                        text: qsTr("Cancel")
                        visible: eventReader.isReading
                        onClicked: eventReader.cancelRead()
                    }
                    
                    Button {
                        text: eventReader.isSubscribed ? qsTr("Stop Monitor") : qsTr("Monitor Live")
                        onClicked: {
                            if (eventReader.isSubscribed) {
                                eventReader.unsubscribe()
                            } else {
                                eventReader.subscribeToEvents(channelCombo.currentText)
                            }
                        }
                    }
                }
            }
        }
        
        // Progress bar
        ProgressBar {
            id: progressBar
            Layout.fillWidth: true
            visible: eventReader.isReading
            from: 0
            to: 1
        }
        
        // Error label
        Label {
            id: errorLabel
            visible: false
            color: "red"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        
        // Events table
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            border.color: "#ddd"
            radius: 4
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0
                
                // Header
                Rectangle {
                    Layout.fillWidth: true
                    height: 32
                    color: "#f5f5f5"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 0
                        
                        Label {
                            text: qsTr("Level")
                            font.bold: true
                            Layout.preferredWidth: 80
                        }
                        
                        Label {
                            text: qsTr("Time")
                            font.bold: true
                            Layout.preferredWidth: 160
                        }
                        
                        Label {
                            text: qsTr("Source")
                            font.bold: true
                            Layout.preferredWidth: 180
                        }
                        
                        Label {
                            text: qsTr("Event ID")
                            font.bold: true
                            Layout.preferredWidth: 80
                        }
                        
                        Label {
                            text: qsTr("Message")
                            font.bold: true
                            Layout.fillWidth: true
                        }
                    }
                }
                
                // Event list
                ListView {
                    id: eventListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: eventModel
                    clip: true
                    
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    
                    delegate: Rectangle {
                        width: eventListView.width
                        height: 36
                        color: {
                            if (ListView.isCurrentItem) return "#e3f2fd"
                            if (index % 2 === 0) return "white"
                            return "#fafafa"
                        }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 0
                            
                            // Level indicator
                            Rectangle {
                                width: 70
                                height: 20
                                radius: 3
                                color: {
                                    switch (model.level) {
                                        case 1: return "#d32f2f" // Critical
                                        case 2: return "#f44336" // Error
                                        case 3: return "#ff9800" // Warning
                                        case 4: return "#2196f3" // Information
                                        case 5: return "#9e9e9e" // Verbose
                                        default: return "#757575"
                                    }
                                }
                                
                                Label {
                                    anchors.centerIn: parent
                                    text: model.levelStr || ""
                                    color: "white"
                                    font.pixelSize: 11
                                }
                            }
                            
                            Item { width: 10 }
                            
                            Label {
                                text: model.timestampStr || ""
                                Layout.preferredWidth: 150
                                elide: Text.ElideRight
                                font.pixelSize: 12
                            }
                            
                            Label {
                                text: model.provider || ""
                                Layout.preferredWidth: 180
                                elide: Text.ElideRight
                                font.pixelSize: 12
                            }
                            
                            Label {
                                text: model.eventId || ""
                                Layout.preferredWidth: 80
                                font.pixelSize: 12
                            }
                            
                            Label {
                                text: (model.message || "").split("\n")[0]
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                font.pixelSize: 12
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                eventListView.currentIndex = index
                            }
                            onDoubleClicked: {
                                eventDetailDialog.eventData = eventModel.get(index)
                                eventDetailDialog.open()
                            }
                        }
                    }
                }
            }
        }
        
        // Status bar
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                id: statusLabel
                text: eventReader.isSubscribed 
                    ? qsTr("Monitoring: %1").arg(eventReader.currentChannel)
                    : qsTr("%1 events").arg(eventModel.count)
                color: "#666"
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Export to File")
                enabled: eventModel.count > 0
                onClicked: {
                    var tempPath = StandardPaths.writableLocation(StandardPaths.TempLocation) + "/event_log_export.txt"
                    if (eventReader.exportToFile(tempPath)) {
                        root.eventsExported(tempPath)
                        root.close()
                    }
                }
            }
            
            Button {
                text: qsTr("Clear")
                onClicked: eventModel.clear()
            }
        }
    }
    
    // Event Detail Dialog
    Dialog {
        id: eventDetailDialog
        title: qsTr("Event Details")
        width: 600
        height: 500
        modal: true
        
        property var eventData: ({})
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            
            GridLayout {
                columns: 2
                rowSpacing: 8
                columnSpacing: 16
                Layout.fillWidth: true
                
                Label { text: qsTr("Level:"); font.bold: true }
                Label { text: eventDetailDialog.eventData.levelStr || "" }
                
                Label { text: qsTr("Time:"); font.bold: true }
                Label { text: eventDetailDialog.eventData.timestampStr || "" }
                
                Label { text: qsTr("Source:"); font.bold: true }
                Label { text: eventDetailDialog.eventData.provider || "" }
                
                Label { text: qsTr("Event ID:"); font.bold: true }
                Label { text: eventDetailDialog.eventData.eventId || "" }
                
                Label { text: qsTr("Computer:"); font.bold: true }
                Label { text: eventDetailDialog.eventData.computer || "" }
                
                Label { text: qsTr("Process ID:"); font.bold: true }
                Label { text: eventDetailDialog.eventData.processId || "" }
                
                Label { text: qsTr("Thread ID:"); font.bold: true }
                Label { text: eventDetailDialog.eventData.threadId || "" }
                
                Label { text: qsTr("Record ID:"); font.bold: true }
                Label { text: eventDetailDialog.eventData.recordId || "" }
            }
            
            Label { 
                text: qsTr("Message:") 
                font.bold: true 
            }
            
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                TextArea {
                    text: eventDetailDialog.eventData.message || ""
                    readOnly: true
                    wrapMode: TextEdit.Wrap
                    font.family: "Consolas"
                    font.pixelSize: 12
                }
            }
        }
        
        standardButtons: Dialog.Close
    }
    
    function loadEvents() {
        errorLabel.visible = false
        eventModel.clear()
        progressBar.value = 0
        
        var channel = channelCombo.currentText
        var levels = levelCombo.currentValue || []
        var maxEvents = maxEventsSpinner.value
        
        // Calculate time range
        var startTime = null
        var endTime = null
        
        if (timeRangeCombo.currentValue > 0) {
            var now = new Date()
            startTime = new Date(now.getTime() - timeRangeCombo.currentValue * 60 * 60 * 1000)
        } else if (timeRangeCombo.currentValue === -1) {
            // Custom time range
            if (customStartTime.text) {
                startTime = Date.parse(customStartTime.text)
            }
            if (customEndTime.text) {
                endTime = Date.parse(customEndTime.text)
            }
        }
        
        // Build query and read
        if (levels.length > 0) {
            eventReader.readEventsByLevel(channel, levels, maxEvents)
        } else if (startTime) {
            eventReader.readEventsByTime(channel, startTime, endTime, maxEvents)
        } else {
            eventReader.readEvents(channel, "", maxEvents)
        }
    }
    
    standardButtons: Dialog.Close
}
