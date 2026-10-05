#pragma once
#ifndef CURVES
#define CURVES

#include "verification.h"

class Curves : public Verification
{
	using Verification::Verification;

public:
	Curves(unordered_map<string, unordered_map<string, unordered_map<string, double>>> data,
		bool Wind = true, string W_Directino = "right", int scale = 50, double angle_tolerance = 5,
		double YoungModule = 1, double Inertie_equivalent = 0.05022003, double precision = 0.001f);
	
private:

	double mu(double x, double q, double l);
	double to(double x, double q, double l);

	double Moment(string& line, double x);
	double MomentConsol(string& line, double x);
	double EffortTranchant(string& line, double x);
	double EffortTranchantConsol(string& line, double x);
	map<string, double> CurvesAnalisys(string& line);


	int precision; 
	double MUU;


public: 
	map<string,vector<double>> alpha; 

	map<string, vector<double>> MomentsCurves; 
	map<string, vector<double>> AbscisseMomentsCurves;

	map<string, vector<double>> ShearsCurves;
	map<string, vector<double>> AbscisseShearsCurves;
	map<string, map<string, double>> AnalyseCourbe; 


};


#endif // !CURVES
