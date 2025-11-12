#ifndef INLINETEXTEDITOR_H
#define INLINETEXTEDITOR_H

#include <QObject>
#include <QGraphicsProxyWidget>
#include <QTextEdit>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QFocusEvent>
#include "../graphics/textitem.h"

// 前向声明
class CustomTextEdit;

/**
 * @brief 内联文本编辑器
 * 
 * 在画布上提供原地文本编辑功能，当用户双击文本项时激活。
 * 编辑完成后自动更新文本项内容并退出编辑模式。
 */
class InlineTextEditor : public QObject
{
    Q_OBJECT

public:
    explicit InlineTextEditor(QObject *parent = nullptr);
    ~InlineTextEditor();

    // 开始编辑指定的文本项
    void startEditing(TextItem* textItem, QGraphicsScene* scene);
    
    // 完成编辑
    void finishEditing();
    
    // 取消编辑
    void cancelEditing();

    // 检查是否正在编辑
    bool isEditing() const;

    // 获取当前编辑的文本项
    TextItem* currentTextItem() const;

signals:
    // 编辑开始信号
    void editingStarted(TextItem* textItem);
    
    // 编辑完成信号（文本已更新）
    void editingFinished(TextItem* textItem, const QString& newText);
    
    // 编辑取消信号（文本未更改）
    void editingCancelled(TextItem* textItem);

protected:
    // 事件过滤器，用于捕获关键事件
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // 文本编辑器失去焦点时的处理
    void onTextEditFocusOut();

private:
    // 私有成员变量
    QGraphicsProxyWidget* m_proxyWidget;   // 代理小部件
    CustomTextEdit* m_textEdit;            // 文本编辑器
    TextItem* m_currentTextItem;           // 当前编辑的文本项
    QGraphicsScene* m_scene;               // 当前场景
    QString m_originalText;                // 原始文本（用于取消编辑）
    bool m_isEditing;                      // 是否正在编辑

    // 私有方法
    void setupTextEdit();                  // 设置文本编辑器
    void positionTextEdit();               // 定位文本编辑器
    void applyTextStyle();                 // 应用文本样式
    void cleanupEditor();                  // 清理编辑器资源
};

/**
 * @brief 自定义文本编辑器类
 * 
 * 继承自QTextEdit，用于处理特殊的键盘事件和焦点事件。
 */
class CustomTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit CustomTextEdit(QWidget *parent = nullptr);

signals:
    // 请求完成编辑
    void editingFinished();
    
    // 请求取消编辑
    void editingCancelled();

protected:
    // 重写键盘事件处理
    void keyPressEvent(QKeyEvent *event) override;
    
    // 重写焦点事件处理
    void focusOutEvent(QFocusEvent *event) override;
};

#endif // INLINETEXTEDITOR_H
