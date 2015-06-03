#define _GLIBCXX_DEBUG
#include <microscopes/models/base.hpp>
#include <microscopes/common/entity_state.hpp>
#include <microscopes/common/group_manager.hpp>
#include <microscopes/common/variadic/dataview.hpp>
#include <microscopes/common/util.hpp>
#include <microscopes/common/typedefs.hpp>
#include <microscopes/common/assert.hpp>
#include <microscopes/io/schema.pb.h>
#include <distributions/special.hpp>
#include <distributions/models/dd.hpp>

#include <math.h>
#include <assert.h>
#include <vector>
#include <set>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <stdexcept>


namespace microscopes {
namespace lda {

typedef std::vector<std::shared_ptr<models::group>> group_type;


template<typename T> void
removeFirst(std::vector<T> &v, T element){
    auto it = std::find(v.begin(),v.end(), element);
    if (it != v.end()) {
      v.erase(it);
    }
}

// http://stackoverflow.com/a/1267878/982745
template< class T >
std::vector<T> selectByIndex(std::vector<T> &v, std::vector<size_t> const &index )  {
    std::vector<T> new_v;
    for(size_t i: index){
        new_v.push_back(v[i]);
    }

    return new_v;
}


class model_definition {
public:
    model_definition(size_t n, size_t v)
        : n_(n), v_(v)
    {
        MICROSCOPES_DCHECK(n > 0, "no docs");
        MICROSCOPES_DCHECK(v > 0, "no terms");
    }


    inline size_t n() const { return n_; }
    inline size_t v() const { return v_; }
private:
    size_t n_;
    size_t v_;
};

class state {
public:
    // Quick and dirty constructor for Shuyo implementation.
    state(const model_definition &def,
        float alpha,
        float beta,
        float gamma,
        const std::vector<std::vector<size_t>> &docs,
        common::rng_t &rng)
            : alpha_(alpha), beta_(beta), gamma_(gamma) {
        V = def.v();
        M = def.n();
        rng_ = rng;
        std::cout << "1. iterate i to M " << M << std::endl;
        for(size_t i = 0; i < M; ++i) {
            using_t.push_back({0});
        }
        using_k = {0};

        x_ji = std::vector<std::vector<size_t>>(docs);
        std::cout << "2. iterate j  to M " << M << std::endl;
        for(size_t j = 0; j < M; ++j) {
            k_jt.push_back({0});
            n_jt.push_back({0});

            n_jtv.push_back(std::vector< std::map<size_t, size_t>>());
            std::cout << "    3. iterate t to using_t.size() " << using_t[j].size() << std::endl;
            for (size_t t = 0; t < using_t[j].size(); ++t)
            {
                std::map<size_t, size_t> term_dict;
                // std::cout << "    4. iterate i to V " << V << " (t=" << t << ")" << std::endl;
                // for (size_t i = 0; i < V; ++i)
                // {
                //     term_dict[i] = 0;
                // }
                n_jtv[j].push_back(term_dict);
            }
        }
        std::cout << "arrays created" << std::endl;
        m = 0;
        m_k = std::vector<size_t> {1};
        n_k = std::vector<float> {beta_ * V};

        std::map<size_t, size_t> term_count;
        for (size_t i = 0; i < V; ++i)
        {
            term_count[i] = 0;
        }
        n_kv.push_back(term_count);

        for(size_t i=0; i < docs.size(); i++){

            t_ji.push_back(std::vector<size_t>(docs[i].size(), 0));
        }

    }

    void
    inference(){
        for (size_t j = 0; j < x_ji.size(); ++j)
            for (size_t i = 0; i < x_ji[j].size(); ++i){
                std::cout << "sampling_t(" << j << "," << i << ")" << std::endl;
                sampling_t(j, i);
            }
        for (size_t j = 0; j < M; ++j)
            for (auto t: using_t[j]){
                std::cout << "sampling_k(" << j << "," << t << ")" << std::endl;
                sampling_k(j, t);
            }
    }

    size_t
    usedDishes(){
        return using_k.size();
    }


    std::vector<std::map<size_t, size_t>>
    wordDist(){
        // Distribution over words for each topic
        std::vector<std::map<size_t, size_t>> vec;
        for(auto k: using_k){
            if(k==0) continue;
            vec.push_back(std::map<size_t, size_t>());
            for(size_t v = 0; v < V; ++v) {
                if(n_kv[k].find(v) != n_kv[k].end()){
                    vec.back()[v] = n_kv[k][v] / n_k[k];
                }
                else{
                    vec.back()[v] = beta_ / n_k[k];
                }
            }
        }
        return vec;
    }


private:
    void
    sampling_t(size_t j, size_t i){
        leave_from_table(j, i);
        size_t v = x_ji[j][i];
        std::vector<float> f_k = calc_f_k(v);

        // assert f_k[0] == 0 # f_k[0] is a dummy and will be erased
        std::vector<float> p_t = calc_table_posterior(j, f_k);
        // if len(p_t) > 1 and p_t[1] < 0: self.dump()
        size_t word = common::util::sample_discrete(p_t, rng_);
        size_t t_new = using_t[j][word];
        if (t_new == 0)
        {
            std::vector<float> p_k = calc_dish_posterior_w(f_k);
            size_t topic_index = common::util::sample_discrete(p_k, rng_);
            size_t k_new = using_k[topic_index];
            if (k_new == 0)
            {
                k_new = add_new_dish();
            }
            t_new = add_new_table(j, k_new);
        }
        seat_at_table(j, i, t_new);
    }

    void
    sampling_k(size_t j, size_t t){
        leave_from_dish(j, t);
        std::vector<float> p_k = calc_dish_posterior_t(j, t);

        size_t topic_index = common::util::sample_discrete(p_k, rng_);
        size_t k_new = using_k[topic_index];
        if (k_new == 0)
        {
            k_new = add_new_dish();
        }
        seat_at_dish(j, t, k_new);
    }

    void
    leave_from_dish(size_t j, size_t t){
        size_t k = k_jt[j][t];
        // assert k > 0
        // assert m_k[k] > 0
        m_k[k] -= 1;
        m -= 1;
        if (m_k[k] == 0)
        {
            removeFirst(using_k, k);
            k_jt[j][t] = 0;
        }
    }

    std::vector<float>
    calc_dish_posterior_t(size_t j, size_t t){
        size_t k_old = k_jt[j][t];
        float Vbeta = V * beta_;
        std::vector<float> new_n_k = n_k;

        size_t n_jt_val = n_jt[j][t];
        n_k[k_old] -= n_jt_val;
        new_n_k = selectByIndex(new_n_k, using_k);
        std::vector<float> log_p_k;
        // numpy.log(self.m_k[self.using_k]) + gammaln(n_k) - gammaln(n_k + n_jt)
        for(auto k: using_k){
            log_p_k.push_back(log(m_k[k]) + lgamma(n_k[k]) - lgamma(Vbeta + n_jt_val));
        }
        float log_p_k_new = log(gamma_) + lgamma(Vbeta) - lgamma(Vbeta + n_jt_val);
        // # TODO: FINISH https://github.com/shuyo/iir/blob/master/lda/hdplda2.py#L250-L270

        for(auto kv: n_jtv[j][t]){
            auto w = kv.first;
            auto n_jtw = kv.second;
            if (n_jtw == 0)
                continue;

            // n_kw = numpy.array([n.get(w, self.beta) for n in self.n_kv])
            std::vector<float> n_kw {};
            for(auto n: n_kv){
                if(n.count(w) > 0){
                    n_kw.push_back(n[w]);
                }
                else{
                    n_kw.push_back(beta_);
                }
            }
            n_kw[k_old] -= n_jtw;
            n_kw = selectByIndex(n_kw, using_k);
            n_kw[0] = 1; // # dummy for logarithm's warning
            for(size_t i = 0; i < n_kw.size(); i++){
                log_p_k[i] += lgamma(n_kw[i] + n_jtw) - lgamma(n_kw[i]);
            }
            log_p_k_new += lgamma(beta_ + n_jtw) - lgamma(beta_);
        }

        log_p_k[0] = log_p_k_new;
        std::vector<float> p_k;
        float max_value = *std::max_element(log_p_k.begin(), log_p_k.end());
        float p_k_sum = 0;
        for(auto log_p_k_value: log_p_k){
            p_k.push_back(exp(log_p_k_value + max_value));
            p_k_sum += exp(log_p_k_value + max_value);
        }
        for(size_t i = 0; i < p_k.size(); i++){
            p_k[i] /= p_k_sum;
        }

        return p_k;
    }

    void
    seat_at_dish(size_t j, size_t t, size_t k_new){
        m += 1;
        m_k[k_new] += 1;

        size_t k_old = k_jt[j][t];
        if (k_new == k_old)
        {
            k_jt[j][t] = k_new;
            float n_jt_val = n_jt[j][t];

            if (k_old != 0)
            {
                n_k[k_old] -= n_jt_val;
            }
            n_k[k_new] += n_jt_val;
            for(auto kv: n_jtv[j][t]){
                if (k_old != 0)
                {
                    n_kv[k_old][kv.first] -= kv.second;
                }
                n_kv[k_new][kv.second] += kv.second;
            }
        }
    }


    void
    seat_at_table(size_t j, size_t i, size_t t_new){
        // assert t_new in self.using_t[j]
        t_ji[j][i] = t_new;
        n_jt[j][t_new] += 1;

        size_t k_new = k_jt[j][t_new];
        n_k[k_new] += 1;

        size_t v = x_ji[j][i];
        n_kv[k_new][v] += 1;
        n_jtv[j][t_new][v] += 1;
    }


    size_t
    add_new_dish(){
        size_t k_new = using_k.size();
        for (size_t i = 0; i < using_k.size(); ++i)
        {
            if (i != using_k[i])
            {
                k_new = i;
                break;
            }
        }
        if (k_new == using_k.size())
        {
            n_k.push_back(n_k[0]);
            m_k.push_back(m_k[0]);
            n_kv.push_back(std::map<size_t, size_t>());
        }

        using_k.insert(using_k.begin()+k_new, k_new);
        n_k[k_new] = beta_ * (float)V;
        m_k[k_new] = 0;

        for (size_t i = 0; i < V; ++i)
        {
            n_kv[k_new][i] = 0;
        }
        return k_new;

    }

    size_t
    add_new_table(size_t j, size_t k_new)
    {
        // assert k_new in self.using_k
        size_t t_new = using_t[j].size();
        for (size_t i = 0; i < using_t[j].size(); ++i)
        {
            if (i != using_t[j][i])
            {
                t_new = i;
                break;
            }
        }
        if (t_new == using_t[j].size())
        {
            n_jt[j].emplace_back();
            k_jt[j].push_back(k_jt[j][0]);

            std::map<size_t, size_t> term_dict;
            n_jtv[j].push_back(term_dict);
        }
        using_t[j].insert(using_t[j].begin()+t_new, t_new);
        n_jt[j][t_new] = 0;
        for (size_t i = 0; i < V; ++i)
        {
            n_jtv[j][t_new][i] = 0;
        }

        k_jt[j][t_new] = k_new;
        m_k[k_new] += 1;
        m += 1;

        return t_new;
    }

    std::vector<float>
    calc_dish_posterior_w(std::vector<float> &f_k){
        std::map<size_t, float> p_k_map;
        for(auto& k: using_k){
            p_k_map[k] = m_k[k] + f_k[k];
        }
        float sum_p_k = 0;
        for(auto& kv: p_k_map){
            sum_p_k += kv.second;
        }
        std::vector<float> p_k;
        for (size_t i = 0; i < p_k_map.size(); ++i)
        {
            p_k.push_back(p_k_map[i] / sum_p_k);
        }
        return p_k;
    }

    std::vector<float>
    calc_table_posterior(size_t j, std::vector<float> &f_k){
        std::vector<size_t> using_table = using_t[j];
        std::vector<float> p_t;
        for(auto& p: using_table){
            p_t.push_back(n_jt[j][p] + f_k[k_jt[j][p]]);
        }
        float p_x_ji = gamma_ / (float)V;
        for (size_t k = 0; k < f_k.size(); ++k)
        {
            p_x_ji += m_k[k] * f_k[k];
        }
        p_t[0] = p_x_ji * alpha_ / (gamma_ + m);
        float sum_p_t = 0;
        for(auto& kv: p_t){
            sum_p_t += kv;
        }
        for (size_t i = 0; i < p_t.size(); ++i)
        {
            p_t[i] /= sum_p_t;
        }
        return p_t ;
    }


    void
    leave_from_table(size_t j, size_t i){
        size_t t = t_ji[j][i];
        if (t > 0)
        {
            size_t k = k_jt[j][t];
            // decrease counters
            size_t v = x_ji[j][i];
            n_kv[k][v] -= 1;
            n_k[k] -= 1;
            n_jt[j][t] -= 1;
            n_jtv[j][t][v] -= 1;

            if (n_jt[j][t] == 0)
            {
                remove_table(j, t);
            }
        }
    }

    void
    remove_table(size_t j, size_t t){
        size_t k = k_jt[j][t];
        removeFirst(using_t[j], t);
        m_k[k] -= 1;
        m -= 1;
        if (m_k[k] == 0)
        {
            removeFirst(using_k, k);
        }
    }

    std::vector<float>
    calc_f_k(size_t v){
        std::vector<float> f_k {};

        for (size_t k=0; k < n_kv.size(); k++)
        {
            f_k.push_back(n_kv[k][v] / n_k[k]);
        }

        return f_k;
    }

    size_t V; // Vocabulary size
    size_t M; // Num documents
    size_t m;
    float alpha_;
    float beta_;
    float gamma_;
    common::rng_t rng_;
    std::vector<std::vector<size_t>> using_t;
    std::vector<size_t> using_k;
    std::vector<std::vector<size_t>> x_ji;
    std::vector<std::vector<size_t>> k_jt;
    std::vector<std::vector<size_t>> n_jt;
    std::vector<std::vector<std::map<size_t, size_t>>> n_jtv;
    std::vector<size_t> m_k;
    std::vector<float> n_k;
    std::vector<std::map<size_t, size_t>> n_kv;
    std::vector<std::vector<size_t>> t_ji;


};

}
}