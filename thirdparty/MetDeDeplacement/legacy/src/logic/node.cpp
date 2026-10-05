/**
 * @file node.cpp
 * @brief Implementation of the Node class for managing nodes in a structural system
 * @author Tsaraloha Nomenjanahary Christinot Leonnel Calixte
 * @date 02/08/2025 10:00
 * 
 * This file contains the implementation of the Node class which manages nodes
 * in a structure. A node represents a connection point between different
 * structural elements (lines, beams, columns).
 * 
 * Main features:
 * - Node identification and management
 * - Node type determination (fixed, hinge, free)
 * - Fictitious displacement calculations
 * - Association with connected lines
 * - Position verification relative to circles
 */

#include "node.h"

/**
 * @brief Returns the inverse node name
 * @param node Current node name ("nd_1" or "nd_2")
 * @return Inverse node name
 * 
 * This utility function returns the name of the opposite node in a connection.
 * If the current node is "nd_1", it returns "nd_2" and vice versa.
 */
static constexpr string inverse_node(string node) {
	if (node == "nd_1") return "nd_2";
	else return "nd_1";
}

/**
 * @brief Finds the maximum value in a vector
 * @tparam T Vector data type
 * @param vecteur Vector to analyze
 * @return Maximum value of the vector
 * 
 * This template function finds and returns the maximum value
 * in a vector of type T.
 */
template <typename T>
constexpr T max_vect(vector<T>& vecteur) {
	T val = vecteur[0]; 
	for (auto& i : vecteur) {
		val = std::max(val, i); 
	}
	return val; 
}

/**
 * @brief Node constructor with numeric ID
 * @param id Numeric identifier of the node
 * @param data Structure data containing information about nodes and lines
 * 
 * Initializes a node with a numeric identifier and automatically calls
 * initialization to determine the type and properties of the node.
 */
Node::Node(int id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data) : id(id), data(data) 
{
	node_key = "nd_" + to_string(id);
	initilisation(); 
}

/**
 * @brief Node constructor with string ID
 * @param id String identifier of the node
 * @param data Structure data containing information about nodes and lines
 * 
 * Initializes a node with a string identifier and automatically calls
 * initialization to determine the type and properties of the node.
 */
Node::Node(string id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data) : id_string(id), data(data)
{
	node_key = id;
	initilisation();
}

/**
 * @brief Finds all lines connected to this node
 * @return Vector of pairs [line_name, connection_type] connected to the node
 * 
 * This function iterates through all lines in the structure and identifies
 * those connected to the current node. It returns a vector containing
 * the line name and connection type ("nd_1" or "nd_2").
 */
vector<vector<string>> Node::line_with()
{
	vector<vector<string>> value; 

	for (int id_line = 0; id_line < data["nombre"]["propriete"]["ligne"]; id_line++) {
		if (data["ln_" + to_string(id_line)]["nd_1"] == data["noeuds"][node_key]) {
			value.push_back({ "ln_" + to_string(id_line), "nd_1" });
		}
		else if (data["ln_" + to_string(id_line)]["nd_2"] == data["noeuds"][node_key]) {
			value.push_back({ "ln_" + to_string(id_line), "nd_2" });
		}
	}
	
	return value;
}

/**
 * @brief Checks if the node is inside a given circle
 * @param id_cirle Circle identifier to check
 * @return true if the node is inside the circle, false otherwise
 * 
 * Calculates the distance between the node and the circle center and compares
 * with the circle radius to determine if the node is inside.
 */
bool Node::is_in_cirlce(int id_cirle)
{
	double dx = data["noeuds"][node_key]["x"] - data["circle_" + to_string(id_cirle)]["center"]["x"];
	double dy = data["noeuds"][node_key]["y"] - data["circle_" + to_string(id_cirle)]["center"]["y"];
	double dz = data["noeuds"][node_key]["z"] - data["circle_" + to_string(id_cirle)]["center"]["z"];
	double R = data["circle_" + to_string(id_cirle)]["propriete"]["radius"];
	return dx*dx + dy*dy + dz*dz <= R*R ;
}

/**
 * @brief Determines the node type
 * @return Node type: "free", "rotule", or "fix"
 * 
 * Determines the node type according to the following criteria:
 * - "free": Free node (single consolidated line)
 * - "rotule": Node inside a circle (hinge support)
 * - "fix": Fixed node (default)
 */
string Node::name_type() 
{	
	// Vérifier si le nœud a des lignes associées
	if (ligne_associer.empty()) {
		return "fix"; // Nœud sans connexions, considéré comme fixe
	}
	
	// Vérifier si c'est un nœud libre (une seule ligne consolidée)
	if (ligne_associer.size() == 1) {
		Line line(ligne_associer[0][0], data);
		if (line.is_consol) {
			return "free";
		}
	}
	
	// Vérifier si le nœud est dans un cercle (rotule)
	int nombre_circles = static_cast<int>(data["nombre"]["propriete"]["cercle"]);
	for (int i = 1; i <= nombre_circles; ++i) {
		if (is_in_cirlce(i)) {
			return "rotule";
		}
	}

	return "fix";
}

/**
 * @brief Calculates fictitious displacements of the node
 * @return Deplacement structure containing displacement information
 * 
 * Analyzes the configuration of lines connected to the node to determine
 * fictitious displacement possibilities. Considers the following cases:
 * - 2 or 3 connected lines: displacement possible
 * - Mix of columns and beams: horizontal and vertical displacements
 * - Columns only: vertical displacement
 * - Beams only: horizontal displacement
 */
Deplacement Node::appuis_fictif()
{
	bool dep = false;
	bool horz = false;
	bool vect = false; 
	bool incline = false;

	unordered_map<string, unordered_map<string, bool>> depla;
	int compteur = 0;

	vector<APN> line_obj; 

	for (const auto& line : ligne_associer) {
		Line ln = Line(line[0], data); 
		if (!ln.is_consol) {
			line_obj.push_back({ln, line[1]});
		}
	}

	if (line_obj.size() == 2 or line_obj.size() == 3 ) {
		dep = true; 
		vector<double> y_column;
		vector<double> y_beam;
		vector<double> x_column;
		vector<double> x_beam; 
		unordered_map<string, double> oposide_node;

		for (auto& i : line_obj) {
			oposide_node = data[i.line.line_key] [inverse_node(i.node)];
			auto& line_c = i.line; 
			if (line_c.is_beam) {
				y_beam.push_back(oposide_node["y"]);
				x_beam.push_back(oposide_node["x"]); 
			}
			else if (line_c.is_column) {
				y_column.push_back(oposide_node["y"]);
				x_column.push_back(oposide_node["x"]);
			}
		}

		size_t column_numbre = y_column.size();
		size_t beam_numbre = y_beam.size();

		if (column_numbre == beam_numbre) {
			horz = true; 
			vect = true; 

			double Y_beam = max_vect(y_beam); 
			double X_beam = max_vect(x_beam); 
			double Y_column = max_vect(y_column); 
			double X_column = max_vect(x_column); 

			depla["vecticale"] = { {"haut" , Y_beam > Y_column}, {"bas", Y_beam < Y_column}};
			depla["horizontale"] = { {"left" , X_beam < X_column}, {"right", X_beam > X_column} };
		}
		else if (column_numbre == 1 and beam_numbre == 2) {
			vect = true;

			double Y_beam = max_vect(y_beam);
			double Y_column = max_vect(y_column);

			depla["vecticale"] = { {"haut" , Y_beam > Y_column}, {"bas", Y_beam < Y_column} };
		}
		else if (column_numbre == 2 and beam_numbre == 1) {
			horz = true;

			double X_beam = max_vect(x_beam);
			double X_column = max_vect(x_column);

			depla["horizontale"] = { {"left" , X_beam < X_column}, {"right", X_beam > X_column} };
		}
	}
	return Deplacement(dep, horz, vect, incline, depla);
}

/**
 * @brief Initializes node properties
 * 
 * This private function is called by constructors to initialize
 * all node properties:
 * - ligne_associer: connected lines
 * - type: node type (free, rotule, fix)
 * - support_fictif: possible fictitious displacements
 */
void Node::initilisation()
{
	node_key_invered = inverse_node(node_key);
	ligne_associer = line_with();
	type = name_type();
	support_fictif = appuis_fictif();
}

