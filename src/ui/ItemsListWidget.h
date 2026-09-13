#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>

#include "../services/SearchService.h"
#include "../core/enums/ItemStatus.h"

class ItemsListWidget : public QWidget {
    Q_OBJECT

public:
    enum class Mode { All, Favorites, Trash };

    explicit ItemsListWidget(archive::services::SearchService& svc, QWidget* parent = nullptr);
    void refresh();
    void set_mode(Mode mode);

signals:
    void item_selected(const QString& item_id);

private:
    archive::services::SearchService& svc_;
    QLineEdit* search_box_ = nullptr;
    QListWidget* list_ = nullptr;
    Mode mode_ = Mode::All;

    void on_search_changed(const QString& text);
    void on_row_changed(int row);
};
