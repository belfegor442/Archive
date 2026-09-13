#include "ItemDetailWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QFrame>
#include <QFont>

ItemDetailWidget::ItemDetailWidget(
    archive::storage::ArchiveItemRepository& items,
    archive::services::NoteService& notes,
    archive::services::VersionService& versions,
    archive::services::ActivityService& activities,
    QWidget* parent
) : QWidget(parent)
  , items_(items)
  , notes_(notes)
  , versions_(versions)
  , activities_(activities)
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

    back_btn_ = new QPushButton(tr("← Back"));
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

    favorite_btn_ = new QPushButton(tr("☆ Favorite"));
    favorite_btn_->setStyleSheet(R"(
        QPushButton { background-color: #313244; color: #f9e2af; border: none;
            border-radius: 6px; padding: 8px 16px; }
        QPushButton:hover { background-color: #45475a; }
    )");
    toolbar_layout->addWidget(favorite_btn_);

    main_layout->addWidget(toolbar);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 16, 24, 24);

    auto make_info_row = [&](const QString& label_text, QLabel*& value) -> QHBoxLayout* {
        auto* row = new QHBoxLayout();
        auto* label = new QLabel(label_text);
        label->setStyleSheet("color: #a6adc8; font-size: 13px; min-width: 100px;");
        row->addWidget(label);
        value = new QLabel("-");
        value->setStyleSheet("color: #cdd6f4; font-size: 13px;");
        value->setWordWrap(true);
        row->addWidget(value);
        return row;
    };

    auto* info_card = new QFrame();
    info_card->setStyleSheet("background-color: #1e1e2e; border-radius: 12px; padding: 16px;");
    auto* info_layout = new QVBoxLayout(info_card);
    info_layout->addLayout(make_info_row(tr("Type:"), type_label_));
    info_layout->addLayout(make_info_row(tr("Status:"), status_label_));
    info_layout->addLayout(make_info_row(tr("Size:"), size_label_));
    info_layout->addLayout(make_info_row(tr("Path:"), path_label_));
    info_layout->addLayout(make_info_row(tr("Checksum:"), checksum_label_));
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

    auto* save_note_btn = new QPushButton(tr("Save Note"));
    save_note_btn->setStyleSheet(R"(
        QPushButton { background-color: #6366f1; color: white; border: none;
            border-radius: 6px; padding: 8px 16px; }
        QPushButton:hover { background-color: #818cf8; }
    )");
    notes_layout->addWidget(save_note_btn);
    connect(save_note_btn, &QPushButton::clicked, this, &ItemDetailWidget::on_save_note_clicked);

    layout->addWidget(notes_card);

    auto* versions_card = new QFrame();
    versions_card->setStyleSheet("background-color: #1e1e2e; border-radius: 12px; padding: 16px;");
    auto* versions_layout = new QVBoxLayout(versions_card);
    auto* versions_title = new QLabel(tr("Versions"));
    versions_title->setStyleSheet("color: #a6adc8; font-size: 14px; font-weight: bold;");
    versions_layout->addWidget(versions_title);
    versions_list_ = new QListWidget();
    versions_list_->setStyleSheet(R"(
        QListWidget { background-color: #313244; border: none; border-radius: 8px; }
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
        QListWidget { background-color: #313244; border: none; border-radius: 8px; }
        QListWidget::item { color: #cdd6f4; padding: 8px; }
    )");
    activity_layout->addWidget(activity_list_);
    layout->addWidget(activity_card);

    layout->addStretch();
    scroll->setWidget(content);
    main_layout->addWidget(scroll);

    connect(back_btn_, &QPushButton::clicked, this, &ItemDetailWidget::on_back_clicked);
    connect(favorite_btn_, &QPushButton::clicked, this, &ItemDetailWidget::on_favorite_clicked);
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
    checksum_label_->setText(QString::fromStdString(item->checksum));

    if (item->is_favorite) {
        favorite_btn_->setText(tr("★ Unfavorite"));
    } else {
        favorite_btn_->setText(tr("☆ Favorite"));
    }

    auto notes = notes_.get_notes(current_item_id_.toStdString());
    notes_edit_->clear();
    for (const auto& note : notes) {
        notes_edit_->append(QString::fromStdString(note.content));
    }

    auto vers = versions_.get_versions(current_item_id_.toStdString());
    versions_list_->clear();
    for (const auto& v : vers) {
        versions_list_->addItem(
            QString("v%1 - %2").arg(v.version_number).arg(QString::fromStdString(v.created_at))
        );
    }

    auto acts = activities_.get_for_item(current_item_id_.toStdString());
    activity_list_->clear();
    for (const auto& a : acts) {
        activity_list_->addItem(
            QString("[%1] %2 %3")
                .arg(QString::fromStdString(a.created_at))
                .arg(QString::fromStdString(archive::core::to_string(a.action)))
                .arg(QString::fromStdString(a.details))
        );
    }
}

void ItemDetailWidget::on_back_clicked() {
    emit parent()->metaObject()->invokeMethod(
        qobject_cast<QWidget*>(parent()->parent()),
        "on_sidebar_item_clicked",
        Qt::DirectConnection,
        Q_ARG(QString, "all")
    );
}

void ItemDetailWidget::on_favorite_clicked() {
    auto item = items_.find_by_id(current_item_id_.toStdString());
    if (item) {
        items_.set_favorite(current_item_id_.toStdString(), !item->is_favorite);
        refresh();
    }
}

void ItemDetailWidget::on_save_note_clicked() {
    QString content = notes_edit_->toPlainText();
    if (!content.isEmpty()) {
        notes_.add(current_item_id_.toStdString(), content.toStdString());
        refresh();
    }
}
