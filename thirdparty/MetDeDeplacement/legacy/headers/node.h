#pragma once
#ifndef NODE
#define NODE

#include "line.h"

struct Deplacement {
	bool deplacable = false;
	bool horizontale = false;
	bool verticale = false;
	bool incline = false; 
	unordered_map<string, unordered_map<string, bool>> val;
};

struct APN {
	Line line;
	string node; 
};

class Node
{
public:
	Node(int id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data);

	Node(string id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data);

	int id; 
	string id_string; 

private:
	unordered_map<string, unordered_map<string, unordered_map<string, double>>> data;
	vector<vector<string>> line_with(); 
	bool is_in_cirlce(int id_cirle);
	string name_type(); 
	Deplacement appuis_fictif(); 
	void initilisation(); 

public:
	string node_key; 
	string node_key_invered;
 	vector<vector<string>> ligne_associer;
	string type;
	Deplacement support_fictif;  

};

#endif // !NODE
