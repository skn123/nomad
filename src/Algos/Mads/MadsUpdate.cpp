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

#include "../../Algos/CacheInterface.hpp"
#include "../../Algos/EvcInterface.hpp"
#include "../../Algos/Mads/MadsMegaIteration.hpp"
#include "../../Algos/Mads/MadsUpdate.hpp"
#include "../../Algos/SubproblemManager.hpp"
#include "../../Eval/ComputeSuccessType.hpp"
#include "../../Output/OutputQueue.hpp"

void NOMAD::MadsUpdate::init()
{
    setStepType(NOMAD::StepType::UPDATE);
    verifyParentNotNull();

    auto megaIter = getParentOfType<NOMAD::MadsMegaIteration*>();
    if (nullptr == megaIter)
    {
        throw NOMAD::Exception(__FILE__,__LINE__,"Error: An instance of class MadsUpdate must have a MegaIteration among its ancestors");
    }
    
    _clearEvalQueue = true;
    auto evc = NOMAD::EvcInterface::getEvaluatorControl();
    if (nullptr != evc)
    {
        _clearEvalQueue = evc->getEvaluatorControlGlobalParams()->getAttributeValue<bool>("EVAL_QUEUE_CLEAR");
    }

}


std::string NOMAD::MadsUpdate::getName() const
{
    return getAlgoName() + NOMAD::stepTypeToString(_stepType);
}


bool NOMAD::MadsUpdate::runImp()
{
    // NOTE: update use success determined from barrier to update mesh
    // The success of parent mega iter is not considered.
    
    // megaIter barrier is already in subproblem.
    // So no need to convert refBestFeas and refBestInf
    // from full dimension to subproblem.
    auto megaIter = getParentOfType<NOMAD::MadsMegaIteration*>();
    auto barrier = megaIter->getBarrier();
    auto mesh = megaIter->getMesh();
    std::string s;  // for output
    
    // Must have a barrier
    if (nullptr == barrier)
    {
        throw NOMAD::Exception(__FILE__,__LINE__,"Error: barrier is null");
    }
    
    // Which eval type 
    auto evalType = barrier->getEvalType();

    OUTPUT_DEBUG_START
    s = "Running " + getName() + ". Barrier: ";
    AddOutputDebug(s);
    std::vector<std::string> vs = barrier->display(4);
    for (const auto & si : vs)
    {
        AddOutputDebug(si);
    }
    OUTPUT_DEBUG_END

    // Barrier is already updated from previous steps (IterationUtils::postProcessing).
    // Get the best feasible and infeasible references, and then update
    // reference values.
    auto refBestFeas = barrier->getRefBestFeas();
    auto refBestInf  = barrier->getRefBestInf();

    barrier->updateRefBests();

    NOMAD::EvalPointPtr newBestFeas = barrier->getCurrentIncumbentFeas();
    NOMAD::EvalPointPtr newBestInf  = barrier->getCurrentIncumbentInf();

    if (nullptr != refBestFeas || nullptr != refBestInf)
    {
        // Compute success
        // Get which of newBestFeas and newBestInf is improving
        // the solution. Check newBestFeas first.
        auto computeType = barrier->getFHComputeType();
        
        NOMAD::ComputeSuccessType computeSuccess(computeType);
        std::shared_ptr<NOMAD::EvalPoint> newBest;
        NOMAD::SuccessType success = computeSuccess(newBestFeas, refBestFeas);
        if (success >= NOMAD::SuccessType::PARTIAL_SUCCESS)
        {
            // newBestFeas is the improving point.
            newBest = newBestFeas;
            // Workaround: If we do not have the point from which newBest was computed,
            // use refBestFeas instead if available or refBestInf.
            if (nullptr == newBest->getPointFrom() )
            {
                if ( nullptr != refBestFeas )
                    newBest->setPointFrom(refBestFeas, NOMAD::SubproblemManager::getInstance()->getSubFixedVariable(this));
                else if ( nullptr != refBestInf )
                    newBest->setPointFrom(refBestInf, NOMAD::SubproblemManager::getInstance()->getSubFixedVariable(this));
                else
                    throw NOMAD::Exception(__FILE__,__LINE__,"Error: Cannot set the point at the origin of newBest (feasible)");
            }
            OUTPUT_INFO_START
            if (refBestFeas)
            {
                s = "Update: improving feasible point";
                AddOutputInfo(s);
                s = " from " + refBestFeas->display();
                AddOutputInfo(s);
                s = " to " + newBestFeas->display();
                AddOutputInfo(s);
            }
            OUTPUT_INFO_END
        }
        else
        {
            // Check newBestInf
            NOMAD::SuccessType success2 = computeSuccess(newBestInf, refBestInf);
            if (success2 > success)
            {
                success = success2;
            }
            if (success >= NOMAD::SuccessType::PARTIAL_SUCCESS)
            {
                // newBestInf is the improving point.
                newBest = newBestInf;
                // Workaround: If we do not have the point from which newBest was computed,
                // use refBestInf instead if available or refBestFeas.
                if (nullptr == newBest->getPointFrom() )
                {
                    if ( nullptr != refBestInf )
                        newBest->setPointFrom(refBestInf, NOMAD::SubproblemManager::getInstance()->getSubFixedVariable(this));
                    else if ( nullptr != refBestFeas )
                        newBest->setPointFrom(refBestFeas, NOMAD::SubproblemManager::getInstance()->getSubFixedVariable(this));
                    else
                        throw NOMAD::Exception(__FILE__,__LINE__,"Error: Cannot set the point at the origin of newBest (infeasible)");
                }
                OUTPUT_INFO_START
                if (refBestInf)
                {
                    s = "Update: improving infeasible point ";
                    AddOutputInfo(s);
                    s = " from " + refBestInf->display();
                    AddOutputInfo(s);
                    s = " to " + newBestInf->display();
                    AddOutputInfo(s);
                }
                OUTPUT_INFO_END
            }
        }
        if (success == NOMAD::SuccessType::UNSUCCESSFUL)
        {
            OUTPUT_INFO_START
            s = "Update: no success found";
            AddOutputInfo(s);
            OUTPUT_INFO_END
        }


        // Debug verification
        // Compare computed success with value from MegaIteration.
        // This is the value from the previous MegaIteration. If it
        // was not evaluated, ignore the test.
        // If queue is not cleared between runs, also ignore the test.

        const bool megaIterEvaluated = (NOMAD::SuccessType::UNDEFINED != megaIter->getSuccessType());
        if (!_clearEvalQueue && megaIterEvaluated && (success != megaIter->getSuccessType()))
        {
            s = "Warning: MegaIteration success type: ";
            s += NOMAD::enumStr(megaIter->getSuccessType());
            s += ". Is different than computed success type: " + NOMAD::enumStr(success);
            if (refBestFeas)
            {
                s += "\nRef best feasible:   " + refBestFeas->displayAll();
            }
            if (newBestFeas)
            {
                s += "\nNew best feasible:   " + newBestFeas->displayAll();
            }
            if (refBestInf)
            {
                s += "\nRef best infeasible: " + refBestInf->displayAll();
            }
            if (newBestInf)
            {
                s += "\nNew best infeasible: " + newBestInf->displayAll();
            }
            AddOutputWarning(s);
        }

        if (NOMAD::EvalType::BB == evalType)
        {
            // The directions of last successes may be used to sort points. Update values.
            // These dimensions are always in full space.
            auto dirFeas = (newBestFeas) ? newBestFeas->getDirection() : nullptr;
            auto dirInf  = (newBestInf)  ? newBestInf->getDirection() : nullptr;

            if (nullptr != dirFeas)
            {
                OUTPUT_INFO_START
                std::string dirStr = "New direction (feasible) ";
                dirStr += dirFeas->display();
                AddOutputInfo(dirStr);
                OUTPUT_INFO_END
            }
            if (nullptr != dirInf)
            {
                OUTPUT_INFO_START
                std::string dirStr = "New direction (infeasible) ";
                dirStr += dirInf->display();
                AddOutputInfo(dirStr);
                OUTPUT_INFO_END
            }
            auto evc = NOMAD::EvcInterface::getEvaluatorControl();
            if (nullptr != evc)
            {
                evc->setLastSuccessfulFeasDir(dirFeas);
                evc->setLastSuccessfulInfDir(dirInf);
            }
        }

        if (success >= NOMAD::SuccessType::PARTIAL_SUCCESS)
        {
            OUTPUT_INFO_START
            if (success == NOMAD::SuccessType::PARTIAL_SUCCESS)
            {
                AddOutputInfo("Last Iteration Improving. Delta remains the same.");
            }
            OUTPUT_INFO_END

            if (success >= NOMAD::SuccessType::FULL_SUCCESS)
            {
                if (mesh->enlargeDeltaFrameSize(*newBest->getDirection()))
                {
                    OUTPUT_INFO_START
                    AddOutputInfo("Last Iteration Successful. Delta is enlarged.");
                    OUTPUT_INFO_END
                }
                else
                {
                    OUTPUT_INFO_START
                    AddOutputInfo("Last Iteration Successful. Delta remains the same.");
                    OUTPUT_INFO_END
                }
            }
        }
        else
        {
            OUTPUT_INFO_START
            AddOutputInfo("Last Iteration Unsuccessful. Delta is refined.");
            OUTPUT_INFO_END
            mesh->refineDeltaFrameSize();
        }
    }

    mesh->updatedeltaMeshSize();
    OUTPUT_INFO_START
    AddOutputInfo("delta mesh  size = " + mesh->getdeltaMeshSize().display());
    AddOutputInfo("Delta frame size = " + mesh->getDeltaFrameSize().display());
    OUTPUT_INFO_END


    return true;
}


