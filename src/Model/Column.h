#pragma once

#include <string>

namespace TSA::Model
{

class Model;

class Column
{
public:
    Column() = default;
    Column(int id, int startNodeId, int endNodeId, double width = 0.30, double height = 0.30);

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int startNodeId() const { return m_startNodeId; }
    void setStartNodeId(int nodeId) { m_startNodeId = nodeId; }

    int endNodeId() const { return m_endNodeId; }
    void setEndNodeId(int nodeId) { m_endNodeId = nodeId; }

    double width() const { return m_width; }
    void setWidth(double width) { m_width = width; }

    double height() const { return m_height; }
    void setHeight(double height) { m_height = height; }

    void setDimensions(double width, double height);

    double length(const Model& model) const;
    bool isVertical(const Model& model, double tol = 1e-3) const;
    double bottomElevation(const Model& model) const;
    double topElevation(const Model& model) const;
    std::string direction(const Model& model) const;

private:
    int m_id = 0;
    int m_startNodeId = 0;
    int m_endNodeId = 0;
    double m_width = 0.30;   // Section b (mètres)
    double m_height = 0.30;  // Section h (mètres)
};

} // namespace TSA::Model
