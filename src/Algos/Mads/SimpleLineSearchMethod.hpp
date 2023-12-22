/*---------------------------------------------------------------------------------*/
/*  NOMAD - Nonlinear Optimization by Mesh Adaptive Direct Search -                */
/*                                                                                 */
/*  NOMAD - Version 4 has been created and developed by                            */
/*                 Viviane Rochon Montplaisir  - Polytechnique Montreal            */
/*                 Christophe Tribes           - Polytechnique Montreal            */
/*                                                                                 */
/*  The copyright of NOMAD - version 4 is owned by                                 */
/*                 Charles Audet               - Polytechnique Montreal            */
/*                 Sebastien Le Digabel        - Polytechnique Montreal            */
/*                 Viviane Rochon Montplaisir  - Polytechnique Montreal            */
/*                 Christophe Tribes           - Polytechnique Montreal            */
/*                                                                                 */
/*  NOMAD 4 has been funded by Rio Tinto, Hydro-Québec, Huawei-Canada,             */
/*  NSERC (Natural Sciences and Engineering Research Council of Canada),           */
/*  InnovÉÉ (Innovation en Énergie Électrique) and IVADO (The Institute            */
/*  for Data Valorization)                                                         */
/*                                                                                 */
/*  NOMAD v3 was created and developed by Charles Audet, Sebastien Le Digabel,     */
/*  Christophe Tribes and Viviane Rochon Montplaisir and was funded by AFOSR       */
/*  and Exxon Mobil.                                                               */
/*                                                                                 */
/*  NOMAD v1 and v2 were created and developed by Mark Abramson, Charles Audet,    */
/*  Gilles Couture, and John E. Dennis Jr., and were funded by AFOSR and           */
/*  Exxon Mobil.                                                                   */
/*                                                                                 */
/*  Contact information:                                                           */
/*    Polytechnique Montreal - GERAD                                               */
/*    C.P. 6079, Succ. Centre-ville, Montreal (Quebec) H3C 3A7 Canada              */
/*    e-mail: nomad@gerad.ca                                                       */
/*                                                                                 */
/*  This program is free software: you can redistribute it and/or modify it        */
/*  under the terms of the GNU Lesser General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or (at your     */
/*  option) any later version.                                                     */
/*                                                                                 */
/*  This program is distributed in the hope that it will be useful, but WITHOUT    */
/*  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or          */
/*  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License    */
/*  for more details.                                                              */
/*                                                                                 */
/*  You should have received a copy of the GNU Lesser General Public License       */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.           */
/*                                                                                 */
/*  You can find information on the NOMAD software at www.gerad.ca/nomad           */
/*---------------------------------------------------------------------------------*/

#ifndef __NOMAD_4_4_SIMPLELINESEARCHMETHOD__
#define __NOMAD_4_4_SIMPLELINESEARCHMETHOD__

#include "../../Algos/AlgoStopReasons.hpp"
#include "../../Algos/Mads/SearchMethodAlgo.hpp"
#include "../../Algos/SimpleLineSearch/SimpleLineSearch.hpp"

#include "../../nomad_nsbegin.hpp"

/// Class to perform a simple line search method from a speculative search.
/**
Perform a speculative search plus an extra point.
If speculative point is not a success, find the best position along the direction of last success.
  */
class SimpleLineSearchMethod final : public SearchMethodAlgo
{
private:
    
    std::shared_ptr<AlgoStopReasons<SimpleLineSearchStopType>> _simpleLineSearchStopReasons;
    
    std::unique_ptr<SimpleLineSearch> _simpleLineSearch;

public:
    /// Constructor
    /**
     /param parentStep      The parent of this search step -- \b IN.
     */
    explicit SimpleLineSearchMethod(const Step* parentStep )
      : SearchMethodAlgo(parentStep ),
        _simpleLineSearchStopReasons(nullptr),
        _simpleLineSearch(nullptr)
    {
        init();
    }


    /**
     Execute (start, run, end) of the simple line search. Returns a \c true flag if the algorithm found better point.
     */
    virtual bool runImp() override ;


private:

    /// Helper for constructor.
    /**
     Test if search is enabled or not. Set the maximum number of trial points.
     */
    void init();

    /// Generate new points (no evaluation)
    /**
     \copydoc SearchMethodAlgo::generateTrialPointsFinal 
     Iterative random generation of trial points
     */
     void generateTrialPointsFinal() override;

};

#include "../../nomad_nsend.hpp"

#endif // __NOMAD_4_4_SIMPLELINESEARCHMETHOD__

