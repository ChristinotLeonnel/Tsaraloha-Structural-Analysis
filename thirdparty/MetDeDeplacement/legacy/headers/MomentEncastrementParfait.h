#pragma once
#ifndef MomenEncastrement
#define MomenEncastrement

#include "constante.h"
#include <unordered_map>
#include <string>
#include <vector>

#include <sstream>
#include <iostream>

using std::vector;
using std::unordered_map;
using std::string;

class MomemtEncastrement : public ConstanteLine
{
	using ConstanteLine::ConstanteLine;

public:
	MomemtEncastrement(int id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data,
		double angle_tolerance = 5, int scale = 50, double YoungModule = 1, double Inertie_equivalent = 0.05022003);

	MomemtEncastrement(string id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data,
		double angle_tolerance = 5, int scale = 50, double YoungModule = 1, double Inertie_equivalent = 0.05022003);

	int i, j; 
	unordered_map<string, double> M; // Map to hold moment values
	unordered_map<string, double> Tork; // Map to hold torque values
	double appui_fictif;  

private:
	void init_1(); // Initialisation sp�cifique pour le moment d'encastrement
	unordered_map<string, double> rectangular(); 
	unordered_map<string, double> Rotation(Gamma T);
	unordered_map<string, double> To(Gamma T);
	void nomage_extremite();
};

#endif // !MomenEncastrement
