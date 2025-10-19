#pragma once

#include <QWidget>

class PlaylistWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlaylistWidget(QWidget* parent = nullptr);
    virtual ~PlaylistWidget() = default;
};