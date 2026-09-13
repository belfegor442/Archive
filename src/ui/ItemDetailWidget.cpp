#include "ItemDetailWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QMessageBox>

ItemDetailWidget::ItemDetailWidget(
    archive::storage::ArchiveItemRepository& items,
    archive::services::NoteService& notes,
    archive::services::VersionService& versions,
    archive::services::ActivityService& activities,
    archive::services::UpdateService& update_svc,
    QWidget* parent
) : QWidget(parent)
  , items_(items)
  , notes_(notes)
  , versions_(versions)
  , activities_(activities)
  , update_svc_(update_svc)
{
    setup_ui();
    setStyleSheet("background-color: #11111b; color: #cdd6f4;");
}

void ItemDetailWidget::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    auto* toolbar = new QWidget();
    toolbar->setStyleSheet("background-color: #1e1e2e; padding: 8px;");
    auto* toolbar_layout = new QHBoxLayout(toolbar);
    toolbar_layout->setContentsMargins(16, 8, 16, 8);

    back_btn_ = new QPushButton(tr("< Back"));
    back_btn_->setStyleSheet(R"(
        QPushButton { background-color: #313244; color: #cdd6f4; border: none;
            border-radius: 6px; padding: 8px 16px; }
        QPushButton:hover { background-color: #45475a; }
    )");
    toolbar_layout->addWidget(back_btn_);

    name_label_ = new QLabel();
    name_label_->setStyleSheet("color: #cdd6f4; font-size: 18px; font-weight: bold;");
    toolbar_layout->addWidget(name_label_);

    toolbar_layout->addStretch();

    favorite_btn_ = new QPushButton(tr("Favorite"));
    favorite_btn_->setStyleSheet(R"(
        QPushButton { background-color: #313244; color: #f9e2af; border: none;
            border-radius: 6px; padding: 8px 16px; }
        QPushButton:hover { background-color: #45475a; }
    )");
    toolbar_layout->addWidget(favorite_btn_);

    trash_btn_ = new QPushButton(tr("Trash"));
    trash_btn_->setStyleSheet(R"(
        QPushButton { background-color: #f38ba8; color: #1e1e2e; border: none;
            border-radius: 6px; padding: 8px 16px; font-weight: bold; }
        QPushButton:hover { background-color: #eba0ac; }
    )");
    toolbar_layout->addWidget(trash_btn_);

    main_layout->addWidget(toolbar);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 16, 24, 24);

    auto make_info_row = [&](const QString& label_text, QLabel*& value) -> QGridLayout* {
        (void)label_text;
        (void)value;
        return nullptr;
    };

    auto* info_card = new QFrame();
    info_card->setStyleSheet("background-color: #1e1e2e; border-radius: 12px; padding: 16px;");
    auto* info_layout = new QGridLayout(info_card);

    auto add_row = [&](int row, const QString& lbl, QLabel*& val) {
        auto* label = new QLabel(lbl);
        label->setStyleSheet("color: #a6adc8; font-size: 13px;");
        info_layout->addWidget(label, row, 0);
        val = new QLabel("-");
        val->setStyleSheet("color: #cdd6f4; font-size: 13px;");
        val->setWordWrap(true);
        info_layout->addWidget(val, row, 1);
    };

    add_row(0, tr("Type:"), type_label_);
    add_row(1, tr("Status:"), status_label_);
    add_row(2, tr("Size:"), size_label_);
    add_row(3, tr("Version:"), version_label_);
    add_row(4, tr("Path:"), path_label_);
    add_row(5, tr("Checksum:"), checksum_label_);

    layout->addWidget(info_card);

    auto* notes_card = new QFrame();
    notes_card->setStyleSheet("background-color: #1e1e2e; border-radius: 12px; padding: 16px;");
    auto* notes_layout = new QVBoxLayout(notes_card);
    auto* notes_title = new QLabel(tr("Notes"));
    notes_title->setStyleSheet("color: #a6adc8; font-size: 14px; font-weight: bold;");
    notes_layout->addWidget(notes_title);
    notes_edit_ = new QTextEdit();
    notes_edit_->setStyleSheet(R"(
        QTextEdit { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a;
            border-radius: 8px; padding: 8px; font-size: 13px; }
    )");
    notes_edit_->setMinimumHeight(80);
    notes_layout->addWidget(notes_edit_);

    save_note_btn_ = new QPushButton(tr("Save Note"));
    save_note_btn_->setStyleSheet(R"(
        QPushButton { background-color: #6366f1; color: white; border: none;
            border-radius: 6px; padding: 8px 16px; }
        QPushButton:hover { background-color: #818cf8; }
    )");
    notes_layout->addWidget(save_note_btn_);
    layout->addWidget(notes_card);

    auto* versions_card = new QFrame();
    versions_card->setStyleSheet("background-color: #1e1e2e; border-radius: 12px; padding: 16px;");
    auto* versions_layout = new QVBoxLayout(versions_card);
    auto* versions_title = new QLabel(tr("Versions"));
    versions_title->setStyleSheet("color: #a6adc8; font-size: 14px; font-weight: bold;");
    versions_layout->addWidget(versions_title);
    versions_list_ = new QListWidget();
    versions_list_->setStyleSheet(R"(
        QListWidget { background-color: #313244; border: none; border-radius: 8px; max-height: 200px; }
        QListWidget::item { color: #cdd6f4; padding: 8px; }
    )");
    versions_layout->addWidget(versions_list_);
    layout->addWidget(versions_card);

    auto* activity_card = new QFrame();
    activity_card->setStyleSheet("background-color: #1e1e2e; border-radius: 12px; padding: 16px;");
    auto* activity_layout = new QVBoxLayout(activity_card);
    auto* activity_title = new QLabel(tr("Activity Log"));
    activity_title->setStyleSheet("color: #a6adc8; font-size: 14px; font-weight: bold;");
    activity_layout->addWidget(activity_title);
    activity_list_ = new QListWidget();
    activity_list_->setStyleSheet(R"(
        QListWidget { background-color: #313244; border: none; border-radius: 8px; max-height: 200px; }
        QListWidget::item { color: #cdd6f4; padding: 8px; }
    )");
    activity_layout->addWidget(activity_list_);
    layout->addWidget(activity_card);

    layout->addStretch();
    scroll->setWidget(content);
    main_layout->addWidget(scroll);

    connect(back_btn_, &QPushButton::clicked, this, &ItemDetailWidget::on_back_clicked);
    connect(favorite_btn_, &QPushButton::clicked, this, &ItemDetailWidget::on_favorite_clicked);
    connect(trash_btn_, &QPushButton::clicked, this, &ItemDetailWidget::on_trash_clicked);
    connect(save_note_btn_, &QPushButton::clicked, this, &ItemDetailWidget::on_save_note_clicked);
}

void ItemDetailWidget::load_item(const QString& item_id) {
    current_item_id_ = item_id;
    refresh();
}

void ItemDetailWidget::refresh() {
    auto item = items_.find_by_id(current_item_id_.toStdString());
    if (!item) return;

    name_label_->setText(QString::fromStdString(item->name));
    type_label_->setText(QString::fromStdString(archive::core::to_string(item->type)));
    status_label_->setText(QString::fromStdString(archive::core::to_string(item->status)));
    size_label_->setText(QString::number(item->size) + " bytes");
    path_label_->setText(QString::fromStdString(item->original_path));
    checksum_label_->setText(item->checksum.empty() ? tr("N/A") : QString::fromStdString(item->checksum).left(32) + "...");
    version_label_->setText(QString::number(item->current_version));

    favorite_btn_->setText(item->is_favorite ? tr("Unfavorite") : tr("Favorite"));

    auto notes = notes_.get_notes(current_item_id_.toStdString());
    notes_edit_->clear();
    for (const auto& note : notes) {
        notes_edit_->append(QString::fromStdString(note.content));
    }

    auto vers = versions_.get_versions(current_item_id_.toStdString());
    versions_list_->clear();
    for (const auto& v : vers) {
        versions_list_->addItem(
            QString("v%1 - %2 - %3 bytes")
                .arg(v.version_number)
                .arg(QString::fromStdString(v.created_at))
                .arg(v.size)
        );
    }

    auto acts = activities_.get_for_item(current_item_id_.toStdString());
    activity_list_->clear();
    for (const auto& a : acts) {
        activity_list_->addItem(
            QString("[%1] %2 %3")
                .arg(QString::fromStdString(a.created_at).left(19))
                .arg(QString::fromStdString(archive::core::to_string(a.action)))
                .arg(QString::fromStdString(a.details))
        );
    }
}

void ItemDetailWidget::on_back_clicked() {
    emit back_clicked();
}

void ItemDetailWidget::on_favorite_clicked() {
    update_svc_.toggle_favorite(current_item_id_.toStdString());
    refresh();
}

void ItemDetailWidget::on_trash_clicked() {
    auto item = items_.find_by_id(current_item_id_.toStdString());
    if (!item) return;

    auto reply = QMessageBox::question(this, tr("Move to Trash"),
        tr("Move '%1' to trash?").arg(QString::fromStdString(item->name)),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        update_svc_.move_to_trash(current_item_id_.toStdString());
        emit back_clicked();
    }
}

void ItemDetailWidget::on_save_note_clicked() {
    QString content = notes_edit_->toPlainText().trimmed();
    if (!content.isEmpty()) {
        notes_.add(current_item_id_.toStdString(), content.toStdString());
        refresh();
    }
}
