#include "Inertie.h"

Inertie::Inertie(std::string shape, std::string dimension) : shape(shape), Dimention(dimension)
{
	I = calculerInertie();
	if (I == 0.0)
	{
		std::cout << "Erreur dans le calcul de l'inertie pour la forme: " << shape << " avec les dimensions: " << Dimention << std::endl;
	}
}

double Inertie::calculerInertie()
{
	if (shape == "rectangular")
	{
		double largeur, hauteur;
		if (parserDimensions(largeur, hauteur))
		{
			return calculerInertieRectangulaire(largeur, hauteur);
		}
	}
	else if (shape == "circular")
	{
		double rayon = parserRayon();
		if (rayon > 0)
		{
			return calculerInertieCirculaire(rayon);
		}
	}
	
	std::cout << "Forme non reconnue ou dimensions invalides" << std::endl;
	return 0.0;
}

double Inertie::calculerInertieRectangulaire(double largeur, double hauteur)
{
	// Inertie d'un rectangle : I = (b * h^3) / 12
	// où b = largeur, h = hauteur
	return (largeur*0.01 * pow(hauteur*0.01, 3)) / 12.0;
}

double Inertie::calculerInertieCirculaire(double rayon)
{
	// Inertie d'un cercle : I = (π * r^4) / 4
	// où r = rayon
	return (M_PI * pow(rayon*0.01, 4)) / 4.0;
}

bool Inertie::parserDimensions(double& largeur, double& hauteur)
{
	// Format attendu: "45x58"
	size_t pos = Dimention.find('x');
	if (pos != std::string::npos)
	{
		std::string largeurStr = Dimention.substr(0, pos);
		std::string hauteurStr = Dimention.substr(pos + 1);
		
		try {
			largeur = std::stod(largeurStr);
			hauteur = std::stod(hauteurStr);
			return true;
		}
		catch (const std::exception& e) {
			std::cout << "Erreur lors du parsing des dimensions: " << e.what() << std::endl;
			return false;
		}
	}
	return false;
}

double Inertie::parserRayon()
{
	// Format attendu: "Rx54" où R = rayon, x = séparateur, 54 = valeur du rayon
	size_t pos = Dimention.find('x');
	if (pos != std::string::npos && pos > 0)
	{
		std::string rayonStr = Dimention.substr(pos + 1);
		try {
			return std::stod(rayonStr);
		}
		catch (const std::exception& e) {
			std::cout << "Erreur lors du parsing du rayon: " << e.what() << std::endl;
			return -1.0;
		}
	}
	
	// Si pas de 'x', essayer de parser directement le nombre après 'R'
	if (Dimention.length() > 1 && Dimention[0] == 'R')
	{
		std::string rayonStr = Dimention.substr(1);
		try {
			return std::stod(rayonStr);
		}
		catch (const std::exception& e) {
			std::cout << "Erreur lors du parsing du rayon: " << e.what() << std::endl;
			return -1.0;
		}
	}
	
	return -1.0;
} 