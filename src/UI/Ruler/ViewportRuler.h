#pragma once

#include <QWidget>

class OccView;

namespace TSA::UI
{

class CornerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CornerWidget(QWidget* parent = nullptr);
    ~CornerWidget() override = default;

protected:
    void paintEvent(QPaintEvent* event) override;
};

class HorizontalRulerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HorizontalRulerWidget(OccView* occView, QWidget* parent = nullptr);
    ~HorizontalRulerWidget() override = default;

    void setCursorPos(int px);
    void updateRuler();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    OccView* m_occView = nullptr;
    int m_cursorPixelX = -1;
};

class VerticalRulerWidget : public QWidget
{
    Q_OBJECT
public:
    enum class Position
    {
        Left,
        Right
    };

    explicit VerticalRulerWidget(OccView* occView, Position pos = Position::Left, QWidget* parent = nullptr);
    ~VerticalRulerWidget() override = default;

    void setCursorPos(int py);
    void updateRuler();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    OccView* m_occView = nullptr;
    Position m_position = Position::Left;
    int m_cursorPixelY = -1;
};

} // namespace TSA::UI
