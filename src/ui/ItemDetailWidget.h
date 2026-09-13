#pragma once

#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QListWidget>
#include <QPushButton>

#include "../storage/ArchiveItemRepository.h"
#include "../services/NoteService.h"
#include "../services/VersionService.h"
#include "../services/ActivityService.h"
#include "../services/UpdateService.h"

class ItemDetailWidget : public QWidget {
    Q_OBJECT

public:
    explicit ItemDetailWidget(
        archive::storage::ArchiveItemRepository& items,
        archive::services::NoteService& notes,
        archive::services::VersionService& versions,
        archive::services::ActivityService& activities,
        archive::services::UpdateService& update_svc,
        QWidget* parent = nullptr
    );

    void load_item(const QString& item_id);

signals:
    void back_clicked();

private:
    archive::storage::ArchiveItemRepository& items_;
    archive::services::NoteService& notes_;
    archive::services::VersionService& versions_;
    archive::services::ActivityService& activities_;
    archive::services::UpdateService& update_svc_;

    QString current_item_id_;

    QLabel* name_label_ = nullptr;
    QLabel* type_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* size_label_ = nullptr;
    QLabel* path_label_ = nullptr;
    QLabel* checksum_label_ = nullptr;
    QLabel* version_label_ = nullptr;
    QTextEdit* notes_edit_ = nullptr;
    QListWidget* versions_list_ = nullptr;
    QListWidget* activity_list_ = nullptr;
    QPushButton* back_btn_ = nullptr;
    QPushButton* favorite_btn_ = nullptr;
    QPushButton* trash_btn_ = nullptr;
    QPushButton* save_note_btn_ = nullptr;

    void setup_ui();
    void refresh();
    void on_back_clicked();
    void on_favorite_clicked();
    void on_trash_clicked();
    void on_save_note_clicked();
};
