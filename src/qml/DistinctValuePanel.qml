import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    width: Math.min(900, parent ? parent.width * 0.9 : 900)
    height: Math.min(700, parent ? parent.height * 0.9 : 700)
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    
    property var selectedColumn: null
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
        anchors.margins: 16
        spacing: 12
        
        // Header
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: qsTr("Distinct Value Analysis")
                font.pixelSize: 18
                font.bold: true
                color: _themeManager ? _themeManager.textColor : "#fff"
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Auto-Detect Columns")
                onClicked: {
                    if (typeof _logModel !== 'undefined') {
                        var sample = _logModel.getSampleLines(100)
                        var detected = _distinctAnalyzer.detectColumns(sample)
                        for (var i = 0; i < detected.length; i++) {
                            _distinctAnalyzer.addColumn(detected[i])
                        }
                    }
                }
            }
            
            Button {
                text: "X"
                flat: true
                onClicked: root.close()
            }
        }
        
        // Main content
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12
            
            // Left panel - Column configuration
            Rectangle {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                color: "#252525"
                radius: 4
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    
                    Label {
                        text: qsTr("Columns")
                        font.bold: true
                        color: "#aaa"
                    }
                    
                    // Add column form
                    GroupBox {
                        Layout.fillWidth: true
                        title: qsTr("Add Column")
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4
                            
                            TextField {
                                id: colNameField
                                Layout.fillWidth: true
                                placeholderText: qsTr("Column name")
                            }
                            
                            TextField {
                                id: colRegexField
                                Layout.fillWidth: true
                                placeholderText: qsTr("Regex pattern (with capture group)")
                            }
                            
                            Button {
                                text: qsTr("Add")
                                Layout.fillWidth: true
                                enabled: colNameField.text.length > 0 && colRegexField.text.length > 0
                                onClicked: {
                                    _distinctAnalyzer.addColumn({
                                        name: colNameField.text,
                                        type: 5, // Regex
                                        regexPattern: colRegexField.text,
                                        captureGroup: 1
                                    })
                                    colNameField.text = ""
                                    colRegexField.text = ""
                                }
                            }
                        }
                    }
                    
                    // Column list
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: _distinctAnalyzer.columns
                        clip: true
                        
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 36
                            color: selectedColumn === modelData.name ? "#3a5a8a" : (index % 2 === 0 ? "#333" : "#2a2a2a")
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 4
                                
                                Label {
                                    text: modelData.name
                                    color: "#fff"
                                    Layout.fillWidth: true
                                }
                                
                                Button {
                                    text: "X"
                                    flat: true
                                    implicitWidth: 24
                                    implicitHeight: 24
                                    onClicked: _distinctAnalyzer.removeColumn(modelData.name)
                                }
                            }
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: selectedColumn = modelData.name
                            }
                        }
                    }
                    
                    // Actions
                    RowLayout {
                        Layout.fillWidth: true
                        
                        Button {
                            text: qsTr("Analyze")
                            Layout.fillWidth: true
                            enabled: !_distinctAnalyzer.isAnalyzing && _distinctAnalyzer.columns.length > 0
                            onClicked: {
                                if (typeof _logModel !== 'undefined') {
                                    var lines = _logModel.getAllLines()
                                    _distinctAnalyzer.analyzeLines(lines)
                                }
                            }
                        }
                        
                        Button {
                            text: qsTr("Clear")
                            onClicked: _distinctAnalyzer.clearColumns()
                        }
                    }
                    
                    // Progress
                    ProgressBar {
                        Layout.fillWidth: true
                        visible: _distinctAnalyzer.isAnalyzing
                        value: _distinctAnalyzer.progress / 100
                    }
                }
            }
            
            // Right panel - Results
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#252525"
                radius: 4
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    
                    // Results header
                    RowLayout {
                        Layout.fillWidth: true
                        
                        Label {
                            text: selectedColumn ? qsTr("Results for: %1").arg(selectedColumn) : qsTr("Results")
                            font.bold: true
                            color: "#aaa"
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Button {
                            text: qsTr("Export CSV")
                            enabled: _distinctAnalyzer.results.length > 0
                            onClicked: {
                                var csv = _distinctAnalyzer.exportToCsv()
                                // Would open save dialog
                                console.log(csv)
                            }
                        }
                        
                        Button {
                            text: qsTr("Export JSON")
                            enabled: _distinctAnalyzer.results.length > 0
                            onClicked: {
                                var json = _distinctAnalyzer.exportToJson()
                                console.log(json)
                            }
                        }
                    }
                    
                    // Summary stats
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        visible: selectedColumn !== null
                        
                        property var stats: selectedColumn ? _distinctAnalyzer.getResult(selectedColumn) : ({})
                        
                        Label { text: qsTr("Total:"); color: "#888" }
                        Label { text: parent.stats.totalCount || "0"; color: "#fff" }
                        Label { text: qsTr("Distinct:"); color: "#888" }
                        Label { text: parent.stats.distinctCount || "0"; color: "#fff" }
                        
                        Label { text: qsTr("Uniqueness:"); color: "#888" }
                        Label { text: parent.stats.uniquenessRatio ? (parent.stats.uniquenessRatio * 100).toFixed(1) + "%" : "0%"; color: "#fff" }
                        Label { text: qsTr("Empty:"); color: "#888" }
                        Label { text: parent.stats.emptyCount || "0"; color: "#fff" }
                        
                        Label { text: qsTr("Most Common:"); color: "#888" }
                        Label { text: parent.stats.mostCommon || "-"; color: "#4fc3f7"; Layout.columnSpan: 3; elide: Text.ElideRight }
                    }
                    
                    // Numeric stats (if applicable)
                    GroupBox {
                        Layout.fillWidth: true
                        title: qsTr("Numeric Statistics")
                        visible: selectedColumn && _distinctAnalyzer.getResult(selectedColumn).isNumeric
                        
                        property var stats: selectedColumn ? _distinctAnalyzer.getStatistics(selectedColumn) : ({})
                        
                        GridLayout {
                            anchors.fill: parent
                            columns: 6
                            
                            Label { text: qsTr("Min:"); color: "#888" }
                            Label { text: parent.parent.stats.min?.toFixed(2) || "-"; color: "#fff" }
                            Label { text: qsTr("Max:"); color: "#888" }
                            Label { text: parent.parent.stats.max?.toFixed(2) || "-"; color: "#fff" }
                            Label { text: qsTr("Avg:"); color: "#888" }
                            Label { text: parent.parent.stats.avg?.toFixed(2) || "-"; color: "#fff" }
                            
                            Label { text: qsTr("Median:"); color: "#888" }
                            Label { text: parent.parent.stats.median?.toFixed(2) || "-"; color: "#fff" }
                            Label { text: qsTr("StdDev:"); color: "#888" }
                            Label { text: parent.parent.stats.stdDev?.toFixed(2) || "-"; color: "#fff" }
                        }
                    }
                    
                    // Top values
                    Label {
                        text: qsTr("Top Values")
                        font.bold: true
                        color: "#aaa"
                    }
                    
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: selectedColumn ? _distinctAnalyzer.getTopValues(selectedColumn, 20) : []
                        clip: true
                        
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 32
                            color: index % 2 === 0 ? "#333" : "#2a2a2a"
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 4
                                spacing: 8
                                
                                Label {
                                    text: (index + 1) + "."
                                    color: "#666"
                                    Layout.preferredWidth: 30
                                }
                                
                                Label {
                                    text: modelData.value
                                    color: "#fff"
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                
                                Rectangle {
                                    Layout.preferredWidth: Math.max(40, modelData.count / Math.max(1, _distinctAnalyzer.getTopValues(selectedColumn, 1)[0]?.count || 1) * 150)
                                    Layout.preferredHeight: 16
                                    color: "#4fc3f7"
                                    radius: 2
                                }
                                
                                Label {
                                    text: modelData.count
                                    color: "#aaa"
                                    Layout.preferredWidth: 60
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }
                    
                    // Patterns
                    Label {
                        text: qsTr("Value Patterns")
                        font.bold: true
                        color: "#aaa"
                    }
                    
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 100
                        model: selectedColumn ? _distinctAnalyzer.getPatterns(selectedColumn) : []
                        clip: true
                        
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 24
                            color: "#2a2a2a"
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 4
                                
                                Label {
                                    text: modelData.pattern
                                    color: "#81c784"
                                    font.family: "Consolas"
                                    Layout.fillWidth: true
                                }
                                
                                Label {
                                    text: modelData.count
                                    color: "#aaa"
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
