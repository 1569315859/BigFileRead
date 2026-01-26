import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform

/**
 * @file ParserTestPanel.qml
 * @brief Parser testing tool panel - test log parsing configuration
 * 
 * Features:
 * - Sample log input (paste or load from file)
 * - Real-time parsing preview
 * - Field extraction visualization
 * - Error highlighting
 * - Multiline merge preview
 */
Popup {
    id: root
    width: Math.min(900, parent ? parent.width * 0.9 : 900)
    height: Math.min(700, parent ? parent.height * 0.9 : 700)
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    
    property var logParser: null  // LogParser reference
    property string sampleText: ""
    property var parsedResults: []
    property var detectedFormat: null
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
    
    signal testCompleted(bool success, string message)
    signal applyConfiguration()
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10
        
        // Header
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: qsTr("Parser Test Tool")
                font.bold: true
                font.pixelSize: 16
                color: _themeManager ? _themeManager.textColor : palette.text
            }
            
            Item { Layout.fillWidth: true }
            
            ToolButton {
                text: "×"
                font.pixelSize: 18
                onClicked: root.close()
            }
        }
        
        // Sample Input Section
        GroupBox {
            title: qsTr("Sample Log Input")
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 5
                
                RowLayout {
                    Layout.fillWidth: true
                    
                    Button {
                        text: qsTr("Load from File")
                        icon.name: "document-open"
                        onClicked: fileDialog.open()
                    }
                    
                    Button {
                        text: qsTr("Paste from Clipboard")
                        icon.name: "edit-paste"
                        onClicked: {
                            sampleInput.paste()
                        }
                    }
                    
                    Button {
                        text: qsTr("Clear")
                        icon.name: "edit-clear"
                        onClicked: {
                            sampleInput.text = ""
                            root.sampleText = ""
                            root.parsedResults = []
                        }
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    Label {
                        text: qsTr("Lines: %1").arg(sampleInput.text.split("\n").length)
                        color: "gray"
                    }
                }
                
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    TextArea {
                        id: sampleInput
                        placeholderText: qsTr("Paste or load sample log lines here...")
                        font.family: "Consolas, Monaco, monospace"
                        font.pixelSize: 12
                        wrapMode: TextEdit.NoWrap
                        selectByMouse: true
                        
                        onTextChanged: {
                            root.sampleText = text
                            parseTimer.restart()
                        }
                    }
                }
            }
        }
        
        // Auto-detect Section
        GroupBox {
            title: qsTr("Format Detection")
            Layout.fillWidth: true
            
            RowLayout {
                anchors.fill: parent
                spacing: 10
                
                Button {
                    text: qsTr("Auto-Detect Format")
                    icon.name: "system-search"
                    enabled: root.sampleText.length > 0
                    onClicked: {
                        if (_logParser) {
                            var result = _logParser.detectFormat(root.sampleText)
                            root.detectedFormat = result
                        }
                    }
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: root.detectedFormat ? "#e8f5e9" : "#f5f5f5"
                    border.color: "#ddd"
                    radius: 4
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 10
                        
                        Label {
                            text: qsTr("Detected:")
                            color: "gray"
                        }
                        
                        Label {
                            text: root.detectedFormat ? root.detectedFormat.formatName : qsTr("None")
                            font.bold: true
                        }
                        
                        Label {
                            text: root.detectedFormat ? qsTr("Confidence: %1%").arg(root.detectedFormat.confidence) : ""
                            color: {
                                if (!root.detectedFormat) return "gray"
                                if (root.detectedFormat.confidence >= 80) return "#4caf50"
                                if (root.detectedFormat.confidence >= 50) return "#ff9800"
                                return "#f44336"
                            }
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Button {
                            text: qsTr("Apply")
                            enabled: root.detectedFormat !== null
                            onClicked: {
                                if (_logParser && root.sampleText.length > 0) {
                                    _logParser.autoDetectAndConfigure(root.sampleText)
                                    root.applyConfiguration()
                                    parseTimer.restart()
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Parsing Results Section
        GroupBox {
            title: qsTr("Parsing Results")
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 5
                
                // Column headers
                RowLayout {
                    Layout.fillWidth: true
                    
                    Label {
                        text: qsTr("Line")
                        font.bold: true
                        Layout.preferredWidth: 50
                    }
                    
                    Repeater {
                        model: _logParser ? _logParser.columnHeaders : []
                        
                        Label {
                            text: modelData
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#ddd"
                }
                
                // Results table
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    ListView {
                        id: resultsView
                        model: root.parsedResults
                        clip: true
                        
                        delegate: Rectangle {
                            width: resultsView.width
                            height: 30
                            color: index % 2 === 0 ? "#ffffff" : "#f9f9f9"
                            border.color: modelData.error ? "#ffcdd2" : "transparent"
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 5
                                anchors.rightMargin: 5
                                spacing: 5
                                
                                // Line number
                                Label {
                                    text: modelData.lineRange || (index + 1)
                                    color: "gray"
                                    font.pixelSize: 11
                                    Layout.preferredWidth: 50
                                }
                                
                                // Parsed fields
                                Repeater {
                                    model: modelData.fields || []
                                    
                                    Label {
                                        text: modelData || ""
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                        font.pixelSize: 11
                                        color: {
                                            // Highlight log levels
                                            var lower = text.toLowerCase()
                                            if (lower === "error" || lower === "fatal") return "#f44336"
                                            if (lower === "warn" || lower === "warning") return "#ff9800"
                                            if (lower === "info") return "#2196f3"
                                            if (lower === "debug" || lower === "trace") return "#9e9e9e"
                                            return "black"
                                        }
                                    }
                                }
                                
                                // Error indicator
                                Label {
                                    visible: modelData.error
                                    text: "⚠"
                                    color: "#f44336"
                                    ToolTip.visible: mouseArea.containsMouse
                                    ToolTip.text: modelData.errorMessage || qsTr("Parse error")
                                    
                                    MouseArea {
                                        id: mouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Summary
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    color: "#f5f5f5"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 5
                        
                        Label {
                            text: qsTr("Total: %1 lines").arg(root.parsedResults.length)
                        }
                        
                        Label {
                            property int errorCount: {
                                var count = 0
                                for (var i = 0; i < root.parsedResults.length; i++) {
                                    if (root.parsedResults[i].error) count++
                                }
                                return count
                            }
                            text: qsTr("Errors: %1").arg(errorCount)
                            color: errorCount > 0 ? "#f44336" : "#4caf50"
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Button {
                            text: qsTr("Re-parse")
                            icon.name: "view-refresh"
                            onClicked: parseTimer.restart()
                        }
                        
                        Button {
                            text: qsTr("Export Results")
                            icon.name: "document-save"
                            enabled: root.parsedResults.length > 0
                            onClicked: exportResults()
                        }
                    }
                }
            }
        }
    }
    
    // Parse timer (debounce)
    Timer {
        id: parseTimer
        interval: 300
        onTriggered: parseAllLines()
    }
    
    // File dialog for loading sample
    Platform.FileDialog {
        id: fileDialog
        title: qsTr("Load Sample Log File")
        nameFilters: ["Log files (*.log *.txt)", "All files (*)"]
        onAccepted: {
            // Load first 100 lines from file
            loadSampleFromFile(file)
        }
    }
    
    function parseAllLines() {
        if (!_logParser || root.sampleText.length === 0) {
            root.parsedResults = []
            return
        }
        
        var lines = root.sampleText.split(/\r?\n/)
        var results = []
        var currentMergedLines = []
        var mergeStartLine = 0
        
        for (var i = 0; i < Math.min(lines.length, 100); i++) {
            var line = lines[i]
            
            // Check for multiline continuation
            if (_logParser.isMultilineEnabled() && _logParser.isContinuationLine(line) && currentMergedLines.length > 0) {
                currentMergedLines.push(line)
                continue
            }
            
            // Flush previous merged entry
            if (currentMergedLines.length > 0) {
                var mergedText = currentMergedLines.join("\\n")
                var mergedResult = parseSingleLine(mergedText, mergeStartLine, mergeStartLine + currentMergedLines.length - 1)
                results.push(mergedResult)
            }
            
            // Start new entry
            currentMergedLines = [line]
            mergeStartLine = i + 1
        }
        
        // Flush last entry
        if (currentMergedLines.length > 0) {
            var lastMerged = currentMergedLines.join("\\n")
            var lastResult = parseSingleLine(lastMerged, mergeStartLine, mergeStartLine + currentMergedLines.length - 1)
            results.push(lastResult)
        }
        
        root.parsedResults = results
    }
    
    function parseSingleLine(line, startLine, endLine) {
        var result = {
            lineRange: startLine === endLine ? startLine.toString() : (startLine + "-" + endLine),
            fields: [],
            error: false,
            errorMessage: ""
        }
        
        try {
            if (_logParser) {
                result.fields = _logParser.parseLine(line)
                if (result.fields.length === 0 || (result.fields.length === 1 && result.fields[0] === line)) {
                    // Parsing didn't extract any fields
                    result.error = true
                    result.errorMessage = qsTr("No fields extracted - pattern may not match")
                }
            }
        } catch (e) {
            result.error = true
            result.errorMessage = e.toString()
        }
        
        return result
    }
    
    function loadSampleFromFile(fileUrl) {
        // Use FileIO or request from C++ side
        // For now, just set a placeholder
        console.log("Loading from:", fileUrl)
    }
    
    function exportResults() {
        var output = []
        var headers = _logParser ? _logParser.columnHeaders : []
        output.push("Line," + headers.join(","))
        
        for (var i = 0; i < root.parsedResults.length; i++) {
            var row = root.parsedResults[i]
            var fields = [row.lineRange].concat(row.fields || [])
            output.push(fields.map(function(f) {
                return '"' + (f || "").replace(/"/g, '""') + '"'
            }).join(","))
        }
        
        // Copy to clipboard or save
        console.log(output.join("\n"))
    }
    
    Component.onCompleted: {
        if (sampleText.length > 0) {
            parseTimer.restart()
        }
    }
}
