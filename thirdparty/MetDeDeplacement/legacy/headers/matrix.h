#pragma once
#ifndef MATRIX
#define MATRIX

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <map> 
#include <iostream>
#include <stdexcept>

#include "node.h"
#include "MomentEncastrementParfait.h"

using std::string;
using std::unordered_map;
using std::vector;
using std::map; 

/**
 * @brief Classe Matrix pour la gestion des matrices de rigidité et d'analyse structurelle
 * 
 * Cette classe gère l'analyse structurelle d'un bâtiment en utilisant la méthode des éléments finis.
 * Elle calcule les matrices de rigidité, gère les nœuds et les éléments de structure,
 * et prend en compte les charges de vent et les propriétés mécaniques des matériaux.
 */
class Matrix
{
public:
	/**
	 * @brief Constructeur de la classe Matrix
	 * @param data Données structurelles organisées en map tri-dimensionnelle
	 * @param Wind Booléen indiquant si les charges de vent sont considérées (défaut: true)
	 * @param W_Directino Direction du vent ("right", "left", etc.) (défaut: "right")
	 * @param scale Échelle de représentation (défaut: 50)
	 * @param angle_tolerance Tolérance angulaire en degrés (défaut: 5)
	 * @param YoungModule Module d'Young du matériau (défaut: 1)
	 * @param Inertie_equivalent Inertie équivalente de la section (défaut: 0.05022003)
	 */
	Matrix(unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, 
		bool Wind = true, string W_Directino = "right", int scale = 50, double angle_tolerance = 5, 
		double YoungModule = 1, double Inertie_equivalent = 0.05022003);

protected:
	/** @brief Données structurelles organisées en map tri-dimensionnelle */
	unordered_map<string, unordered_map<string, unordered_map<string, double>>> data; 

	/** @brief Méthode d'initialisation interne */
	void inits(); 

	/** @brief Valeur des force cumule de Haut en bas d'une meme section sur chaque section*/
	unordered_map<string, double> Wind; 

	/**
	 * @brief Retourne la liste des nœuds ignorés dans l'analyse
	 * @return Vecteur des identifiants des nœuds ignorés
	 */
	vector<string> Ignored_nodes();

	/**
	 * @brief Retourne les nœuds mobiles organisés par étage
	 * @return Vecteur de vecteurs contenant les nœuds mobiles par étage
	 */
	vector<vector<string>> MovingNodes() const; 

	/**
	 * @brief Obtient la colonne de déplacement pour un nœud donné (poteau)
	 * @param node_name Nom du nœud
	 * @return Identifiant de la colonne de déplacement
	 */
	string GetMoveColumn(string node_name); // Poteau

	/**
	 * @brief Obtient le choix de poutre pour un nœud donné
	 * @param node_name Nom du nœud
	 * @param PreviousBeam Nom de la poutre précédente
	 * @return Identifiant de la poutre choisie
	 */
	string GetBeamChoce(string node_name, string PreviousBeam) const; // Poutre 

	/**
	 * @brief Obtient toutes les colonnes de déplacement pour un nœud donné
	 * @param node_name Nom du nœud
	 * @return Vecteur des identifiants de colonnes de déplacement
	 */
	vector<string> GetAllMoveColumn(string node_name); 

	vector<string> Inconue;  

public: 
	/** @brief Map des nœuds de la structure */
	std::unique_ptr<unordered_map<string, std::unique_ptr<Node>>> ND;

	/** @brief Map des moments d'encastrement */
	std::unique_ptr<unordered_map<string, std::unique_ptr<MomemtEncastrement>>> LINE;

	/** @brief Booléen indiquant si les charges de vent sont considérées */
	bool With_Wind;

	/** @brief Direction du vent */
	string Direction; 

	/** @brief Échelle de représentation */
	int ECHELLE; 

	/** @brief Module d'Young du matériau */
	double E; 

	/** @brief Inertie équivalente de la section */
	double I_equi; 

	/** @brief Angle de tolérance en degrés */
	double teta; // angle de tolerance 

	/** @brief Nombre total de nœuds dans la structure */
	int total_noeuds; 

	/** @brief Nombre total de lignes (éléments) dans la structure */
	int total_lignes; 

	/** @brief Vecteur des nœuds de la matrice */
	vector<string> Matrix_nodes;

	/** @brief Vecteur des nœuds verticaux */
	vector<string> V_nodes;

	/** @brief Vecteur des nœuds horizontaux */
	vector<string> H_nodes;

	/** @brief Map des étages avec leurs nœuds associés */
	unordered_map<string, vector<string>> floor; 

	/** @brief Map des sections avec leurs nœuds associés */
	unordered_map<string, vector<string>> section;  

	/** @brief Taille de la matrice de rigidité */
	int RigidityMatrixSize; 

	/**
	 * @brief Calcule et retourne la matrice de rigidité de la structure
	 * @return Matrice de rigidité Eigen::MatrixXd
	 */
	vector<vector<double>>  RigidityMatrix();
	vector<vector<double>> Matrice_de_rigidite; 

	vector<vector<double>>  SecondMember();
	vector<vector<double>> second_membre; 

	map<string, double> Second_Membres_Key_Val; 
	map<string, double> Solution;  
};

#endif // !MATRIX
