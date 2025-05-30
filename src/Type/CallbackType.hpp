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
/**
 \file   CallbackType.hpp
 \brief  Types for Callback
 \author Viviane Rochon Montplaisir
 \date   November 2019
 \see    CallbackType.cpp
 */


#ifndef __NOMAD_4_5_CALLBACK_TYPE__
#define __NOMAD_4_5_CALLBACK_TYPE__

#include "../nomad_nsbegin.hpp"

enum class CallbackType
{
    ITERATION_END,      ///< Called at the end of an Iteration
    MEGA_ITERATION_START, ///< Called at the start of a MegaIteration (after defaultStart, that is,  _success is reset to undefined. After Update to have mesh/barrier updated)
    MEGA_ITERATION_END, ///< Called at the end of a MegaIteration (after defaultEnd, no update done but success is up to date)
    EVAL_OPPORTUNISTIC_CHECK, ///< Called after each evaluation for a special opportunistic check for cancelling remaining queue evaluations (maybe not a success) or propagate stop to iteration (customOpportunisticIterStop)
    EVAL_FAIL_CHECK, ///< Called after each failed evaluation for handling eval point: re-evaluated fail or change the eval status and update value (DiscoMads for hidden constraints)
    EVAL_STOP_CHECK, ///< Called after each evaluation for handling global stop
    PRE_EVAL_UPDATE,  ///< Called before each evaluation for discarding an evaluation point (using a user surrogate for example)
    PRE_EVAL_BLOCK_UPDATE,  ///< Called before each evaluation for discarding an evaluation point (using a user surrogate for example)
    POST_EVAL_UPDATE, ///< Called just after each evaluation (in EvaluatorControl::evalBlock) for a special update defined by the user
    POSTPROCESSING_CHECK, ///< Called during postProcessing after trial points have been evaluated. Step is passed for check but evalPoint is not (this is different than Eval Stop Check)
    HOT_RESTART,        ///< Called at the beginning of Hot Restart process
    USER_METHOD_SEARCH,  ///< Called for a user search method
    USER_METHOD_SEARCH_2,  ///< Called for the second user search method
    USER_METHOD_SEARCH_END,  ///< Called after evaluation of trial points proposed by user search method
    USER_METHOD_POLL,       ///< Called for a user poll method
    USER_METHOD_FREE_POLL,    ///< Called for a user poll method as a third pass independent of the regular method.
    USER_METHOD_FREE_POLL_END,   ///< Called after evaluation of trial points proposed by user free poll method
};


#include "../nomad_nsend.hpp"
#endif  // __NOMAD_4_5_CALLBACK_TYPE__
