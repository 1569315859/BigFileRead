import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @brief 滚动日志合并对话框
 * 
 * 检测并合并相关的滚动日志文件（如 app.log.1, app.log.2 等）
 */
Dialog {
    id: root
    
    title: qsTr("合并滚动日志")
    width: 550
    height: 450
    modal: true
    
    // 检测到的日志文件
    property var detectedFiles: []
    // 选中的文件
    property var selectedFiles: []
    
    signal filesLoaded()
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        
        Label {
            text: qsTr("检测到以下相关的滚动日志文件：")
            font.bold: true
        }
        
        // 文件列表
        ListView {
            id: fileList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            model: root.detectedFiles
            
            delegate: ItemDelegate {
                width: fileList.width
                height: 50
                
                required property string modelData
                required property int index
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 12
                    
                    CheckBox {
                        id: fileCheckbox
                        checked: root.selectedFiles.indexOf(modelData) >= 0
                        onCheckedChanged: {
                            var idx = root.selectedFiles.indexOf(modelData)
                            if (checked && idx < 0) {
                                root.selectedFiles.push(modelData)
                            } else if (!checked && idx >= 0) {
                                root.selectedFiles.splice(idx, 1)
                            }
                            root.selectedFilesChanged()
                        }
                    }
                    
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        
                        Label {
                            text: getFileName(modelData)
                            font.bold: index === root.detectedFiles.length - 1
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        
                        Label {
                            text: getFileInfo(modelData)
                            font.pixelSize: 11
                            opacity: 0.7
                        }
                    }
                    
                    Label {
                        visible: index === root.detectedFiles.length - 1
                        text: qsTr("当前")
                        font.pixelSize: 10
                        padding: 4
                        color: "white"
                        background: Rectangle {
                            color: "#4CAF50"
                            radius: 3
                        }
                    }
                }
            }
            
            // 空列表提示
            Label {
                anchors.centerIn: parent
                visible: fileList.count === 0
                text: qsTr("未检测到滚动日志文件")
                opacity: 0.5
            }
        }
        
        // 选择控制
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            
            Button {
                text: qsTr("全选")
                onClicked: {
                    root.selectedFiles = root.detectedFiles.slice()
                    root.selectedFilesChanged()
                }
            }
            
            Button {
                text: qsTr("取消全选")
                onClicked: {
                    root.selectedFiles = []
                    root.selectedFilesChanged()
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Label {
                text: qsTr("已选择 %1 个文件").arg(root.selectedFiles.length)
                opacity: 0.7
            }
        }
        
        // 合并说明
        GroupBox {
            title: qsTr("合并说明")
            Layout.fillWidth: true
            
            Label {
                anchors.fill: parent
                text: qsTr("滚动日志文件将按时间顺序合并为一个临时文件。\n" +
                          "较旧的日志（编号较大）会放在前面，\n" +
                          "最新的日志会放在最后。")
                wrapMode: Text.Wrap
                font.pixelSize: 11
                opacity: 0.8
            }
        }
        
        // 底部按钮
        DialogButtonBox {
            Layout.fillWidth: true
            
            Button {
                text: qsTr("合并并加载")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                enabled: root.selectedFiles.length > 0
            }
            
            Button {
                text: qsTr("取消")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
    }
    
    function getFileName(path) {
        var parts = path.replace(/\\/g, "/").split("/")
        return parts[parts.length - 1]
    }
    
    function getFileInfo(path) {
        // 这里可以通过 C++ 获取更详细的文件信息
        return path
    }
    
    function detectFiles(basePath) {
        if (_logModel) {
            root.detectedFiles = _logModel.detectRollingLogs(basePath)
            // 默认选择所有文件
            root.selectedFiles = root.detectedFiles.slice()
        }
    }
    
    onAccepted: {
        if (_logModel && root.selectedFiles.length > 0) {
            _logModel.loadRollingLogs(root.selectedFiles)
            root.filesLoaded()
        }
    }
    
    onOpened: {
        // 自动检测当前文件的相关滚动日志
        if (_logModel && _logModel.filePath) {
            detectFiles(_logModel.filePath)
        }
    }
}
