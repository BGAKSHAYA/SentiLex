//////////////
// Includes //
//////////////
#include "expansion.h"

#include <algorithm>		// std::swap(), std::sort()
#include <cassert>		// assert
#include <cfloat>		// DBL_MAX
#include <climits>		// UINT_MAX
#include <cmath>		// sqrt(), fabs()
#include <cstdlib>		// size_t
#include <cstdint>		// MAX_SIZE
#include <iostream>		// std::cerr
#include <unordered_set>	// std::unordered_set
#include <queue>		// std::priority_queue

///////////
// types //
///////////

/** Integral type for polarity id */
using pol_t = size_t;

/** Set of NWE id's */
using vids_t = std::unordered_set<vid_t>;

/** Map from polarity id to a set of respective NWE id's */
using pi2v_t = std::unordered_map<pol_t, vids_t>;

/** Map from NWE id to polarity id */
using v2pi_t = std::unordered_map<vid_t, pol_t>;

/** 3-tuple of vector id, vector polarity, and its distance to the
    nearest centroid */
using vpd_t = struct VPD {
  dist_t m_distance {};
  pol_t m_polarity {};
  vid_t m_vecid {};

  VPD(void) {}

  VPD(dist_t a_distance, pol_t a_polarity, vid_t a_vecid):
    m_distance{a_distance}, m_polarity{a_polarity}, m_vecid{a_vecid}
  {}

  bool operator <(const VPD& rhs) const {
    return m_distance < rhs.m_distance;
  }
};

/** vector of 3-tuples of vector id, vector polarity, and distctance
    to the centroid */
using vpd_v_t = std::vector<vpd_t>;

/** priority queue of 3-tuples of vector id, vector polarity, and
    distctance to the centroid */
using vpd_pq_t = std::priority_queue<vpd_t>;

/** struct comprising means and standard deviations of polarity  */
using pol_stat_t = struct {
  vid_t m_dim;			// dimension with the longest distance
				// between vectors pertaining to
				// different polarity classes

  size_t m_n_pos;		// number of positive vectors
  dist_t m_pos_mean;		// mean of positive vectors
  dist_t m_pos_dev;		// deviation of positive vectors along
				// the given dimension
  size_t m_n_neg;		// number of negative vectors
  dist_t m_neg_mean;		// mean of negative vectors
  dist_t m_neg_dev;		// deviation of negative vectors along
				// the given dimension
  size_t m_n_neut;		// number of neutral vectors
  dist_t m_neut_mean;		// mean of neutral vectors
  dist_t m_neut_dev;		// deviation of neutral vectors along
				// the given dimension
};

///////////////
// Constants //
///////////////
const vid_t POS_VID = static_cast<vid_t>(Polarity::POSITIVE);
const vid_t NEG_VID = static_cast<vid_t>(Polarity::NEGATIVE);
const vid_t NEUT_VID = static_cast<vid_t>(Polarity::NEUTRAL);

/////////////
// Methods //
/////////////

/**
 * Check if two matrices are equal
 *
 * @param a_mat1 - 1-st matrix to compare
 * @param a_mat2 - 2-nd matrix to compare
 *
 * @return \c true if both matrices are equal, \c false otherwise
 */
static bool _cmp_mat(const arma::mat *a_mat1, const arma::mat *a_mat2) {
  bool ret = ((a_mat1->n_rows == a_mat2->n_rows) && (a_mat1->n_cols == a_mat2->n_cols));
  if (!ret)
    return ret;

  size_t i, j;
  for (i = 0; i < a_mat1->n_cols; ++i) {
    for (j = 0; j < a_mat1->n_rows; ++j) {
      if (!(ret = ((*a_mat1)(j, i) == (*a_mat2)(j, i))))
	return ret;
    }
  }
  return ret;
}

/**
 * Compute unnormaized Euclidean distance between two vectors
 *
 * @param a_vec1 - 1-st vector
 * @param a_vec2 - 2-nd vector
 * @param a_N - number of elements in each vector
 *
 * @return unnormaized Euclidean distance between vectors
 */
static inline dist_t _unnorm_eucl_distance(const dist_t *a_vec1, const dist_t *a_vec2, size_t a_N) {
  dist_t tmp_i = 0., idistance = 0;

  for (size_t i = 0; i < a_N; ++i) {
    tmp_i = a_vec1[i] - a_vec2[i];
    idistance += tmp_i * tmp_i;
  }
  return idistance;
}

/**
 * Add newly extracted terms to the polarity lexicon
 *
 * @param a_vecid2pol - target dictionary mapping vector id's to polarities
 * @param a_vpds - source vector of NWE ids, their polarities, and distances
 * @param a_j - actual number of new terms
 * @param a_N - maximum number of terms to extract (-1 means unlimited)
 *
 * @return \c void
 */
static inline void _add_terms(v2p_t *a_vecid2pol, vpd_v_t *a_vpds, const int a_j, const int a_N) {
  // sort vpds according to their distances to the centroids
  std::sort(a_vpds->begin(), a_vpds->end());
  // add new terms to the dictionary
  vpd_t *ivpd;
  for (int i = 0; (a_N < 0 || i < a_N) && i < a_j; ++i) {
    ivpd = &(*a_vpds)[i];
    a_vecid2pol->emplace(ivpd->m_vecid, static_cast<Polarity>(ivpd->m_polarity));
  }
}

/**
 * Find cluster whose centroid is nearest to the given word vector
 *
 * @param a_centroids - matrix of pre-computed cluster centroids
 * @param a_vec - word vector whose nearest cluster should be found
 * @param a_dist - (optional) pointer to a variable in which actual
 *                  Euclidean distance to the vector should be stored
 * @return id of the cluster with the nearest centroid
 */
static pol_t _nc_find_cluster(const arma::mat *a_centroids,	\
			       const double *a_vec,		\
			       dist_t *a_dist = nullptr) {
  pol_t ret = 0;
  const double *centroid;
  dist_t idistance = 1., mindistance = DBL_MAX;
  for (size_t i = 0; i < a_centroids->n_cols; ++i) {
    centroid = a_centroids->colptr(i);
    // compute Euclidean distance from vector to centroid
    idistance = _unnorm_eucl_distance(a_vec, centroid, a_centroids->n_rows);
    // compare distance with
    if (idistance < mindistance) {
      mindistance = idistance;
      ret = i;
    }
  }
  if (a_dist != nullptr)
    *a_dist = sqrt(idistance);

  return ret;
}

/**
 * Assign word vectors to the newly computed centroids
 *
 * @param a_polid2vecids - map from polarity id's to vector id's
 * @param a_vecid2polid - map from vector id's to polarity id's
 * @param a_centroids - newly computed centroids
 * @param a_nwe - matrix containing neural word embeddings
 *
 * @return \c true if clusters changed, \c false otherwise
 */
static bool _nc_assign(pi2v_t *a_polid2vecids, v2pi_t *a_vecid2polid, \
		       const arma::mat *a_centroids, const arma::mat *a_nwe) {
  bool ret = false;
  bool is_absent = false, differs = false;
  pol_t polid;
  vid_t N = a_nwe->n_cols;
  vids_t::iterator v_id_pos;
  v2pi_t::iterator it, it_end = a_vecid2polid->end();
  // iterate over each word, find its new cluster, and reassign if
  // necessary
  for (vid_t vecid = 0; vecid < N; ++vecid) {
    polid = _nc_find_cluster(a_centroids, a_nwe->colptr(vecid));
    // obtain previous polarity of this vector
    it = a_vecid2polid->find(vecid);
    // assign vecid to new cluster if necessary
    if ((is_absent = (it == it_end)) || (differs = (it->second != polid))) {
      // remove word vector from the previous polarity class
      if (differs) {
	v_id_pos = (*a_polid2vecids)[it->second].find(vecid);
	// obtain position of the vector id in its previous class set
	if (v_id_pos != (*a_polid2vecids)[it->second].end())
	  (*a_polid2vecids)[it->second].erase(v_id_pos);

	differs = false;
      }
      (*a_vecid2polid)[vecid] = polid;
      (*a_polid2vecids)[polid].insert(vecid);
      ret = true;
    }
  }
  return ret;
}

/**
 * Compute centroids of previously populated clusters
 *
 * @param a_new_centroids - container for storing new centroid coordinates
 * @param a_old_centroids - container storing previously computed centroids
 * @param a_pol2vecids - previously populated clusters
 * @param a_nwe - matrix containing neural word embeddings
 *
 * @return \c true if centroids changed, \c false otherwise
 */
static bool _nc_compute_centroids(arma::mat *a_new_centroids, const arma::mat *a_old_centroids, \
				  const pi2v_t *a_pol2vecids, const arma::mat *a_nwe) {
  // zero-out new centroids
  a_new_centroids->zeros();
  // compute new centroids
  pol_t c_id;
  for (auto& p2v: *a_pol2vecids) {
    c_id = p2v.first;
    // sum-up coordinates of all the vectors pertaining to the given
    // polarity
    for (auto& vecid: p2v.second) {
      a_new_centroids->col(c_id) += a_nwe->col(vecid);
    }
    // take the mean of the new centroid
    if (p2v.second.size())
      a_new_centroids->col(c_id) /= float(p2v.second.size());
  }

  return !_cmp_mat(a_new_centroids, a_old_centroids);
}

/**
 * Perform a single iteration of the nearest centroids algorithm
 *
 * @param a_new_centroids - container for storing new centroid coordinates
 * @param a_old_centroids - container storing previously computed centroids
 * @param a_pol2vecids - previously populated clusters
 * @param a_nwe - matrix containing neural word embeddings of single terms
 *
 * @return \c true if clusters changes, \c false otherwise
 */
static inline bool _nc_run(arma::mat *a_new_centroids, arma::mat *a_old_centroids, \
			   pi2v_t *a_polid2vecids, v2pi_t *a_vecid2polid, \
			   const arma::mat *a_nwe) {
  bool ret = false;
  // calculate centroids
  if ((ret = _nc_compute_centroids(a_new_centroids, a_old_centroids, a_polid2vecids, a_nwe)))
    // assign new items to their new nearest centroids (can return
    // `ret =` here, but then remove assert from )
    _nc_assign(a_polid2vecids, a_vecid2polid, a_new_centroids, a_nwe);

  return ret;
}

/**
 * Expand polarity sets by adding terms that are closest to centroids
 *
 * @param a_vecid2pol - matrix of pre-computed cluster centroids
 * @param a_vec - word vector whose nearest cluster should be found
 *
 * @return id of the cluster with the nearest centroid
 */
static void _nc_expand(v2p_t *a_vecid2pol, const arma::mat *const a_centroids, \
		       const arma::mat *a_nwe, const int a_N) {
  // vector of word vector ids, their respective polarities (aka
  // nearest centroids), and distances to the nearest centroids
  vpd_v_t vpds;
  vpds.reserve(a_vecid2pol->size());

  dist_t idist;
  pol_t ipol;
  int j = 0;
  v2p_t::const_iterator v2p_end = a_vecid2pol->end();
  // populate
  for (vid_t i = 0; i < a_nwe->n_cols; ++i) {
    if (a_vecid2pol->find(i) != v2p_end)
      continue;

    // obtain polarity class and minimum distance to the nearest
    // centroid
    ipol = _nc_find_cluster(a_centroids, a_nwe->colptr(i), &idist);

    // add new element to the vector
    vpds.push_back(VPD {idist, ipol, i});
    ++j;
  }
  _add_terms(a_vecid2pol, &vpds, j, a_N);
}

void expand_nearest_centroids(v2p_t *a_vecid2pol, const arma::mat *a_nwe, const int a_N, \
			      const bool a_early_break) {
  // determine the number of clusters
  size_t n_clusters = static_cast<size_t>(Polarity::MAX_SENTINEL);
  // create two matrices for storing centroids
  arma::mat *centroids = new arma::mat(a_nwe->n_rows, n_clusters);
  arma::mat *new_centroids = new arma::mat(a_nwe->n_rows, n_clusters);

  // populate intial clusters
  pol_t polid;
  pi2v_t polid2vecids;
  polid2vecids.reserve(n_clusters);
  v2pi_t vecid2polid;
  vecid2polid.reserve(a_nwe->n_cols);
  for (auto &v2p: *a_vecid2pol) {
    polid = static_cast<size_t>(v2p.second);
    polid2vecids[polid].insert(v2p.first);
    vecid2polid[v2p.first] = polid;
  }
  int i = 0;
  // run the algorithm until convergence
  while (_nc_run(centroids, new_centroids, &polid2vecids, &vecid2polid, a_nwe)) {
    std::cerr << "Run #" << i++ << '\r';
    // early break
    if (a_early_break) {
      *centroids = *new_centroids;
      break;
    }
    // swap the centroids
    std::swap(centroids, new_centroids);
  }
  std::cerr << std::endl;

  // add new terms to the polarity sets based on their distance to the
  // centroids (centroids and new centroids should contain identical
  // values here)
  assert(_cmp_mat(centroids, new_centroids));
  _nc_expand(a_vecid2pol, new_centroids, a_nwe, a_N);

  delete new_centroids;
  delete centroids;
}

/**
 * Find K known neighbors nearest to the vector `a_vid`
 *
 * @param a_vid - id of the vector whose neighbors should be found
 * @param a_nwe - matrix of neural word embeddings
 * @param a_vecid2pol - map of vector id's with known polarities
 * @param a_knn - vector for storing K earest neighbors
 * @param a_K - number of nearest neighbors to use
 *
 * @return \c void
 */
static void _knn_find_nearest(vid_t a_vid, const arma::mat *a_nwe, const v2p_t * const a_vecid2pol, \
			      vpd_pq_t *a_knn, const int a_K) {
  bool filled = false;
  int added = 0;

  // reset KNN vector
  while (! a_knn->empty()) {
    a_knn->pop();
  }

  dist_t idistance, mindistance = DBL_MAX;
  const dist_t *ivec = a_nwe->colptr(a_vid);
  const size_t n_rows = a_nwe->n_rows;
  // iterate over each known vector and find K nearest ones
  for (auto& v2p: *a_vecid2pol) {
    // compute distance between
    idistance = _unnorm_eucl_distance(ivec, a_nwe->colptr(v2p.first), n_rows);

    if (idistance >= mindistance && filled)
      continue;

    // check if container is full and pop one element if necessary
    if (filled)
      a_knn->pop();
    else
      filled = (++added == a_K);

    a_knn->push(VPD {idistance, static_cast<pol_t>(v2p.second), v2p.first});
    mindistance = a_knn->top().m_distance;
  }
}

/**
 * Compute most probable polarity class for given item from its K neighbors
 *
 * @param a_vpd - element in which to store the result
 * @param a_vid - id of the vector in question
 * @param a_knn - priority queue of K nearest neighbors
 * @param a_workbench - workbench for constructing polarities
 *
 * @return \c void
 */
static void _knn_add(vpd_t *a_vpd, const vid_t a_vid, vpd_pq_t *a_knn, \
		     vpd_v_t *a_workbench) {

  // reset workbench
  for (auto& vpd: *a_workbench) {
    vpd.m_vecid = 0;		// will serve as neighbor counter
    vpd.m_distance = 0.;	// will store the sum of the distances
  }

  const vpd_t *vpd;
  // iterate over neighbors
  while (! a_knn->empty()) {
    vpd = &a_knn->top();
    ++(*a_workbench)[vpd->m_polarity].m_vecid;
    (*a_workbench)[vpd->m_polarity].m_distance += vpd->m_distance;
    a_knn->pop();
  }

  dist_t idistance, maxdistance = 0.;
  pol_t pol = 0, maxpol = static_cast<pol_t>(Polarity::MAX_SENTINEL);
  for (pol_t ipol = 0; ipol < maxpol; ++ipol) {
    if ((*a_workbench)[ipol].m_distance == 0)
      continue;

    // square the number of neighbors for that class and divide by the distance
    idistance = (*a_workbench)[ipol].m_vecid;
    idistance *= idistance;
    idistance /= (*a_workbench)[ipol].m_distance;

    if (idistance > maxdistance) {
      maxdistance = idistance;
      pol = ipol;
    }
  }
  assert(pol != static_cast<pol_t>(Polarity::MAX_SENTINEL));
  *a_vpd = VPD {maxdistance, pol, a_vid};
}

void expand_knn(v2p_t *a_vecid2pol, const arma::mat *a_nwe, const int a_N, const int a_K) {
  vpd_v_t vpds;
  vpds.reserve(a_nwe->n_cols);
  vpd_v_t _knn(a_K);
  vpd_pq_t knn(_knn.begin(), _knn.end());
  vpd_v_t workbench(static_cast<size_t>(Polarity::MAX_SENTINEL));

  vpd_t ivpd;
  size_t i = 0;
  v2p_t::const_iterator v2p_end = a_vecid2pol->end();

  // iterate over each word vector and find k-nearest neigbors for it
  for (vid_t vid = 0; vid < a_nwe->n_cols; ++vid) {
    a_vecid2pol->find(vid);

    // skip vector if its polarity is already known
    if (a_vecid2pol->find(vid) != v2p_end)
      continue;

    _knn_find_nearest(vid, a_nwe, a_vecid2pol, &knn, a_K);
    _knn_add(&vpds[i++], vid, &knn, &workbench);
  }
  _add_terms(a_vecid2pol, &vpds, i, a_N);
}

/**
 * Compute means of polarity vectors along dimension with the biggest
 * deviation
 *
 * @param a_vecid2pol - mapping from vector id's to polarities
 * @param a_prjctd - NWE matrix projected on the PCA space
 * @param a_pol_stat - struct comprising statiscs about polarity
 *                     vectors
 *
 * @return \c void
 */
static void _pca_compute_means(const v2pi_t *a_vecid2polid, const arma::mat *a_prjctd, \
			       pol_stat_t *a_pol_stat) {
  a_pol_stat->m_n_pos = 0; a_pol_stat->m_n_neg = 0; a_pol_stat->m_n_neut = 0;
  /// means of polarity vectors
  arma::mat pol_means(a_prjctd->n_cols, static_cast<size_t>(Polarity::MAX_SENTINEL));
  pol_means.zeros();

  // obtain unnormalized means of polarity vectors
  for (auto &v2p: *a_vecid2polid) {
    switch (v2p.second) {
    case POS_VID:
      ++a_pol_stat->m_n_pos;
      break;
    case NEG_VID:
      ++a_pol_stat->m_n_neg;
      break;
    default:
      ++a_pol_stat->m_n_neut;
      break;
    }
    pol_means.col(v2p.second) += a_prjctd->row(v2p.first).t();
  }

  // normalize means of polarity vectors
  if (! a_pol_stat->m_n_pos || !a_pol_stat->m_n_neg)
    return;

  pol_means.col(POS_VID) /= static_cast<double>(a_pol_stat->m_n_pos);
  pol_means.col(NEG_VID) /= static_cast<double>(a_pol_stat->m_n_neg);

  if (a_pol_stat->m_n_neut)
    pol_means.col(NEUT_VID) /= static_cast<double>(a_pol_stat->m_n_neut);

  // look for the dimension with the biggest difference for different
  // polarity classes
  dist_t max_delta = DBL_MIN;
  // best dimension is the one which maximizes the equation: `AC -
  // (AC/2 - B)`, where AC is the distance between positive and
  // negative means, B is the mean of neutral vectors, and AC/2 is the
  // point in-between the positive and negative means
  arma::vec vdelta = pol_means.col(POS_VID) - pol_means.col(NEG_VID);
  if (a_pol_stat->m_n_neut)
    vdelta = arma::abs(vdelta) -  arma::abs(pol_means.col(POS_VID) - vdelta / 2 - \
					    pol_means.col(NEUT_VID));
  else
    vdelta = arma::abs(vdelta)

  for (vid_t i = 0; i < vdelta.n_rows; ++i) {
    if (vdelta(i) > max_delta) {
      max_delta = vdelta(i);
      a_pol_stat->m_dim = i;
    }
  }
  a_pol_stat->m_pos_mean = pol_means(a_pol_stat->m_dim, POS_VID);
  a_pol_stat->m_neg_mean = pol_means(a_pol_stat->m_dim, NEG_VID);
  a_pol_stat->m_neut_mean = pol_means(a_pol_stat->m_dim, NEUT_VID);
}

/**
 * Compute standard deviation of polarity vectors along the dimension
 * with the biggest distance between different classes
 *
 * @param a_vecid2pol - mapping from vector id's to polarities
 * @param a_prjctd - NWE matrix projected on the PCA space
 * @param a_pol_stat - struct comprising statiscs about polarity
 *                     vectors
 *
 * @return dimension with the maximal distance between polarity classes
 */
static void _pca_compute_dev(const v2pi_t *a_vecid2polid, const arma::mat *a_prjctd, \
			     pol_stat_t *a_pol_stat) {
  // obtain unnormalized standard deviations of polarity vectors along
  // the given dimension
  dist_t tmp_j;
  a_pol_stat->m_pos_dev = 0.;
  a_pol_stat->m_neg_dev = 0.;
  a_pol_stat->m_neut_dev = 0.;
  for (auto &v2p: *a_vecid2polid) {
    tmp_j = (*a_prjctd)(v2p.first, a_pol_stat->m_dim);

    switch (v2p.second) {
    case POS_VID:
      tmp_j -= a_pol_stat->m_pos_mean;
      a_pol_stat->m_pos_dev += tmp_j * tmp_j;
      break;
    case NEG_VID:
      tmp_j -= a_pol_stat->m_neg_mean;
      a_pol_stat->m_neg_dev += tmp_j * tmp_j;
      break;
    default:
      tmp_j -= a_pol_stat->m_neut_mean;
      a_pol_stat->m_neut_dev += tmp_j * tmp_j;
    }
  }
  if (a_pol_stat->m_n_pos)
    a_pol_stat->m_pos_dev /= a_pol_stat->m_n_pos;

  if (a_pol_stat->m_n_neg)
    a_pol_stat->m_pos_dev /= a_pol_stat->m_n_neg;

  if (a_pol_stat->m_n_neut)
    a_pol_stat->m_pos_dev /= a_pol_stat->m_n_neut;
}

/**
 * Obtain means and deviations of polarity vectors along the dimension
 * with the biggest spread
 *
 * @param a_vecid2pol - mapping from vector id's to polarities
 * @param a_prjctd - NWE matrix projected on the PCA space
 * @param a_pol_stat - struct comprising statiscs about polarity
 *                     vectors
 *
 * @return \c void
 */
static void _pca_compute_stat(const v2pi_t *a_vecid2polid, const arma::mat *a_prjctd, \
			       pol_stat_t *a_pol_stat) {
  _pca_compute_means(a_vecid2polid, a_prjctd, a_pol_stat);
  std::cerr << "pca means computed" << std::endl;
  _pca_compute_dev(a_vecid2polid, a_prjctd, a_pol_stat);
  std::cerr << "pca stddev computed" << std::endl;
}

/**
 * Expand polarity sets by adding terms that are farthermost to the
 * mean of neutral vectors
 *
 * @param a_vecid2pol - mapping from vector id's to polarities
 * @param a_pca_nwe - neural-word embedding matrix projected onto the
 *                    PCA space
 * @param a_N - maximum number of terms to extract (-1 means unlimited)
 * @param a_pol_stat - struct comprising statiscs about polarity
 *                     vectors
 *
 * @return \c void
 */
static void _pca_expand(v2p_t *a_vecid2pol, const arma::mat *a_pca_nwe, \
			const int a_N, const pol_stat_t *a_pol_stat) {
  // vector of word vector ids, their respective polarities, and
  // distances to the boundaries
  vpd_v_t vpds;
  vpds.reserve(a_pca_nwe->n_rows - a_vecid2pol->size());

  bool neg_is_greater = false;
  dist_t min = 0., max = 0.;
  if (a_pol_stat->m_neg_mean < a_pol_stat->m_pos_mean) {
    min = a_pol_stat->m_neg_mean + a_pol_stat->m_neg_dev;
    max = a_pol_stat->m_pos_mean - a_pol_stat->m_pos_dev;
  } else {
    neg_is_greater = true;
    min = a_pol_stat->m_pos_mean + a_pol_stat->m_pos_dev;
    max = a_pol_stat->m_neg_mean - a_pol_stat->m_neg_dev;
  }

  int j = 0;
  pol_t ipol;
  dist_t idim, idist;
  v2p_t::const_iterator v2p_end = a_vecid2pol->end();
  // populate
  for (vid_t i = 0; i < a_pca_nwe->n_rows; ++i) {
    if (a_vecid2pol->find(i) != v2p_end)
      continue;

    idim = (*a_pca_nwe)(i, a_pol_stat->m_dim);
    idist = a_pol_stat->m_neut_mean - idim;

    if (idim < min) {
      if (neg_is_greater)
	ipol = POS_VID;
      else
	ipol = NEG_VID;
    } else if (idim > max) {
      if (neg_is_greater)
	ipol = NEG_VID;
      else
	ipol = POS_VID;
    } else {
      ipol = NEUT_VID;
    }
    vpds.push_back(VPD {fabs(idist), ipol, i});
    ++j;
  }
  _add_terms(a_vecid2pol, &vpds, j, a_N);
}

void expand_pca(v2p_t *a_vecid2pol, const arma::mat *a_nwe, const int a_N) {
  // convert polarity enum's to polarity indices
  size_t polid;
  v2pi_t vecid2polid;
  vecid2polid.reserve(a_vecid2pol->size());
  for (auto &v2p: *a_vecid2pol) {
    polid = static_cast<size_t>(v2p.second);
    vecid2polid[v2p.first] = polid;
  }

  // obtain PCA coordinates for the neural word embeddings data
  arma::mat pca_coeff, prjctd;
  std::cerr << "a_nwe->size() = " << a_nwe->n_rows << 'x' << a_nwe->n_cols << std::endl;
  arma::princomp(pca_coeff, prjctd, a_nwe->t());
  std::cerr << "prjctd->size() = " << prjctd.n_rows << 'x' << prjctd.n_cols << std::endl;

  // look for the principal component with the maximum distance
  // between the means of the vectors pertaining to different
  // poalarities
  pol_stat_t pol_stat;
  _pca_compute_stat(&vecid2polid, &prjctd, &pol_stat);
  std::cerr << "pca stat computed" << std::endl;
  // add new terms
  _pca_expand(a_vecid2pol, &prjctd, a_N, &pol_stat);
}

void expand_projected(v2p_t *a_vecid2pol, const arma::mat *a_nwe, const int a_N) {

}

void expand_linear_transform(v2p_t *a_vecid2pol, const arma::mat *a_nwe, const int a_N) {

}
