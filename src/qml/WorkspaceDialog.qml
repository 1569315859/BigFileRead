import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

/**
 * @brief 工作区管理对话框
 * 
 * 提供保存、加载、管理工作区的功能
 */
Dialog {
    id: root
    
    title: qsTr("工作区管理")
    width: 650
    height: 550
    modal: true
    
    // 当前状态（用于保存）
    property var currentState: ({})
    
    // 选中的工作区
    property var selectedWorkspace: null
    
    // 加载工作区信号
    signal workspaceLoaded(var workspace)
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        
        // 工具栏
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Button {
                text: qsTr("保存当前状态")
                icon.name: "document-save"
                onClicked: saveDialog.open()
            }
            
            Button {
                text: qsTr("快速保存")
                icon.name: "document-save-as"
                enabled: workspaceManager && workspaceManager.currentWorkspace.length > 0
                onClicked: {
                    if (workspaceManager) {
                        workspaceManager.setCurrentState(root.currentState)
                        workspaceManager.quickSave()
                    }
                }
                
                ToolTip.text: qsTr("更新当前工作区: %1").arg(
                    workspaceManager ? workspaceManager.currentWorkspace : ""
                )
                ToolTip.visible: hovered
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("导入")
                icon.name: "document-import"
                onClicked: importFileDialog.open()
            }
            
            Button {
                text: qsTr("导出")
                icon.name: "document-export"
                enabled: root.selectedWorkspace !== null
                onClicked: exportFileDialog.open()
            }
        }
        
        // 当前工作区指示
        RowLayout {
            Layout.fillWidth: true
            visible: workspaceManager && workspaceManager.currentWorkspace.length > 0
            
            Label {
                text: qsTr("当前工作区:")
                font.bold: true
            }
            Label {
                text: workspaceManager ? workspaceManager.currentWorkspace : ""
                color: "#2196F3"
            }
        }
        
        // 工作区列表
        GroupBox {
            title: qsTr("已保存的工作区 (%1)").arg(
                workspaceManager ? workspaceManager.workspaceCount : 0
            )
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 8
                
                ListView {
                    id: workspaceList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    
                    model: workspaceManager ? workspaceManager.workspaces : []
                    
                    delegate: ItemDelegate {
                        width: workspaceList.width
                        height: 80
                        
                        required property var modelData
                        required property int index
                        
                        highlighted: root.selectedWorkspace && 
                                     root.selectedWorkspace.name === modelData.name
                        
                        onClicked: {
                            root.selectedWorkspace = modelData
                        }
                        
                        onDoubleClicked: {
                            root.selectedWorkspace = modelData
                            applySelectedWorkspace()
                        }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 12
                            
                            // 工作区图标
                            Rectangle {
                                width: 48
                                height: 48
                                radius: 8
                                color: workspaceManager && 
                                       workspaceManager.currentWorkspace === modelData.name 
                                       ? "#2196F3" : "#607D8B"
                                
                                Label {
                                    anchors.centerIn: parent
                                    text: modelData.name.charAt(0).toUpperCase()
                                    font.pixelSize: 20
                                    font.bold: true
                                    color: "white"
                                }
                            }
                            
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                
                                RowLayout {
                                    Label {
                                        text: modelData.name
                                        font.bold: true
                                        font.pixelSize: 14
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    
                                    Label {
                                        visible: workspaceManager && 
                                                 workspaceManager.currentWorkspace === modelData.name
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
                                
                                Label {
                                    text: modelData.description || modelData.filePath || qsTr("无描述")
                                    font.pixelSize: 12
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                
                                RowLayout {
                                    spacing: 16
                                    
                                    Label {
                                        text: qsTr("创建: %1").arg(formatDate(modelData.createdAt))
                                        font.pixelSize: 10
                                        opacity: 0.5
                                    }
                                    
                                    Label {
                                        text: qsTr("最后使用: %1").arg(formatDate(modelData.lastUsed))
                                        font.pixelSize: 10
                                        opacity: 0.5
                                    }
                                }
                            }
                            
                            // 操作按钮
                            ColumnLayout {
                                spacing: 4
                                
                                ToolButton {
                                    icon.name: "edit-rename"
                                    ToolTip.text: qsTr("重命名")
                                    ToolTip.visible: hovered
                                    onClicked: {
                                        root.selectedWorkspace = modelData
                                        renameDialog.open()
                                    }
                                }
                                
                                ToolButton {
                                    icon.name: "edit-delete"
                                    ToolTip.text: qsTr("删除")
                                    ToolTip.visible: hovered
                                    onClicked: {
                                        root.selectedWorkspace = modelData
                                        deleteConfirmDialog.open()
                                    }
                                }
                            }
                        }
                    }
                    
                    // 空列表提示
                    Label {
                        anchors.centerIn: parent
                        visible: workspaceList.count === 0
                        text: qsTr("没有保存的工作区\n使用「保存当前状态」创建工作区")
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.5
                    }
                }
            }
        }
        
        // 工作区详情
        GroupBox {
            title: qsTr("工作区详情")
            Layout.fillWidth: true
            visible: root.selectedWorkspace !== null
            
            GridLayout {
                anchors.fill: parent
                columns: 4
                columnSpacing: 16
                rowSpacing: 8
                
                Label { text: qsTr("文件:"); font.bold: true }
                Label { 
                    text: root.selectedWorkspace ? root.selectedWorkspace.filePath || qsTr("无") : ""
                    Layout.columnSpan: 3
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
                
                Label { text: qsTr("过滤:"); font.bold: true }
                Label { 
                    text: root.selectedWorkspace && root.selectedWorkspace.filterKeyword 
                          ? root.selectedWorkspace.filterKeyword : qsTr("无")
                }
                
                Label { text: qsTr("书签:"); font.bold: true }
                Label { 
                    text: root.selectedWorkspace && root.selectedWorkspace.bookmarks 
                          ? root.selectedWorkspace.bookmarks.length + qsTr(" 个")
                          : "0" + qsTr(" 个")
                }
            }
        }
        
        // 底部按钮
        DialogButtonBox {
            Layout.fillWidth: true
            
            Button {
                text: qsTr("加载")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                enabled: root.selectedWorkspace !== null
            }
            
            Button {
                text: qsTr("关闭")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
    }
    
    function formatDate(isoString) {
        if (!isoString) return qsTr("未知")
        var date = new Date(isoString)
        return date.toLocaleDateString()
    }
    
    function applySelectedWorkspace() {
        if (root.selectedWorkspace && workspaceManager) {
            var workspace = workspaceManager.loadWorkspace(root.selectedWorkspace.name)
            root.workspaceLoaded(workspace)
            root.close()
        }
    }
    
    onAccepted: applySelectedWorkspace()
    
    onOpened: {
        root.selectedWorkspace = null
    }
    
    // 保存工作区对话框
    Dialog {
        id: saveDialog
        title: qsTr("保存工作区")
        width: 400
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            
            Label { text: qsTr("工作区名称:") }
            
            TextField {
                id: newWorkspaceName
                Layout.fillWidth: true
                placeholderText: qsTr("输入工作区名称")
            }
            
            Label { text: qsTr("描述 (可选):") }
            
            TextArea {
                id: newWorkspaceDesc
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                placeholderText: qsTr("输入工作区描述")
            }
            
            Label {
                text: qsTr("将保存当前打开的文件、过滤条件、书签和视图设置。")
                font.pixelSize: 11
                opacity: 0.7
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            
            DialogButtonBox {
                Layout.fillWidth: true
                
                Button {
                    text: qsTr("保存")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                    enabled: newWorkspaceName.text.length > 0
                }
                
                Button {
                    text: qsTr("取消")
                    DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                }
            }
        }
        
        onAccepted: {
            if (workspaceManager) {
                workspaceManager.setCurrentState(root.currentState)
                workspaceManager.saveWorkspace(
                    newWorkspaceName.text,
                    newWorkspaceDesc.text
                )
                newWorkspaceName.text = ""
                newWorkspaceDesc.text = ""
            }
        }
        
        onOpened: {
            newWorkspaceName.text = ""
            newWorkspaceDesc.text = ""
            newWorkspaceName.forceActiveFocus()
        }
    }
    
    // 重命名对话框
    Dialog {
        id: renameDialog
        title: qsTr("重命名工作区")
        width: 350
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            
            Label { text: qsTr("新名称:") }
            
            TextField {
                id: renameField
                Layout.fillWidth: true
                text: root.selectedWorkspace ? root.selectedWorkspace.name : ""
            }
            
            DialogButtonBox {
                Layout.fillWidth: true
                
                Button {
                    text: qsTr("重命名")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                    enabled: renameField.text.length > 0
                }
                
                Button {
                    text: qsTr("取消")
                    DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                }
            }
        }
        
        onAccepted: {
            if (workspaceManager && root.selectedWorkspace) {
                workspaceManager.renameWorkspace(
                    root.selectedWorkspace.name,
                    renameField.text
                )
            }
        }
        
        onOpened: {
            renameField.text = root.selectedWorkspace ? root.selectedWorkspace.name : ""
            renameField.selectAll()
            renameField.forceActiveFocus()
        }
    }
    
    // 删除确认对话框
    Dialog {
        id: deleteConfirmDialog
        title: qsTr("删除工作区")
        width: 350
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            
            Label {
                text: qsTr("确定要删除工作区「%1」吗？").arg(
                    root.selectedWorkspace ? root.selectedWorkspace.name : ""
                )
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            
            DialogButtonBox {
                Layout.fillWidth: true
                
                Button {
                    text: qsTr("删除")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                }
                
                Button {
                    text: qsTr("取消")
                    DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                }
            }
        }
        
        onAccepted: {
            if (workspaceManager && root.selectedWorkspace) {
                workspaceManager.deleteWorkspace(root.selectedWorkspace.name)
                root.selectedWorkspace = null
            }
        }
    }
    
    // 导入文件对话框
    FileDialog {
        id: importFileDialog
        title: qsTr("导入工作区")
        nameFilters: [qsTr("JSON 文件 (*.json)")]
        fileMode: FileDialog.OpenFile
        
        onAccepted: {
            if (workspaceManager) {
                var path = selectedFile.toString().replace("file:///", "")
                workspaceManager.importWorkspace(path, false)
            }
        }
    }
    
    // 导出文件对话框
    FileDialog {
        id: exportFileDialog
        title: qsTr("导出工作区")
        nameFilters: [qsTr("JSON 文件 (*.json)")]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        
        onAccepted: {
            if (workspaceManager && root.selectedWorkspace) {
                var path = selectedFile.toString().replace("file:///", "")
                workspaceManager.exportWorkspace(root.selectedWorkspace.name, path)
            }
        }
    }
}
