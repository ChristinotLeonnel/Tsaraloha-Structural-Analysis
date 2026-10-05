#pragma once
#ifndef LIGNE
#define LIGNE 

#include <unordered_map>
#include <iostream>
#include <string>
#include <memory>
#include <math.h>
#include <cmath> 
#include <optional>
#include <any>
#include <vector>
#include <tuple>
#include <numbers> 

using std::sqrt; 
using std::string; 
using std::unordered_map;
using std::make_unique; 
using std::unique_ptr; 
using std::pow; 
using std::numbers::pi; 
using std::abs;
using std::atan2;
using std::vector;
using std::to_string; 
using std::optional;
using std::any;
using std::tuple;

// Struct to hold rectangle line intersection results
struct RectangleLineResult {
    bool is_valid;
    optional<bool> positive;
    vector<int> nodes_on_line;
};

// Struct to hold section line middle results
struct SectionLineResult {
    bool is_in_middle = false;
    string dimensions = "";
};


struct SectionLine {
	string type; 
	string dimensions;
	int id; 
};

struct Force {
	double Intensiter;
	string type;
};

class Line

{
public:
	Line(int id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, double angle_tolerance = 5, int scale = 50);
	Line(string id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, double angle_tolerance = 5, int scale = 50);
	
	unordered_map<string, double> node_1;
	unordered_map<string, double> node_2;

	
	string line_key;
	double length; 
	bool is_column;
	bool is_beam;
	bool is_inclined;
	double inclination_angle;
	unordered_map<string, vector<string>> associated_lines; 
	bool is_consol; 
	SectionLine forme;
	double charge; 
	string type_force; 

protected: 
	unordered_map<string, unordered_map<string, unordered_map<string, double>>> data;
	int id;
	string id_str;

private:
	double tolerance;
	int scale;  
	unordered_map<string, vector<string>> find_associated_lines();
	bool nodes_approx_equal(const unordered_map<string, double>& node_1, const unordered_map<string, double>& node_2, double tolerance);
    bool check_is_consol() const;
	bool is_column_type(unordered_map<string, double> node_1, unordered_map<string, double> node_2) const;

	bool is_point_on_line(std::vector<double> coordonner);

	bool is_point_one_off_nd_line(std::vector<double> coord_point);
	bool is_rectangle_node_in_line(int id_node_recte, int id_rectangle);
	RectangleLineResult is_rectangle_in_line(int id_rectangle); 
	SectionLineResult is_rectangle_in_middle_of_line(int id_rectangle);
	SectionLineResult is_circle_in_middle_of_line(int id_circle);
	SectionLine section(); 
	Force force();
	void initialisation(); 
};


#endif // !LIGNE
