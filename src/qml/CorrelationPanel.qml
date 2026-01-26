/**
 * CorrelationPanel.qml
 * Log Correlation Analysis Panel for BigFileViewer
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    width: Math.min(1000, parent ? parent.width * 0.9 : 1000)
    height: Math.min(700, parent ? parent.height * 0.9 : 700)
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    
    property var model: null
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
    
    signal navigateToLine(string sourceFile, int lineNumber)
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8
        
        // Toolbar
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Label {
                text: qsTr("Log Correlation Analysis")
                font.bold: true
                font.pixelSize: 14
                color: root.textColor
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Add Source")
                icon.name: "list-add"
                onClicked: {
                    if (root.model) {
                        var name = root.model.filePath().split('/').pop().split('\\').pop()
                        _correlationAnalyzer.addSource(name, root.model)
                    }
                }
            }
            
            Button {
                text: qsTr("Clear Sources")
                onClicked: _correlationAnalyzer.clearSources()
            }
            
            ToolButton {
                text: "×"
                font.pixelSize: 16
                onClicked: root.close()
            }
        }
        
        // Sources list
        GroupBox {
            Layout.fillWidth: true
            title: qsTr("Sources (%1)").arg(sourcesList.count)
            
            ListView {
                id: sourcesList
                implicitHeight: Math.min(100, contentHeight)
                width: parent.width
                model: _correlationAnalyzer.getSources()
                clip: true
                
                delegate: RowLayout {
                    width: sourcesList.width
                    Label {
                        Layout.fillWidth: true
                        text: modelData
                    }
                    ToolButton {
                        text: "×"
                        onClicked: _correlationAnalyzer.removeSource(modelData)
                    }
                }
                
                Label {
                    anchors.centerIn: parent
                    text: qsTr("No sources added. Click 'Add Source' to add current file.")
                    visible: sourcesList.count === 0
                    font.italic: true
                    opacity: 0.7
                }
            }
        }
        
        // Analysis options
        GroupBox {
            Layout.fillWidth: true
            title: qsTr("Analysis Options")
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 8
                
                RowLayout {
                    Label { text: qsTr("Analysis Type:") }
                    ComboBox {
                        id: analysisTypeCombo
                        Layout.fillWidth: true
                        model: [
                            qsTr("Auto-detect IDs (UUID, Request ID, etc.)"),
                            qsTr("Time Window Correlation"),
                            qsTr("Custom ID Pattern")
                        ]
                    }
                }
                
                // Time window options
                RowLayout {
                    visible: analysisTypeCombo.currentIndex === 1
                    Label { text: qsTr("Time Window (seconds):") }
                    SpinBox {
                        id: timeWindowSpinBox
                        from: 1
                        to: 3600
                        value: 5
                    }
                }
                
                // Custom pattern options
                RowLayout {
                    visible: analysisTypeCombo.currentIndex === 2
                    Label { text: qsTr("ID Pattern (regex):") }
                    TextField {
                        id: customPatternField
                        Layout.fillWidth: true
                        placeholderText: qsTr("e.g., request-id[=:]([\\w-]+)")
                    }
                }
                
                RowLayout {
                    visible: analysisTypeCombo.currentIndex === 2
                    Label { text: qsTr("ID Name:") }
                    TextField {
                        id: customIdNameField
                        Layout.fillWidth: true
                        text: "customId"
                    }
                }
                
                RowLayout {
                    Button {
                        text: _correlationAnalyzer.isAnalyzing ? qsTr("Cancel") : qsTr("Analyze")
                        highlighted: !_correlationAnalyzer.isAnalyzing
                        enabled: sourcesList.count > 0
                        onClicked: {
                            if (_correlationAnalyzer.isAnalyzing) {
                                _correlationAnalyzer.cancelAnalysis()
                            } else {
                                switch (analysisTypeCombo.currentIndex) {
                                    case 0:
                                        _correlationAnalyzer.analyzeWithAutoDetection()
                                        break
                                    case 1:
                                        _correlationAnalyzer.analyzeByTimeWindow(timeWindowSpinBox.value)
                                        break
                                    case 2:
                                        _correlationAnalyzer.analyzeByIdPattern(customPatternField.text, customIdNameField.text)
                                        break
                                }
                            }
                        }
                    }
                    
                    Label {
                        id: statusLabel
                        text: _correlationAnalyzer.isAnalyzing ? qsTr("Analyzing...") : ""
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    Button {
                        text: qsTr("Export...")
                        enabled: _correlationAnalyzer.groupCount > 0
                        onClicked: exportMenu.open()
                        
                        Menu {
                            id: exportMenu
                            MenuItem {
                                text: qsTr("Export as JSON...")
                                onTriggered: { exportDialog.exportFormat = "json"; exportDialog.open() }
                            }
                            MenuItem {
                                text: qsTr("Export as CSV...")
                                onTriggered: { exportDialog.exportFormat = "csv"; exportDialog.open() }
                            }
                            MenuItem {
                                text: qsTr("Export as HTML...")
                                onTriggered: { exportDialog.exportFormat = "html"; exportDialog.open() }
                            }
                        }
                    }
                }
            }
        }
        
        // Results
        GroupBox {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: qsTr("Correlation Groups (%1)").arg(_correlationAnalyzer.groupCount)
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 4
                
                // Statistics
                RowLayout {
                    visible: _correlationAnalyzer.groupCount > 0
                    Label {
                        property var stats: _correlationAnalyzer.getStatistics()
                        text: qsTr("Total Events: %1 | Avg Duration: %2 ms")
                            .arg(stats.totalEvents || 0)
                            .arg(stats.averageDurationMs || 0)
                        font.italic: true
                    }
                }
                
                // Groups list
                SplitView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: Qt.Horizontal
                    
                    // Groups
                    ListView {
                        id: groupsListView
                        SplitView.preferredWidth: 300
                        SplitView.minimumWidth: 200
                        clip: true
                        model: groupsModel
                        currentIndex: -1
                        
                        delegate: ItemDelegate {
                            width: groupsListView.width
                            highlighted: ListView.isCurrentItem
                            
                            contentItem: ColumnLayout {
                                spacing: 2
                                Label {
                                    text: model.correlationId.length > 40 ? 
                                          model.correlationId.substring(0, 40) + "..." : 
                                          model.correlationId
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: qsTr("%1 events | %2 | %3 ms")
                                        .arg(model.eventCount)
                                        .arg(model.correlationType)
                                        .arg(model.durationMs)
                                    font.pixelSize: 11
                                    opacity: 0.7
                                }
                            }
                            
                            onClicked: {
                                groupsListView.currentIndex = index
                                loadGroupEvents(model.correlationId)
                            }
                        }
                        
                        ScrollBar.vertical: ScrollBar { active: true }
                        
                        Label {
                            anchors.centerIn: parent
                            text: qsTr("No correlation groups found.\nRun analysis to find related log entries.")
                            visible: groupsListView.count === 0
                            horizontalAlignment: Text.AlignHCenter
                            font.italic: true
                            opacity: 0.7
                        }
                    }
                    
                    // Events
                    ListView {
                        id: eventsListView
                        SplitView.fillWidth: true
                        clip: true
                        model: eventsModel
                        
                        delegate: ItemDelegate {
                            width: eventsListView.width
                            
                            contentItem: RowLayout {
                                spacing: 8
                                
                                Label {
                                    text: model.sourceFile
                                    font.pixelSize: 11
                                    Layout.preferredWidth: 100
                                    elide: Text.ElideMiddle
                                }
                                
                                Label {
                                    text: ":" + model.lineNumber
                                    font.pixelSize: 11
                                    Layout.preferredWidth: 50
                                }
                                
                                Rectangle {
                                    width: 50
                                    height: 18
                                    radius: 3
                                    color: {
                                        switch (model.level) {
                                            case "ERROR":
                                            case "FATAL":
                                            case "CRITICAL":
                                                return "#ffcccc"
                                            case "WARN":
                                            case "WARNING":
                                                return "#fff3cd"
                                            case "INFO":
                                                return "#cce5ff"
                                            case "DEBUG":
                                                return "#e2e3e5"
                                            default:
                                                return "transparent"
                                        }
                                    }
                                    
                                    Label {
                                        anchors.centerIn: parent
                                        text: model.level || "-"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                                
                                Label {
                                    text: model.message.substring(0, 100)
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    font.pixelSize: 11
                                }
                            }
                            
                            onDoubleClicked: {
                                root.navigateToLine(model.sourceFile, model.lineNumber)
                            }
                        }
                        
                        ScrollBar.vertical: ScrollBar { active: true }
                        
                        Label {
                            anchors.centerIn: parent
                            text: qsTr("Select a correlation group to view events")
                            visible: eventsListView.count === 0
                            font.italic: true
                            opacity: 0.7
                        }
                    }
                }
            }
        }
    }
    
    // Models
    ListModel {
        id: groupsModel
    }
    
    ListModel {
        id: eventsModel
    }
    
    function loadGroupEvents(correlationId) {
        eventsModel.clear()
        var events = _correlationAnalyzer.getCorrelatedEvents(correlationId)
        for (var i = 0; i < events.length; i++) {
            eventsModel.append(events[i])
        }
    }
    
    // Connections
    Connections {
        target: _correlationAnalyzer
        
        function onAnalysisCompleted(groupCount) {
            statusLabel.text = qsTr("Analysis completed: %1 groups found").arg(groupCount)
            
            // Reload groups
            groupsModel.clear()
            var groups = _correlationAnalyzer.getCorrelationGroups()
            for (var i = 0; i < groups.length; i++) {
                groupsModel.append(groups[i])
            }
        }
        
        function onAnalysisProgress(current, total) {
            statusLabel.text = qsTr("Analyzing: %1 / %2").arg(current).arg(total)
        }
        
        function onErrorOccurred(error) {
            statusLabel.text = qsTr("Error: %1").arg(error)
        }
        
        function onSourceAdded(name) {
            sourcesList.model = _correlationAnalyzer.getSources()
        }
        
        function onSourceRemoved(name) {
            sourcesList.model = _correlationAnalyzer.getSources()
        }
    }
    
    // Export Dialog
    Dialog {
        id: exportDialog
        title: qsTr("Export Correlation Results")
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        
        property string exportFormat: "json"
        
        ColumnLayout {
            spacing: 8
            
            Label {
                text: qsTr("File Path:")
            }
            
            TextField {
                id: exportPathField
                Layout.preferredWidth: 400
                text: "correlation_results." + exportDialog.exportFormat
            }
        }
        
        onAccepted: {
            _correlationAnalyzer.exportResults(exportPathField.text, exportFormat)
        }
    }
}
