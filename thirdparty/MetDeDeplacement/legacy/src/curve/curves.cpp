#include "curves.h"
#include <thread>
#include <future>
#include <algorithm>

Curves::Curves(unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, bool Wind, string W_Directino, int scale, double angle_tolerance, double YoungModule, double Inertie_equivalent, double precision) :
	Verification(data, Wind, W_Directino, scale, angle_tolerance, YoungModule, Inertie_equivalent), precision(precision)
{
	vector<double> vec; 
	string line_key;
	double add; 

	// abscisse de x entre :: 0 < x < L :: division de L en plusieurs morceaux
	for (int i = 0; i < total_lignes; ++i) {
		add = 0;
		while (add < (*LINE_Mij).at("ln_" + to_string(i))->length ) {
			vec.push_back(add);
			add += precision; 
		}
		vec.push_back((*LINE_Mij).at("ln_" + to_string(i))->length);
		alpha["ln_" + to_string(i)] = vec; 
		vec.clear() ;
	}
	
	int compteur = 1;
	double y, x;

	for (int i = 0; i < total_lignes; ++i) { 
		compteur = 0; 
		line_key = "ln_" + to_string(i);

		auto& ln = (*LINE_Mij).at(line_key);

		y = data["noeuds"]["nd_" + to_string(ln->i)]["y"];
		x = data["noeuds"]["nd_" + to_string(ln->i)]["x"];

		AbscisseMomentsCurves[line_key] = alpha[line_key]; 

		if (ln->is_beam and !ln->is_consol) {
			vec.push_back(y); // Y
			for (const auto& k : alpha[line_key]) {

				vec.push_back( (Moment(line_key, k)) / ECHELLE + y ); // Y
				AbscisseMomentsCurves[line_key][compteur] += x; 
				compteur += 1; 
			}

			AbscisseMomentsCurves[line_key].insert(AbscisseMomentsCurves[line_key].begin(), 0 + x);

			vec.push_back(data["noeuds"]["nd_" + to_string(ln->j)]["y"]); // Y
			AbscisseMomentsCurves[line_key].push_back(ln->length + x);
		}
		else if(ln->is_column) {
			vec.push_back(y); // Y
			for (const auto& k : alpha[line_key]) {

				// Effectuer une rotation de PI/2 (90°) autour de l'origine locale (k, Moment)
				{
					double X_local = k;
					double Y_local = Moment(line_key, k) / ECHELLE;
					// Rotation de 90°: X' = -Y, Y' = X
					double X_rot = -Y_local;
					double Y_rot = X_local;
					// Passage en coordonnées globales
					vec.push_back(Y_rot + y); // Y global
					AbscisseMomentsCurves[line_key][compteur] = X_rot + x; // X global
					compteur += 1;
				}
			}
			AbscisseMomentsCurves[line_key].insert(AbscisseMomentsCurves[line_key].begin(), 0 + x);

			vec.push_back( ln->length + y); // Y
			AbscisseMomentsCurves[line_key].push_back(data["noeuds"]["nd_" + to_string(ln->j)]["x"]);
		}
		else {
			y = data["noeuds"]["nd_" + to_string(ln->i)]["y"];
			vec.push_back(y); // Y
			for (const auto& k : alpha[line_key]) {
				vec.push_back((MomentConsol(line_key, k)) / ECHELLE + y ); // Y 
				AbscisseMomentsCurves[line_key][compteur] += x;
				compteur += 1;
			}
			AbscisseMomentsCurves[line_key].insert(AbscisseMomentsCurves[line_key].begin(), 0 + x);

			vec.push_back(data["noeuds"]["nd_" + to_string(ln->j)]["y"]); // Y
			AbscisseMomentsCurves[line_key].push_back(ln->length + x);
		}

		MomentsCurves[line_key] = vec; 
		vec.clear() ;
	}

	vec.clear(); 
	for (int i = 0; i < total_lignes; ++i) {
		compteur = 0;
		line_key = "ln_" + to_string(i);

		auto& ln = (*LINE_Mij).at(line_key);

		y = data["noeuds"]["nd_" + to_string(ln->i)]["y"];
		x = data["noeuds"]["nd_" + to_string(ln->i)]["x"];

		AbscisseShearsCurves[line_key] = alpha[line_key]; 

		vec.push_back(y); // Y

		if (ln->is_beam and !ln->is_consol) {
			for (const auto& k : alpha[line_key]) {

				vec.push_back((EffortTranchant(line_key, k)) / ECHELLE + y); // Y
				AbscisseShearsCurves[line_key][compteur] += x;
				compteur += 1;
			}

			AbscisseShearsCurves[line_key].insert(AbscisseShearsCurves[line_key].begin(), 0 + x);

			vec.push_back(data["noeuds"]["nd_" + to_string(ln->j)]["y"]); // Y
			AbscisseShearsCurves[line_key].push_back(ln->length + x);
		}
		else if (ln->is_column) {
			for (const auto& k : alpha[line_key]) {

				// Effectuer une rotation de PI/2 (90°) autour de l'origine locale (k, Moment)
				{
					double X_local = k;
					double Y_local = EffortTranchant(line_key, k) / ECHELLE;
					// Rotation de 90°: X' = -Y, Y' = X
					double X_rot = -Y_local;
					double Y_rot = X_local;
					// Passage en coordonnées globales
					vec.push_back(Y_rot + y); // Y global
					AbscisseShearsCurves[line_key][compteur] = X_rot + x; // X global
					compteur += 1;
				}
			}
			AbscisseShearsCurves[line_key].insert(AbscisseShearsCurves[line_key].begin(), 0 + x);

			vec.push_back(ln->length + y); // Y
			AbscisseShearsCurves[line_key].push_back(data["noeuds"]["nd_" + to_string(ln->j)]["x"]);
		}
		else {
			y = data["noeuds"]["nd_" + to_string(ln->i)]["y"];
			for (const auto& k : alpha[line_key]) {
				vec.push_back((EffortTranchantConsol(line_key, k)) / ECHELLE + y); // Y 
				AbscisseShearsCurves[line_key][compteur] += x;
				compteur += 1;
			}

			AbscisseShearsCurves[line_key].insert(AbscisseShearsCurves[line_key].begin(), 0 + x);

			vec.push_back(data["noeuds"]["nd_" + to_string(ln->j)]["y"]); // Y
			AbscisseShearsCurves[line_key].push_back(ln->length + x);
		}

		ShearsCurves[line_key] = vec;
		vec.clear();
	}

	for (int i = 0; i < total_lignes; ++i) {
		line_key = "ln_" + to_string(i); 
		AnalyseCourbe[line_key] = CurvesAnalisys(line_key); 
	}
	
}

double Curves::mu(double x, double q, double l)
{
	return  - q*l*x/2 + q*x*x/2;
}

double Curves::to(double x, double q, double l)
{
	return - q * l / 2 + q * x;
}

double Curves::Moment(string& line, double x)
{
	auto& ln = (*LINE_Mij).at(line);
	if (ln->is_column) MUU = -mu(x, ln->charge, ln->length);
	else MUU = mu(x, ln->charge, ln->length);

	return  - (MUU - (1 - x / ln->length) * ln->Mij["M_" + to_string(ln->i)] + ln->Mij["M_" + to_string(ln->j)] * x / ln->length);
}

double Curves::MomentConsol(string& line, double x)
{
	auto& ln = (*LINE_Mij).at(line);
	double q = ln->charge; 
	if ((*ND).at("nd_" + to_string(ln->i))->type == "fix") {
		return - q * pow(ln->length - x, 2) / 2;
	}
	else {
		return - q * pow(x, 2) / 2;
	}
}

double Curves::EffortTranchant(string& line, double x)
{
	auto& ln = (*LINE_Mij).at(line);
	return - (to(x, ln->charge, ln->length) + (ln->Mij["M_" + to_string(ln->j)] + ln->Mij["M_" + to_string(ln->i)]) / ln->length);
}

double Curves::EffortTranchantConsol(string& line, double x)
{
	auto& ln = (*LINE_Mij).at(line);
	double q = ln->charge;
	if ((*ND).at("nd_" + to_string(ln->i))->type == "fix") {
		return - q * (ln->length - x) ;
	}
	else {
		return - q * x ;
	}
}

map<string, double> Curves::CurvesAnalisys(string& line)
{
	auto& ln = (*LINE_Mij).at(line); 
	double a = ln->charge / 2;
	double b = (ln->Mij["M_" + to_string(ln->i)] + ln->Mij["M_" + to_string(ln->j)]) / ln->length - ln->charge * ln->length / 2;
	double c = - ln->Mij["M_" + to_string(ln->i)]; 
	double DELTA = b * b - 4 * a * c;

	double x_1 = 0;
	double x_2 = 0; 

	double x_max = 0; 
	double Mmax = 0; 
	
	if (DELTA >= 0) {

		x_1 = (-b - sqrt(DELTA)) / (2 * a); 
		x_2 = (-b + sqrt(DELTA)) / (2 * a);
		// Correction ligne 259 : remplacer (1 / ln->charge) par (1.0 / ln->charge) pour éviter la division entière
		x_max = (1.0 / ln->charge) * (ln->charge * ln->length / 2 - (ln->Mij["M_" + to_string(ln->i)] + ln->Mij["M_" + to_string(ln->j)]) / ln->length);
		Mmax = Moment(line, x_max); 

		if (x_1 < 0 or x_1 > ln->length) {
			x_1 = 0; 
		}
		if (x_2 < 0 or x_2 > ln->length) {
			x_2 = 0;
		}
		if (x_max < 0 or x_max > ln->length) {
			x_max = 0;
			Mmax = 0; 
		}
	}

	double	y = data["noeuds"]["nd_" + to_string(ln->i)]["y"];
	double	x = data["noeuds"]["nd_" + to_string(ln->i)]["x"];

	if (ln->is_beam) {

		return { {"x_1", x_1} , {"x_2" , x_2} , {"x_en_travee_max", x_max}, {"M_en_travee_max", Mmax} , 
			{"M_i" , ln->Mij["M_"+ to_string(ln->i)]}, {"M_j" , ln->Mij["M_" + to_string(ln->j)]}, 
			{"coo_X", x_max + x}, {"coo_y", Mmax / ECHELLE + y }, 
			{"ET_i", EffortTranchant(line, 0)}, {"ET_j", EffortTranchant(line, ln->length)} }; 
	}
	else {
		return { {"x_1", x_1} , {"x_2" , x_2} , {"x_en_travee_max", x_max}, {"M_en_travee_max", Mmax} ,
		{"M_i" , ln->Mij["M_" + to_string(ln->i)]}, {"M_j" , ln->Mij["M_" + to_string(ln->j)]},
		
		{"coo_X", Mmax / ECHELLE - data["noeuds"]["nd_" + to_string(ln->j)]["x"] }, {"coo_y", x_max + data["noeuds"]["nd_" + to_string(ln->j)]["y"] },
		
		{"ET_i", EffortTranchant(line, 0)}, {"ET_j", EffortTranchant(line, ln->length)} };
	}

}
