#pragma once

#include <QWidget>
#include <QListWidget>

class SidebarWidget : public QWidget {
    Q_OBJECT

public:
    explicit SidebarWidget(QWidget* parent = nullptr);

signals:
    void item_clicked(const QString& page);

private:
    QListWidget* list_;

    void on_row_changed(int row);
};
