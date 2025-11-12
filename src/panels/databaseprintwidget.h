#ifndef DATABASEPRINTWIDGET_H
#define DATABASEPRINTWIDGET_H

#include <QGroupBox>
#include <QString>
#include <memory>

class QComboBox;
class QStackedWidget;
class QPlainTextEdit;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QTableWidget;
class QCheckBox;
class DataSource;
class TableDataSource;
class MySqlDataSource;

class DatabasePrintWidget : public QGroupBox
{
    Q_OBJECT

public:
    explicit DatabasePrintWidget(QWidget *parent = nullptr);

    QComboBox *sourceModeCombo() const { return m_sourceModeCombo; }
    QPlainTextEdit *batchInputEdit() const { return m_batchInputEdit; }
    QComboBox *delimiterCombo() const { return m_delimiterCombo; }
    QLineEdit *customDelimiterEdit() const { return m_customDelimiterEdit; }
    QPushButton *uploadButton() const { return m_uploadButton; }
    QLineEdit *selectedFileEdit() const { return m_selectedFileEdit; }
    QComboBox *sheetCombo() const { return m_sheetCombo; }
    QComboBox *headerCombo() const { return m_headerCombo; }
    QSpinBox *startRowSpin() const { return m_startRowSpin; }
    QTableWidget *previewTable() const { return m_previewTable; }

    bool isDataSourceEnabled() const;
    void setDataSourceEnabled(bool enabled);

    std::shared_ptr<DataSource> createDataSource(QString *errorMessage = nullptr) const;
    void setDataSource(const std::shared_ptr<DataSource>& source);
    void clearInputs();

signals:
    void dataSourceEnabledChanged(bool enabled);
    void applyRequested();

private slots:
    void onSourceModeChanged(int index);
    void onDelimiterChanged(int index);
    void onUploadFileClicked();
    void onSheetChanged(int index);
    void onHeaderChanged(int index);
    void onColumnChanged(int index);
    void onStartRowChanged(int value);
    void onEndRowChanged(int value);
    void onMySqlTestConnection();
    void onMySqlRefreshTables();
    void onMySqlTableChanged(int index);
    void onMySqlColumnChanged(int index);
    void onMySqlPreviewClicked();

private:
    QString currentDelimiterText() const;
    void clearTablePreview();
    void refreshTablePreview();
    void syncTableSourceFromUi();
    void updateTableControlsFromSource(bool preserveSelection = true);
    void populateHeaderCombo(int maxRow);
    void populateColumnCombo(bool preserveSelection);
    void updateRowSpins(int maxRow);
    static QString columnLabelForIndex(int columnIndex, const QString &headerText);
    void clearMySqlPreview();
    void refreshMySqlPreview();
    void syncMySqlSourceFromUi();
    void updateMySqlControlsFromSource(bool loadTables = false);
    bool ensureMySqlSource();
    void populateMySqlTableCombo(const QStringList &tables, bool preserveSelection);
    void populateMySqlColumnCombo(const QStringList &columns, bool preserveSelection);

    QComboBox *m_sourceModeCombo = nullptr;
    QStackedWidget *m_sourceStack = nullptr;
    QPlainTextEdit *m_batchInputEdit = nullptr;
    QComboBox *m_delimiterCombo = nullptr;
    QLineEdit *m_customDelimiterEdit = nullptr;
    QPushButton *m_uploadButton = nullptr;
    QLineEdit *m_selectedFileEdit = nullptr;
    QComboBox *m_sheetCombo = nullptr;
    QComboBox *m_headerCombo = nullptr;
    QSpinBox *m_startRowSpin = nullptr;
    QSpinBox *m_endRowSpin = nullptr;
    QComboBox *m_columnCombo = nullptr;
    QTableWidget *m_previewTable = nullptr;
    QCheckBox *m_enableCheck = nullptr;
    QPushButton *m_applyButton = nullptr;
    std::shared_ptr<TableDataSource> m_tableSource;
    bool m_updatingTableControls = false;
    std::shared_ptr<MySqlDataSource> m_mysqlSource;
    bool m_updatingMySqlControls = false;

    QLineEdit *m_mysqlHostEdit = nullptr;
    QSpinBox *m_mysqlPortSpin = nullptr;
    QLineEdit *m_mysqlDatabaseEdit = nullptr;
    QLineEdit *m_mysqlUserEdit = nullptr;
    QLineEdit *m_mysqlPasswordEdit = nullptr;
    QComboBox *m_mysqlTableCombo = nullptr;
    QComboBox *m_mysqlColumnCombo = nullptr;
    QLineEdit *m_mysqlFilterEdit = nullptr;
    QPushButton *m_mysqlPreviewButton = nullptr;
    QTableWidget *m_mysqlPreviewTable = nullptr;
    QPushButton *m_mysqlRefreshTablesButton = nullptr;
    QPushButton *m_mysqlTestButton = nullptr;
};

#endif // DATABASEPRINTWIDGET_H
