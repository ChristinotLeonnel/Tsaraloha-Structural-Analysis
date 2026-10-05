#include "matrix.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <utility>
#include <algorithm>
#include <cmath>
#include <thread>
#include <future>

// For std::invalid_argument and std::runtime_error
#include <stdexcept>

using std::unordered_map;
using std::string;
using std::vector;

static inline string inverse_node(const string& node) {
	if (node == "nd_1") return "nd_2";
	else return "nd_1";
}

template <typename T>
static T min_of_vector(const vector<T>& vec) {
	if (vec.empty()) throw std::invalid_argument("Vector is empty");
	return *std::min_element(vec.begin(), vec.end());
}

template <typename T, typename U>
T key_by_value(unordered_map<T, U>& myMap, const U &value) {
	for (const auto& pair : myMap) {
        if (pair.second == value) {
            return pair.first;
        }
    }
	return T();
}

static string TNT(const string& key, const vector<vector<string>>& Vect, int key_idx = 0, int val_idx = 1) {
	for (const auto& i : Vect) {
		if (i[key_idx] == key) return i[val_idx];
	}
	return ""; 
}

template <typename T>
static int index_of(const std::vector<T>& vec, const T& value) {
    auto it = std::find(vec.begin(), vec.end(), value);
    if (it != vec.end()) {
        return static_cast<int>(std::distance(vec.begin(), it));
    } else {
        return -1; // valeur non trouvée
    }
}

template <typename T>
static T sum_values(const unordered_map<string, T>& dictionary) {
	T val = 0;
	for (const auto& [key, value] : dictionary) {
		val += value; 
	}
	return val; 
}

constexpr static vector<vector<double>> inverseMatrix(const vector<vector<double>>& mat) {
	int n = static_cast<int>(mat.size());
	if (n == 0) throw std::invalid_argument("La matrice ne doit pas être vide.");
	for (int i = 0; i < n; ++i) {
		if (static_cast<int>(mat[i].size()) != n) throw std::invalid_argument("La matrice doit être carrée.");
	}

	vector<vector<double>> augmented(n, vector<double>(2 * n, 0.0));

	// Créer la matrice augmentée [mat | I]
	for (int i = 0; i < n; ++i) {
		for (int j = 0; j < n; ++j) {
			augmented[i][j] = mat[i][j];
		}
		augmented[i][n + i] = 1.0;
	}

	// Appliquer Gauss-Jordan avec pivot partiel
	for (int i = 0; i < n; ++i) {
		// Pivot partiel: trouver la ligne avec le plus grand pivot en valeur absolue
		int pivot_row = i;
		double max_val = std::abs(augmented[i][i]);
		for (int r = i + 1; r < n; ++r) {
			double val = std::abs(augmented[r][i]);
			if (val > max_val) {
				max_val = val;
				pivot_row = r;
			}
		}
		if (max_val == 0.0) throw std::runtime_error("La matrice est singulière et non inversible.");
		if (pivot_row != i) std::swap(augmented[i], augmented[pivot_row]);

		double pivot = augmented[i][i];
		for (int j = 0; j < 2 * n; ++j) {
			augmented[i][j] /= pivot;
		}

		// Éliminer les autres lignes
		for (int k = 0; k < n; ++k) {
			if (k != i) {
				double factor = augmented[k][i];
				for (int j = 0; j < 2 * n; ++j) {
					augmented[k][j] -= factor * augmented[i][j];
				}
			}
		}
	}

	// Extraire la matrice inverse
	vector<vector<double>> inverse(n, vector<double>(n, 0.0));
	for (int i = 0; i < n; ++i) {
		for (int j = 0; j < n; ++j) {
			inverse[i][j] = augmented[i][j + n];
		}
	}

	return inverse;
}

constexpr static vector<vector<double>> matrixProduct(const vector<vector<double>>& A, const vector<vector<double>>& B) {
	if (A.empty() || B.empty()) throw std::invalid_argument("Les matrices ne doivent pas être vides.");
	int rowsA = static_cast<int>(A.size());
	int colsA = static_cast<int>(A[0].size());
	int rowsB = static_cast<int>(B.size());
	int colsB = static_cast<int>(B[0].size());

	if (colsA != rowsB) {
		throw std::invalid_argument("Le nombre de colonnes de A doit être égal au nombre de lignes de B.");
	}

	vector<vector<double>> result(rowsA, vector<double>(colsB, 0.0));

	for (int i = 0; i < rowsA; ++i) {
		for (int j = 0; j < colsB; ++j) {
			for (int k = 0; k < colsA; ++k) {
				result[i][j] += A[i][k] * B[k][j];
			}
		}
	}

	return result;
}

// Version parallélisée de la multiplication de matrices
static vector<vector<double>> matrixProductParallel(const vector<vector<double>>& A, const vector<vector<double>>& B) {
	if (A.empty() || B.empty()) throw std::invalid_argument("Les matrices ne doivent pas être vides.");
	int rowsA = static_cast<int>(A.size());
	int colsA = static_cast<int>(A[0].size());
	int rowsB = static_cast<int>(B.size());
	int colsB = static_cast<int>(B[0].size());

	if (colsA != rowsB) {
		throw std::invalid_argument("Le nombre de colonnes de A doit être égal au nombre de lignes de B.");
	}

	vector<vector<double>> result(rowsA, vector<double>(colsB, 0.0));

	// Utiliser des threads pour paralléliser la multiplication
	const int num_threads = std::min(static_cast<int>(std::thread::hardware_concurrency()), rowsA);
	const int rows_per_thread = rowsA / num_threads;
	
	std::vector<std::future<void>> futures;
	
	for (int t = 0; t < num_threads; ++t) {
		int start_row = t * rows_per_thread;
		int end_row = (t == num_threads - 1) ? rowsA : (t + 1) * rows_per_thread;
		
		futures.push_back(std::async(std::launch::async, [&A, &B, &result, start_row, end_row, colsA, colsB]() {
			for (int i = start_row; i < end_row; ++i) {
				for (int j = 0; j < colsB; ++j) {
					for (int k = 0; k < colsA; ++k) {
						result[i][j] += A[i][k] * B[k][j];
					}
				}
			}
		}));
	}
	
	// Attendre que tous les threads se terminent
	for (auto& future : futures) {
		future.wait();
	}

	return result;
}


Matrix::Matrix(unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, 
	bool Wind, string W_Directino, int scale, double angle_tolerance, double YoungModule, double Inertie_equivalent) :
	data(data), With_Wind(Wind), Direction(W_Directino), ECHELLE(scale), teta(angle_tolerance), 
	E(YoungModule), I_equi(Inertie_equivalent), LINE(nullptr), ND(nullptr) 
{
	ND = std::make_unique<unordered_map<string, std::unique_ptr<Node>>>();
	LINE = std::make_unique<unordered_map<string, std::unique_ptr<MomemtEncastrement>>>();

	inits(); 
}

void Matrix::inits()
{
	// Correction : initialisation correcte des pointeurs uniques LINE et ND
	total_lignes = static_cast<int>(data["nombre"]["propriete"]["ligne"]);
	for (int i = 0; i < total_lignes; ++i) {
		(*LINE)["ln_" + to_string(i)] = std::make_unique<MomemtEncastrement>(i, data, teta, ECHELLE, E, I_equi);
	}

	total_noeuds = static_cast<int>(data["nombre"]["propriete"]["noeuds"]);
	for (int i = 0; i < total_noeuds; ++i) {
		(*ND)["nd_" + to_string(i)] = std::make_unique<Node>(i, data);
	}

	// Correction : regrouper les noeuds par valeur de y (niveau), puis les trier dans chaque étage
	std::map<double, std::vector<std::string>> niveaux;
	int total_nodes = static_cast<int>(data["nombre"]["propriete"]["noeuds"]);
	for (int i = 0; i < total_nodes; ++i) {
		double y = data["noeuds"]["nd_" + to_string(i)]["y"];
		niveaux[y].push_back("nd_" + to_string(i));
	}
	int etage = 0;
	for (auto& [y, nodes] : niveaux) {
		floor["floor_" + to_string(etage)] = nodes;
		++etage;
	}

	// Correction : regrouper les noeuds par valeur de x (section), puis les trier dans chaque section
	std::map<double, std::vector<std::string>> niveaux_2; 
	for (int i = 0; i < total_nodes; ++i) {
		double x = data["noeuds"]["nd_" + to_string(i)]["x"];
		niveaux_2[x].push_back("nd_" + to_string(i));
	}
	etage = 0;
	for (const auto& [x, nodes] : niveaux_2) {
		section["section_" + to_string(etage)] = nodes;
		++etage;
	}

	string CC; 
	double  val_W; 
	for (const auto& [pair, value] : section) {
		val_W = 0; 
		if (value.size() >= 2) {
			for (size_t i = value.size() - 1; i > 0; --i) {
				CC = GetMoveColumn(value[i]); 
				if (CC != "None") {
					val_W += (*LINE).at(CC)->charge * (*LINE).at(CC)->length; 
					Wind[CC] = val_W ;
				}
			}
		}
	}

	vector<vector<string>> a = MovingNodes();
	V_nodes = a[1];
	H_nodes = a[2];

	vector<string> b = Ignored_nodes();
	for (int i = 0; i < total_noeuds; ++i) {
		std::string node_name = "nd_" + std::to_string(i);
		// Vérifier si le nœud n'est pas dans Ignored_nodes()
		if (std::find(b.begin(), b.end(), node_name) == b.end()) {
			Matrix_nodes.push_back(node_name);
		}
	}
	
	RigidityMatrixSize = static_cast<int>(Matrix_nodes.size() + H_nodes.size() + V_nodes.size());

	Matrice_de_rigidite = RigidityMatrix(); 
	second_membre = SecondMember();

	vector<vector<double>> Sol = matrixProductParallel(inverseMatrix(Matrice_de_rigidite), second_membre); 

	int compteur = 0;
	for (const auto& node : Matrix_nodes) {
		Solution[node] = Sol[compteur][0];
		Second_Membres_Key_Val[node] = second_membre[compteur][0];
		Inconue.push_back(node); 
		compteur += 1;
	}
	for (const auto& node : H_nodes) {
		Solution["H_" + node.substr(node.find("_") + 1)] = Sol[compteur][0];
		Second_Membres_Key_Val["H_" + node.substr(node.find("_") + 1)] = second_membre[compteur][0];
		Inconue.push_back("H_" + node.substr(node.find("_") + 1));
		compteur += 1;
	}
	for (const auto& node : V_nodes) {
		Solution["V_" + node.substr(node.find("_") + 1)] = Sol[compteur][0];
		Second_Membres_Key_Val["V_" + node.substr(node.find("_") + 1)] = second_membre[compteur][0];

		Inconue.push_back("V_" + node.substr(node.find("_") + 1));
		compteur += 1;
	}
}

vector<string> Matrix::Ignored_nodes()
{
	// Pour chaque valeur de x, trouver le nœud ayant la plus petite valeur de y (le plus bas)
	std::map<double, std::pair<double, std::string>> min_y_per_x; // x -> (y, node_name)
	int total_nodes = static_cast<int>(data["nombre"]["propriete"]["noeuds"]);
	std::string node_name;
	double x, y; 

	for (int i = 0; i < total_nodes; ++i) {

		node_name = "nd_" + to_string(i);

		x = data["noeuds"][node_name]["x"];
		y = data["noeuds"][node_name]["y"];

		if (min_y_per_x.count(x) == 0 || y < min_y_per_x[x].first) {
			min_y_per_x[x] = std::make_pair(y, node_name);
		}
	}

	vector<string> noeuds_plus_bas;
	for (const auto& [x, y_node] : min_y_per_x) {
		noeuds_plus_bas.push_back(y_node.second);
	}
	
	vector<string> noeuds_plus_bas_non_deplacables;
	for (const auto& i : noeuds_plus_bas) {
		auto& node_ptr = (*ND).at(i);
		auto& ln = (*LINE).at(node_ptr->ligne_associer[0][0]);
		if (!node_ptr->support_fictif.deplacable && node_ptr->ligne_associer.size() == 1 && ln->is_column ) {
			noeuds_plus_bas_non_deplacables.push_back(i);
	 	}
		else if (!node_ptr->support_fictif.deplacable && node_ptr->ligne_associer.size() == 1 &&  ln->is_consol) {
			noeuds_plus_bas_non_deplacables.push_back(i);
		}
	}

	for (int i = 0; i < total_nodes; ++i) {
		node_name = "nd_" + to_string(i);
		if ((*ND).at(node_name)->type == "free" and std::find(noeuds_plus_bas_non_deplacables.begin(), noeuds_plus_bas_non_deplacables.end(), node_name) == noeuds_plus_bas_non_deplacables.end()) {
			noeuds_plus_bas_non_deplacables.push_back(node_name);
		}
	}

	return noeuds_plus_bas_non_deplacables;
}

vector<vector<string>> Matrix::MovingNodes() const
{
	vector<string> All;
	vector<string> Vertical;
	vector<string> Horizontal;

	for (auto& [key, values] : (*ND)) {
		if (values->support_fictif.verticale and values->support_fictif.val["vecticale"]["bas"]) {
			Vertical.push_back(key);
			All.push_back(key); 
		}
		if (With_Wind and values->support_fictif.horizontale and values->support_fictif.val["horizontale"][Direction]) {
			Horizontal.push_back(key);
			All.push_back(key);
		}
	}

	// Remove duplicates using unordered_set
	std::unordered_set<string> all_set(All.begin(), All.end());
	std::unordered_set<string> vertical_set(Vertical.begin(), Vertical.end());
	std::unordered_set<string> horizontal_set(Horizontal.begin(), Horizontal.end());
	
	All = vector<string>(all_set.begin(), all_set.end());
	Vertical = vector<string>(vertical_set.begin(), vertical_set.end());
	Horizontal = vector<string>(horizontal_set.begin(), horizontal_set.end());

	return vector<vector<string>>{All, Vertical, Horizontal};
}

string Matrix::GetMoveColumn(string node_name)
{
	unordered_map<double, string> candidates;
	vector<double> Y;
	for (auto& line : (*ND).at(node_name)->ligne_associer) {
		auto& ln = (*LINE).at(line[0]); 
		string other_nd_key = inverse_node(line[1]); 
		string other_nd_id = ln->real_nodes[other_nd_key];
		double other_nd_y = data["noeuds"][other_nd_id]["y"];

		if (ln->is_column && other_nd_y < data["noeuds"][node_name]["y"]) {
			candidates[other_nd_y] = line[0];
			Y.push_back(other_nd_y);
		}
	}
	if (!candidates.empty()) return candidates[min_of_vector(Y)];
	else return "None"; 
}

string Matrix::GetBeamChoce(string node_name, string PreviousBeam) const
{
	for (const auto& line : (*ND).at(node_name)->ligne_associer) {
		auto& ln = (*LINE).at(line[0]);
		if (ln->is_beam && !ln->is_consol && line[0] != PreviousBeam) {
			return line[0]; // Retourne l'ID de la poutre
		}
	}

	for (const auto& line : (*ND).at(node_name)->ligne_associer) {
		auto& ln = (*LINE).at(line[0]);
		if (ln->is_beam && !ln->is_consol) {
			return line[0]; // Retourne l'ID de la poutre
		}
	}

	std::cout << "Aucune poutre trouvée pour le nœud " << node_name << std::endl;
	return "None";
}

vector<string> Matrix::GetAllMoveColumn(string node_name)
{
	vector<string> Column; 
	string current_node = node_name; 
	string previous_beam = " ";
	int maxIteration = total_lignes + 2; 
	vector<string> visited = {};
	string column;
	string beam;
	string next_node_key;
	string next_node; 

	for (int compteur = 0; compteur < maxIteration; ++compteur) {
		if (std::find(visited.begin(), visited.end(), current_node) != visited.end()) break;
		visited.push_back(current_node); 

		column = GetMoveColumn(current_node);

		if (column != "None" and std::find(Column.begin(), Column.end(), column) == Column.end()) Column.push_back(column);
		else break;

		auto& node_obj = (*ND).at(current_node);
		beam = GetBeamChoce(current_node, previous_beam);

		if (beam == " " or beam == previous_beam) break;
		
		next_node_key = inverse_node(TNT(beam, (*ND).at(current_node)->ligne_associer, 0, 1));
		next_node = (*LINE).at(beam)->real_nodes[next_node_key]; 

		previous_beam = beam; 
		current_node = next_node; 
	}

	return Column;
}

vector<vector<double>>  Matrix::RigidityMatrix()
{
	// Initialisation de la matrice de rigidité
	vector<vector<double>> RigidityMatrix;
	vector<double> Second;
	for (int i = 0; i < RigidityMatrixSize; ++i) {
		Second.clear();
		for (int j = 0; j < RigidityMatrixSize; ++j) {
			Second.push_back(0);
		}
		RigidityMatrix.push_back(Second); 
	}

	// Calcul de la matrice de rigidité pour les noeuds fixes
	// Diagonale de la matrice de rigidité
	int idx;
	double diagonal;

	for (const auto& node : Matrix_nodes) {
		idx = index_of(Matrix_nodes, node); 
		diagonal = 0; 
		for (auto& line : (*ND).at(node)->ligne_associer) {
			diagonal += (*LINE).at(line[0])->K; 
		}
		RigidityMatrix[idx][idx] = diagonal;  
	}

	int H = static_cast<int>(Matrix_nodes.size());
	vector<string> MoveColumn; 
	vector<string> TypeRealNodesLine; 
	double length_reference;

	for (const auto& node : H_nodes) {
		idx = H + index_of(H_nodes, node); 
		diagonal = 0;
		MoveColumn = GetAllMoveColumn(node);
		if (MoveColumn.empty()) {
			RigidityMatrix[idx][idx] = 0;
			continue;
		}
		length_reference = (*LINE).at(MoveColumn[0])->length; // Utiliser la première colonne comme référence
		
		for (const auto& column : MoveColumn) {
			TypeRealNodesLine.clear();
			
			auto& ln = (*LINE).at(column);
			for (const auto& [node, real_node] : ln->real_nodes) {
				TypeRealNodesLine.push_back((*ND).at(real_node)->type);
			}
			// Si le rotule ne se trouve pas dans la liste de type de noeuds 
			if (std::find(TypeRealNodesLine.begin(), TypeRealNodesLine.end(), "rotule") == TypeRealNodesLine.end()) {
				diagonal += 2 * ln->K * 1.5 * (std::pow(length_reference,2) / std::pow(ln->length, 2)); 
			}
			// Si non :: appuis articuler 
			else {
				diagonal += ln->K * (std::pow(length_reference,2) / std::pow(ln->length, 2));
			}
		}
		RigidityMatrix[idx][idx] = diagonal;
	}

	// Hort diagoneal 
	double Out_diagonal = 0; 
	string other_node;
	for (const auto& node : Matrix_nodes) {
		idx = index_of(Matrix_nodes, node);
		for (auto& line : (*ND).at(node)->ligne_associer) {
			other_node = (*LINE).at(line[0])->real_nodes[inverse_node(line[1])];
			if (std::find(Matrix_nodes.begin(), Matrix_nodes.end(), other_node) != Matrix_nodes.end()) {
				RigidityMatrix[idx][index_of(Matrix_nodes, other_node)] = 0.5 * (*LINE).at(line[0])->K;
			}
		}
	}

	vector<string> RealNodesLine;
	for (const auto& node : H_nodes) {
		idx = H + index_of(H_nodes, node);
		auto moveColumns = GetAllMoveColumn(node);
		if (moveColumns.empty()) continue;
		length_reference = (*LINE).at(moveColumns[0])->length;
		for (const auto& culumn : moveColumns) {
			RealNodesLine.clear();
			
			for (const auto& [pair, second] : (*LINE).at(culumn)->real_nodes) {
				RealNodesLine.push_back(second); 
			}

			for (const auto& i : RealNodesLine) {
				if (std::find(Matrix_nodes.begin(), Matrix_nodes.end(), i) != Matrix_nodes.end()) {
					auto& ln = (*LINE).at(culumn);
					double val = -(1 + 0.5) * ln->K * (length_reference / ln->length);
						RigidityMatrix[idx][index_of(Matrix_nodes, i)] = val;
						RigidityMatrix[index_of(Matrix_nodes, i)][idx] = val;
				}
			}
		}
	}

	return RigidityMatrix;
}

vector<vector<double>>  Matrix::SecondMember()
{
	vector<vector<double>> Mat;
	vector<double> vec = { 0 };
	for (size_t i = 0; i < RigidityMatrixSize; ++i) {
		Mat.push_back(vec); 
	}

	double val; 
	int idx; 
	for (const auto& node : Matrix_nodes) {
		idx = index_of(Matrix_nodes, node);
		val = 0;
		for (auto& line : (*ND).at(node)->ligne_associer) {
			auto& ln = (*LINE).at(line[0]); 
			if (not ln->is_consol) {
				val -= ln->M["M_" + node.substr(node.find("_") + 1)];
			}
		}
		Mat[idx][0] = val;
	}

	int H = static_cast<int>(Matrix_nodes.size());
	vector<string> MoveCulumn;
	vector<string> RealNodesLine;
	double length_reference;

	for (const auto& node : H_nodes) {
		idx = H + index_of(H_nodes, node);
		MoveCulumn = GetAllMoveColumn(node); 
		if (MoveCulumn.empty()) { Mat[idx][0] = 0; continue; }
		length_reference = (*LINE).at(MoveCulumn[0])->length;
		val = 0; 
		for (const auto& culumn : MoveCulumn) {
			RealNodesLine.clear();

			for (const auto& [pair, second] : (*LINE).at(culumn)->real_nodes) {
				RealNodesLine.push_back(second);
			}
			
			if (std::find(Matrix_nodes.begin(), Matrix_nodes.end(), "rotule") == Matrix_nodes.end() and 
				std::find(Matrix_nodes.begin(), Matrix_nodes.end(), "free") == Matrix_nodes.end() ) {
				auto& ln = (*LINE).at(culumn);
				val -=  - sum_values(ln->M) * (length_reference / ln->length) + (- ln->appui_fictif + Wind[ln->line_key]) * length_reference;
			}
		}
		Mat[idx][0] = val; 
	}

	return Mat;
}

