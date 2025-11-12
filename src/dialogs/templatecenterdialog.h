#ifndef TEMPLATECENTERDIALOG_H
#define TEMPLATECENTERDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QHash>
#include <QPixmap>

class TemplateCenterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TemplateCenterDialog(QWidget *parent = nullptr);
    ~TemplateCenterDialog() override = default;

    // 获取用户选择的信息
    bool isTemplateSelected() const { return m_templateSelected; }
    QString selectedTemplatePath() const { return m_selectedTemplatePath; }
    QString labelName() const;
    int labelWidth() const;
    int labelHeight() const;

private slots:
    void onCategoryChanged(int row);
    void onTemplateSelected(QListWidgetItem *item);
    void onTemplateDoubleClicked(QListWidgetItem *item);
    void onSearchTextChanged(const QString &text);
    void updateCreateButtonState();
    void onImportTemplates();
    void onOpenTemplatesFolder();
    void onDeleteTemplates();
    void updateMyTemplateControlsState();

private:
    void setupUI();
    void loadTemplates();
    void loadMyTemplates();
    void loadCategoryTemplates(const QString &category);
    void filterTemplates(const QString &searchText);
    QString ensureUniqueFilePath(const QString &dir, const QString &fileName) const;
    QPixmap getTemplateThumbnail(const QString &filePath,
                                 const QString &displayName,
                                 const QString &sizeInfo) const;
    QPixmap renderTemplateThumbnailFromFile(const QString &filePath,
                                            const QSize &targetSize = QSize(150, 150)) const;

    // UI组件
    QLineEdit *m_searchEdit;
    QListWidget *m_categoryList;
    QStackedWidget *m_contentStack;
    
    // 新建空白页面
    QWidget *m_blankPage;
    QLineEdit *m_nameEdit;
    QSpinBox *m_widthSpin;
    QSpinBox *m_heightSpin;
    
    // 模板选择页面
    QWidget *m_templatePage;
    QListWidget *m_templateList;
    QWidget *m_templateControlsBar; // 仅在“我的模板”页显示的工具条（导入/打开文件夹）
    QPushButton *m_importButton;
    QPushButton *m_deleteButton;
    QPushButton *m_openFolderButton;
    
    QDialogButtonBox *m_buttonBox;
    QPushButton *m_createButton;
    
    // 状态变量
    bool m_templateSelected;
    QString m_selectedTemplatePath;
    
    // 模板目录路径
    QString m_myTemplatesPath;
    QString m_templatesBasePath;
    
    // 存储所有模板项用于搜索过滤
    QList<QListWidgetItem*> m_allTemplateItems;
    
    // 当前选中的分类
    QString m_currentCategory;

    // 缩略图缓存（filePath -> pixmap）
    mutable QHash<QString, QPixmap> m_thumbnailCache;
};

#endif // TEMPLATECENTERDIALOG_H
