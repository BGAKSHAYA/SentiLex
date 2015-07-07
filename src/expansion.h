#ifndef VEC2DIC_EXPANSION_H_
# define VEC2DIC_EXPANSION_H_ 1

//////////////
// Includes //
//////////////
#include <armadillo>		// arma::mat
#include <string>		// std::string
#include <unordered_map>	// std::unordered_map

///////////
// Types //
///////////
enum class Polarity: char {
  POSITIVE = 0,
    NEGATIVE,
    NEUTRAL
};

typedef std::unordered_map<std::string, Polarity> w2p_t;

typedef std::unordered_map<unsigned int, Polarity> v2p_t;

typedef std::unordered_map<std::string, unsigned int> w2v_t;

typedef std::unordered_map<unsigned int, std::string> v2w_t;

/////////////
// Methods //
/////////////

/**
 * Apply Rocchio clustering algorithm to expand seed sets of polar terms
 *
 * @param a_vecid2pol - dictionary mapping known vector id's to the
 *                      polarities of their respective words
 * @param a_nwe - matrix of neural word embeddings
 * @param a_N - number of polar terms to extract
 *
 * @return \c void (`a_vecid2pol` is modified in place)
 */
void expand_rocchio(v2p_t &a_vecid2pol, const arma::mat &a_nwe, const size_t a_N);

/**
 * Apply K-nearest neighbors clustering algorithm to expand seed sets of polar terms
 *
 * @param a_vecid2pol - dictionary mapping known vector id's to the
 *                      polarities of their respective words
 * @param a_nwe - matrix of neural word embeddings
 * @param a_N - number of polar terms to extract
 *
 * @return \c void (`a_vecid2pol` is modified in place)
 */
void expand_knn(v2p_t &a_vecid2pol, const arma::mat &a_nwe, const size_t a_N);

/**
 * Apply projection to expand seed sets of polar terms
 *
 * This algorithm first projects all unknown terms on the vector
 * subspace defined by the known polar items and then applies standard
 * clustering algorithm on the projections.
 *
 * @param a_vecid2pol - dictionary mapping known vector id's to the
 *                      polarities of their respective words
 * @param a_nwe - matrix of neural word embeddings
 * @param a_N - number of polar terms to extract
 *
 * @return \c void (`a_vecid2pol` is modified in place)
 */
void expand_projected(v2p_t &a_vecid2pol, const arma::mat &a_nwe, const size_t a_N);

/**
 * Apply projection to expand seed sets of polar terms
 *
 * This algorithm projects all unknown terms on the vector subspace
 * defined by the known polar items and then extends seed sets of
 * known polar terms according to the lengths of projection vectors.
 *
 * @param a_vecid2pol - dictionary mapping known vector id's to the
 *                      polarities of their respective words
 * @param a_nwe - matrix of neural word embeddings
 * @param a_N - number of polar terms to extract
 *
 * @return \c void (`a_vecid2pol` is modified in place)
 */
void expand_projected_length(v2p_t &a_vecid2pol, const arma::mat &a_nwe, const size_t a_N);

/**
 * Derive projection matrix to expand seed sets of polar terms
 *
 * This algorithm tries to derive a projection matrix which makes all
 * known neutral terms the picture of the projection, and maps all
 * known positive and negative terms to vectos (1; 0) and (0; -1)
 * respectively.
 *
 * @param a_vecid2pol - dictionary mapping known vector id's to the
 *                      polarities of their respective words
 * @param a_nwe - matrix of neural word embeddings
 * @param a_N - number of polar terms to extract
 *
 * @return \c void (`a_vecid2pol` is modified in place)
 */
void expand_linear_length(v2p_t &a_vecid2pol, const arma::mat &a_nwe, const size_t a_N);

#endif	// VEC2DIC_EXPANSION_H_
