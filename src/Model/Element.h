#pragma once

#include <string>
#include <vector>

namespace TSA::Model
{

class Model;
struct Section;
struct Material;

/**
 * @brief Classe de base conceptuelle pour tous les éléments structuraux de TSA.
 */
class Element
{
public:
    virtual ~Element() = default;

    virtual int id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string typeName() const = 0;

    virtual double volume(const Model& model) const { (void)model; return 0.0; }
    virtual double weight(const Model& model) const { (void)model; return 0.0; }
};

/**
 * @brief Classe de base pour les éléments structuraux filaires 1D (Poutres, Poteaux, Barres, Câbles).
 */
class LinearElement : public Element
{
public:
    ~LinearElement() override = default;

    virtual int startNodeId() const = 0;
    virtual int endNodeId() const = 0;

    virtual const Section& section() const = 0;
    virtual const Material& material() const = 0;

    virtual double length(const Model& model) const = 0;
};

using ElementLineaire = LinearElement;

/**
 * @brief Classe de base pour les éléments structuraux surfaciques 2D (Dalles, Voiles, Panneaux).
 */
class SurfaceElement : public Element
{
public:
    ~SurfaceElement() override = default;

    virtual const std::vector<int>& nodeIds() const = 0;
    virtual double thickness() const = 0;
    virtual const Material& material() const = 0;

    virtual double area(const Model& model) const = 0;
};

using ElementSurfacique = SurfaceElement;

} // namespace TSA::Model
