import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    modal: true
    width: 520
    height: 480
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape
    padding: 20

    property color textColor: _themeManager.textColor
    property color panelColor: _themeManager.panelBackground
    property color bgColor: _themeManager.backgroundColor
    property color accentColor: _themeManager.accentColor
    property color borderColor: _themeManager.borderColor
    
    // 支持动态绑定不同的模型，默认为全局 _logModel
    property var targetLogModel: _logModel

    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 6
    }

    contentItem: ColumnLayout {
        spacing: 16

        // Title
        Text {
            text: qsTr("Advanced Filter")
            font.bold: true
            font.pixelSize: 18
            color: textColor
            Layout.alignment: Qt.AlignHCenter
        }

        // ========== 时间区间 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: enableTimeFilter.checked ? 90 : 50
            color: Qt.darker(panelColor, 1.05)
            border.color: borderColor
            radius: 4
            
            Behavior on Layout.preferredHeight { NumberAnimation { duration: 150 } }
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8
                
                CheckBox {
                    id: enableTimeFilter
                    text: qsTr("Enable Time Range Filter")
                    spacing: 6
                    indicator: Rectangle {
                        implicitWidth: 16
                        implicitHeight: 16
                        x: 0
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        
                        Text {
                            text: "✓"
                            color: accentColor
                            anchors.centerIn: parent
                            font.pixelSize: 12
                            font.bold: true
                            visible: enableTimeFilter.checked
                        }
                    }
                    contentItem: Text { 
                        text: enableTimeFilter.text
                        color: textColor
                        font.pixelSize: 12
                        font.bold: true
                        leftPadding: enableTimeFilter.indicator.width + enableTimeFilter.spacing
                        verticalAlignment: Text.AlignVCenter 
                    }
                }
                
                RowLayout {
                    spacing: 10
                    visible: enableTimeFilter.checked
                    opacity: enableTimeFilter.checked ? 1.0 : 0.5
                    
                    Label {
                        text: qsTr("From:")
                        color: textColor
                        font.pixelSize: 12
                    }
                    
                    TextField {
                        id: startTimeInput
                        Layout.preferredWidth: 150
                        implicitHeight: 28
                        placeholderText: "YYYY-MM-DD HH:mm:ss"
                        font.pixelSize: 11
                        color: textColor
                        background: Rectangle { 
                            color: bgColor
                            border.color: borderColor
                            radius: 4 
                        }
                    }
                    
                    Label {
                        text: qsTr("To:")
                        color: textColor
                        font.pixelSize: 12
                    }
                    
                    TextField {
                        id: endTimeInput
                        Layout.preferredWidth: 150
                        implicitHeight: 28
                        placeholderText: "YYYY-MM-DD HH:mm:ss"
                        font.pixelSize: 11
                        color: textColor
                        background: Rectangle { 
                            color: bgColor
                            border.color: borderColor
                            radius: 4 
                        }
                    }
                }
            }
        }

        // ========== Log Level Section ==========
        ColumnLayout {
            spacing: 8
            Layout.fillWidth: true
            
            Label {
                text: qsTr("Log Level:")
                color: textColor
                font.pixelSize: 13
                font.bold: true
            }
            
            Flow {
                Layout.fillWidth: true
                spacing: 12
                
                CheckBox {
                    id: levelAll
                    text: "All"
                    checked: true
                    onCheckedChanged: {
                        if (checked) {
                            levelInfo.checked = false
                            levelDebug.checked = false
                            levelWarn.checked = false
                            levelError.checked = false
                            levelFatal.checked = false
                        }
                    }
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 14
                        implicitHeight: 14
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        Text { text: "✓"; color: accentColor; anchors.centerIn: parent; font.pixelSize: 10; visible: levelAll.checked }
                    }
                    contentItem: Text { text: levelAll.text; color: textColor; font.pixelSize: 11; leftPadding: 20; verticalAlignment: Text.AlignVCenter }
                }
                
                CheckBox {
                    id: levelInfo
                    text: "INFO"
                    onCheckedChanged: if (checked) levelAll.checked = false
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 14
                        implicitHeight: 14
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        Text { text: "✓"; color: accentColor; anchors.centerIn: parent; font.pixelSize: 10; visible: levelInfo.checked }
                    }
                    contentItem: Text { text: levelInfo.text; color: "#00BFFF"; font.pixelSize: 11; font.bold: true; leftPadding: 20; verticalAlignment: Text.AlignVCenter }
                }
                
                CheckBox {
                    id: levelDebug
                    text: "DEBUG"
                    onCheckedChanged: if (checked) levelAll.checked = false
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 14
                        implicitHeight: 14
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        Text { text: "✓"; color: accentColor; anchors.centerIn: parent; font.pixelSize: 10; visible: levelDebug.checked }
                    }
                    contentItem: Text { text: levelDebug.text; color: "#888888"; font.pixelSize: 11; font.bold: true; leftPadding: 20; verticalAlignment: Text.AlignVCenter }
                }
                
                CheckBox {
                    id: levelWarn
                    text: "WARN"
                    onCheckedChanged: if (checked) levelAll.checked = false
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 14
                        implicitHeight: 14
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        Text { text: "✓"; color: accentColor; anchors.centerIn: parent; font.pixelSize: 10; visible: levelWarn.checked }
                    }
                    contentItem: Text { text: levelWarn.text; color: "#FFA500"; font.pixelSize: 11; font.bold: true; leftPadding: 20; verticalAlignment: Text.AlignVCenter }
                }
                
                CheckBox {
                    id: levelError
                    text: "ERROR"
                    onCheckedChanged: if (checked) levelAll.checked = false
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 14
                        implicitHeight: 14
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        Text { text: "✓"; color: accentColor; anchors.centerIn: parent; font.pixelSize: 10; visible: levelError.checked }
                    }
                    contentItem: Text { text: levelError.text; color: "#FF4444"; font.pixelSize: 11; font.bold: true; leftPadding: 20; verticalAlignment: Text.AlignVCenter }
                }
                
                CheckBox {
                    id: levelFatal
                    text: "FATAL"
                    onCheckedChanged: if (checked) levelAll.checked = false
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 14
                        implicitHeight: 14
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        Text { text: "✓"; color: accentColor; anchors.centerIn: parent; font.pixelSize: 10; visible: levelFatal.checked }
                    }
                    contentItem: Text { text: levelFatal.text; color: "#FF0000"; font.pixelSize: 11; font.bold: true; leftPadding: 20; verticalAlignment: Text.AlignVCenter }
                }
            }
        }

        // ========== Keywords Section ==========
        ColumnLayout {
            spacing: 6
            Layout.fillWidth: true
            
            Label {
                text: qsTr("Keywords (space or comma separated):")
                color: textColor
                font.pixelSize: 13
                font.bold: true
            }
            TextField {
                id: keywordsInput
                Layout.fillWidth: true
                implicitHeight: 32
                placeholderText: qsTr("e.g. error timeout database")
                font.pixelSize: 12
                color: textColor
                background: Rectangle { 
                    color: bgColor
                    border.color: borderColor
                    radius: 4 
                }
            }
        }

        // ========== Match Logic & Options ==========
        RowLayout {
            spacing: 30
            Layout.fillWidth: true
            
            // Match Logic
            RowLayout {
                spacing: 16
                
                Label {
                    text: qsTr("Logic:")
                    color: textColor
                    font.pixelSize: 12
                    font.bold: true
                }
                
                RadioButton {
                    id: andRadio
                    text: "AND"
                    checked: true
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 14
                        implicitHeight: 14
                        y: (parent.height - height) / 2
                        radius: 7
                        border.color: borderColor
                        color: bgColor
                        Rectangle { width: 6; height: 6; radius: 3; anchors.centerIn: parent; color: accentColor; visible: andRadio.checked }
                    }
                    contentItem: Text { text: andRadio.text; color: textColor; font.pixelSize: 11; leftPadding: 18; verticalAlignment: Text.AlignVCenter }
                }
                
                RadioButton {
                    id: orRadio
                    text: "OR"
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 14
                        implicitHeight: 14
                        y: (parent.height - height) / 2
                        radius: 7
                        border.color: borderColor
                        color: bgColor
                        Rectangle { width: 6; height: 6; radius: 3; anchors.centerIn: parent; color: accentColor; visible: orRadio.checked }
                    }
                    contentItem: Text { text: orRadio.text; color: textColor; font.pixelSize: 11; leftPadding: 18; verticalAlignment: Text.AlignVCenter }
                }
            }
            
            Item { Layout.fillWidth: true }
            
            // Options
            RowLayout {
                spacing: 16
                
                CheckBox {
                    id: regexCheck
                    text: qsTr("Regex")
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 14
                        implicitHeight: 14
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        Text { text: "✓"; color: accentColor; anchors.centerIn: parent; font.pixelSize: 10; visible: regexCheck.checked }
                    }
                    contentItem: Text { text: regexCheck.text; color: textColor; font.pixelSize: 11; leftPadding: 18; verticalAlignment: Text.AlignVCenter }
                }
                
                CheckBox {
                    id: caseSensitiveCheck
                    text: qsTr("Case")
                    spacing: 4
                    indicator: Rectangle {
                        implicitWidth: 14
                        implicitHeight: 14
                        y: (parent.height - height) / 2
                        radius: 3
                        border.color: borderColor
                        color: bgColor
                        Text { text: "✓"; color: accentColor; anchors.centerIn: parent; font.pixelSize: 10; visible: caseSensitiveCheck.checked }
                    }
                    contentItem: Text { text: caseSensitiveCheck.text; color: textColor; font.pixelSize: 11; leftPadding: 18; verticalAlignment: Text.AlignVCenter }
                }
            }
        }
        
        Item { Layout.fillHeight: true }

        // ========== Buttons ==========
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 10
            
            Button {
                text: qsTr("Clear All")
                implicitHeight: 32
                implicitWidth: 90
                onClicked: {
                    enableTimeFilter.checked = false
                    startTimeInput.text = ""
                    endTimeInput.text = ""
                    levelAll.checked = true
                    keywordsInput.text = ""
                    regexCheck.checked = false
                    caseSensitiveCheck.checked = false
                    andRadio.checked = true
                    _logModel.clearFilter()
                    root.close()
                }
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
            
            Button {
                text: qsTr("Apply")
                implicitHeight: 32
                implicitWidth: 90
                onClicked: {
                    // 收集选中的日志级别 (Collect selected log levels)
                    var levels = []
                    if (!levelAll.checked) {
                        if (levelInfo.checked) levels.push("INFO")
                        if (levelDebug.checked) levels.push("DEBUG")
                        if (levelWarn.checked) levels.push("WARN")
                        if (levelError.checked) levels.push("ERROR")
                        if (levelFatal.checked) levels.push("FATAL")
                    }
                    
                    // 解析关键词（支持空格和逗号分隔）
                    var keywordText = keywordsInput.text.trim()
                    var keywords = []
                    if (keywordText) {
                        keywords = keywordText.split(/[\s,]+/).filter(function(s) { return s !== "" })
                    }
                    
                    if (targetLogModel) {
                        targetLogModel.applyAdvancedFilter(levels, keywords, andRadio.checked, regexCheck.checked)
                    } else {
                        console.error("AdvancedFilterDialog: targetLogModel is null!")
                    }
                    root.close()
                }
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
                implicitWidth: 90
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
}
