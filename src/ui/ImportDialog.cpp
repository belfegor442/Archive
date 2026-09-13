#include "ImportDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>

ImportDialog::ImportDialog(archive::services::ImportService* svc, QWidget* parent)
    : QDialog(parent)
    , svc_(svc)
{
    setWindowTitle(tr("Import Items"));
    resize(500, 400);
    setStyleSheet("background-color: #11111b; color: #cdd6f4;");

    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel(tr("Select files or folders to archive"));
    title->setStyleSheet("font-size: 16px; font-weight: bold; padding: 8px;");
    layout->addWidget(title);

    file_list_ = new QListWidget();
    file_list_->setStyleSheet(R"(
        QListWidget { background-color: #1e1e2e; border: 1px solid #313244;
            border-radius: 8px; padding: 8px; }
        QListWidget::item { color: #cdd6f4; padding: 4px; }
    )");
    layout->addWidget(file_list_);

    auto* btn_layout = new QHBoxLayout();

    auto* add_files_btn = new QPushButton(tr("Add Files"));
    add_files_btn->setStyleSheet(R"(
        QPushButton { background-color: #313244; color: #cdd6f4; border: none;
            border-radius: 6px; padding: 10px 16px; }
        QPushButton:hover { background-color: #45475a; }
    )");
    connect(add_files_btn, &QPushButton::clicked, this, &ImportDialog::on_add_files);
    btn_layout->addWidget(add_files_btn);

    auto* add_folder_btn = new QPushButton(tr("Add Folder"));
    add_folder_btn->setStyleSheet(R"(
        QPushButton { background-color: #313244; color: #cdd6f4; border: none;
            border-radius: 6px; padding: 10px 16px; }
        QPushButton:hover { background-color: #45475a; }
    )");
    connect(add_folder_btn, &QPushButton::clicked, this, &ImportDialog::on_add_folder);
    btn_layout->addWidget(add_folder_btn);

    btn_layout->addStretch();

    import_btn_ = new QPushButton(tr("Import"));
    import_btn_->setStyleSheet(R"(
        QPushButton { background-color: #6366f1; color: white; border: none;
            border-radius: 6px; padding: 10px 24px; font-weight: bold; }
        QPushButton:hover { background-color: #818cf8; }
        QPushButton:disabled { background-color: #45475a; color: #6c7086; }
    )");
    import_btn_->setEnabled(false);
    connect(import_btn_, &QPushButton::clicked, this, &ImportDialog::on_import);
    btn_layout->addWidget(import_btn_);

    layout->addLayout(btn_layout);
}

void ImportDialog::on_add_files() {
    auto files = QFileDialog::getOpenFileNames(this, tr("Select Files"));
    for (const auto& file : files) {
        file_list_->addItem(file);
    }
    update_import_button();
}

void ImportDialog::on_add_folder() {
    auto folder = QFileDialog::getExistingDirectory(this, tr("Select Folder"));
    if (!folder.isEmpty()) {
        file_list_->addItem(folder);
    }
    update_import_button();
}

void ImportDialog::on_import() {
    archive::core::ImportRequest req;
    for (int i = 0; i < file_list_->count(); i++) {
        req.paths.push_back(file_list_->item(i)->text().toStdString());
    }

    auto result = svc_->import(req);

    QString msg;
    msg += tr("Imported: %1 items\n").arg(result.success_count());
    if (result.has_errors()) {
        msg += tr("Errors: %1\n").arg(result.error_count());
        for (const auto& err : result.errors) {
            msg += "  - " + QString::fromStdString(err.path) + ": " + QString::fromStdString(err.error) + "\n";
        }
    }

    QMessageBox::information(this, tr("Import Complete"), msg);
    accept();
}

void ImportDialog::update_import_button() {
    import_btn_->setEnabled(file_list_->count() > 0);
}
