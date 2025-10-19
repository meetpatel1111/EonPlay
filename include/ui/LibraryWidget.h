#pragma once

#include <QWidget>

class LibraryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryWidget(QWidget* parent = nullptr);
    virtual ~LibraryWidget() = default;
};