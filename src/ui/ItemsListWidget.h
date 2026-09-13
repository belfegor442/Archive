#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>

#include "../services/SearchService.h"
#include "../core/enums/ItemStatus.h"

class ItemsListWidget : public QWidget {
    Q_OBJECT

public:
    explicit ItemsListWidget(archive::services::SearchService& svc, QWidget* parent = nullptr);
    void refresh();
    void set_filters(const QString& category_id, bool favorites_only);
    void set_status_filter(archive::core::ItemStatus status);

signals:
    void item_selected(const QString& item_id);

private:
    archive::services::SearchService& svc_;
    QLineEdit* search_box_ = nullptr;
    QListWidget* list_ = nullptr;
    QString category_filter_;
    bool favorites_only_ = false;
    archive::core::ItemStatus status_filter_ = archive::core::ItemStatus::Archived;

    void on_search_changed(const QString& text);
    void on_row_changed(int row);
};
