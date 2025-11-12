#include "inlinetexteditor.h"
#include <QApplication>
#include <QGraphicsView>
#include <QTextDocument>
#include <QTextCursor>
#include <QDebug>

// InlineTextEditor 实现

InlineTextEditor::InlineTextEditor(QObject *parent)
    : QObject(parent)
    , m_proxyWidget(nullptr)
    , m_textEdit(nullptr)
    , m_currentTextItem(nullptr)
    , m_scene(nullptr)
    , m_isEditing(false)
{
}

InlineTextEditor::~InlineTextEditor()
{
    cleanupEditor();
}

void InlineTextEditor::startEditing(TextItem* textItem, QGraphicsScene* scene)
{
    if (!textItem || !scene || m_isEditing) {
        return;
    }

    // 设置当前编辑状态
    m_currentTextItem = textItem;
    m_scene = scene;
    m_originalText = textItem->text();
    m_isEditing = true;

    // 设置文本项为编辑状态
    textItem->setEditing(true);

    // 创建文本编辑器
    setupTextEdit();
    
    // 定位和样式设置
    positionTextEdit();
    applyTextStyle();

    // 设置焦点并选中所有文本
    m_textEdit->setFocus();
    m_textEdit->selectAll();

    emit editingStarted(textItem);
}

void InlineTextEditor::finishEditing()
{
    if (!m_isEditing || !m_currentTextItem) {
        return;
    }

    // 获取编辑后的文本
    QString newText = m_textEdit->toPlainText();
    
    // 更新文本项
    m_currentTextItem->setText(newText);
    m_currentTextItem->setEditing(false);

    // 发送编辑完成信号
    emit editingFinished(m_currentTextItem, newText);

    // 清理
    cleanupEditor();
}

void InlineTextEditor::cancelEditing()
{
    if (!m_isEditing || !m_currentTextItem) {
        return;
    }

    // 恢复原始文本
    m_currentTextItem->setText(m_originalText);
    m_currentTextItem->setEditing(false);

    // 发送编辑取消信号
    emit editingCancelled(m_currentTextItem);

    // 清理
    cleanupEditor();
}

bool InlineTextEditor::isEditing() const
{
    return m_isEditing;
}

TextItem* InlineTextEditor::currentTextItem() const
{
    return m_currentTextItem;
}

bool InlineTextEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_textEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        
        // 处理特殊键
        if (keyEvent->key() == Qt::Key_Escape) {
            cancelEditing();
            return true;
        } else if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // 如果按下的是Ctrl+Enter或Shift+Enter，允许换行
            if (keyEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) {
                return false; // 让QTextEdit处理换行
            } else {
                // 否则完成编辑
                finishEditing();
                return true;
            }
        }
    }
    
    return QObject::eventFilter(obj, event);
}

void InlineTextEditor::onTextEditFocusOut()
{
    // 当文本编辑器失去焦点时，完成编辑
    if (m_isEditing) {
        finishEditing();
    }
}

void InlineTextEditor::setupTextEdit()
{
    // 创建自定义文本编辑器
    m_textEdit = new CustomTextEdit();
    
    // 连接信号
    connect(m_textEdit, &CustomTextEdit::editingFinished, this, &InlineTextEditor::finishEditing);
    connect(m_textEdit, &CustomTextEdit::editingCancelled, this, &InlineTextEditor::cancelEditing);
    
    // 设置文本内容
    m_textEdit->setPlainText(m_originalText);
    
    // 设置文本编辑器属性
    m_textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textEdit->setFrameStyle(QFrame::Box);
    m_textEdit->setLineWidth(1);
    
    // 安装事件过滤器
    m_textEdit->installEventFilter(this);
    
    // 创建代理小部件
    m_proxyWidget = m_scene->addWidget(m_textEdit);
    m_proxyWidget->setZValue(1000); // 确保在所有其他项目之上
}

void InlineTextEditor::positionTextEdit()
{
    if (!m_textEdit || !m_currentTextItem || !m_proxyWidget) {
        return;
    }

    // 获取文本项的位置和尺寸
    QPointF itemPos = m_currentTextItem->pos();
    QSizeF itemSize = m_currentTextItem->size();
    
    // 设置文本编辑器的尺寸
    QSizeF editSize = itemSize;
    editSize.setWidth(qMax(100.0, editSize.width()));
    editSize.setHeight(qMax(30.0, editSize.height()));
    
    m_textEdit->setFixedSize(editSize.toSize());
    
    // 设置代理小部件的位置
    m_proxyWidget->setPos(itemPos);
}

void InlineTextEditor::applyTextStyle()
{
    if (!m_textEdit || !m_currentTextItem) {
        return;
    }

    // 应用字体
    m_textEdit->setFont(m_currentTextItem->font()); // 字距已包含在 TextItem 的字体中
    
    // 设置文本颜色
    QPalette palette = m_textEdit->palette();
    palette.setColor(QPalette::Text, m_currentTextItem->textColor());
    palette.setColor(QPalette::Base, m_currentTextItem->backgroundColor());
    m_textEdit->setPalette(palette);
    
    // 设置对齐方式
    QTextCursor cursor = m_textEdit->textCursor();
    QTextBlockFormat blockFormat = cursor.blockFormat();
    
    Qt::Alignment alignment = m_currentTextItem->alignment();
    if (alignment & Qt::AlignLeft) {
        blockFormat.setAlignment(Qt::AlignLeft);
    } else if (alignment & Qt::AlignRight) {
        blockFormat.setAlignment(Qt::AlignRight);
    } else if (alignment & Qt::AlignHCenter) {
        blockFormat.setAlignment(Qt::AlignHCenter);
    }
    
    cursor.setBlockFormat(blockFormat);
    m_textEdit->setTextCursor(cursor);
    
    // 设置换行模式
    if (m_currentTextItem->wordWrap()) {
        m_textEdit->setWordWrapMode(QTextOption::WordWrap);
    } else {
        m_textEdit->setWordWrapMode(QTextOption::NoWrap);
    }
}

void InlineTextEditor::cleanupEditor()
{
    if (m_proxyWidget) {
        if (m_scene) {
            m_scene->removeItem(m_proxyWidget);
        }
        delete m_proxyWidget;
        m_proxyWidget = nullptr;
    }
    
    m_textEdit = nullptr; // 会被代理小部件自动删除
    m_currentTextItem = nullptr;
    m_scene = nullptr;
    m_originalText.clear();
    m_isEditing = false;
}

// CustomTextEdit 实现

CustomTextEdit::CustomTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
}

void CustomTextEdit::keyPressEvent(QKeyEvent *event)
{
    // 处理特殊键
    if (event->key() == Qt::Key_Escape) {
        emit editingCancelled();
        return;
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // 如果没有修饰键，完成编辑
        if (event->modifiers() == Qt::NoModifier) {
            emit editingFinished();
            return;
        }
    } else if (event->key() == Qt::Key_Tab) {
        // Tab键也完成编辑
        emit editingFinished();
        return;
    }
    
    // 调用基类处理其他键
    QTextEdit::keyPressEvent(event);
}

void CustomTextEdit::focusOutEvent(QFocusEvent *event)
{
    // 失去焦点时完成编辑
    QTextEdit::focusOutEvent(event);
    
    // 延迟发射信号，避免在某些情况下的竞争条件
    QMetaObject::invokeMethod(this, [this]() {
        emit editingFinished();
    }, Qt::QueuedConnection);
}

#include "inlinetexteditor.moc"
