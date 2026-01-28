import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform

Popup {
    id: root
    modal: true
    width: 500
    height: 420
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape

    property color textColor: _themeManager.textColor
    property color panelColor: _themeManager.panelBackground
    property color bgColor: _themeManager.backgroundColor
    property color accentColor: _themeManager.accentColor
    property color borderColor: _themeManager.borderColor
    
    // Export options
    property int exportFormat: 0  // 0 = CSV, 1 = HTML
    property bool bookmarksOnly: false
    property int startLine: 1
    property int endLine: logModel ? logModel.totalLineCount() : 0
    property bool exportAll: true
    property bool enableSanitization: false  // 脱敏开关
    
    // Add logModel property
    property var logModel: null
    
    signal exportCompleted(string path)
    signal configureRequested()  // 请求打开脱敏配置对话框

    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 6
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Title
        Text {
            text: qsTr("Export Data")
            font.bold: true
            font.pixelSize: 16
            color: textColor
            Layout.alignment: Qt.AlignHCenter
        }

        // Format selection
        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            
            Label {
                text: qsTr("Format:")
                color: textColor
                font.pixelSize: 13
            }
            
            RadioButton {
                id: csvRadio
                text: "CSV"
                checked: true
                onCheckedChanged: if (checked) exportFormat = 0
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 13
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
            
            RadioButton {
                id: htmlRadio
                text: "HTML"
                onCheckedChanged: if (checked) exportFormat = 1
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 13
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
        }
        
        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: borderColor
        }

        // Range options
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Label {
                text: qsTr("Export Range:")
                color: textColor
                font.pixelSize: 13
            }
            
            RadioButton {
                id: allLinesRadio
                text: qsTr("All lines (%1 total)").arg(logModel ? logModel.totalLineCount().toLocaleString() : "0")
                checked: true
                onCheckedChanged: if (checked) exportAll = true
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 12
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
            
            RadioButton {
                id: currentViewRadio
                text: qsTr("Current view (%1 filtered lines)").arg(logModel ? logModel.lineCount.toLocaleString() : "0")
                visible: logModel ? logModel.isFilterMode : false
                onCheckedChanged: if (checked) exportAll = false
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 12
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
            
            RadioButton {
                id: rangeRadio
                text: qsTr("Line range:")
                onCheckedChanged: if (checked) exportAll = false
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 12
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
            
            // Range inputs
            RowLayout {
                Layout.leftMargin: 30
                spacing: 10
                enabled: rangeRadio.checked
                opacity: enabled ? 1.0 : 0.5
                
                TextField {
                    id: startLineInput
                    implicitWidth: 100
                    implicitHeight: 28
                    text: "1"
                    font.pixelSize: 12
                    color: textColor
                    placeholderText: qsTr("Start")
                    validator: IntValidator { bottom: 1; top: logModel ? logModel.totalLineCount() : 1 }
                    background: Rectangle {
                        color: bgColor
                        border.color: borderColor
                        radius: 4
                    }
                }
                
                Label {
                    text: "-"
                    color: textColor
                    font.pixelSize: 13
                }
                
                TextField {
                    id: endLineInput
                    implicitWidth: 100
                    implicitHeight: 28
                    text: logModel ? logModel.totalLineCount().toString() : "0"
                    font.pixelSize: 12
                    color: textColor
                    placeholderText: qsTr("End")
                    validator: IntValidator { bottom: 1; top: logModel ? logModel.totalLineCount() : 1 }
                    background: Rectangle {
                        color: bgColor
                        border.color: borderColor
                        radius: 4
                    }
                }
            }
        }
        
        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: borderColor
        }
        
        // Bookmarks only option
        CheckBox {
            id: bookmarksOnlyCheck
            text: qsTr("Export bookmarked lines only (%1 bookmarks)").arg(logModel && logModel.bookmarkLines ? logModel.bookmarkLines.length : 0)
            enabled: logModel && logModel.bookmarkLines ? logModel.bookmarkLines.length > 0 : false
            onCheckedChanged: bookmarksOnly = checked
            contentItem: Text {
                text: parent.text
                color: textColor
                font.pixelSize: 12
                leftPadding: parent.indicator.width + parent.spacing
                opacity: parent.enabled ? 1.0 : 0.5
            }
        }
        
        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: borderColor
        }
        
        // Data Sanitization Section
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            
            CheckBox {
                id: sanitizationCheck
                text: qsTr("Enable data sanitization before export")
                checked: enableSanitization
                onCheckedChanged: enableSanitization = checked
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 12
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
            
            RowLayout {
                Layout.leftMargin: 30
                spacing: 10
                enabled: sanitizationCheck.checked
                opacity: enabled ? 1.0 : 0.5
                
                Label {
                    text: qsTr("Sanitization Level:")
                    color: textColor
                    font.pixelSize: 12
                }
                
                ComboBox {
                    id: sanitizationLevelCombo
                    implicitWidth: 150
                    implicitHeight: 28
                    model: [qsTr("Minimal"), qsTr("Standard"), qsTr("Strict")]
                    currentIndex: 1  // Default: Standard
                    
                    background: Rectangle {
                        color: bgColor
                        border.color: borderColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: sanitizationLevelCombo.displayText
                        color: textColor
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }
                    popup: Popup {
                        y: sanitizationLevelCombo.height
                        width: sanitizationLevelCombo.width
                        implicitHeight: contentItem.implicitHeight
                        padding: 1
                        
                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: sanitizationLevelCombo.popup.visible ? sanitizationLevelCombo.delegateModel : null
                            currentIndex: sanitizationLevelCombo.highlightedIndex
                        }
                        
                        background: Rectangle {
                            color: panelColor
                            border.color: borderColor
                            radius: 4
                        }
                    }
                    delegate: ItemDelegate {
                        width: sanitizationLevelCombo.width
                        contentItem: Text {
                            text: modelData
                            color: textColor
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                        }
                        highlighted: sanitizationLevelCombo.highlightedIndex === index
                        background: Rectangle {
                            color: highlighted ? accentColor : "transparent"
                        }
                    }
                }
                
                Button {
                    text: qsTr("Configure...")
                    implicitHeight: 28
                    implicitWidth: 90
                    onClicked: {
                        root.configureRequested()
                    }
                    background: Rectangle {
                        color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                        border.color: borderColor
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: textColor
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        
        Item { Layout.fillHeight: true }

        // Buttons
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 10
            
            Button {
                text: qsTr("Export")
                implicitHeight: 32
                implicitWidth: 100
                onClicked: doExport()
                background: Rectangle {
                    color: parent.down ? Qt.darker(accentColor, 1.2) : accentColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            
            Button {
                text: qsTr("Cancel")
                implicitHeight: 32
                implicitWidth: 80
                onClicked: root.close()
                background: Rectangle {
                    color: parent.down ? Qt.darker(panelColor, 1.3) : (parent.hovered ? Qt.darker(panelColor, 1.1) : panelColor)
                    border.color: borderColor
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: textColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
    
    // File save dialog
    Platform.FileDialog {
        id: saveDialog
        title: exportFormat === 0 ? qsTr("Export as CSV") : qsTr("Export as HTML")
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: exportFormat === 0 
            ? ["CSV files (*.csv)", "All files (*)"]
            : ["HTML files (*.html *.htm)", "All files (*)"]
        
        onAccepted: {
            var path = _appController.urlToLocalPath(file)
            var start = rangeRadio.checked ? parseInt(startLineInput.text) - 1 : 0
            var end = rangeRadio.checked ? parseInt(endLineInput.text) - 1 : -1
            
            // Apply sanitization if enabled
            var sanitizationLevel = enableSanitization ? sanitizationLevelCombo.currentIndex : -1
            
            var success = false
            if (logModel) {
            if (exportFormat === 0) {
                success = logModel.exportToCSV(path, start, end, bookmarksOnly, sanitizationLevel)
            } else {
                success = logModel.exportToHTML(path, start, end, bookmarksOnly, sanitizationLevel)
            }
            }
            
            if (success) {
                root.exportCompleted(path)
                root.close()
            }
        }
    }
    
    function doExport() {
        if (!logModel) return
        // Default filename based on source file
        var baseName = logModel.filePath.split('/').pop().split('\\').pop()
        var extension = exportFormat === 0 ? ".csv" : ".html"
        var suffix = bookmarksOnly ? "_bookmarks" : ""
        saveDialog.currentFile = baseName + suffix + extension
        saveDialog.open()
    }
    
    onOpened: {
        // Reset to defaults
        csvRadio.checked = true
        allLinesRadio.checked = true
        bookmarksOnlyCheck.checked = false
        sanitizationCheck.checked = false
        sanitizationLevelCombo.currentIndex = 1  // Standard
        startLineInput.text = "1"
        endLineInput.text = logModel ? logModel.totalLineCount().toString() : "0"
    }
}
