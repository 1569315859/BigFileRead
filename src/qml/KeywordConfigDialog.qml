import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

/**
 * 关键字配置对话框
 * 用于添加、编辑、删除关键字及其颜色
 */
Dialog {
    id: keywordConfigDialog
    title: qsTr("Keyword Color Configuration")
    modal: true
    width: 550
    height: 500
    anchors.centerIn: parent
    
    // 主题颜色
    property color bgColor: _themeManager.backgroundColor
    property color panelColor: _themeManager.panelBackground
    property color textColor: _themeManager.textColor
    property color accentColor: _themeManager.accentColor
    property color borderColor: _themeManager.borderColor
    
    // 当前编辑的关键字
    property string editingKeyword: ""
    property string editingColor: "#FFFFFF"
    
    background: Rectangle {
        color: panelColor
        border.color: borderColor
        radius: 8
    }
    
    header: Rectangle {
        height: 45
        color: bgColor
        radius: 8
        
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: parent.radius
            color: parent.color
        }
        
        Text {
            text: keywordConfigDialog.title
            color: textColor
            font.pixelSize: 16
            font.bold: true
            anchors.centerIn: parent
        }
        
        Button {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: "✕"
            flat: true
            onClicked: keywordConfigDialog.close()
            contentItem: Text {
                text: parent.text
                color: parent.hovered ? "#FF6B6B" : textColor
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle { color: "transparent" }
        }
    }
    
    contentItem: ColumnLayout {
        spacing: 10
        
        // 添加新关键字区域
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            color: Qt.darker(panelColor, 1.05)
            radius: 6
            border.color: borderColor
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8
                
                Text {
                    text: qsTr("Add New Keyword")
                    color: textColor
                    font.pixelSize: 13
                    font.bold: true
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    
                    TextField {
                        id: newKeywordInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        placeholderText: qsTr("Enter keyword...")
                        color: textColor
                        font.pixelSize: 13
                        background: Rectangle {
                            color: bgColor
                            border.color: newKeywordInput.activeFocus ? accentColor : borderColor
                            radius: 4
                        }
                    }
                    
                    Rectangle {
                        id: colorPreview
                        width: 32
                        height: 32
                        radius: 4
                        color: editingColor
                        border.color: borderColor
                        border.width: 1
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: colorDialog.open()
                        }
                        
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Click to choose color")
                        
                        property bool hovered: false
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: parent.hovered = true
                            onExited: parent.hovered = false
                            onClicked: colorDialog.open()
                        }
                    }
                    
                    Button {
                        text: qsTr("Add")
                        implicitHeight: 32
                        implicitWidth: 60
                        enabled: newKeywordInput.text.trim().length > 0
                        onClicked: {
                            _keywordConfig.setKeyword(newKeywordInput.text.trim(), editingColor)
                            newKeywordInput.text = ""
                            editingColor = "#FFFFFF"
                        }
                        background: Rectangle {
                            color: parent.enabled ? (parent.down ? Qt.darker(accentColor, 1.2) : accentColor) : Qt.darker(panelColor, 1.1)
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? "white" : Qt.darker(textColor, 1.5)
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
        
        // 关键字列表
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: bgColor
            radius: 6
            border.color: borderColor
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0
                
                // 列表头
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 35
                    color: Qt.darker(panelColor, 1.05)
                    radius: 6
                    
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: parent.radius
                        color: parent.color
                    }
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 15
                        anchors.rightMargin: 15
                        spacing: 8
                        
                        Text {
                            text: qsTr("Keyword")
                            color: textColor
                            font.pixelSize: 12
                            font.bold: true
                            Layout.preferredWidth: 100
                        }
                        
                        Text {
                            text: qsTr("Color")
                            color: textColor
                            font.pixelSize: 12
                            font.bold: true
                            Layout.preferredWidth: 70
                        }
                        
                        Text {
                            text: qsTr("Preview")
                            color: textColor
                            font.pixelSize: 12
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        
                        Text {
                            text: qsTr("Actions")
                            color: textColor
                            font.pixelSize: 12
                            font.bold: true
                            Layout.preferredWidth: 60
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
                
                // 关键字列表
                ListView {
                    id: keywordListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: _keywordConfig.keywordList
                    
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    
                    delegate: Rectangle {
                        width: keywordListView.width
                        height: 40
                        color: index % 2 === 0 ? "transparent" : Qt.rgba(borderColor.r, borderColor.g, borderColor.b, 0.3)
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 15
                            anchors.rightMargin: 15
                            spacing: 8
                            
                            // 关键字
                            Text {
                                text: modelData.keyword
                                color: textColor
                                font.pixelSize: 13
                                font.family: "Consolas"
                                Layout.preferredWidth: 100
                                elide: Text.ElideRight
                            }
                            
                            // 颜色块和代码
                            RowLayout {
                                Layout.preferredWidth: 70
                                spacing: 4
                                
                                Rectangle {
                                    width: 20
                                    height: 20
                                    radius: 3
                                    color: modelData.color
                                    border.color: borderColor
                                }
                                
                                Text {
                                    text: modelData.color
                                    color: Qt.darker(textColor, 1.3)
                                    font.pixelSize: 9
                                    font.family: "Consolas"
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }
                            
                            // 预览
                            Text {
                                text: "[" + modelData.keyword + "] Sample log..."
                                color: modelData.color
                                font.pixelSize: 12
                                font.family: "Consolas"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            
                            // 操作按钮
                            RowLayout {
                                Layout.preferredWidth: 60
                                Layout.alignment: Qt.AlignRight
                                spacing: 2
                                
                                Button {
                                    text: "🎨"
                                    flat: true
                                    implicitWidth: 28
                                    implicitHeight: 28
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Edit Color")
                                    onClicked: {
                                        editingKeyword = modelData.keyword
                                        editingColor = modelData.color
                                        editColorDialog.open()
                                    }
                                    background: Rectangle {
                                        color: parent.hovered ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2) : "transparent"
                                        radius: 4
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        font.pixelSize: 14
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                                
                                Button {
                                    text: "🗑"
                                    flat: true
                                    implicitWidth: 28
                                    implicitHeight: 28
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Delete")
                                    onClicked: _keywordConfig.removeKeyword(modelData.keyword)
                                    background: Rectangle {
                                        color: parent.hovered ? Qt.rgba(1, 0, 0, 0.2) : "transparent"
                                        radius: 4
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        font.pixelSize: 14
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // 底部按钮
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            
            Button {
                text: qsTr("Reset to Defaults")
                onClicked: confirmResetDialog.open()
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
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Close")
                onClicked: keywordConfigDialog.close()
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
        }
    }
    
    // 颜色选择对话框 - 添加新关键字
    ColorDialog {
        id: colorDialog
        title: qsTr("Choose Color")
        onAccepted: editingColor = selectedColor
    }
    
    // 颜色选择对话框 - 编辑现有关键字
    ColorDialog {
        id: editColorDialog
        title: qsTr("Edit Color for") + " " + editingKeyword
        onAccepted: {
            _keywordConfig.setKeyword(editingKeyword, selectedColor)
            editingKeyword = ""
        }
    }
    
    // 确认重置对话框
    Dialog {
        id: confirmResetDialog
        title: qsTr("Confirm Reset")
        modal: true
        anchors.centerIn: parent
        
        background: Rectangle {
            color: panelColor
            border.color: borderColor
            radius: 8
        }
        
        contentItem: ColumnLayout {
            spacing: 20
            
            Text {
                text: qsTr("Are you sure you want to reset all keywords to defaults?\nThis action cannot be undone.")
                color: textColor
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                
                Item { Layout.fillWidth: true }
                
                Button {
                    text: qsTr("Cancel")
                    onClicked: confirmResetDialog.close()
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
                    text: qsTr("Reset")
                    onClicked: {
                        _keywordConfig.resetToDefaults()
                        confirmResetDialog.close()
                    }
                    background: Rectangle {
                        color: parent.down ? "#CC0000" : "#FF4444"
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
            }
        }
    }
}
