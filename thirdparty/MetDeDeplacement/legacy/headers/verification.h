#pragma once
#ifndef __Verification__
#define __Verification__

#include "matrix.h"
#include "MomentTransmie.h"

class Verification : public Matrix {

	using Matrix::Matrix; 

public:
	Verification(unordered_map<string, unordered_map<string, unordered_map<string, double>>> data,
		bool Wind = true, string W_Directino = "right", int scale = 50, double angle_tolerance = 5,
		double YoungModule = 1, double Inertie_equivalent = 0.05022003);

	map<string, double> VERDICTE;

protected:
	std::unique_ptr<unordered_map<string, std::unique_ptr<MomentTransmie>>> LINE_Mij;
};

#endif // !Verification
