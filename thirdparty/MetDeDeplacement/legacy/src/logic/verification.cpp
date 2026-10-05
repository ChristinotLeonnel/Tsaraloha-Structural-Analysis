#include "verification.h"

static string getLastElementAfterSplit(const string& str, char delimiter) {
    std::stringstream ss(str);
    string item;
    string last_item;

    while (getline(ss, item, delimiter)) {
        last_item = item;
    }

    return last_item;
}

Verification::Verification(unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, bool Wind, 
	string W_Directino, int scale, double angle_tolerance, double YoungModule, double Inertie_equivalent) : 
	Matrix(data, Wind, W_Directino, scale, angle_tolerance, YoungModule, Inertie_equivalent) 
{	
	LINE_Mij = std::make_unique<unordered_map<string, std::unique_ptr<MomentTransmie>>>();
	vector<vector<string>> AllCulumnMove; 
 	for (const auto& H_nd : H_nodes) {
		AllCulumnMove.push_back(GetAllMoveColumn(H_nd));
	}

	for (int i = 0; i < total_lignes; ++i) {
        (*LINE_Mij)["ln_" + to_string(i)] = std::make_unique<MomentTransmie>("ln_" + to_string(i), data, Solution, AllCulumnMove, angle_tolerance, scale, YoungModule, Inertie_equivalent);
	}

    double val = 0;
    for (const auto& node_mat : Matrix_nodes) {
        val = 0; 
        Node node_obj(node_mat, data);
        for (const auto& line : node_obj.ligne_associer) {

            auto& line_obj = (*LINE_Mij).at(line[0]); 

            if (!line_obj->is_consol) {
                val += line_obj->Mij["M_"+getLastElementAfterSplit(node_mat, '_')];
            }
        }
        VERDICTE[node_mat] = val; 
    }

}
