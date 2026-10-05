#pragma once
#ifndef __MomentTransmie__
#define __MomentTransmie__

#include "node.h"
#include "MomentEncastrementParfait.h"

#include <map>
#include <unordered_map>

using std::unordered_map; 
using std::map; 

const double LAMBDA = 0.5; 


class MomentTransmie : public MomemtEncastrement
{
public:
	MomentTransmie(string line_id,
		unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, 
		map<string, double> solution_matrice, vector<vector<string>> MoveCulumn,
		double angle_tolerance = 5, int scale = 50, double YoungModule = 1, 
		double Inertie_equivalent = 0.05022003 );
	
	map<string, double> Mij; 

private:
	void INIT(); 

protected:
	map<string, double> solution_matrice;
	vector<vector<string>> MoveCulumn;


};


#endif // !MomentTransmie
