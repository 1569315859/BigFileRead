/**
 * TraceViewerDialog.qml
 * System Trace Viewer UI for BigFileViewer
 * Supports ETW (Windows), perf (Linux), DTrace (macOS)
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: traceDialog
    title: qsTr("System Trace Viewer") + " - " + _traceReader.platformType.toUpperCase()
    width: 900
    height: 700
    modal: true
    standardButtons: Dialog.Close
    anchors.centerIn: parent
    
    property var events: []
    property string selectedProvider: ""
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
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    onOpened: {
        // Refresh available status
        platformLabel.text = getPlatformDescription()
    }
    
    function getPlatformDescription() {
        if (!_traceReader.available) {
            return qsTr("Trace reading not available on this platform")
        }
        
        switch (_traceReader.platformType) {
            case "etw":
                return qsTr("Windows ETW (Event Tracing for Windows) - Supports .etl files")
            case "perf":
                return qsTr("Linux perf - Supports perf.data files")
            case "dtrace":
                return qsTr("macOS DTrace - Supports .dtrace output files")
            default:
                return qsTr("Unknown platform")
        }
    }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        
        // Platform info
        Label {
            id: platformLabel
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: _traceReader.available ? _themeManager.textColor : "orange"
        }
        
        // File selection
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            TextField {
                id: filePathField
                Layout.fillWidth: true
                placeholderText: qsTr("Select trace file...")
                readOnly: true
            }
            
            Button {
                text: qsTr("Browse...")
                enabled: _traceReader.available && !_traceReader.isReading
                onClicked: fileDialog.open()
            }
            
            Button {
                text: qsTr("Open")
                enabled: filePathField.text.length > 0 && !_traceReader.isReading
                onClicked: {
                    _traceReader.openTraceFile(filePathField.text, maxEventsSpinner.value)
                }
            }
        }
        
        // Options row
        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            
            Label { text: qsTr("Max Events:") }
            SpinBox {
                id: maxEventsSpinner
                from: 0
                to: 1000000
                value: 10000
                stepSize: 1000
                editable: true
                
                textFromValue: function(value) {
                    return value === 0 ? qsTr("Unlimited") : value.toString()
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Label {
                text: qsTr("Events: %1").arg(_traceReader.eventCount)
                visible: _traceReader.eventCount > 0
            }
            
            BusyIndicator {
                running: _traceReader.isReading
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
            }
            
            Button {
                text: qsTr("Cancel")
                visible: _traceReader.isReading
                onClicked: _traceReader.cancelRead()
            }
        }
        
        // Filter row
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: traceDialog.events.length > 0
            
            Label { text: qsTr("Filter by Provider:") }
            
            ComboBox {
                id: providerFilter
                Layout.preferredWidth: 250
                model: ["All Providers"].concat(_traceReader.getProviders())
                onCurrentTextChanged: {
                    if (currentIndex === 0) {
                        _traceReader.filterByProvider("")
                    } else {
                        _traceReader.filterByProvider(currentText)
                    }
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Export to File")
                enabled: traceDialog.events.length > 0
                onClicked: exportDialog.open()
            }
            
            Button {
                text: qsTr("Open in Viewer")
                enabled: traceDialog.events.length > 0
                onClicked: {
                    // Export to temp file and open
                    var tempPath = _appController.getTempDir() + "/trace_export.log"
                    if (_traceReader.exportToFile(tempPath)) {
                        _appController.openFile(tempPath)
                        traceDialog.close()
                    }
                }
            }
        }
        
        // Events table
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: _themeManager.isDark ? "#1e1e1e" : "#ffffff"
            border.color: _themeManager.isDark ? "#444" : "#ccc"
            border.width: 1
            radius: 4
            
            ListView {
                id: eventsList
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: traceDialog.events
                
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                
                header: Rectangle {
                    width: eventsList.width
                    height: 30
                    color: _themeManager.isDark ? "#2d2d2d" : "#f0f0f0"
                    z: 2
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8
                        
                        Label {
                            text: qsTr("Timestamp")
                            Layout.preferredWidth: 160
                            font.bold: true
                        }
                        Label {
                            text: qsTr("Provider")
                            Layout.preferredWidth: 150
                            font.bold: true
                        }
                        Label {
                            text: qsTr("Event")
                            Layout.preferredWidth: 120
                            font.bold: true
                        }
                        Label {
                            text: qsTr("PID:TID")
                            Layout.preferredWidth: 80
                            font.bold: true
                        }
                        Label {
                            text: qsTr("Message")
                            Layout.fillWidth: true
                            font.bold: true
                        }
                    }
                }
                
                delegate: Rectangle {
                    width: eventsList.width
                    height: 28
                    color: index % 2 === 0 
                        ? (_themeManager.isDark ? "#252525" : "#ffffff")
                        : (_themeManager.isDark ? "#2a2a2a" : "#f8f8f8")
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8
                        
                        Label {
                            text: modelData.timestampStr || ""
                            Layout.preferredWidth: 160
                            elide: Text.ElideRight
                            font.family: "Consolas"
                            font.pixelSize: 12
                        }
                        Label {
                            text: modelData.provider || ""
                            Layout.preferredWidth: 150
                            elide: Text.ElideRight
                            font.pixelSize: 12
                        }
                        Label {
                            text: modelData.eventName || ""
                            Layout.preferredWidth: 120
                            elide: Text.ElideRight
                            font.pixelSize: 12
                        }
                        Label {
                            text: (modelData.processId || "0") + ":" + (modelData.threadId || "0")
                            Layout.preferredWidth: 80
                            font.family: "Consolas"
                            font.pixelSize: 12
                        }
                        Label {
                            text: modelData.message || ""
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            font.pixelSize: 12
                        }
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: eventsList.currentIndex = index
                        onDoubleClicked: {
                            eventDetailDialog.eventData = _traceReader.getEventDetails(index)
                            eventDetailDialog.open()
                        }
                    }
                }
                
                // Empty state
                Label {
                    anchors.centerIn: parent
                    text: _traceReader.isReading 
                        ? qsTr("Reading trace file...")
                        : qsTr("No events loaded.\nSelect a trace file to begin.")
                    horizontalAlignment: Text.AlignHCenter
                    visible: traceDialog.events.length === 0
                    opacity: 0.6
                }
            }
        }
        
        // Error display
        Label {
            Layout.fillWidth: true
            text: _traceReader.lastError
            color: "red"
            visible: _traceReader.lastError.length > 0
            wrapMode: Text.WordWrap
        }
    }
    
    // File dialog
    FileDialog {
        id: fileDialog
        title: qsTr("Select Trace File")
        nameFilters: {
            var filters = _traceReader.supportedExtensions()
            if (filters.length > 0) {
                return [qsTr("Trace Files") + " (" + filters.join(" ") + ")", qsTr("All Files") + " (*)"]
            }
            return [qsTr("All Files") + " (*)"]
        }
        onAccepted: {
            filePathField.text = selectedFile.toString().replace("file:///", "")
        }
    }
    
    // Export dialog
    FileDialog {
        id: exportDialog
        title: qsTr("Export Trace Events")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Log Files") + " (*.log)", qsTr("Text Files") + " (*.txt)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "")
            _traceReader.exportToFile(path)
        }
    }
    
    // Event detail dialog
    Dialog {
        id: eventDetailDialog
        title: qsTr("Event Details")
        width: 600
        height: 400
        modal: true
        standardButtons: Dialog.Close
        
        property var eventData: ({})
        
        ScrollView {
            anchors.fill: parent
            
            TextArea {
                readOnly: true
                font.family: "Consolas"
                text: JSON.stringify(eventDetailDialog.eventData, null, 2)
                wrapMode: TextArea.Wrap
            }
        }
    }
    
    // Connections
    Connections {
        target: _traceReader
        
        function onEventsLoaded(events) {
            traceDialog.events = events
        }
        
        function onReadComplete(totalEvents) {
            console.log("Trace read complete:", totalEvents, "events")
        }
    }
}
