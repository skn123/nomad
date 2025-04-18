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
#ifndef __NOMAD_4_5_QUAD_MODEL_EVALUATION__
#define __NOMAD_4_5_QUAD_MODEL_EVALUATION__

#include "../../Eval/Evaluator.hpp"
#include "../../Output/OutputInfo.hpp"

#include "../../../ext/sgtelib/src/Surrogate.hpp"

#include "../../nomad_nsbegin.hpp"

/// Class for evaluating trial points as EvalType::MODEL.
class QuadModelEvaluator : public Evaluator
{
private:
    const std::shared_ptr<SGTELIB::Surrogate> _model;
    std::string                _modelDisplay;
    OutputLevel                _displayLevel;
    Point                      _fixedVariable;  ///< Points maybe sent to evaluator in  full space. Evaluator works in local sub space. In this case this member is used for conversions. Can be undefined: sub=full.

    
    size_t _nbConstraints,_nbModels;
    
    
public:
    /// Constructor
    /**
     Quad model evaluators work in the local full space. No need to pass the fixed variables
     */
    explicit QuadModelEvaluator(const std::shared_ptr<EvalParameters>& evalParams,
                                const std::shared_ptr<SGTELIB::Surrogate>& model,
                                const std::string& modelDisplay,
                                const Point& fixedVariable)
      : Evaluator(evalParams, EvalType::MODEL),
        _model(model),
        _modelDisplay(modelDisplay),
        _displayLevel(OutputLevel::LEVEL_INFO),
        _fixedVariable(fixedVariable)
    {
        init();
    }

    virtual ~QuadModelEvaluator();

    /**
     Points for evaluations are given in a block. Sgtelib models handle the points as a matrix and return a matrix for outputs.
     */
    std::vector<bool> eval_block(Block &block,
                                 const Double &NOMAD_UNUSED(hMax),
                                 std::vector<bool> &countEval) const override;

private:
    void init();


};

#include "../../nomad_nsend.hpp"

#endif // __NOMAD_4_5_QUAD_MODEL_EVALUATION__
