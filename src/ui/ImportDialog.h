#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>

#include "../services/ImportService.h"

class ImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImportDialog(archive::services::ImportService* svc, QWidget* parent = nullptr);

private slots:
    void on_add_files();
    void on_add_folder();
    void on_import();

private:
    archive::services::ImportService* svc_;
    QListWidget* file_list_;
    QPushButton* import_btn_;

    void update_import_button();
};
