/*
 * Copyright (c) 2016 Reed A. Cartwright
 * Authors:  Reed A. Cartwright <reed@cartwrig.ht>
 *
 * This file is part of DeNovoGear.
 *
 * DeNovoGear is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef DNG_PROBABILITY_H
#define DNG_PROBABILITY_H

#include <array>
#include <vector>

#include <dng/genotyper.h>
#include <dng/relationship_graph.h>
#include <dng/mutation.h>

#include <dng/detail/unit_test.h>

namespace dng {

class LogProbability {
public:
    struct params_t {
        double theta;
        double asc_bias_hom;
        double asc_bias_het;
        double asc_bias_hap;
        
        double over_dispersion_hom;
        double over_dispersion_het;
        double ref_bias;
        double error_rate;
        double error_entropy;

        double mutation_entropy;
    };

    struct value_t {
        double log_data;
        double log_scale;        
    };

    LogProbability(RelationshipGraph graph, params_t params);

    template<typename A>
    value_t CalculateLLD(const A &depths, int num_alts, bool has_ref=true);

    template<typename A>
    value_t operator()(const A &depths, int num_alts, bool has_ref=true) {
        return CalculateLLD(depths, num_alts, has_ref);
    }

    const peel::workspace_t& work() const { return work_; };

protected:
    using matrices_t = std::array<TransitionMatrixVector, 4>;

    matrices_t CreateMutationMatrices(const int mutype = MUTATIONS_ALL) const;

    GenotypeArray DiploidPrior(int num_alts, bool has_ref=true);
    GenotypeArray HaploidPrior(int num_alts, bool has_ref=true);

    RelationshipGraph graph_;
    params_t params_;
    peel::workspace_t work_; // must be declared after graph_ (see constructor)

    matrices_t transition_matrices_;

    double prob_monomorphic_[4];

    Genotyper genotyper_;

    std::array<GenotypeArray,4> diploid_prior_; // Holds P(G | theta)
    std::array<GenotypeArray,4> haploid_prior_; // Holds P(G | theta)
    std::array<GenotypeArray,4> diploid_prior_noref_; // Holds P(G | theta)
    std::array<GenotypeArray,4> haploid_prior_noref_; // Holds P(G | theta)

    DNG_UNIT_TEST_CLASS(unittest_dng_log_probability);
};

// returns 'log10 P(Data ; model)-log10 scale' and log10 scaling.
template<typename A>
LogProbability::value_t LogProbability::CalculateLLD(
    const A &depths, int num_alts, bool has_ref)
{
    if(num_alts >= transition_matrices_.size()) {
        num_alts = transition_matrices_.size()-1;
    }

    // calculate genotype likelihoods and store in the lower library vector
    double scale = work_.SetGenotypeLikelihoods(genotyper_, depths, num_alts);

    // Set the prior probability of the founders given the reference
    work_.SetGermline(DiploidPrior(num_alts, has_ref), HaploidPrior(num_alts, has_ref));

    // Calculate log P(Data ; model)
    double logdata = graph_.PeelForwards(work_, transition_matrices_[num_alts]);

    return {logdata/M_LN10, scale/M_LN10};
}

TransitionMatrixVector create_mutation_matrices(const RelationshipGraph &pedigree,
        int num_alleles, double entropy, const int mutype = MUTATIONS_ALL);

inline
LogProbability::matrices_t LogProbability::CreateMutationMatrices(const int mutype) const {
    // Construct the complete matrices
    matrices_t ret;
    for(int i=0;i<ret.size();++i) {
        ret[i] = create_mutation_matrices(graph_, i+1, params_.mutation_entropy, mutype);
    }
    return ret;
}

inline
GenotypeArray LogProbability::DiploidPrior(int num_alts, bool has_ref) {
    assert(num_alts >= 0);
    if(has_ref) {
        return (num_alts < 4) ? diploid_prior_[num_alts]
            : population_prior_diploid_ia(params_.theta, params_.asc_bias_hom, params_.asc_bias_het, num_alts, true);
    } else {
        assert(num_alts >= 1);
        return (num_alts < 5) ? diploid_prior_noref_[num_alts-1]
            : population_prior_diploid_ia(params_.theta, params_.asc_bias_hom, params_.asc_bias_het, num_alts, false);
    }
}

inline
GenotypeArray LogProbability::HaploidPrior(int num_alts, bool has_ref) {
    assert(num_alts >= 0);
    if(has_ref) {
        return (num_alts < 4) ? haploid_prior_[num_alts]
            : population_prior_haploid_ia(params_.theta, params_.asc_bias_hap, num_alts, true);
    } else {
        assert(num_alts >= 1);
        return (num_alts < 5) ? haploid_prior_noref_[num_alts-1]
            : population_prior_haploid_ia(params_.theta, params_.asc_bias_hap, num_alts, false);
    }
}

template<typename A>
inline
LogProbability::params_t get_model_parameters(const A& a) {
    LogProbability::params_t ret;
    
    ret.theta = a.theta;
    ret.asc_bias_hom = a.asc_hom;
    ret.asc_bias_het = a.asc_het;
    ret.asc_bias_hap = a.asc_hap;
        
    ret.over_dispersion_hom = a.lib_overdisp_hom;
    ret.over_dispersion_het = a.lib_overdisp_het;
    ret.ref_bias = a.lib_bias;
    ret.error_rate = a.lib_error;
    ret.error_entropy = a.lib_error_entropy*M_LN2;

    ret.mutation_entropy = a.mu_entropy*M_LN2;

    return ret;
}

}; // namespace dng

#endif // DNG_PROBABILITY_H
