#include "MomentEncastrementParfait.h"


// Helper function to split string by delimiter and get last element
static string getLastElementAfterSplit(const string& str, char delimiter) {
    std::stringstream ss(str);
    string item;
    string last_item;
    
    while (getline(ss, item, delimiter)) {
        last_item = item;
    }
    
    return last_item;
}


MomemtEncastrement::MomemtEncastrement(int id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, 
	double angle_tolerance, int scale, double YoungModule, double Inertie_equivalent) : 
	ConstanteLine(id, data, angle_tolerance, scale, YoungModule, Inertie_equivalent)
{
	init_1();
}

MomemtEncastrement::MomemtEncastrement(string id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, 
	double angle_tolerance, int scale, double YoungModule, double Inertie_equivalent) : 
	ConstanteLine(id, data, angle_tolerance, scale, YoungModule, Inertie_equivalent)
{
	init_1();
}

void MomemtEncastrement::init_1()
{
    nomage_extremite(); 

    if (type_force == "rectangular") {
		M = rectangular();
    }
    else if (type_force == "Rotation") {
        for (auto& T : GAMMA) {
            auto result = Rotation(T);
            M["M_" + std::to_string(i)] += result["M_" + std::to_string(i)];
            M["M_" + std::to_string(j)] += result["M_" + std::to_string(j)];
        }
    }
    else {
        // std::cerr << "Erreur: Type de force non reconnu pour le moment d'encastrement." << std::endl;
        M["M_" + std::to_string(i)] = 0;
        M["M_" + std::to_string(j)] = 0; 
	}

    if (not GAMMA.empty()) {
        for (auto& T : GAMMA) {
            auto result = To(T);
            Tork["To_" + std::to_string(i)] += result["To_" + std::to_string(i)];
            Tork["To_" + std::to_string(j)] += result["To_" + std::to_string(j)];
        }
    }else {
        Tork["To_" + std::to_string(i)] = charge * length / 2;
		Tork["To_" + std::to_string(j)] = charge * length / 2;
	}
    appui_fictif = Tork["To_" + std::to_string(i)]; 
}

unordered_map<string, double> MomemtEncastrement::rectangular()
{
    unordered_map<string, double> M ;
    if (is_column) {
        M = { {"M_" + std::to_string(i), charge * length * length / 12},
              {"M_" + std::to_string(j), -charge * length * length / 12} };
    }

    else {
        M = { {"M_" + std::to_string(i), -charge * length * length / 12},
        {"M_" + std::to_string(j), charge * length * length / 12} };
    }
    return M;
}

unordered_map<string, double> MomemtEncastrement::Rotation(Gamma T)
{
    if (is_column) T.gamma = std::abs(T.gamma);

    return { {"M_" + std::to_string(i), -(T.gamma / pow(length, 2)) * (length*length - 4 * T.a * T.a) },
             {"M_" + std::to_string(j), (T.gamma / pow(length, 2)) * (3 * T.a * T.a - 2 *T.a * length) } };
}

unordered_map<string, double> MomemtEncastrement::To(Gamma T)
{
    return {{"To_" + std::to_string(i), charge * length / 2 + T.gamma / length},
            {"To_" + std::to_string(j), charge * length / 2 - T.gamma / length} };
}

void MomemtEncastrement::nomage_extremite()
{
    if (is_column) {
        double min_x = std::min(node_1["y"], node_2["y"]);
        double max_x = std::max(node_1["y"], node_2["y"]);

        if (std::abs(node_1["y"] - min_x) < 1e-6) {
            i = std::stoi(getLastElementAfterSplit(real_nodes["nd_1"], '_'));
            j = std::stoi(getLastElementAfterSplit(real_nodes["nd_2"], '_'));
        }
        else {
            i = std::stoi(getLastElementAfterSplit(real_nodes["nd_2"], '_'));
            j = std::stoi(getLastElementAfterSplit(real_nodes["nd_1"], '_'));
        }
    }
    else if (is_beam) {
        double min_y = std::min(node_1["x"], node_2["x"]);
        double max_y = std::max(node_1["x"], node_2["x"]);
        if (std::abs(node_1["x"] - min_y) < 1e-6) {
            i = std::stoi(getLastElementAfterSplit(real_nodes["nd_1"], '_'));
            j = std::stoi(getLastElementAfterSplit(real_nodes["nd_2"], '_'));
        }
        else {
            i = std::stoi(getLastElementAfterSplit(real_nodes["nd_2"], '_'));
            j = std::stoi(getLastElementAfterSplit(real_nodes["nd_1"], '_'));
        }
    }
}

