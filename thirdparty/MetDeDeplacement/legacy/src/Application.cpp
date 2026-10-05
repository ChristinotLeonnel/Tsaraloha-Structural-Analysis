#include <cstdlib> 
#include <fstream>
#include <chrono>
#include <thread>
#include <future>
#include <vector>

#include "Application.h"
#include "output.h"

// Fonction pour traiter un portique individuel
static void processPortique(int portique_index, const json& ff, std::mutex& output_mutex) {
    try {
        std::string jsonFilePath = ff["Truss " + std::to_string(portique_index)];
        
        std::ifstream file(jsonFilePath);
        if (!file.is_open()) {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cerr << "Erreur: Impossible d'ouvrir le fichier " << jsonFilePath << std::endl;
            return;
        }

        json data;
        file >> data;
        file.close();

        double Ieq = ConstanteLine("ln_0", data).I;
        Output Mat = Output(jsonToNestedMap(data), true, "right", 50, 5, 1, Ieq, 0.01, ff["output_path"], 
                                        ff["Truss Name " + std::to_string(portique_index)]);

        // Afficher les résultats avec synchronisation
        {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cout << "\n=== Portique " << portique_index << " ===" << std::endl;
            std::cout << "Matrice de rigidité :" << std::endl;
            
            for (const auto& row : Mat.Matrice_de_rigidite) {
                for (double val : row) {
                    std::cout << std::setw(10) << std::fixed << std::setprecision(7) << val << " ";
                }
                std::cout << "\n";
            }

            std::cout << "\nSecond Membre :: \n";
            for (const auto& row : Mat.second_membre) {
                for (double val : row) {
                    std::cout << std::setw(10) << std::fixed << std::setprecision(7) << val << " ";
                }
                std::cout << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cerr << "Erreur lors du traitement du portique " << portique_index << ": " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Equivalent C++ code to read the first .json file in the current directory and load it to ff
	json f;
    json ff;
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.path().extension() == ".json") {
            std::ifstream jsonfile(entry.path());
            if (jsonfile.is_open()) {
                jsonfile >> f;
                jsonfile.close();
                break;
            }
        }
    } 

    json jj;
    if (f.contains("output_path")) {
        std::string jj_path = f["output_path"].get<std::string>() + "/TrussAnalysed.json";
        std::ifstream jj_file(jj_path);
        if (jj_file.is_open()) {
            try {
                jj_file >> jj;
                ff = jj;
            } catch (const std::exception& e) {
                std::cerr << "Erreur lors de la lecture du fichier JSON à '" << jj_path << "' : " << e.what() << "\n";
            }
            jj_file.close();
        } else {
            std::cerr << "Impossible d'ouvrir le fichier JSON à '" << jj_path << "'\n";
        }
    } else {
        std::cerr << "Champ 'output_path' non trouvé dans le JSON de configuration d'entrée.\n";
    }
    
    auto start = std::chrono::high_resolution_clock::now();

    // Traitement parallélisé des portiques
    int nombre_courbes = ff["Truss Number"];
    std::mutex output_mutex;
    
    // Limiter le nombre de threads pour éviter la surcharge
    const int max_threads = std::min(static_cast<int>(std::thread::hardware_concurrency()), nombre_courbes);
    const int portiques_per_thread = nombre_courbes / max_threads;
    
    std::vector<std::future<void>> futures;
    
    // Traiter les portiques en parallèle
    for (int t = 0; t < max_threads; ++t) {
        int start_portique = t * portiques_per_thread;
        int end_portique = (t == max_threads - 1) ? nombre_courbes : (t + 1) * portiques_per_thread;
        
        futures.push_back(std::async(std::launch::async, [start_portique, end_portique, &ff, &output_mutex]() {
            for (int i = start_portique; i < end_portique; ++i) {
                processPortique(i, ff, output_mutex);
            }
        }));
    }
    
    // Attendre que tous les threads se terminent
    for (auto& future : futures) {
        future.wait();
    }

   auto end = std::chrono::high_resolution_clock::now();
   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

   std::cout << "\n" << duration << " milliseconde";
   
   return 0; 
}
