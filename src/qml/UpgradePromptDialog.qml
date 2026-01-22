/**
 * @file UpgradePromptDialog.qml
 * @brief 升级提示对话框
 * @details 当用户尝试使用未授权功能时显示，引导用户购买升级
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: upgradeDialog
    
    // ========================================================================
    // 属性
    // ========================================================================
    
    /** @brief 功能名称 */
    property string featureTitle: ""
    
    /** @brief 功能描述 */
    property string featureDescription: ""
    
    /** @brief 所需授权等级名称 */
    property string requiredTierName: "Pro"
    
    /** @brief 价格 */
    property string price: "¥39"
    
    /** @brief 购买链接 */
    property string purchaseUrl: "https://bigfileviewer.com/purchase"
    
    // ========================================================================
    // 对话框配置
    // ========================================================================
    
    title: qsTr("功能需要升级")
    modal: true
    
    width: Math.min(450, parent ? parent.width * 0.9 : 450)
    height: contentColumn.implicitHeight + 120
    
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    
    // ========================================================================
    // 背景
    // ========================================================================
    
    background: Rectangle {
        color: _themeManager.panelBackground
        border.color: _themeManager.accentColor
        border.width: 2
        radius: 8
    }
    
    // ========================================================================
    // 头部
    // ========================================================================
    
    header: Rectangle {
        height: 50
        color: _themeManager.accentColor
        radius: 8
        
        // 底部圆角遮罩
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 10
            color: parent.color
        }
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10
            
            // 锁图标
            Text {
                text: "🔒"
                font.pixelSize: 24
            }
            
            Text {
                text: qsTr("功能需要升级")
                color: "white"
                font.pixelSize: 16
                font.bold: true
                Layout.fillWidth: true
            }
            
            // 关闭按钮
            Button {
                text: "×"
                flat: true
                implicitWidth: 30
                implicitHeight: 30
                
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                background: Rectangle {
                    color: parent.hovered ? Qt.rgba(1, 1, 1, 0.2) : "transparent"
                    radius: 4
                }
                
                onClicked: upgradeDialog.close()
            }
        }
    }
    
    // ========================================================================
    // 内容
    // ========================================================================
    
    contentItem: ColumnLayout {
        id: contentColumn
        spacing: 16
        
        // 功能图标和标题
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            
            // 功能图标
            Rectangle {
                width: 48
                height: 48
                radius: 8
                color: _themeManager.accentColor
                opacity: 0.2
                
                Text {
                    anchors.centerIn: parent
                    text: "✨"
                    font.pixelSize: 24
                }
            }
            
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                
                Text {
                    text: featureTitle
                    color: _themeManager.textColor
                    font.pixelSize: 18
                    font.bold: true
                }
                
                Text {
                    text: qsTr("需要 %1 版本").arg(requiredTierName)
                    color: _themeManager.accentColor
                    font.pixelSize: 13
                }
            }
        }
        
        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: _themeManager.borderColor
        }
        
        // 功能描述
        Text {
            Layout.fillWidth: true
            text: featureDescription
            color: _themeManager.textColor
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            lineHeight: 1.4
        }
        
        // 价格和版本信息卡片
        Rectangle {
            Layout.fillWidth: true
            height: 70
            radius: 8
            color: Qt.rgba(_themeManager.accentColor.r, 
                          _themeManager.accentColor.g, 
                          _themeManager.accentColor.b, 0.1)
            border.color: _themeManager.accentColor
            border.width: 1
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    
                    Text {
                        text: requiredTierName + qsTr(" 版")
                        color: _themeManager.textColor
                        font.pixelSize: 16
                        font.bold: true
                    }
                    
                    Text {
                        text: requiredTierName === "Pro" 
                              ? qsTr("解锁高级分析功能")
                              : qsTr("解锁全部功能 + 远程支持")
                        color: _themeManager.textColor
                        opacity: 0.7
                        font.pixelSize: 12
                    }
                }
                
                Text {
                    text: price
                    color: _themeManager.accentColor
                    font.pixelSize: 24
                    font.bold: true
                }
            }
        }
        
        // 功能对比提示
        Text {
            Layout.fillWidth: true
            text: qsTr("升级后可永久使用，包含免费更新和技术支持。")
            color: _themeManager.textColor
            opacity: 0.6
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
        }
    }
    
    // ========================================================================
    // 底部按钮
    // ========================================================================
    
    footer: DialogButtonBox {
        background: Rectangle {
            color: _themeManager.panelBackground
            radius: 8
            
            // 顶部圆角遮罩
            Rectangle {
                width: parent.width
                height: 10
                color: parent.color
            }
        }
        
        Button {
            text: qsTr("稍后再说")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            
            contentItem: Text {
                text: parent.text
                color: _themeManager.textColor
                opacity: 0.7
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            
            background: Rectangle {
                implicitWidth: 100
                implicitHeight: 36
                color: parent.hovered ? Qt.rgba(_themeManager.textColor.r,
                                                _themeManager.textColor.g,
                                                _themeManager.textColor.b, 0.1) : "transparent"
                radius: 6
            }
        }
        
        Button {
            text: qsTr("立即升级")
            highlighted: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            
            contentItem: Text {
                text: parent.text
                color: "white"
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            
            background: Rectangle {
                implicitWidth: 120
                implicitHeight: 36
                color: parent.hovered ? Qt.darker(_themeManager.accentColor, 1.1) 
                                      : _themeManager.accentColor
                radius: 6
            }
            
            onClicked: {
                Qt.openUrlExternally(purchaseUrl)
            }
        }
    }
    
    // ========================================================================
    // 公共方法
    // ========================================================================
    
    /**
     * @brief 显示升级提示
     * @param featureInfo 功能信息对象 {title, description, requiredTierName, price, purchaseUrl}
     */
    function showForFeature(featureInfo) {
        featureTitle = featureInfo.title || ""
        featureDescription = featureInfo.description || ""
        requiredTierName = featureInfo.requiredTierName || "Pro"
        price = featureInfo.price || "¥39"
        purchaseUrl = featureInfo.purchaseUrl || "https://bigfileviewer.com/purchase"
        
        open()
    }
}
