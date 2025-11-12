#ifndef ASSETBROWSERDIALOG_H
#define ASSETBROWSERDIALOG_H

#include <QDialog>
#include <QPixmap>
#include <QVector>
#include <QString>
class QDir;

class QListWidget;
class QListWidgetItem;
class QComboBox;
class QLineEdit;
class QPushButton;
class QUrl;

struct AssetItem {
    QString id;
    QString name;
    QString category; // 国标/警示语/能效/人物
    QPixmap thumbnail; // 预览缩略图（约128x128）
    QPixmap full;      // 全尺寸（建议512x512以内）
};

class AssetBrowserDialog : public QDialog {
    Q_OBJECT
public:
    explicit AssetBrowserDialog(QWidget* parent = nullptr);

    // 选择结果
    bool hasSelection() const;
    AssetItem selected() const;

private slots:
    void onCategoryChanged(int index);
    void onFilterTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);
    void onAccept();

private:
    void buildUi();
    void populateBuiltinAssets(); // 不再默认调用，仅保留以备将来需要
    void populateExternalAssets();
    void updateCategories();
    void refreshList();

    // 生成内置素材
    static QPixmap genWarningIcon(const QString& text);
    static QPixmap genEnergyLabel();
    static QPixmap genGbMark();
    static QPixmap genPersonSilhouette();
    static QPixmap makeThumb(const QPixmap& src);

    QVector<AssetItem> m_all;
    QString m_currentCategory; // 空=全部
    QString m_filter;
    QString m_assetsDir;       // 外部素材根目录（应用目录下 assets）

    QComboBox* m_categoryCombo{};
    QLineEdit* m_searchEdit{};
    QListWidget* m_list{};
    QPushButton* m_addButton{};
    QPushButton* m_cancelButton{};
    QPushButton* m_openFolderButton{};
    QPushButton* m_refreshButton{};

    int m_currentIndex{-1};
};

#endif // ASSETBROWSERDIALOG_H
