#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include "PortTypes.h"

namespace TSA::UI
{

/**
 * @brief Conteneur individuel d'un port d'affichage (Viewport) dans l'espace de travail multi-vues.
 * Comprend un en-tête moderne avec sélecteur de contenu, bouton d'agrandissement plein écran
 * et bordure de surbrillance active.
 */
class PortWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PortWidget(int portId, PortType initialType = PortType::Model3D, QWidget* parent = nullptr);
    ~PortWidget() override = default;

    int portId() const { return m_portId; }

    PortType portType() const { return m_portType; }
    void setPortType(PortType type);

    void setContentWidget(QWidget* widget);
    QWidget* contentWidget() const { return m_contentWidget; }

    bool isActive() const { return m_isActive; }
    void setActive(bool active);

    bool isPortMaximized() const { return m_isMaximized; }
    void setPortMaximized(bool max);

    QPushButton* addPortButton() const { return m_btnAddPort; }
    void setCanClose(bool canClose);

signals:
    void portTypeChanged(int portId, PortType newType);
    void maximizeRequested(int portId);
    void portActivated(int portId);
    void addPortRequested(int portId);
    void closePortRequested(int portId);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onTypeComboChanged(int index);
    void onMaximizeClicked();

private:
    void setupUi();
    void updateStyle();

private:
    int m_portId = 0;
    PortType m_portType = PortType::Model3D;
    bool m_isActive = false;
    bool m_isMaximized = false;

    QWidget* m_header = nullptr;
    QLabel* m_lblTitle = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QPushButton* m_btnAddPort = nullptr;
    QPushButton* m_btnMaximize = nullptr;
    QPushButton* m_btnClosePort = nullptr;

    QVBoxLayout* m_mainLayout = nullptr;
    QWidget* m_contentWidget = nullptr;
};

} // namespace TSA::UI
