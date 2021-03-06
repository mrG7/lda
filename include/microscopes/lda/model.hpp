#pragma once
#pragma once

#include <microscopes/common/util.hpp>
#include <microscopes/common/typedefs.hpp>
#include <microscopes/common/assert.hpp>
#include <microscopes/lda/util.hpp>

#include <math.h>
#include <vector>
#include <set>
#include <map>

namespace microscopes {
namespace lda {

typedef std::vector<std::vector<size_t>> nested_vector;

class model_definition {
public:
    model_definition(size_t, size_t);
    inline size_t n() const { return n_; }
    inline size_t v() const { return v_; }
private:
    size_t n_;
    size_t v_;
};

class state {
public:
    size_t V; //!< Total number of unique vocabulary words
    float alpha_; //!< Hyperparamter on second level Dirichlet process (\alpha_0)
    float beta_; //!< Hyperparameter of base Dirichlet distribution (over term distributions) (\beta)
    float gamma_; //!< Hyperparameter on first level Dirichlet process (\gamma)
    nested_vector using_t; //!< Nested vector giving list of indices of
                           //!< active tables for each document
                           //!< table==0 means we need to create new table for word
    std::vector<size_t> dishes_; //!< List of indices of active dishes/topics (using_k in shuyo's code)
    const nested_vector x_ji; //!< Integer representation of documents
    nested_vector dish_assignments_; //!< Nested vector mapping doc/table pair to topic (k_jt)
                                //!< dish==0 means we need to create new dish
    nested_vector n_jt; //!< Nested vector giving counts for words assigned to doc/table pairs
    std::vector<std::vector<std::map<size_t, size_t>>> n_jtv; //!< Nested vector giving counts for doc/table/word triples
    std::vector<size_t> m_k; //!< Number of tables assigned to each dish
    lda_util::defaultdict<size_t, float> n_k; //!< Number of words assigned to each dish plus beta * V
    std::vector<lda_util::defaultdict<size_t, float>> n_kv; //!< Number of times a given word is assigned to
                                                            //!< each dish plus beta
    nested_vector table_assignments_; //!< Nested vector giving table assignment for each doc/word pair (t_ji)

    template <class... Args>
    static inline std::shared_ptr<state>
    initialize(Args &&... args)
    {
        return std::make_shared<state>(std::forward<Args>(args)...);
    }

private:
    state(const model_definition &defn,
          float alpha,
          float beta,
          float gamma,
          const nested_vector &docs);

public:
    state(const model_definition &defn,
          float alpha,
          float beta,
          float gamma,
          size_t initial_dishes,
          const nested_vector &docs,
          common::rng_t &);

    state(const model_definition &defn,
          float alpha,
          float beta,
          float gamma,
          const nested_vector &dish_assignments,
          const nested_vector &table_assignments,
          const nested_vector &docs);

    nested_vector
    assignments();

    /**
    * Returns, for each entity, a map from
    * table IDs -> (global) dish assignments
    *
    */
    nested_vector
    dish_assignments();

    /**
    * Returns, for each entity, an assignment vector
    * from each word to the (local) table it is assigned to.
    *
    */
    nested_vector
    table_assignments();

    // Not implemented
    float
    score_assignment() const;

    // Not implemented
    float
    score_data(common::rng_t &rng) const;

    std::vector<std::map<size_t, float>>
    word_distribution();

    std::vector<std::vector<float>>
    document_distribution();

    double
    perplexity();

    void
    leave_from_dish(size_t j, size_t t);

    void
    validate_n_k_values();

    void
    seat_at_dish(size_t j, size_t t, size_t k_new);

    void
    add_table(size_t eid, size_t t_new, size_t did);

    void
    create_entity(size_t eid);

    size_t
    create_dish();

    void
    create_dish(size_t k_new);

    size_t
    create_table(size_t eid, size_t k_new);

    void
    remove_table(size_t eid, size_t tid);

    void
    delete_table(size_t eid, size_t tid);

    inline size_t get_word(size_t eid, size_t word_index) const { return get_entity(eid)[word_index]; }

    inline std::vector<size_t> get_entity(size_t eid) const { return x_ji[eid]; }

    inline size_t tablesize(size_t eid, size_t tid) const { return n_jt[eid][tid]; }

    inline size_t dish_assignment(size_t eid, size_t tid) const { return dish_assignments_[eid][tid]; }

    inline void delete_dish(size_t did) { lda_util::removeFirst(dishes_, did); }

    inline std::vector<size_t> dishes() const { return dishes_; }

    inline size_t nentities() const { return x_ji.size(); }

    inline size_t ntopics() const { return dishes_.size() - 1; }

    inline size_t nwords() const { return V; }

    inline size_t nterms(size_t eid) const { return get_entity(eid).size(); }

    inline size_t ntables(size_t eid) const { return using_t[eid].size(); }

    inline std::vector<size_t> tables(size_t eid) const { return using_t[eid]; }

    inline int ntables() const { return std::accumulate(m_k.begin()+1, m_k.end(), 0); }

};

}
}