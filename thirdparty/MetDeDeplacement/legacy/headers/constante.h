#pragma once
#ifndef Constante
#define Constante

#include "line.h"
#include "node.h"
#include "Inertie.h" 

struct Gamma {
	bool signe;
	bool condition_a; 
	double gamma;
	double a; 
};

class ConstanteLine : public Line  
{
	using Line::Line; 

public:
	ConstanteLine(int id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, 
		double angle_tolerance = 5, int scale = 50, double YoungModule = 1, double Inertie_equivalent = 0.05022003);
	
	ConstanteLine(string id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, 
		double angle_tolerance = 5, int scale = 50, double YoungModule = 1, double Inertie_equivalent = 0.05022003);

public: 
	double E; // Module de Young
	double I_equivalent; // Inertie équivalente
	double I; // Inetie 
	double alpha; // rapport entre l'inertie et l'inertie équivalente
	double K; // costante de rigidite 
	double a; // a = L / (3 * E * alpha)
	double b; // b = L / (6 * E * alpha)
	double c; // c = L / (3 * E * alpha)
	vector<Gamma> GAMMA; // vecteur des valeurs de gamma
	unordered_map<string, string> real_nodes; // Mappage des nœuds réels associés à cette ligne

private:
	void init(); // Initialisation des constantes
	vector<Gamma> gamma(); // Calcul des valeurs de gamma pour les lignes associées


};

#endif // !Constante
