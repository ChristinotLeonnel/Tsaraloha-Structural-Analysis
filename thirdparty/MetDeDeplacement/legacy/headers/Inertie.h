#pragma once
#ifndef INERTIE
#define INERTIE

#include <string>
#include <cmath>
#include <sstream>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Inertie
{
public:
	Inertie(std::string shape, std::string dimension);
	
	double I; 
private:
	// Méthodes pour calculer l'inertie
	double calculerInertie();
	double calculerInertieRectangulaire(double largeur, double hauteur);
	double calculerInertieCirculaire(double rayon);
	
	// Méthodes pour parser les dimensions
	bool parserDimensions(double& dim1, double& dim2);
	double parserRayon();

	std::string shape; 
	std::string Dimention; 
};

#endif // !INERTIE

