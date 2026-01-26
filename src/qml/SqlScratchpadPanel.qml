/**
 * SqlScratchpadPanel.qml
 * SQL Query Analysis Panel for BigFileViewer
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
                text: qsTr("SQL Scratchpad")
                font.bold: true
                font.pixelSize: 14
                color: root.textColor
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Import Data")
                icon.name: "document-import"
                onClicked: importMenu.open()
                
                Menu {
                    id: importMenu
                    
                    MenuItem {
                        text: qsTr("Import All Lines")
                        onTriggered: {
                            if (root.model) {
                                _sqlScratchpad.importFromModel(root.model, 0)
                            }
                        }
                    }
                    MenuItem {
                        text: qsTr("Import First 10,000 Lines")
                        onTriggered: {
                            if (root.model) {
                                _sqlScratchpad.importFromModel(root.model, 10000)
                            }
                        }
                    }
                    MenuItem {
                        text: qsTr("Import First 100,000 Lines")
                        onTriggered: {
                            if (root.model) {
                                _sqlScratchpad.importFromModel(root.model, 100000)
                            }
                        }
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Import Filtered Results")
                        onTriggered: {
                            if (root.model) {
                                _sqlScratchpad.importFilteredData(root.model)
                            }
                        }
                    }
                }
            }
            
            Button {
                text: qsTr("Saved Queries")
                icon.name: "bookmark"
                onClicked: savedQueriesMenu.open()
                
                Menu {
                    id: savedQueriesMenu
                    
                    Instantiator {
                        model: _sqlScratchpad.savedQueries
                        
                        MenuItem {
                            text: modelData
                            onTriggered: {
                                queryEditor.text = _sqlScratchpad.getSavedQuery(modelData)
                            }
                        }
                        
                        onObjectAdded: (index, object) => savedQueriesMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => savedQueriesMenu.removeItem(object)
                    }
                    
                    MenuSeparator { visible: _sqlScratchpad.savedQueries.length > 0 }
                    
                    MenuItem {
                        text: qsTr("Save Current Query...")
                        onTriggered: saveQueryDialog.open()
                    }
                }
            }
            
            Button {
                text: qsTr("History")
                icon.name: "history"
                onClicked: historyMenu.open()
                
                Menu {
                    id: historyMenu
                    
                    Instantiator {
                        model: _sqlScratchpad.queryHistory.slice(0, 10)
                        
                        MenuItem {
                            text: modelData.length > 50 ? modelData.substring(0, 50) + "..." : modelData
                            onTriggered: queryEditor.text = modelData
                        }
                        
                        onObjectAdded: (index, object) => historyMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => historyMenu.removeItem(object)
                    }
                    
                    MenuSeparator { visible: _sqlScratchpad.queryHistory.length > 0 }
                    
                    MenuItem {
                        text: qsTr("Clear History")
                        onTriggered: _sqlScratchpad.clearHistory()
                    }
                }
            }
            
            ToolButton {
                text: "×"
                font.pixelSize: 16
                onClicked: root.close()
            }
        }
        
        // Query Editor
        GroupBox {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            title: qsTr("SQL Query")
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 4
                
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    TextArea {
                        id: queryEditor
                        placeholderText: qsTr("Enter SQL query here...\nExample: SELECT * FROM logs WHERE level = 'ERROR' LIMIT 100")
                        font.family: "Consolas, Monaco, monospace"
                        wrapMode: TextEdit.Wrap
                        
                        Keys.onPressed: (event) => {
                            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && 
                                (event.modifiers & Qt.ControlModifier)) {
                                executeButton.clicked()
                                event.accepted = true
                            }
                        }
                    }
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    
                    Button {
                        id: executeButton
                        text: qsTr("Execute (Ctrl+Enter)")
                        highlighted: true
                        enabled: !_sqlScratchpad.isLoading && queryEditor.text.trim().length > 0
                        onClicked: {
                            _sqlScratchpad.executeQuery(queryEditor.text)
                        }
                    }
                    
                    Button {
                        text: qsTr("Cancel")
                        visible: _sqlScratchpad.isLoading
                        onClicked: _sqlScratchpad.cancelQuery()
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    Label {
                        text: qsTr("Table: logs (%1 rows)").arg(_sqlScratchpad.rowCount)
                        font.italic: true
                    }
                    
                    ComboBox {
                        id: templateCombo
                        model: [
                            qsTr("-- Templates --"),
                            "SELECT * FROM logs LIMIT 100",
                            "SELECT level, COUNT(*) FROM logs GROUP BY level",
                            "SELECT * FROM logs WHERE level = 'ERROR'",
                            "SELECT * FROM logs WHERE message LIKE '%error%'",
                            "SELECT * FROM logs WHERE timestamp >= '2024-01-01'",
                            "SELECT DISTINCT source FROM logs",
                            "SELECT * FROM logs ORDER BY timestamp DESC LIMIT 50"
                        ]
                        onActivated: {
                            if (currentIndex > 0) {
                                queryEditor.text = currentText
                                currentIndex = 0
                            }
                        }
                    }
                }
            }
        }
        
        // Results Table
        GroupBox {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: qsTr("Results") + (resultColumns.length > 0 ? " (" + resultColumns.length + " columns)" : "")
            
            property var resultColumns: _sqlScratchpad.getResultColumns()
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 4
                
                // Column headers
                Rectangle {
                    Layout.fillWidth: true
                    height: 30
                    color: palette.alternateBase
                    
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        spacing: 1
                        
                        Repeater {
                            model: parent.parent.parent.resultColumns
                            
                            Rectangle {
                                width: 150
                                height: parent.height
                                color: "transparent"
                                
                                Label {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    text: modelData
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
                
                // Results list
                ListView {
                    id: resultsView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: resultModel
                    
                    ScrollBar.vertical: ScrollBar { active: true }
                    ScrollBar.horizontal: ScrollBar { active: true }
                    
                    delegate: Rectangle {
                        width: resultsView.width
                        height: 28
                        color: index % 2 === 0 ? "transparent" : palette.alternateBase
                        
                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 4
                            spacing: 1
                            
                            Repeater {
                                model: Object.keys(modelData)
                                
                                Rectangle {
                                    width: 150
                                    height: parent.height
                                    color: "transparent"
                                    
                                    Label {
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        text: modelData[modelData] !== undefined ? modelData[modelData] : ""
                                        elide: Text.ElideRight
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
                        text: _sqlScratchpad.isLoading ? qsTr("Executing...") : ""
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    Button {
                        text: qsTr("Export...")
                        enabled: resultModel.count > 0
                        onClicked: exportMenu.open()
                        
                        Menu {
                            id: exportMenu
                            
                            MenuItem {
                                text: qsTr("Export as CSV...")
                                onTriggered: { exportDialog.exportFormat = "csv"; exportDialog.open() }
                            }
                            MenuItem {
                                text: qsTr("Export as JSON...")
                                onTriggered: { exportDialog.exportFormat = "json"; exportDialog.open() }
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
    }
    
    // Result data model
    ListModel {
        id: resultModel
    }
    
    // Connections
    Connections {
        target: _sqlScratchpad
        
        function onQueryCompleted(queryId, rowCount, elapsed) {
            statusLabel.text = qsTr("Query completed: %1 rows in %2 ms").arg(rowCount).arg(elapsed)
            
            // Load results
            resultModel.clear()
            var results = _sqlScratchpad.getResults(0, 1000)
            for (var i = 0; i < results.length; i++) {
                resultModel.append(results[i])
            }
        }
        
        function onQueryFailed(queryId, error) {
            statusLabel.text = qsTr("Error: %1").arg(error)
        }
        
        function onImportProgress(current, total) {
            statusLabel.text = qsTr("Importing: %1 / %2").arg(current).arg(total)
        }
        
        function onExportCompleted(filePath) {
            statusLabel.text = qsTr("Exported to: %1").arg(filePath)
        }
    }
    
    // Save Query Dialog
    Dialog {
        id: saveQueryDialog
        title: qsTr("Save Query")
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        
        ColumnLayout {
            spacing: 8
            
            Label {
                text: qsTr("Query Name:")
            }
            
            TextField {
                id: queryNameField
                Layout.preferredWidth: 300
                placeholderText: qsTr("Enter a name for this query")
            }
        }
        
        onAccepted: {
            if (queryNameField.text.trim().length > 0) {
                _sqlScratchpad.saveQuery(queryNameField.text.trim(), queryEditor.text)
                queryNameField.text = ""
            }
        }
    }
    
    // Export Dialog (simplified - in production use FileDialog)
    Dialog {
        id: exportDialog
        title: qsTr("Export Results")
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        
        property string exportFormat: "csv"
        
        ColumnLayout {
            spacing: 8
            
            Label {
                text: qsTr("File Path:")
            }
            
            TextField {
                id: exportPathField
                Layout.preferredWidth: 400
                text: "query_results." + exportDialog.exportFormat
            }
        }
        
        onAccepted: {
            _sqlScratchpad.exportResults(exportPathField.text, exportFormat)
        }
    }
    
    // Schema info popup
    Component.onCompleted: {
        // Show table info on first load
        var info = _sqlScratchpad.getTableInfo()
        console.log("SQL Scratchpad table schema:", JSON.stringify(info))
    }
}
