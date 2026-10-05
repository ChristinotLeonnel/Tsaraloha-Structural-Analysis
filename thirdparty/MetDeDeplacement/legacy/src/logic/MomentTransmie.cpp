#include "MomentTransmie.h"
#include <algorithm>


MomentTransmie::MomentTransmie(string line_id, 
    unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, 
    map<string, double> solution_matrice, vector<vector<string>> MoveCulumn,
    double angle_tolerance, int scale, double YoungModule, double Inertie_equivalent) :

    MomemtEncastrement(line_id, data, angle_tolerance, scale, YoungModule, Inertie_equivalent),
    solution_matrice(solution_matrice), MoveCulumn(MoveCulumn)
{
    INIT(); 
}

void MomentTransmie::INIT()
{
    double gamma = 0;
    // Recherche de la valeur gamma
    if (is_column) {
        for (const auto& move : MoveCulumn) {
            if (std::find(move.begin(), move.end(), line_key) != move.end()) {
                for (const auto& item : move) {
                    gamma = solution_matrice["H_" + to_string(MomemtEncastrement(item, data).j)];
                    if (gamma != 0) {
                        break; // On sort de la boucle dès qu'on trouve un gamma non nul
					}
                }
                break; // On sort de la boucle principale dès qu'on trouve un move correspondant
            }
        }
    }
    
    if (Node(i, data).type == "rotule") {
        Mij["M_" + to_string(i)] = 0;
        Mij["M_" + to_string(j)] = M["M_" + to_string(j)] + K * solution_matrice["nd_" + to_string(j)] - K * gamma;
    }
    else if (Node(j, data).type == "rotule") {
        Mij["M_" + to_string(j)] = 0;
        Mij["M_" + to_string(i)] = M["M_" + to_string(i)] + K * solution_matrice["nd_" + to_string(i)] - K * gamma;
    }
    else {
        Mij["M_" + to_string(i)] = M["M_" + to_string(i)] + K * solution_matrice["nd_" + to_string(i)] + 0.5 * K * solution_matrice["nd_" + to_string(j)] - K * 1.5 * gamma;
        Mij["M_" + to_string(j)] = M["M_" + to_string(j)] + K * solution_matrice["nd_" + to_string(j)] + 0.5 * K * solution_matrice["nd_" + to_string(i)] - K * 1.5 * gamma;
    }

    if (is_consol) {
        if (data["noeuds"]["M_" + to_string(j)]["x"] > data["noeuds"]["M_" + to_string(i)]["x"]) {
            Mij["M_" + to_string(j)] = charge * length * length / 2;
            Mij["M_" + to_string(i)] = 0;
        }
        else {
            Mij["M_" + to_string(i)] = charge * length * length / 2;
            Mij["M_" + to_string(j)] = 0;
        }
    }
}
