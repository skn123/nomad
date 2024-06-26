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

#include "../Algos/QuadModel/QuadModelIteration.hpp"
#include "../Cache/CacheBase.hpp"
#include "../Eval/ComparePriority.hpp"
#include "../Eval/EvaluatorControl.hpp"
#include "../Output/OutputQueue.hpp"
#include "../Output/OutputDirectToFile.hpp"
#include "../Type/EvalSortType.hpp"
#include "../Util/AllStopReasons.hpp"
#include "../Util/Clock.hpp"
#include "../Util/MicroSleep.hpp"

/*-----------------------------------*/
/*   static members initialization   */
/*-----------------------------------*/
// Do not forget to reset callbacks in EvaluatorControl::resetCallbacks()
NOMAD::EvalCallbackFunc<NOMAD::CallbackType::EVAL_UPDATE> NOMAD::EvaluatorControl::_cbEvalUpdate = NOMAD::EvaluatorControl::defaultEvalCB<>;
NOMAD::EvalCallbackFunc<NOMAD::CallbackType::EVAL_OPPORTUNISTIC_CHECK> NOMAD::EvaluatorControl::_cbEvalOpportunisticCheck = NOMAD::EvaluatorControl::defaultEvalCB<bool&,bool&>;
bool NOMAD::EvaluatorControl::_customOpportunisticCheck = false;
NOMAD::EvalCallbackFunc<NOMAD::CallbackType::EVAL_STOP_CHECK> NOMAD::EvaluatorControl::_cbEvalStopCheck = NOMAD::EvaluatorControl::defaultEvalCB<bool&>;
NOMAD::EvalCallbackFunc<NOMAD::CallbackType::EVAL_FAIL_CHECK> NOMAD::EvaluatorControl::_cbFailEvalCheck = NOMAD::EvaluatorControl::defaultEvalCB<>;
bool NOMAD::EvaluatorControl::_cbFailEvalCheckIsDefault = true;

std::shared_ptr<NOMAD::ComparePriorityMethod> NOMAD::EvaluatorControl::_userCompMethod = nullptr;


// Template specializations (should be outside class)
// Add evalCallback
template<>
void DLL_EVAL_API NOMAD::EvaluatorControl::addEvalCallback<NOMAD::CallbackType::EVAL_FAIL_CHECK>(const NOMAD::EvalCallbackFunc<CallbackType::EVAL_FAIL_CHECK>& evalCbFunc)
{
    _cbFailEvalCheckIsDefault = false;
    _cbFailEvalCheck = evalCbFunc;
}

template<>
void DLL_EVAL_API NOMAD::EvaluatorControl::addEvalCallback<NOMAD::CallbackType::EVAL_OPPORTUNISTIC_CHECK>(const NOMAD::EvalCallbackFunc<CallbackType::EVAL_OPPORTUNISTIC_CHECK>& evalCbFunc)
{
    _cbEvalOpportunisticCheck = evalCbFunc;
    _customOpportunisticCheck = true;
}


template<>
void NOMAD::EvaluatorControl::addEvalCallback<NOMAD::CallbackType::EVAL_UPDATE>(const NOMAD::EvalCallbackFunc<NOMAD::CallbackType::EVAL_UPDATE>& evalCbFunc)
{
        _cbEvalUpdate = evalCbFunc;
}

template<>
void DLL_EVAL_API NOMAD::EvaluatorControl::addEvalCallback<NOMAD::CallbackType::EVAL_STOP_CHECK>(const NOMAD::EvalCallbackFunc<CallbackType::EVAL_STOP_CHECK>& evalCbFunc)
{
    _cbEvalStopCheck = evalCbFunc;
}

// Run Eval Callback
/// \brief Template specialization. Run user fail eval check callback (no extra argument)
template<>
void NOMAD::EvaluatorControl::runEvalCallback<NOMAD::CallbackType::EVAL_FAIL_CHECK>(NOMAD::EvalQueuePointPtr & evalQueuePoint)
{
    _cbFailEvalCheck(evalQueuePoint);
}

/// \brief Template specialization. Run opportunistic check callback at evaluation (2 extra bool argument)
/// The stop can be an opportunistic evaluation stop (remaining points in queue are not evaluated) OR an iteration
/// stop (remaining points in queue are not evaluated AND remaining steps in iteration are not done)
template<>
void NOMAD::EvaluatorControl::runEvalCallback<NOMAD::CallbackType::EVAL_OPPORTUNISTIC_CHECK>(NOMAD::EvalQueuePointPtr & evalQueuePoint, bool & opportunisticEvalStop, bool &opportunisticIterStop)
{
    // Initialize the flags to indicate the what to do.
    opportunisticEvalStop = opportunisticIterStop = false;
    
    _cbEvalOpportunisticCheck(evalQueuePoint, opportunisticEvalStop, opportunisticIterStop);
    if (opportunisticEvalStop && opportunisticIterStop)
    {
        std::string s = "EvaluatorControl::runEvalCallback<NOMAD::CallbackType::EVAL_OPPORTUNISTIC_CHECK> cannot return both opportunisticEvalStop and opportunisticIterStop to true. The purpose of the callback should be unique." ;
        throw NOMAD::Exception(__FILE__,__LINE__,s);
    }
}

/// \brief Template specialization. Run opportunistic check callback (1 extra bool argument)
template<>
void NOMAD::EvaluatorControl::runEvalCallback<NOMAD::CallbackType::EVAL_STOP_CHECK>(NOMAD::EvalQueuePointPtr & evalQueuePoint, bool & globalStop)
{
    globalStop = false;
    _cbEvalStopCheck(evalQueuePoint, globalStop);
}


/// \brief Template specialization. Run eval update just after evaluation (no extra argument)
template<>
void NOMAD::EvaluatorControl::runEvalCallback<NOMAD::CallbackType::EVAL_UPDATE>(NOMAD::EvalQueuePointPtr & evalQueuePoint)
{
    _cbEvalUpdate(evalQueuePoint);
}

/*------------------------*/
/* Class EvaluatorControl */
/*------------------------*/
// Initialize EvaluatorControl class.
// To be called by the Constructor.
void NOMAD::EvaluatorControl::init()
{
#ifdef _OPENMP
    omp_init_lock(&_evalQueueLock);
#endif // _OPENMP

    _mainThreads.clear();
    _mainThreadInfo.clear();
    addMainThread(NOMAD::getThreadNum(), _evalContParams);

    // Create tmp files
    NOMAD::Evaluator::initializeTmpFiles(_evalContGlobalParams->getAttributeValue<std::string>("TMP_DIR"));

    if (nullptr != _evalContParams)
    {
        _bbMaxBlockSize =  _evalContGlobalParams->getTypeAttribute<size_t>("BB_MAX_BLOCK_SIZE");
        _surrogateMaxBlockSize =  _evalContGlobalParams->getTypeAttribute<size_t>("SURROGATE_MAX_BLOCK_SIZE");
        _modelMaxBlockSize = _evalContGlobalParams->getTypeAttribute<size_t>("MODEL_MAX_BLOCK_SIZE");
        _maxBBEval    = _evalContGlobalParams->getTypeAttribute<size_t>("MAX_BB_EVAL");
        _maxSurrogateEval = _evalContGlobalParams->getTypeAttribute<size_t>("MAX_SURROGATE_EVAL_OPTIMIZATION");
        _maxEval      = _evalContGlobalParams->getTypeAttribute<size_t>("MAX_EVAL");
        _maxBlockEval = _evalContGlobalParams->getTypeAttribute<size_t>("MAX_BLOCK_EVAL");

        _maxModelEval = _evalContGlobalParams->getTypeAttribute<size_t>("MODEL_MAX_EVAL");
        _useCacheFileForRerun = _evalContGlobalParams->getTypeAttribute<bool>("USE_CACHE_FILE_FOR_RERUN");
    }

}


// Terminate EvaluatorControl class.
// To be called by the Destructor.
void NOMAD::EvaluatorControl::destroy()
{
    if (!_evalPointQueue.empty())
    {
        // Show warnings and debug info.
        // Do not scare the user if display degree is medium or low.
        OUTPUT_INFO_START
        std::cout << "Warning: deleting EvaluatorControl with EvalPoints remaining." << std::endl;
        OUTPUT_INFO_END
        bool showDebug = false;
        OUTPUT_DEBUG_START
        showDebug = true;
        OUTPUT_DEBUG_END
        clearQueue(-1, showDebug);
    }

    for (int mainThreadNum : _mainThreads)
    {
        if (remainsEvaluatedPoints(mainThreadNum))
        {
            OUTPUT_INFO_START
            std::cout << "Warning: deleting EvaluatorControl with evaluated points remaining." << std::endl;
            OUTPUT_INFO_END
            // Need to clear number of currently running before retrieving.
            while (getMainThreadInfo(mainThreadNum).getCurrentlyRunning() > 0)
            {
                getMainThreadInfo(mainThreadNum).decCurrentlyRunning();
            }
            // retrieveAllEvaluatedPoints will also clear _evaluatedPoints.
            for (auto evalPoint : retrieveAllEvaluatedPoints(mainThreadNum))
            {
                OUTPUT_DEBUG_START
                std::string s = "Delete evaluated point: ";
                s += evalPoint.display();
                NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
                OUTPUT_DEBUG_END
            }
        }
    }

    // Reset callbacks as they are static attributes, useful for successive runs (also done in EVCInterface::resetCallbacks)
    // NB: valid only because we consider a unique Evaluator in a run
    resetCallbacks();

#ifdef _OPENMP
    omp_destroy_lock(&_evalQueueLock);
#endif // _OPENMP
}

bool NOMAD::EvaluatorControl::hasEvaluator(NOMAD::EvalType evalType) const
{
    return getMainThreadInfo().hasEvaluator(evalType);
}

void NOMAD::EvaluatorControl::setCurrentEvaluatorType(NOMAD::EvalType evalType)
{
    return getMainThreadInfo().setCurrentEvaluatorType(evalType);
}

void NOMAD::EvaluatorControl::setCurrentEvaluatorType(NOMAD::EvalType evalType, const int mainThreadNum)
{
    return _mainThreadInfo.at(mainThreadNum).setCurrentEvaluatorType(evalType);
}

void NOMAD::EvaluatorControl::addEvaluator(NOMAD::EvaluatorPtr evaluator)
{
    return getMainThreadInfo().addEvaluator(evaluator);
}

void NOMAD::EvaluatorControl::addEvaluator(NOMAD::EvaluatorPtr evaluator, const int mainThreadNum)
{
    return _mainThreadInfo.at(mainThreadNum).addEvaluator(evaluator);
}


void NOMAD::EvaluatorControl::addMainThread(const int threadNum,
                                            const std::shared_ptr<NOMAD::EvaluatorControlParameters> evalContParams)
{
    if (!isMainThread(threadNum))
    {
        OUTPUT_DEBUG_START
        std::string s = "Add main thread: " + NOMAD::itos(threadNum);
        NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        OUTPUT_DEBUG_END

        // Each mainthread knows its thread number and has its unique copy of evaluator control parameters.
        _mainThreads.insert(threadNum);
        auto evalContParamsU = std::make_unique<NOMAD::EvaluatorControlParameters>(*evalContParams);
        evalContParamsU->checkAndComply();

        // Since EvcMainThreadInfo has atomic members, we have to use map::emplace and the only way I found to
        // make it work was using this convulated formulation.
        _mainThreadInfo.emplace(std::piecewise_construct, std::forward_as_tuple(threadNum), std::forward_as_tuple(std::move(evalContParamsU)));
    }
}


NOMAD::EvcMainThreadInfo& NOMAD::EvaluatorControl::getMainThreadInfo(const int threadNum) const
{
    int mainThreadNum = threadNum;
    if (-1 == mainThreadNum)
    {
        mainThreadNum = NOMAD::getThreadNum();
    }
    if (!isMainThread(mainThreadNum))
    {
        std::string s = "Thread " + NOMAD::itos(mainThreadNum);
        s += " is not a main thread. EvaluatorControl::getMainThreadInfo called with argument threadNum = " + NOMAD::itos(threadNum);
        throw NOMAD::Exception(__FILE__,__LINE__,s);
    }

    return _mainThreadInfo.at(mainThreadNum);
}


size_t NOMAD::EvaluatorControl::getModelEval(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getModelEval();
}


void NOMAD::EvaluatorControl::resetModelEval(const int mainThreadNum)
{
    getMainThreadInfo(mainThreadNum).resetModelEval();
}


size_t NOMAD::EvaluatorControl::getBbEvalInSubproblem(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getBbEvalInSubproblem();
}


void NOMAD::EvaluatorControl::resetBbEvalInSubproblem(const int mainThreadNum)
{
    getMainThreadInfo(mainThreadNum).resetBbEvalInSubproblem();
}


// getNbEval() and setNbEval interface with Mads.
// The number of cache hits must be added to getNbEval(), and removed when
// setting _nbEvalSentToEvaluator.
size_t NOMAD::EvaluatorControl::getNbEval() const
{
    return _nbEvalSentToEvaluator + NOMAD::CacheBase::getNbCacheHits();
}


void NOMAD::EvaluatorControl::setNbEval(const size_t nbEval)
{
    if (nbEval < NOMAD::CacheBase::getNbCacheHits())
    {
        std::cout << "Warning: trying to set EvaluatorControl NbEval to negative value: " << nbEval << " - " << NOMAD::CacheBase::getNbCacheHits() << std::endl;
    }
    else
    {
        _nbEvalSentToEvaluator = nbEval - NOMAD::CacheBase::getNbCacheHits();
    }
}


size_t NOMAD::EvaluatorControl::getLapMaxBbEval(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getLapMaxBbEval();
}


void NOMAD::EvaluatorControl::setLapMaxBbEval(const size_t maxBbEval)
{
    getMainThreadInfo().setLapMaxBbEval(maxBbEval);
}


void NOMAD::EvaluatorControl::resetLapBbEval()
{
    getMainThreadInfo().resetLapBbEval();
}


size_t NOMAD::EvaluatorControl::getLapBbEval(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getLapBbEval();
}


void NOMAD::EvaluatorControl::incrementNbRevealingIter()
{
    _nbRevealingIter++;
}

size_t NOMAD::EvaluatorControl::getQueueSize(const int mainThreadNum) const
{
    if (-1 == mainThreadNum)
    {
        return _evalPointQueue.size();
    }

    return getMainThreadInfo(mainThreadNum).getNbPointsInQueue();
}


bool NOMAD::EvaluatorControl::getDoneWithEval(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getDoneWithEval();
}


void NOMAD::EvaluatorControl::setDoneWithEval(const int mainThreadNum, const bool doneWithEval)
{
    getMainThreadInfo(mainThreadNum).setDoneWithEval(doneWithEval);
}


void NOMAD::EvaluatorControl::setBarrier(const std::shared_ptr<NOMAD::BarrierBase> barrier)
{
    getMainThreadInfo().setBarrier(barrier);
}


const std::shared_ptr<NOMAD::BarrierBase> NOMAD::EvaluatorControl::getBarrier(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getBarrier();
}

void NOMAD::EvaluatorControl::setBestIncumbent(const int mainThreadNum, const NOMAD::EvalPointPtr bestIncumbent)
{
    getMainThreadInfo(mainThreadNum).setBestIncumbent(bestIncumbent);
}

void NOMAD::EvaluatorControl::resetBestIncumbent(const int mainThreadNum)
{
    getMainThreadInfo(mainThreadNum).resetBestIncumbent();
}


const NOMAD::EvalPointPtr NOMAD::EvaluatorControl::getBestIncumbent(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getBestIncumbent();
}




void NOMAD::EvaluatorControl::setComputeType(NOMAD::ComputeType computeType)
{
    getMainThreadInfo().setComputeType(computeType);
}


const NOMAD::ComputeType& NOMAD::EvaluatorControl::getComputeType(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getComputeType();
}


void NOMAD::EvaluatorControl::setLastSuccessfulFeasDir(const std::shared_ptr<NOMAD::Direction>& feasDir)
{
    getMainThreadInfo().setLastSuccessfulFeasDir(feasDir);
}


void NOMAD::EvaluatorControl::setLastSuccessfulInfDir(const std::shared_ptr<NOMAD::Direction>& infDir)
{
    getMainThreadInfo().setLastSuccessfulInfDir(infDir);
}


const std::shared_ptr<NOMAD::Direction>& NOMAD::EvaluatorControl::getLastSuccessfulFeasDir() const
{
    return getMainThreadInfo().getLastSuccessfulFeasDir();
}


const std::shared_ptr<NOMAD::Direction>& NOMAD::EvaluatorControl::getLastSuccessfulInfDir() const
{
    return getMainThreadInfo().getLastSuccessfulInfDir();
}


void NOMAD::EvaluatorControl::setStopReason(const int mainThreadNum, const NOMAD::EvalMainThreadStopType& s)
{
    getMainThreadInfo(mainThreadNum).setStopReason(s);
}


const NOMAD::StopReason<NOMAD::EvalMainThreadStopType>& NOMAD::EvaluatorControl::getStopReason(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getStopReason();
}


std::string NOMAD::EvaluatorControl::getStopReasonAsString(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getStopReasonAsString();
}


bool NOMAD::EvaluatorControl::testIf(const NOMAD::EvalMainThreadStopType& s) const
{
    return getMainThreadInfo().testIf(s);
}


bool NOMAD::EvaluatorControl::checkEvalTerminate(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).checkEvalTerminate();
}


std::vector<NOMAD::EvalPoint> NOMAD::EvaluatorControl::retrieveAllEvaluatedPoints(const int threadNum)
{
    return getMainThreadInfo(threadNum).retrieveAllEvaluatedPoints();
}


void NOMAD::EvaluatorControl::addEvaluatedPoint(const int threadNum, const NOMAD::EvalPoint& evaluatedPoint)
{
    getMainThreadInfo(threadNum).addEvaluatedPoint(evaluatedPoint);
}


bool NOMAD::EvaluatorControl::remainsEvaluatedPoints(const int threadNum) const
{
    return getMainThreadInfo(threadNum).remainsEvaluatedPoints();
}


void NOMAD::EvaluatorControl::clearEvaluatedPoints(const int threadNum)
{
    getMainThreadInfo(threadNum).clearEvaluatedPoints();
}


const NOMAD::SuccessType& NOMAD::EvaluatorControl::getSuccessType(const int threadNum) const
{
    return getMainThreadInfo(threadNum).getSuccessType();
}


void NOMAD::EvaluatorControl::setSuccessType(const int threadNum, const NOMAD::SuccessType& success)
{
    getMainThreadInfo(threadNum).setSuccessType(success);
}


NOMAD::Double NOMAD::EvaluatorControl::getHMax(const int threadNum) const
{
    NOMAD::Double hMax = NOMAD::INF;
    auto barrier = getBarrier(threadNum);
    if (nullptr != barrier)
    {
        hMax = barrier->getHMax();
    }

    return hMax;
}

std::shared_ptr<NOMAD::EvalParameters> NOMAD::EvaluatorControl::getCurrentEvalParams(const int threadNum) const
{
    return getMainThreadInfo(threadNum).getCurrentEvalParams();
}

const NOMAD::BBOutputTypeList & NOMAD::EvaluatorControl::getCurrentBBOutputTypeList(const int threadNum) const
{
    return getMainThreadInfo(threadNum).getCurrentBBOutputTypeList();
}

NOMAD::EvalSortType NOMAD::EvaluatorControl::getEvalSortType(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getEvalSortType();
}


void NOMAD::EvaluatorControl::setEvalSortType(NOMAD::EvalSortType evalSortType)
{
    getMainThreadInfo().setEvalSortType(evalSortType);
}


bool NOMAD::EvaluatorControl::getOpportunisticEval(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getOpportunisticEval();
}


void NOMAD::EvaluatorControl::setOpportunisticEval(const bool opportunisticEval)
{
    getMainThreadInfo().setOpportunisticEval(opportunisticEval);
}


bool NOMAD::EvaluatorControl::getUseCache(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getUseCache();
}


void NOMAD::EvaluatorControl::setUseCache(const bool useCache)
{
    getMainThreadInfo().setUseCache(useCache);
}

bool NOMAD::EvaluatorControl::getSurrogateOptimization(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getSurrogateOptimization();
}


void NOMAD::EvaluatorControl::setSurrogateOptimization(const bool surrogateOptimization)
{
    getMainThreadInfo().setSurrogateOptimization(surrogateOptimization);
}


NOMAD::EvalType NOMAD::EvaluatorControl::getCurrentEvalType(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getCurrentEvalType();
}


size_t NOMAD::EvaluatorControl::getMaxBbEvalInSubproblem(const int mainThreadNum) const
{
    return getMainThreadInfo(mainThreadNum).getMaxBbEvalInSubproblem();
}


void NOMAD::EvaluatorControl::setMaxBbEvalInSubproblem(const size_t maxBbEval)
{
    return getMainThreadInfo().setMaxBbEvalInSubproblem(maxBbEval);
}


void NOMAD::EvaluatorControl::lockQueue()
{
#ifdef _OPENMP
    // Sanity check before locking the queue.
    // Verify we are in a main thread.
    const int threadNum = NOMAD::getThreadNum();
    if (!isMainThread(threadNum))
    {
        std::string err = "Error: EvaluatorControl::lockQueue called from thread ";
        err += std::to_string(threadNum);
        err += ", which is not a main thread.";
        throw NOMAD::Exception(__FILE__, __LINE__, err);
    }
    // Note: The queue could be already locked, ex. by popBlock.
    omp_set_lock(&_evalQueueLock);
#endif // _OPENMP
}


void NOMAD::EvaluatorControl::unlockQueue(bool doSort, const size_t keepN, const NOMAD::StepType& removeStepType)
{
    const int threadNum = NOMAD::getThreadNum();
    // Sanity checks before unlocking the queue
    // 1- Verify we are in a main thread.
    if (!isMainThread(threadNum))
    {
        std::string err = "Error: EvaluatorControl::unlockQueue called from thread ";
        err += std::to_string(threadNum);
        err += ", which is not a main thread.";
        throw NOMAD::Exception(__FILE__, __LINE__, err);
    }

#ifdef _OPENMP
    // 2- Verify the queue was already locked. The lock should have been set by lockQueue().
    if (omp_test_lock(&_evalQueueLock))
    {
        // Queue was not locked. Queue is now locked.
        std::string err = "Error: trying to unlock a queue that was not locked.";
        omp_unset_lock(&_evalQueueLock);
        throw NOMAD::Exception(__FILE__, __LINE__, err);
    }
#endif // _OPENMP

    // The EvalQueuePoints are added randomly.
    // Sort the queue, using sorting algorithm, if doSort is true (default).
    // In non-opportunistic context, it is useless to sort.
    if (doSort && getOpportunisticEval(threadNum) && getQueueSize(-1) > 1)
    {
        sort(_evalPointQueue,false);
    }

    if (0 == keepN)
    {
        std::string err("EvaluatorControl: unlockQueue: Cannot keep 0 points");
        throw NOMAD::Exception(__FILE__, __LINE__, err);
    }

    // If keepN < INF_SIZE_T, keep only that number of points.
    if (keepN < INF_SIZE_T && keepN < getQueueSize(threadNum))
    {
        // Number of removeable points
        const size_t nbPoints = std::count_if(_evalPointQueue.begin(), _evalPointQueue.end(),
                                [this, threadNum, removeStepType](const std::shared_ptr<NOMAD::EvalQueuePoint>& evalQueuePoint)
                                {
                                    return canErase(evalQueuePoint, threadNum, removeStepType);
                                });
        if (nbPoints > keepN)
        {
            const size_t nbPointsToErase = nbPoints - keepN;
            size_t nbErasablePoints = 0;

            // Find iterator where the removal must end.
            auto itEraseEnd = _evalPointQueue.begin();
            for (; (nbErasablePoints < nbPointsToErase && itEraseEnd < _evalPointQueue.end()); ++itEraseEnd)
            {
                if (canErase((*itEraseEnd), threadNum, removeStepType))
                {
                    nbErasablePoints++;
                }
            }

            // Remove from begin to end, because the points with the higest priority are at the end.
            auto itEraseBegin = std::remove_if(_evalPointQueue.begin(),
                                 itEraseEnd,
                                 [this, threadNum, removeStepType](const NOMAD::EvalQueuePointPtr& evalQueuePoint)
                                {
                                    if (canErase(evalQueuePoint, threadNum, removeStepType))
                                    {
                                        getMainThreadInfo(threadNum).decNbPointsInQueue();
                                        return true;
                                    }
                                    else
                                    {
                                        return false;
                                    }
                                });
            _evalPointQueue.erase(itEraseBegin, itEraseEnd);

            OUTPUT_DEBUG_START
            std::string s = "Removing " + NOMAD::itos(nbErasablePoints) + " points from evaluation queue";
            NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
            s = "Evaluation queue after clean-up:";
            NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
            for (auto it = _evalPointQueue.rbegin(); it != _evalPointQueue.rend(); ++it)
            {
                auto evalPoint = (*it);
                s = "\t" + evalPoint->display();
                NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
            }
            OUTPUT_DEBUG_END
        }
    }

    // Now, unlock the queue.
#ifdef _OPENMP
    omp_unset_lock(&_evalQueueLock);
#endif // _OPENMP
}


bool NOMAD::EvaluatorControl::canErase(const NOMAD::EvalQueuePointPtr &evalQueuePoint,
                                       const int threadNum,
                                       const NOMAD::StepType& removeStepType) const
{
    return (threadNum == evalQueuePoint->getThreadAlgo()
            && removeStepType == evalQueuePoint->getGenStep());
}


// Add an EvalPoint to the Queue
bool NOMAD::EvaluatorControl::addToQueue(const NOMAD::EvalQueuePointPtr evalQueuePoint)
{
    bool pointInserted = false;

    if (!evalQueuePoint->ArrayOfDouble::isComplete())
    {
        std::string err("EvaluatorControl: addToQueue: Adding an undefined Point for evaluation: ");
        err += evalQueuePoint->getX()->NOMAD::Point::display();
        throw NOMAD::Exception(__FILE__, __LINE__, err);
    }
#ifdef _OPENMP
    // Thread-safety necessary here.
    // Ensure we will keep adding points until we ask to eval the queue, using run().
    // I.e. Ensure the queue is already locked.
    if (omp_test_lock(&_evalQueueLock))
    {
        std::string err = "Error: trying to add an element to a queue that was not locked.";
        // Unlock queue before throwing exception.
        // If we are in this section, it means that the queue was locked by
        // the call to omp_test_lock().
        omp_unset_lock(&_evalQueueLock);
        throw NOMAD::Exception(__FILE__, __LINE__, err);
    }
#endif // _OPENMP

    // Insert at the beginning of the queue. The points will be sorted when the
    // queue is unlocked.
    // If points are not sorted, they remain in reverse lexicographical order.
    // Note: If cache is not used, allow multiple evaluations of the same point.

    NOMAD::EvalPoint foundEvalPoint;
    auto evalType = evalQueuePoint->getEvalType();
    const int mainThreadNum = evalQueuePoint->getThreadAlgo();

    // Do not add eval queue point if no evaluator can evaluate this eval type
    // This can happen when calling Mads::Suggest. Some points need to be evaluated by a model (sort, n+1 quad), but BB eval point do not need to be evaluated (a fake BB evaluator is used). This is problematic when popping eval points.
    const NOMAD::Evaluator * evaluator = getMainThreadInfo(mainThreadNum).getEvaluator(evalType);
    if (!evaluator || NOMAD::EvalXDefined::UNDEFINED == evaluator->getEvalXDefined())
    {
        return false;
    }

    bool useCache = getUseCache(mainThreadNum);
    if (std::find_if(_evalPointQueue.begin(), _evalPointQueue.end(), [evalQueuePoint](NOMAD::EvalQueuePointPtr eqp){ return (*eqp == *evalQueuePoint); }) != _evalPointQueue.end())  // Note: test for cache maybe needed. For now it seems not necessary.
    {
        // Point is already in queue, do not insert it again.
        OUTPUT_DEBUG_START
        std::string s = "Point is already in queue (do not insert again)";
        NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        OUTPUT_DEBUG_END
    }
    else if (useCache
             && NOMAD::CacheBase::getInstance()->find(*evalQueuePoint, foundEvalPoint)
             && (NOMAD::EvalStatusType::EVAL_IN_PROGRESS == foundEvalPoint.getEvalStatus(evalType)
                 || NOMAD::EvalStatusType::EVAL_WAIT == foundEvalPoint.getEvalStatus(evalType)))
    {
        // Point is in cache and evaluation already in progress
        OUTPUT_DEBUG_START
        std::string s = "Evaluation is already in progress for point: " + foundEvalPoint.displayAll();
        NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        OUTPUT_DEBUG_END
    }
    else
    {
        auto it = _evalPointQueue.insert(_evalPointQueue.begin(), evalQueuePoint);
        pointInserted = !(it == _evalPointQueue.end());
        if (pointInserted)
        {
            getMainThreadInfo(mainThreadNum).incNbPointsInQueue();
        }
    }

    return pointInserted;
}


// Get the first good EvalPoint from the top of the Queue and pop it
// Return true if it worked, false if it failed.
bool NOMAD::EvaluatorControl::popEvalPoint(NOMAD::EvalQueuePointPtr &evalQueuePoint,
                                           NOMAD::Evaluator*& evaluator,
                                           NOMAD::Double& hMax)
{
    const int threadNum = NOMAD::getThreadNum();
    bool success = false;

    if (!_evalPointQueue.empty())
    {
        int lastIndexEvalQueuePoint = static_cast<int>(_evalPointQueue.size())-1;

#ifndef _OPENMP
        NOMAD::EvalType keepEvalType = _evalPointQueue[lastIndexEvalQueuePoint]->getEvalType();
#endif

        // Find the first EvalPoint from the end that satisfies these conditions:
        // - EvalPoint uses the given evaluator, if evaluator is given;
        // - hMax for the main thread number of this EvalPoint is hMax, if defined;
        // - EvalPoint's generating main thread is still running;
        // and pop that EvalPoint.
        // Update the evaluator (IN/OUT argument) if it was not provided
        // Similarly, update the hMax (IN/OUT argument) if it was not defined
        for (int i = lastIndexEvalQueuePoint; i >= 0; i--)
        {
            // Use an eval queue point to get the main thread number.
            int mainThreadNum = _evalPointQueue[i]->getThreadAlgo();

            // Condition for stopping the current secondary thread coming from main thread
            if (threadNum != mainThreadNum && stopMainEval(mainThreadNum, false /*false: no display if stop*/))
            {
                continue;
            }

            auto evalType = _evalPointQueue[i]->getEvalType();
            // Safeguard. For single thread run (no OPENMP). Eval type must be consistent among eval queue points
#ifndef _OPENMP
            if (evalType != keepEvalType)
            {
                std::string err = "Error: elements of queue have different type.";
                throw NOMAD::Exception(__FILE__, __LINE__, err);
            }
#endif

            // If we don't have an evaluator or an eval type prescribed, we figure it out from the first eval queue point.
            if (   (!evaluator || evaluator == getMainThreadInfo(mainThreadNum).getCurrentEvaluator())
                && (!hMax.isDefined() || hMax == getHMax(mainThreadNum))   )
            {
                if (!evaluator) // Pick up an evaluator of this eval type.
                {
                    evaluator = const_cast<NOMAD::Evaluator*>(getMainThreadInfo(mainThreadNum).getEvaluator(evalType));

                    // Do no pick evalPointQueue that cannot be evaluated.
                    // The evaluator for this eval type is fake with no evaluation capability. This is used to Suggest trial points (no evaluation). There is probably another eval point with Model eval type to pop.
                    if (NOMAD::EvalXDefined::UNDEFINED == evaluator->getEvalXDefined() )
                    {
                        evaluator = nullptr;
                        continue;
                    }
                }

                // Do not pick evalPointQueue with eval type not consistent with evaluator specialty.
                if (evalType != evaluator->getEvalType())
                {
                    continue;
                }

                evalQueuePoint = std::move(_evalPointQueue[i]);
                _evalPointQueue.erase(_evalPointQueue.begin()+i);
                if (!hMax.isDefined())
                {
                    hMax = getHMax(mainThreadNum);
                }
                getMainThreadInfo(mainThreadNum).incCurrentlyRunning();
                getMainThreadInfo(mainThreadNum).decNbPointsInQueue();
                success = true;
                break;
            }
        }
    }

    return success;
}


bool NOMAD::EvaluatorControl::popBlock(NOMAD::BlockForEval &block)
{
    bool success = false;
    bool popWorks = true;
    size_t blockSize = 1;



    // Need to pop a block of points which will all be evaluated using the
    // same evaluator and the same hMax.
    // A block of points may contain points that were generated by different
    // main threads, as long as the evaluator and hMax are the same
    // for all points.
    NOMAD::Evaluator* evaluator = nullptr;
    NOMAD::Double hMax;


    // Note: The queue lock used to be on popEvalPoint.
    // We moved it here to ensure that blocks are filled as much as possible.
    // Otherwise, we could have 2 threads getting half-filled blocks instead
    // of one thread with a full block and one with no blocks.
#ifdef _OPENMP
    omp_set_lock(&_evalQueueLock);
#endif // _OPENMP
    bool firstPop = true;

    while (_evalPointQueue.size() > 0 && block.size() < blockSize && popWorks)
    {
        NOMAD::EvalQueuePointPtr evalQueuePoint;
        popWorks = popEvalPoint(evalQueuePoint, evaluator, hMax);
        if (popWorks)
        {
            block.push_back(std::move(evalQueuePoint));
            success = true;
            if (firstPop)
            {
                // Update blockSize depending on eval type
                switch (evaluator->getEvalType())
                {
                    case NOMAD::EvalType::MODEL:
                        blockSize = _modelMaxBlockSize->getValue();
                        break;
                    case NOMAD::EvalType::SURROGATE:
                        blockSize = _surrogateMaxBlockSize->getValue();
                        break;
                    case NOMAD::EvalType::BB:
                        blockSize = _bbMaxBlockSize->getValue();
                        break;
                    default:
                        blockSize = 1; // default value
                        break;
                }
                firstPop = false;
            }
        }
    }
#ifdef _OPENMP
    omp_unset_lock(&_evalQueueLock);
#endif

    return success;
}

void NOMAD::EvaluatorControl::sort(std::vector<EvalQueuePointPtr> & evalPointsPtrToSort, bool forceRandom)
{

    if (evalPointsPtrToSort.size() == 0)
    {
        return;
    }

    std::shared_ptr<NOMAD::ComparePriorityMethod> compMethod = nullptr;
    auto evalSortType =  getEvalSortType();
    // If there is an user-defined sort method, use it.
    if (nullptr != _userCompMethod)
    {
        compMethod = _userCompMethod;
    }
    else if (NOMAD::EvalSortType::RANDOM == evalSortType || forceRandom)
    {
        compMethod = std::make_shared<NOMAD::RandomComp>(evalPointsPtrToSort.size());
    }
    else if (NOMAD::EvalSortType::DIR_LAST_SUCCESS == evalSortType)
    {
        compMethod = makeCompMethodOrderByDirection();
    }
    else if (NOMAD::EvalSortType::SURROGATE == evalSortType)
    {
        // Consider all SURROGATE evaluations are already done.
        compMethod = std::make_shared<NOMAD::OrderByEval>(NOMAD::EvalType::SURROGATE);
    }
    // For now, consider only quadratic_model, can be generalized
    else if (NOMAD::EvalSortType::QUADRATIC_MODEL == evalSortType)
    {
        // Check if QUADRATIC_MODEL evaluations are available.

        // Check model evals are valid
        bool valid_model_eval = true;
        for (auto eqp : evalPointsPtrToSort)
        {
            if (nullptr == eqp->getEval(EvalType::MODEL) )
            {
                OUTPUT_DEBUG_START
                std::string s = " Model eval missing for: " + eqp->displayAll();
                NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
                OUTPUT_DEBUG_END
                valid_model_eval = false;
                break;
            }
        }

        if (valid_model_eval)
        {
            compMethod = std::make_shared<NOMAD::OrderByEval>(NOMAD::EvalType::MODEL);
        }
        else
        {
            // Revert to Order by direction
            compMethod = makeCompMethodOrderByDirection();
        }
    }
    else if (NOMAD::EvalSortType::LEXICOGRAPHICAL == evalSortType)
    {
        // Points are already in lexicographical order.
    }

    if (nullptr != compMethod)
    {
        NOMAD::ComparePriority comp(compMethod);

        std::string s;
        OUTPUT_DEBUG_START
        std::string sortMethodName = compMethod->getName();
        if (sortMethodName.empty())
        {
            sortMethodName = "User defined method";
        }
        s = "Sort using " + sortMethodName;
        NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        s = "Evaluation points before sort:";
        NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        // Display in reverse order: as the _evalPointQueue is popped,
        // the first point being evaluated is at the end of the queue.
        // We want to show the first point to be evaluated first.
        for (auto it = evalPointsPtrToSort.rbegin(); it != evalPointsPtrToSort.rend(); ++it)
        {
            auto evalPoint = (*it);
            s = "\t" + evalPoint->display();
            NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        }
        OUTPUT_DEBUG_END

        std::sort(evalPointsPtrToSort.begin(), evalPointsPtrToSort.end(), comp);

        OUTPUT_DEBUG_START
        s = "Evaluation points after sort:";
        NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        for (auto it = evalPointsPtrToSort.rbegin(); it != evalPointsPtrToSort.rend(); ++it)
        {
            auto evalPoint = (*it);
            s = "\t" + evalPoint->display();
            NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        }
        OUTPUT_DEBUG_END
    }
}



bool NOMAD::EvaluatorControl::checkModelEvals() const
{
    // Return false if a single eval queue point has no model evaluation information
    bool valid_eval = true;

    for (auto eqp : _evalPointQueue)
    {
        if (nullptr == eqp->getEval(EvalType::MODEL) )
        {
            OUTPUT_DEBUG_START
            std::string s = "    Main thread: " + std::to_string(eqp->getThreadAlgo()) + " Model eval missing for: " + eqp->displayAll();
            NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
            OUTPUT_DEBUG_END
            valid_eval = false;
            break;
        }
    }

    return valid_eval;
}


std::shared_ptr<NOMAD::OrderByDirection> NOMAD::EvaluatorControl::makeCompMethodOrderByDirection() const
{
    // Default: Use last successful directions.
    // Fill vector for argument to OrderByDirection.
    // This sort is also use if a Quadratic Model cannot be constructed.
    // If no valid lass success direction, revert to tag ordering
    std::vector<std::shared_ptr<NOMAD::Direction>> lastSuccessfulFeasDirs(_mainThreads.size());
    std::vector<std::shared_ptr<NOMAD::Direction>> lastSuccessfulInfDirs(_mainThreads.size());
    for (auto mainth : _mainThreads)
    {
        lastSuccessfulFeasDirs[mainth] = getMainThreadInfo(mainth).getLastSuccessfulFeasDir();
        lastSuccessfulInfDirs[mainth] = getMainThreadInfo(mainth).getLastSuccessfulInfDir();
    }

    return std::make_shared<NOMAD::OrderByDirection>(lastSuccessfulFeasDirs, lastSuccessfulInfDirs);

}



size_t NOMAD::EvaluatorControl::clearQueue(const int mainThreadNum, const bool showDebug)
{
    size_t nbPointsErased = 0;

#ifdef _OPENMP
    omp_set_lock(&_evalQueueLock);
#endif // _OPENMP

    if (-1 == mainThreadNum)
    {
        nbPointsErased = _evalPointQueue.size();
        _evalPointQueue.clear();
        for (auto mt : _mainThreads)
        {
            getMainThreadInfo(mt).resetNbPointsInQueue();
        }
    }
    else
    {
        nbPointsErased = std::count_if(_evalPointQueue.begin(), _evalPointQueue.end(),
                                [mainThreadNum](const std::shared_ptr<NOMAD::EvalQueuePoint>& evalQueuePoint)
                                {
                                    return mainThreadNum == evalQueuePoint->getThreadAlgo();
                                });

        auto it = std::remove_if(_evalPointQueue.begin(),
                                 _evalPointQueue.end(),
                                 [mainThreadNum, showDebug](const NOMAD::EvalQueuePointPtr& evalQueuePoint)
                                {
                                    if (mainThreadNum == evalQueuePoint->getThreadAlgo())
                                    {
                                        OUTPUT_DEBUG_START
                                        if (showDebug)
                                        {
                                            std::string s = "Delete point from queue: ";
                                            s += evalQueuePoint->display();
                                            NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
                                        }
                                        OUTPUT_DEBUG_END
                                        return true;
                                    }
                                    else
                                    {
                                        return false;
                                    }
                                });
        _evalPointQueue.erase(it, _evalPointQueue.end());
        getMainThreadInfo(mainThreadNum).resetNbPointsInQueue();
    }

#ifdef _OPENMP
    omp_unset_lock(&_evalQueueLock);
#endif // _OPENMP

    return nbPointsErased;
}


// Evaluate all points in the queue for a single evalType, or stop under some conditions.
// If strategy is opportunistic (parameter EVAL_OPPORTUNISTIC), stop
// as soon as a successful point is found, and flush the queue.
//
// Points must already be in the cache.
NOMAD::SuccessType NOMAD::EvaluatorControl::run()
{
    const int threadNum = NOMAD::getThreadNum();
    const bool inMainThread = isMainThread(threadNum);
    // Main threads only:
    //  - Reset success
    //  - Set Barrier and flag OpportunisticEval
    //  - Print info



    if (inMainThread)
    {
        // At this point, the threads other than the current main thread might have already
        // started evaluating.

#pragma omp critical
        {
            if (nullptr == getMainThreadInfo(threadNum).getCurrentEvaluator())
            {
                std::string err("EvaluatorControl: No evaluator available in MainThread #");
                err += std::to_string(threadNum);
                throw NOMAD::Exception(__FILE__, __LINE__, err);
            }
        }

        setSuccessType(threadNum, NOMAD::SuccessType::UNSUCCESSFUL);

        // An empty eval queue must be accounted for
        if (0 == getQueueSize(threadNum))
        {
            setStopReason(threadNum, NOMAD::EvalMainThreadStopType::EMPTY_LIST_OF_POINTS);
        }

        // Update stop reason.
        if (   (checkEvalTerminate(threadNum))
            || NOMAD::AllStopReasons::checkEvalGlobalTerminate()
            || NOMAD::AllStopReasons::checkBaseTerminate())
        {
            OUTPUT_DEBUG_START
            std::string sStopReason = "EvaluatorControl stop reason (before evaluating queue): ";
            if (checkEvalTerminate(threadNum))
            {
                sStopReason += getStopReasonAsString(threadNum) + " (Eval Main Thread)";
            }
            else if (NOMAD::AllStopReasons::checkEvalGlobalTerminate())
            {
                sStopReason += NOMAD::AllStopReasons::getEvalGlobalStopReasonAsString();
            }
            else
            {
                sStopReason += NOMAD::AllStopReasons::getBaseStopReasonAsString() + " (Base)";
            }
            NOMAD::OutputQueue::Add(sStopReason, NOMAD::OutputLevel::LEVEL_DEBUG);
            NOMAD::OutputQueue::Flush();
            OUTPUT_DEBUG_END
        }
        else
        {
            // Ready to run.
            // Reset EvalMainThreadStopType to STARTED.
            setStopReason(threadNum, NOMAD::EvalMainThreadStopType::STARTED);
        }

        resetSuccessStats();

        OUTPUT_DEBUG_START
        std::string s = "Start evaluation";
#ifdef _OPENMP
        s += " in parallel with ";
        s += NOMAD::itos(omp_get_num_threads());
        s += " threads. Main thread = ";
        s += std::to_string(threadNum);
#endif // _OPENMP
        s += ". Opportunism = ";
        s += NOMAD::boolToString(getOpportunisticEval(threadNum));
        s += ", UseCache = ";
        s += NOMAD::boolToString(getUseCache(threadNum));
        s += ", Barrier";
        auto barrier = getBarrier(threadNum);
        if (nullptr == barrier)
        {
            s += " = NULL";
            NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        }
        else
        {
            NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
            std::vector<std::string> vs = barrier->display(4); // Display a maximum of 4 xFeas and 4 xInf
            for (const auto & si : vs)
            {
                NOMAD::OutputQueue::Add(si, NOMAD::OutputLevel::LEVEL_DEBUG);
            }
        }
        OUTPUT_DEBUG_END
    }

    // Queue runs forever on non-main threads.
    // On main threads, queue runs until stopMainEval() is true.
    bool conditionForStop = false;

    // For debug display
    time_t lastDisplayed = 0;

    // conditionForStop is true if we are in a main thread and stopMainEval() returns true.
    // conditionForStop is true in any thread if reachedMaxEval() returns true; otherwise, it is always false.
    while (!conditionForStop && !_allDoneWithEval)
    {
        // Check for stop conditions
        if (inMainThread)
        {
            conditionForStop = stopMainEval(threadNum, true /*true: display info if stop*/ );
        }

        // If we reached max eval, we also stop (valid for all threads).
        conditionForStop = conditionForStop || reachedMaxEval();

        NOMAD::BlockForEval block;
        if (!conditionForStop && popBlock(block))
        {
            bool evalBlockOk = evalBlock(block);

            // Update SuccessType
            // success is a member of EvcMainThreadInfo and so it can be shared between secundary threads.
#ifdef _OPENMP
#pragma omp critical(updateSuccessType)
#endif // _OPENMP
            {
                if (evalBlockOk)
                {
                    for (auto it = block.begin(); it < block.end(); it++)
                    {
                        NOMAD::EvalQueuePointPtr evalQueuePoint = (*it);
                        const int mainThreadNum = evalQueuePoint->getThreadAlgo();

                        
                        // User callback
                        // Note: should be done before accessing success type as this may be modified in the callback (e.g. in DiscoMads)
                        bool customOpportunisticEvalStop = false, customOpportunisticIterStop = false;
                        if (_customOpportunisticCheck)
                        {
                            runEvalCallback<NOMAD::CallbackType::EVAL_OPPORTUNISTIC_CHECK>(evalQueuePoint,customOpportunisticEvalStop,customOpportunisticIterStop);
                        }

                        const NOMAD::SuccessType success = evalQueuePoint->getSuccess();

                        // Update success type for return
                        if (success > getSuccessType(mainThreadNum))
                        {
                            setSuccessType(mainThreadNum, success);
                        }

                        if (   NOMAD::SuccessType::FULL_SUCCESS == success
                            && evalTypeAsBB(evalQueuePoint->getEvalType(), mainThreadNum))
                        {
                            //PhaseOne full success
                            if (evalQueuePoint->getGenByPhaseOne())
                            {
                                _nbPhaseOneSuccess++;
                            }

                            if (!evalQueuePoint->getRelativeSuccess())
                            {
                                _indexBestInfeasEval = getBbEval();
                            }

                        }
                        if (evalQueuePoint->getRelativeSuccess())
                        {
                            _nbRelativeSuccess++;
                            _indexSuccBlockEval = getBlockEval();
                            _indexBestFeasEval = getBbEval();
                        }

                        // Output in history (always) and solution (FULL_SUCCESS only)
                        addDirectToFileInfo(evalQueuePoint);

                        // Opportunism on full success only (default opportunism only)
                        // See belon when a callback is added for checking custom opportunistic criterion
                        if (!_customOpportunisticCheck && getOpportunisticEval(mainThreadNum) && getSuccessType(mainThreadNum) >= NOMAD::SuccessType::FULL_SUCCESS)
                        {
                            setStopReason(mainThreadNum, NOMAD::EvalMainThreadStopType::OPPORTUNISTIC_SUCCESS);
                        }

                        // Stop reason for OPPORTUNISTIC_SUCCESS must be shadowed by
                        // CUSTOM_OPPORTUNISTIC_EVAL_STOP OR
                        // CUSTOM_OPPORTUNISTIC_ITER_STOP
                        // Both cannot be true to have a clear decision what to do (eval stop or iter stop). The flags are automatically checked after the callback.
                        
                        // Associate custom opportunistic eval stop to mainthread
                        // Testing for success is done later to decide if we do the next step of the iteration.
                        if (customOpportunisticEvalStop)
                        {
                            setStopReason(mainThreadNum, NOMAD::EvalMainThreadStopType::CUSTOM_OPPORTUNISTIC_EVAL_STOP);
                        }

                        // Associate custom opportunistic stop to mainThread
                        if (customOpportunisticIterStop)
                        {
                            setStopReason(mainThreadNum, NOMAD::EvalMainThreadStopType::CUSTOM_OPPORTUNISTIC_ITER_STOP);
                        }
                    }
                }
                else
                {
                    // EvalBlock not ok
                    // Let's try to write block points into file
                    for (auto it = block.begin(); it < block.end(); it++)
                    {
                        NOMAD::EvalQueuePointPtr evalQueuePoint = (*it);
                        // Output in history (always)
                        addDirectToFileInfo(evalQueuePoint);
                        
                    }
                }

                // Cannot modify _succesStats outside of critical
                addStatsInfo(block);
            }   // End critical(updateSuccessType)


            for (size_t i = 0; i < block.size(); i++)
            {
                getMainThreadInfo(block[i]->getThreadAlgo()).decCurrentlyRunning();
            }
        }
        else if (!_allDoneWithEval)
        {
            displayDebugWaitingInfo(lastDisplayed);
        }
        else // Queue is empty and we are doneWithEval
        {
            if (inMainThread)
            {
                OUTPUT_DEBUG_START
                std::string s = "Queue is empty and we are done with evaluations.";
                NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
                OUTPUT_DEBUG_END
            }
            break;
        }

    }   // End of while loop: Exit for this main thread.
        // Other threads keep on looping.

    const bool clearEvalQueue = _evalContGlobalParams->getAttributeValue<bool>("EVAL_QUEUE_CLEAR");


    if (inMainThread)
    {
        // Special case: Some points are still being evaluated. Wait for them.
        bool warningShown = false;
        // Note that when all points are evaluated, getSuccessType() has the correct
        // value, even if it was modified by those last points being evaluated.
        while (getMainThreadInfo(threadNum).getCurrentlyRunning() > 0)
        {
            OUTPUT_INFO_START
            if (!warningShown)
            {
                std::string s = "Waiting for " + NOMAD::itos(getMainThreadInfo(threadNum).getCurrentlyRunning());
                s += " evaluations to complete.";
                NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_INFO);
                warningShown = true;
            }
            OUTPUT_INFO_END
            usleep(10);

            // Update stopReason in case we found a success
            if (getOpportunisticEval(threadNum)
                && getSuccessType(threadNum) >= NOMAD::SuccessType::FULL_SUCCESS)
            {
                setStopReason(threadNum, NOMAD::EvalMainThreadStopType::OPPORTUNISTIC_SUCCESS);
            }

            // Update stopReason in case we reached the different max eval criterions
            reachedMaxEval();
            reachedMaxStepEval(threadNum);
        }

        // All points that were in evaluation are done.
        // Now clear queue.
        if (clearEvalQueue)
        {
            // Remove remaining points from queue, to start fresh next time.
            // Otherwise, we keep on evaluating the points remaining in the queue.
            size_t nbErased = clearQueue(threadNum, false /* showDebug */);
            OUTPUT_DEBUG_START
            NOMAD::OutputQueue::Add("Evaluation is done. Cleared queue of " + NOMAD::itos(nbErased) + " points.", NOMAD::OutputLevel::LEVEL_DEBUG);
            OUTPUT_DEBUG_END
        }

        OUTPUT_DEBUG_START
        std::string s = "EvaluatorControl stop reason (main thread): " + getStopReasonAsString(threadNum);
        s += " (global): " + NOMAD::AllStopReasons::getEvalGlobalStopReasonAsString();
        NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        OUTPUT_DEBUG_END
        NOMAD::OutputQueue::Flush();
    }


    return (inMainThread) ? getSuccessType(threadNum) : NOMAD::SuccessType::UNSUCCESSFUL;
}


void NOMAD::EvaluatorControl::stop()
{
    std::string s;
    const int mainThreadNum = NOMAD::getThreadNum();
    setDoneWithEval(mainThreadNum, true);
    OUTPUT_DEBUG_START
    s = "Stop evaluation queue for main thread " + std::to_string(mainThreadNum);
    NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
    OUTPUT_DEBUG_END

    // Go through all main thread info to see if _allDoneWithEval must be set.
    // If any main thread is not done, allDone is false.
    bool allDone = true;
    if (std::any_of(_mainThreads.begin(), _mainThreads.end(),
                    [this](int mainTh){ return !getDoneWithEval(mainTh); }))
    {
        allDone = false;
    }

    if (allDone)
    {
        OUTPUT_DEBUG_START
        s = "All main threads are done. Done with evaluation queue.";
        NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        OUTPUT_DEBUG_END
        _allDoneWithEval = true;
    }
}


void NOMAD::EvaluatorControl::restart()
{
    _allDoneWithEval = false;
    for (int mainThreadNum : _mainThreads)
    {
        setDoneWithEval(mainThreadNum, false);
    }
}


bool NOMAD::EvaluatorControl::stopMainEval(const int mainThreadNum, bool displayIfStop) const
{
    // Check for stop conditions for a main thread.
    // This function should not be called from other threads. But we
    // do not verify the thread number.

    // Inspect stopReasons
    bool doStopEvalMainThread = checkEvalTerminate(mainThreadNum);
    bool doStopEvalGlobal = NOMAD::AllStopReasons::checkEvalGlobalTerminate();

    // Inspect for opportunistic success set in a previous point
    doStopEvalMainThread = doStopEvalMainThread || getMainThreadInfo(mainThreadNum).testIf(NOMAD::EvalMainThreadStopType::OPPORTUNISTIC_SUCCESS) ||
        getMainThreadInfo(mainThreadNum).testIf(NOMAD::EvalMainThreadStopType::CUSTOM_OPPORTUNISTIC_ITER_STOP) ||
        getMainThreadInfo(mainThreadNum).testIf(NOMAD::EvalMainThreadStopType::CUSTOM_OPPORTUNISTIC_EVAL_STOP);

    // Update stopReason if there is no more point to evaluate for this thread
    if (   (0 == getQueueSize(mainThreadNum))
        && (!doStopEvalMainThread || getMainThreadInfo(mainThreadNum).testIf(NOMAD::EvalMainThreadStopType::EMPTY_LIST_OF_POINTS)))
    {
        getMainThreadInfo(mainThreadNum).setStopReason(NOMAD::EvalMainThreadStopType::ALL_POINTS_EVALUATED);
        doStopEvalMainThread = true;
    }

    // Update on max eval, counters by main thread;
    // Update on max eval, global counters valid for all threads
    doStopEvalMainThread = doStopEvalMainThread || reachedMaxStepEval(mainThreadNum);
    doStopEvalGlobal = doStopEvalGlobal || reachedMaxEval();
    
    

    // Inspect on base stop reason
    bool doStopBase = NOMAD::AllStopReasons::checkBaseTerminate();

    bool doStop = (doStopEvalMainThread || doStopEvalGlobal || doStopBase);
    if (doStop && displayIfStop)
    {
        OUTPUT_DEBUG_START
        std::string s = "stopMainEval returns true";
        if (doStopEvalMainThread)
        {
            s += " for thread " + NOMAD::itos(mainThreadNum) + ": ";
            s += getMainThreadInfo(mainThreadNum).getStopReasonAsString();
        }
        if (doStopEvalGlobal)
        {
            s += " " + NOMAD::AllStopReasons::getEvalGlobalStopReasonAsString();
        }
        if (doStopBase)
        {
            s += " " + NOMAD::AllStopReasons::getBaseStopReasonAsString();
        }
        NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        NOMAD::OutputQueue::Flush();
        OUTPUT_DEBUG_END
    }

    return doStop;
}


// Eval at most a total maxBbEval, maxEval, or maxBlockEval points in the queue.
// If this condition is true, we assume that it will remain true for the
// rest of the optimization.
// If the condition is true temporary, for example lap or model evals,
// use stopMainEval().
bool NOMAD::EvaluatorControl::reachedMaxEval() const
{
    // There is a possiblity that these values are modified
    // in a main thread, and that getAttributeValue() gets called before
    // checkAndComply(). In that case, catch the exception and ignore
    // the test.
    bool ret = false;

    if (   NOMAD::AllStopReasons::testIf(NOMAD::EvalGlobalStopType::MAX_BB_EVAL_REACHED)
        || NOMAD::AllStopReasons::testIf(NOMAD::EvalGlobalStopType::MAX_SURROGATE_EVAL_OPTIMIZATION_REACHED)
        || NOMAD::AllStopReasons::testIf(NOMAD::EvalGlobalStopType::MAX_EVAL_REACHED)
        || NOMAD::AllStopReasons::testIf(NOMAD::EvalGlobalStopType::MAX_BLOCK_EVAL_REACHED))
    {
        // Conditions already reached.
        return true;
    }

    std::string s = "Reached stop criterion: ";
    if (_maxBBEval->getValue() < NOMAD::INF_SIZE_T && _bbEval >= _maxBBEval->getValue())
    {
        // Reached maxBbEval.
        // Note that we have (total number of threads -1) other threads currently
        // running, so the total number of bb evaluations will be over
        // maxBbEval.
        NOMAD::AllStopReasons::set(NOMAD::EvalGlobalStopType::MAX_BB_EVAL_REACHED);
        s += NOMAD::AllStopReasons::getEvalGlobalStopReasonAsString() + " " + NOMAD::itos(_bbEval);
        ret = true;
    }
    else if (_maxSurrogateEval->getValue() < NOMAD::INF_SIZE_T && _surrogateEval >= _maxSurrogateEval->getValue())
    {
        // Reached maxSurrogateEval.
        NOMAD::AllStopReasons::set(NOMAD::EvalGlobalStopType::MAX_SURROGATE_EVAL_OPTIMIZATION_REACHED);
        s += NOMAD::AllStopReasons::getEvalGlobalStopReasonAsString() + " " + NOMAD::itos(_surrogateEval);
        ret = true;
    }
    else if (_maxEval->getValue() < NOMAD::INF_SIZE_T && getNbEval() >= _maxEval->getValue())
    {
        // Reached maxEval.
        NOMAD::AllStopReasons::set(NOMAD::EvalGlobalStopType::MAX_EVAL_REACHED);
        s += NOMAD::AllStopReasons::getEvalGlobalStopReasonAsString() + " " + NOMAD::itos(getNbEval());
        ret = true;
    }
    else if (_maxBlockEval->getValue() < NOMAD::INF_SIZE_T && _blockEval >= _maxBlockEval->getValue())
    {
        // Reached maxBlockEval.
        NOMAD::AllStopReasons::set(NOMAD::EvalGlobalStopType::MAX_BLOCK_EVAL_REACHED);
        s += NOMAD::AllStopReasons::getEvalGlobalStopReasonAsString() + " " + NOMAD::itos(_blockEval);
        ret = true;
    }

#ifdef _OPENMP
    #pragma omp single nowait
#endif // _OPENMP
    {
        if (ret)
        {
            NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_INFO);
        }
    }

    return ret;
}


// Have we reached max eval for a sub step: Number of laps, number of Model evals?
bool NOMAD::EvaluatorControl::reachedMaxStepEval(const int mainThreadNum) const
{
    bool ret = false;

    if (   getMainThreadInfo(mainThreadNum).testIf(NOMAD::EvalMainThreadStopType::MAX_MODEL_EVAL_REACHED)
        || getMainThreadInfo(mainThreadNum).testIf(NOMAD::EvalMainThreadStopType::LAP_MAX_BB_EVAL_REACHED)
        || getMainThreadInfo(mainThreadNum).testIf(NOMAD::EvalMainThreadStopType::SUBPROBLEM_MAX_BB_EVAL_REACHED))
    {
        // Conditions already reached.
        return true;
    }

    const size_t maxLapBbEval = getMainThreadInfo(mainThreadNum).getLapMaxBbEval();
    const size_t maxBbEvalInSub = getMaxBbEvalInSubproblem(mainThreadNum);
    std::string s = "Reached sub step stop criterion: ";
    if (_maxModelEval->getValue() < NOMAD::INF_SIZE_T && getModelEval(mainThreadNum) >= _maxModelEval->getValue())
    {
        // Reached maxModelEval, max number of eval in a Quad or Sgtelib model context.
        getMainThreadInfo(mainThreadNum).setStopReason(NOMAD::EvalMainThreadStopType::MAX_MODEL_EVAL_REACHED);
        s += getStopReasonAsString(mainThreadNum) + " " + NOMAD::itos(getModelEval(mainThreadNum));
        ret = true;
    }
    else if (maxLapBbEval < NOMAD::INF_SIZE_T && getLapBbEval(mainThreadNum) >= maxLapBbEval)
    {
        // Reached lapMaxEval.
        getMainThreadInfo(mainThreadNum).setStopReason(NOMAD::EvalMainThreadStopType::LAP_MAX_BB_EVAL_REACHED);
        s += getStopReasonAsString(mainThreadNum) + " " + NOMAD::itos(getLapBbEval(mainThreadNum));
        ret = true;
    }
    else if (maxBbEvalInSub < NOMAD::INF_SIZE_T && getBbEvalInSubproblem(mainThreadNum) >= maxBbEvalInSub)
    {
        // Reached max eval in supbroblem.
        getMainThreadInfo(mainThreadNum).setStopReason(NOMAD::EvalMainThreadStopType::SUBPROBLEM_MAX_BB_EVAL_REACHED);
        s += getStopReasonAsString(mainThreadNum) + " " + NOMAD::itos(getBbEvalInSubproblem(mainThreadNum));
        ret = true;
    }

#ifdef _OPENMP
    #pragma omp single nowait
#endif // _OPENMP
    {
        if (ret)
        {
            OUTPUT_DEBUG_START
            NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
            OUTPUT_DEBUG_END
        }
    }

    return ret;
}


void NOMAD::EvaluatorControl::debugDisplayQueue() const
{
#ifdef _OPENMP
    omp_set_lock(&_evalQueueLock);
    #pragma omp critical(displayQueue)
#endif // _OPENMP
    {
        std::cout << "Evaluation Queue" << (_evalPointQueue.empty() ? " is empty." : ":") << std::endl;
        for (auto eqp : _evalPointQueue)
        {
            std::cout << "    Main thread: " << eqp->getThreadAlgo() << " EvalType: " << eqp->getEvalType() << " " << eqp->displayAll() << std::endl;
        }
    }
#ifdef _OPENMP
    omp_unset_lock(&_evalQueueLock);
#endif // _OPENMP
}


void NOMAD::EvaluatorControl::displayDebugWaitingInfo(time_t &lastDisplayed) const
{
#ifdef _OPENMP
    // Check lastDisplayed value againt now, so this message
    // is not continuously displayed.
    time_t now;
    time(&now);
    if (difftime(now, lastDisplayed) >= 1)
    {
        OUTPUT_DEBUG_START
        std::string s = "Thread: " + NOMAD::itos(NOMAD::getThreadNum());
        s += " Waiting for points.";
        NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        OUTPUT_DEBUG_END
        lastDisplayed = now;
    }
#endif // _OPENMP
}


void NOMAD::EvaluatorControl::addDirectToFileInfo(NOMAD::EvalQueuePointPtr evalQueuePoint) const
{
    OUTPUT_DIRECTTOFILE_START

    // MODEL optimizations generate a lot of output. Do not write them into file.
    // Only show BB optimizations.
    if (NOMAD::EvalType::BB != evalQueuePoint->getEvalType())
    {
        return;
    }

    // In solution file we write only best feasible incumbent.
    bool writeInSolutionFile = (   evalQueuePoint->getSuccess() == SuccessType::FULL_SUCCESS
                                && evalQueuePoint->isFeasible(NOMAD::EvalType::BB, getComputeType(evalQueuePoint->getThreadAlgo())));
    
    // Evaluation info for output
    NOMAD::StatsInfo info;
    
    info.setBBO(evalQueuePoint->getBBO(NOMAD::EvalType::BB));
    info.setSol(*(evalQueuePoint->getX()));
    
    NOMAD::OutputDirectToFile::Write(info, writeInSolutionFile);
    
    
    // What follows is used only if multiple best feasible points have been obtained.
    // This is the case for Multiobjective pb.
    auto barrier = getBarrier();
    
    if (writeInSolutionFile && nullptr != barrier)
    {
        const std::vector<EvalPointPtr>& xFeas = barrier->getAllXFeas();
        if (xFeas.size() > 1)
        {
            // If we have a success and we have muliple best feasible solution, we rewrite the solution file.
            
            bool append = false;
            for (const EvalPointPtr & ev: xFeas)
            {
                NOMAD::StatsInfo info;
                
                info.setBBO(ev->getBBO(NOMAD::EvalType::BB));
                info.setSol(*(ev->getX()));
                
                NOMAD::OutputDirectToFile::Write(info, writeInSolutionFile, false /* do no write in history file */, append /* append in solution file */);
                append = true;
            }
        }
    }
        
    OUTPUT_DIRECTTOFILE_END
}


void NOMAD::EvaluatorControl::addStatsInfo(const NOMAD::BlockForEval& block)
{
    for (auto it = block.begin(); it < block.end(); it++)
    {
        NOMAD::EvalQueuePointPtr evalQueuePoint = (*it);
        const int mainThreadNum = evalQueuePoint->getThreadAlgo();
        auto evalType = evalQueuePoint->getEvalType();

        // MODEL optimizations generate a lot of stats output. Do not show them.
        // Only show BB optimizations.
        if (!evalTypeAsBB(evalType, mainThreadNum))
        {
            return;
        }

#ifdef _OPENMP
#pragma omp critical
#endif
        {
            _successStats.updateStats(evalQueuePoint->getSuccess(), evalQueuePoint->getGenStep());
        }

        OUTPUT_STATS_START

        // Evaluation info for output
        NOMAD::StatsInfoUPtr stats(new NOMAD::StatsInfo());
        size_t n = evalQueuePoint->size();

        // Values that are unknown at this point
        NOMAD::ArrayOfDouble unknownMeshIndex(n);
        const int threadNum = NOMAD::getThreadNum();

        // As of February 2019, values for bbEval (BBE) and nbEval (EVAL) make
        // enough sense in threaded environment to be printed out. If at some point
        // they do not, for example if too many iterations end up with the same
        // value, or if the user is dissatisfied with this output, we may update it
        // in a stricter way.
        stats->setFailEval(!evalQueuePoint->isEvalOk(evalType));
        stats->setObj(evalQueuePoint->getF(evalType, NOMAD::ComputeType::STANDARD));
        stats->setConsH(evalQueuePoint->getH(evalType, NOMAD::ComputeType::STANDARD));
        stats->setHMax(getHMax(mainThreadNum));
        stats->setBBE(_bbEval);
        stats->setFeasBBE(_feasBBEval);
        stats->setInfBBE(_infBBEval);
        stats->setLap(getLapBbEval(mainThreadNum));
        stats->setModelEval(getModelEval(mainThreadNum));
        stats->setTotalModelEval(_totalModelEval);
        stats->setSurrogateEval(_surrogateEval);
        stats->setBlkEva(_blockEval);
        stats->setBlkSize(block.size());
        stats->setBBO(evalQueuePoint->getBBO(evalType));
        stats->setEval(getNbEval());
        stats->setNbRelativeSuccess(_nbRelativeSuccess);
        stats->setPhaseOneSuccess(_nbPhaseOneSuccess);
        stats->setCacheHits(NOMAD::CacheBase::getNbCacheHits());
        stats->setCacheSize(NOMAD::CacheBase::getInstance()->size());
        stats->setIterNum(evalQueuePoint->getK());
        stats->setTime(NOMAD::Clock::getTimeSinceStart());
        if ( nullptr != evalQueuePoint->getMesh())
        {
            stats->setMeshIndex(evalQueuePoint->getMesh()->getMeshIndex());
            stats->setMeshSize(evalQueuePoint->getMesh()->getdeltaMeshSize());
            stats->setFrameSize(evalQueuePoint->getMesh()->getDeltaFrameSize());
        }
        auto frameCenter = evalQueuePoint->getPointFrom();
        stats->setFrameCenter(frameCenter ? *frameCenter : NOMAD::Point(evalQueuePoint->size()));
        auto direction = evalQueuePoint->getDirection();
        stats->setDirection(direction ? *direction : NOMAD::Direction(evalQueuePoint->size()));
        stats->setSol(*(evalQueuePoint->getX()));
        stats->setSuccessType(evalQueuePoint->getSuccess());
        stats->setThreadAlgo(mainThreadNum);
        stats->setThreadNum(threadNum);
        stats->setRelativeSuccess(evalQueuePoint->getRelativeSuccess());
        stats->setTag(evalQueuePoint->getTag());
        stats->setComment(evalQueuePoint->getComment());
        stats->setGenStep(NOMAD::StepTypeListToString(evalQueuePoint->getGenSteps()));

        std::string s = "Evaluated point: " + evalQueuePoint->displayAll();
        NOMAD::OutputInfo outputInfo("EvaluatorControl", s, NOMAD::OutputLevel::LEVEL_STATS);
        outputInfo.setStatsInfo(std::move(stats));
        NOMAD::OutputQueue::Add(std::move(outputInfo));

        OUTPUT_STATS_END
    }
}


// Eval a block (vector) of EvalQueuePointPtr
bool NOMAD::EvaluatorControl::evalBlock(NOMAD::BlockForEval& blockForEval)
{
    if (blockForEval.empty())
    {
        return false;
    }

    // All EvalPoints in blockForEval have the same mainThreadNum, and are to be evaluated
    // with the same evaluator, using the same EvalType.
    // Note: mainThreadNum may be different for different elements of the block,
    // but EvalType, Evaluator and hMax will be the same.
    const int mainThreadNum = blockForEval[0]->getThreadAlgo();
    const NOMAD::Evaluator* evaluator = getMainThreadInfo(mainThreadNum).getCurrentEvaluator();
    NOMAD::EvalType evalType = evaluator->getEvalType();
    const NOMAD::Double hMax = getHMax(mainThreadNum);

    // Create a block of EvalPoints (Block) from the given block of EvalQueuePoints (BlockForEval),
    // to give to the Evaluator.
    // The remaining points have evaluation info from previous run cache file.
    NOMAD::Block block;
    for (auto it = blockForEval.begin(); it < blockForEval.end(); it++)
    {
        bool inBlockBBEval = true;
        if (_useCacheFileForRerun->getValue() && getUseCache(mainThreadNum))
        {
            NOMAD::EvalPoint foundEvalPoint;
            NOMAD::CacheBase::getInstance()->findInCacheForRerun(*(*it)->getX(), foundEvalPoint);
            NOMAD::Eval *eval = foundEvalPoint.getEval(evalType);
            if (nullptr != eval)
            {
                // Set the eval point
                (*it)->setEval(*eval, evalType);
                (*it)->setEvalIsFromCacheFile(true);
                inBlockBBEval = false;
            }
        }
        if (inBlockBBEval)
        {
            block.push_back(*it);
        }
    }

    // Block evaluation.
    std::vector<bool> evalOk;

    if (block.size() ==0)
    {
        OUTPUT_DEBUG_START
        std::string s = "Block is empty. Evaluation results come from previous run cache file.";
        NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
        OUTPUT_DEBUG_END
    }
    else
    {
        evalOk = evalBlockOfPoints(block, *evaluator, hMax);
    }

    // User callback just after evaluation
    for (size_t i = 0; i < blockForEval.size(); i++)
    {
        NOMAD::EvalQueuePointPtr myPoint = blockForEval[i];
        runEvalCallback<NOMAD::CallbackType::EVAL_UPDATE>(myPoint);
    }

    size_t k = 0;
    size_t nbEvalOkFromCache = 0;
    for (size_t i = 0; i < blockForEval.size(); i++)
    {
        NOMAD::EvalPoint evalPoint;

        // Special treatment if evaluation results come from a cache file for rerun. Must do as if it is evaluated (stats are normally updated in evalBlockOfPoints).
        if (blockForEval[i]->getEvalIsFromCacheFile())
        {
            evalPoint = *blockForEval[i];

            bool evalOk = (evalPoint.getEvalStatus(evalType) == NOMAD::EvalStatusType::EVAL_OK);
            computeSuccess(blockForEval[i], evalOk, true);

            // Update regular cache set, that is not the cacheForRerun set
            if (!NOMAD::CacheBase::getInstance()->update(evalPoint, evalType))
            {
                std::string updateFailed = "Warning: EvaluatorControl::evalBlock: ";
                updateFailed += "Could not update to ";
                updateFailed += NOMAD::enumStr(evalPoint.getEvalStatus(evalType));
                updateFailed += " (" + NOMAD::evalTypeToString(evalType) + ")";
                throw NOMAD::Exception(__FILE__,__LINE__,updateFailed);
            }

            // NOTE: We cannot remove an eval point from the cache for rerun because
            // we may need both bb and surrogate eval types.
            // When points are updated in the regular cache they will not be be kept
            // for evaluation, we should never fetch them twice from the cache for rerun.

            if (NOMAD::EvalType::BB == evalType)
            {
                getMainThreadInfo(mainThreadNum).incBbEvalInSubproblem(1);
                getMainThreadInfo(mainThreadNum).incLapBbEval(1);
                _bbEval++;
                _bbEvalFromCacheForRerun++;
                if (evalOk)
                {
                    (blockForEval[i]->isFeasible(evalType, NOMAD::ComputeType::STANDARD)) ?  _feasBBEval++ : _infBBEval++;
                }
                else
                {
                    _bbEvalNotOk++;
                }
                // All bb evals count for _nbEvalSentToEvaluator (it was sent in a previous run!).
                _nbEvalSentToEvaluator++;
                blockForEval[i]->incNumberBBEval();
                NOMAD::OutputQueue::getInstance()->setTotalEval(_nbEvalSentToEvaluator);
            }
            else if (NOMAD::EvalType::SURROGATE == evalType)
            {
                // Note: bb eval in subproblem and lap bb eval are not updated, only the global counter.
                size_t surrogateCost = _evalContGlobalParams->getAttributeValue<size_t>("EVAL_SURROGATE_COST");
                _surrogateEval++;
                _surrogateEvalFromCacheForRerun++;
                    // When surrogateCost surrogate evaluations have been done, increment _bbEval and _nbEvalSentToEvaluator.
                    if (0 == _surrogateEval % surrogateCost)
                    {
                        _bbEvalFromCacheForRerun++;
                        _nbEvalSentToEvaluator++;
                    }
            }
            if (evalOk)
            {
                nbEvalOkFromCache ++;
            }
        }
        else
        {
            evalPoint = *block[k];

            // Update Eval in BlockForEval. Workaround.
            NOMAD::Eval* eval = evalPoint.getEval(evalType);
            if (nullptr != eval)
            {
                blockForEval[i]->setEval(*eval, evalType);
            }

            computeSuccess(blockForEval[i], evalOk[k], false);
            k++;
        }

        // Put points in evaluatedPoints list for threadAlgo,
        // so the user can get back evaluation info (especially if cache is not used)
        addEvaluatedPoint(evalPoint.getThreadAlgo(), evalPoint);

    }

    size_t nbEvalOk = nbEvalOkFromCache + std::count(evalOk.begin(), evalOk.end(), true);

    return (nbEvalOk > 0);
}


// Helper to update EvalQueuePointPtr
void NOMAD::EvaluatorControl::computeSuccess(NOMAD::EvalQueuePointPtr evalQueuePoint,
                                             const bool evalOk,
                                             bool pointFromCache)
{
    NOMAD::SuccessType success = NOMAD::SuccessType::UNSUCCESSFUL;
    bool newBestIncumbent = false;

    if (evalOk)
    {
        NOMAD::EvalType evalType = evalQueuePoint->getEvalType();
        auto mainThreadNum = evalQueuePoint->getThreadAlgo();
        NOMAD::ComputeType computeType = getComputeType(mainThreadNum);

        auto barrier = getBarrier(mainThreadNum);
        if (nullptr != barrier)
        {
            if (evalQueuePoint->isFeasible(evalType, computeType))
            {
                success = barrier->getSuccessTypeOfPoints(evalQueuePoint, nullptr, evalType, computeType);
            }
            else
            {
                success = barrier->getSuccessTypeOfPoints(nullptr, evalQueuePoint, evalType, computeType);
            }
            OUTPUT_DEBUG_START
            std::string s = NOMAD::evalTypeToString(evalType) + ((pointFromCache)? " (from cache)":"") + " evaluation done for ";
            s += evalQueuePoint->displayAll();
            s += ". Success found: " + NOMAD::enumStr(success);
            NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
            OUTPUT_DEBUG_END

        }
        else
        {
            NOMAD::ComputeSuccessType computeSucc(evalType, computeType);
            success = computeSucc(evalQueuePoint, nullptr, NOMAD::INF); // If barrier is not defined, hmax=INF. We have success if feasible or if h < INF.
        }


#ifdef _OPENMP
#pragma omp critical(updateBestIncumbent)
#endif
        {
            if (evalTypeAsBB(evalType, mainThreadNum) && success >= NOMAD::SuccessType::PARTIAL_SUCCESS)
            {
                auto bestIncumbent = getBestIncumbent(mainThreadNum);
                if (nullptr == bestIncumbent)
                {
                    newBestIncumbent = true;
                }
                else
                {
                    // Make the switch from infeasible best incumbent to feasible incumbent
                    if (evalQueuePoint->isFeasible(evalType,computeType) && !bestIncumbent->isFeasible(evalType,computeType))
                    {
                        newBestIncumbent = true;
                    }
                    else
                    {
                        // If both bestIncumbent and evalQueuePoint are feasible (or infeasible), the success can be >= PARTIAL_SUCCESS. Otherwise, the success is always UNSUCCESSFUL.
                        NOMAD::ComputeSuccessType computeSucc(evalType, computeType);
                        newBestIncumbent = (computeSucc(evalQueuePoint, bestIncumbent) >= NOMAD::SuccessType::PARTIAL_SUCCESS);
                    }
                }
                if (newBestIncumbent)
                {
                    setBestIncumbent(mainThreadNum, evalQueuePoint);
                }
            }
        }
    }

    evalQueuePoint->setSuccess(success);
    evalQueuePoint->setRelativeSuccess(newBestIncumbent);

}


bool NOMAD::EvaluatorControl::evalTypeAsBB(NOMAD::EvalType evalType, const int mainThreadNum) const
{
    return (NOMAD::EvalType::BB == evalType
            || (   NOMAD::EvalType::SURROGATE == evalType
                && getSurrogateOptimization(mainThreadNum)));
}


bool NOMAD::EvaluatorControl::evalTypeCounts(NOMAD::EvalType evalType) const
{
    return (NOMAD::EvalType::BB == evalType || NOMAD::EvalType::SURROGATE == evalType);
}


// Eval a single EvalPoint, calling EvcMainThreadInfo's Evaluator.
// Underlying EvalPoint must already be in the cache.
// Updates only the Eval members of the EvalPoint.
bool NOMAD::EvaluatorControl::evalSinglePoint(NOMAD::EvalPoint &evalPoint,
                                              const int mainThreadNum,
                                              const NOMAD::Double &hMax)
{
    bool evalOk = false;

    // Create a block of one point and evaluate it.
    NOMAD::Block block;
    std::shared_ptr<NOMAD::EvalPoint> epp = std::make_shared<NOMAD::EvalPoint>(evalPoint);
    block.push_back(epp);
    std::vector<bool> vectorEvalOk = evalBlockOfPoints(block, *getMainThreadInfo(mainThreadNum).getCurrentEvaluator(), hMax);
    size_t nbEvalOk = std::count(vectorEvalOk.begin(), vectorEvalOk.end(), true);
    evalOk = (nbEvalOk > 0);

    // Copy back values to evalPoint. Yes, that is ugly.
    // Disclaimer: We expect this method to be called only from Initialization Step.
    evalPoint = *epp;

    return evalOk;
}


// Eval a block of EvalPoints, calling Evaluator.
// Underlying EvalPoints must already be in the cache.
// Updates only the Eval members of the EvalPoints.
// Returns a vector of bools, of the same size as block.
std::vector<bool> NOMAD::EvaluatorControl::evalBlockOfPoints(
                                    NOMAD::Block &block,
                                    const NOMAD::Evaluator& evaluator,
                                    const NOMAD::Double &hMax)
{
    auto evalType = evaluator.getEvalType();

    std::vector<bool> evalOk(block.size(), false);
    std::vector<bool> countEval(block.size(), false);

    for (auto it = block.begin(); it != block.end(); it++)
    {
        if (!(*it)->ArrayOfDouble::isComplete())
        {
            std::string err("EvaluatorControl: Eval Single Point: Trying to evaluate an undefined Point ");
            err += (*it)->getX()->NOMAD::Point::display();
            throw NOMAD::Exception(__FILE__, __LINE__, err);
        }
    }

    // NOTE: Put everything in a single OutputInfo structure and then
    // add it to the OutputQueue all at once when the thread part is done.
    std::string s_thread_info;
    OUTPUT_DEBUG_START
    s_thread_info = "Thread = " + NOMAD::itos(NOMAD::getThreadNum());
    OUTPUT_DEBUG_END
    // Use "EvaluatorControl" as originator.
    NOMAD::OutputInfo evalInfo("EvaluatorControl", s_thread_info, NOMAD::OutputLevel::LEVEL_DEBUG);

    // Evaluation of the block
    try
    {
        for (size_t i = 0; i < block.size(); i++)
        {
            std::shared_ptr<NOMAD::EvalPoint> evalPoint = block[i];
            if (!updateEvalStatusBeforeEval(*evalPoint))
            {
                // evalPoint's evaluation is already in progress from another main thread.
                // Set eval status to wait.
                evalPoint->setEvalStatus(NOMAD::EvalStatusType::EVAL_WAIT, evalType);
                OUTPUT_DEBUG_START
                std::string sWait = "Set eval status to wait: " + evalPoint->displayAll();
                evalInfo.addMsg(sWait);
                OUTPUT_DEBUG_END
            }
        }
        OUTPUT_INFO_START
        std::string startMsg = "Start evaluation of block of " + NOMAD::itos(block.size()) + " points.";
        evalInfo.addMsg(startMsg);
        OUTPUT_INFO_END
#ifdef TIME_STATS
        double evalStartTime = NOMAD::Clock::getCPUTime();
#endif // TIME_STATS
        evalOk = evaluator.eval_block(block, hMax, countEval);
#ifdef TIME_STATS
#ifdef _OPENMP
        #pragma omp critical(computeEvalTime)
#endif // _OPENMP
        {
            _evalTime += NOMAD::Clock::getCPUTime() - evalStartTime;
        }
#endif // TIME_STATS
    }
    catch (std::exception &e)
    {
        std::string err("EvaluatorControl: Eval Block of Points: eval_x returned an exception: ");
        err += e.what();
        err += " \n Blackbox evaluation maybe the cause of this exception. Raw bb outputs obtained: \"";
        for ( const auto & ep : block)
        {
            err += ep->getBBO(evalType);
            err += "\"\n";
        }
        throw NOMAD::Exception(__FILE__, __LINE__, err);
    }


    for (size_t index = 0; index < block.size(); index++)
    {
        NOMAD::EvalPointPtr evalPoint = block[index];
        NOMAD::EvalQueuePointPtr evalQueuePoint = std::dynamic_pointer_cast<NOMAD::EvalQueuePoint>(evalPoint);
        const int mainThreadNum = evalPoint->getThreadAlgo();

        NOMAD::Eval * eval = evalPoint->getEval(evalType);
        // TEMP. Just to be sure
        if (nullptr == eval)
        {
            throw NOMAD::Exception(__FILE__, __LINE__,"Not supposed to be here. For debug.");
        }

        auto bbOutputTypeList = evaluator.getBBOutputTypeList();
        // Adjust bbOutputType if needed
        if (   evalOk[index]
            && nullptr != eval
            && eval->getBBOutputTypeList().empty())
        {
            if (!bbOutputTypeList.empty())
            {
                eval->setBBOutputTypeList(bbOutputTypeList);
                eval->updateForRevealedConstraints();
            }
        }

        // Start by setting EVAL_OK if evalOk is true. The eval status might be modified later.
        if (evalOk[index])
        {
            evalPoint->setEvalStatus(NOMAD::EvalStatusType::EVAL_OK, evalType);
        }

        // Set EvalOk to false if f or h is not defined, check if eval part exists
        if (evalOk[index])
        {
            evalOk[index] = checkIfEvalOk(evaluator, evalPoint, &evalInfo);
        }

        // Update all counters
        // Note: _bbEval, and EvcMainThreadInfo members _lapBbEval, _modelEval, _subBbEval, are atomic.
        if (NOMAD::EvalType::MODEL == evalType)
        {
            getMainThreadInfo(mainThreadNum).incModelEval(1);
            _totalModelEval++;
        }
        else if (NOMAD::EvalType::BB == evalType)
        {
            getMainThreadInfo(mainThreadNum).incBbEvalInSubproblem(1);
            getMainThreadInfo(mainThreadNum).incLapBbEval(1);
            _bbEval += (countEval[index]);
            if (evalOk[index])
            {
                (evalPoint->isFeasible(evalType, NOMAD::ComputeType::STANDARD)) ?  _feasBBEval++ : _infBBEval++;
            }
            else
            {
                _bbEvalNotOk++;
            }
            // All bb evals count for _nbEvalSentToEvaluator.
            _nbEvalSentToEvaluator++;
            evalPoint->incNumberBBEval();
            NOMAD::OutputQueue::getInstance()->setTotalEval(_nbEvalSentToEvaluator);
        }
        else if (NOMAD::EvalType::SURROGATE == evalType)
        {
            // Note: bb eval in subproblem and lap bb eval are not updated, only the global counter.
            size_t surrogateCost = _evalContGlobalParams->getAttributeValue<size_t>("EVAL_SURROGATE_COST");
            if (countEval[index])
            {
                _surrogateEval++;
                // When surrogateCost surrogate evaluations have been done, increment _bbEval and _nbEvalSentToEvaluator.
                if (0 == _surrogateEval % surrogateCost)
                {
                    _bbEval++;
                    _nbEvalSentToEvaluator++;
                }
            }
        }

        // Update eval status if needed.
        updateEvalStatusAfterEval(*evalPoint, evalOk[index]);

        
        // User callback for fail evaluation (only for BB).
        if (!evalOk[index] && NOMAD::EvalType::BB == evalType)
        {
            if(NOMAD::EvaluatorControl::_cbFailEvalCheckIsDefault!=true)
            {
                runEvalCallback<NOMAD::CallbackType::EVAL_FAIL_CHECK>(evalQueuePoint);

                // Change the status to show that the point has been checked by user.
                // After setting BBO the evalOK can be true.
                evalPoint->setUserFailEvalCheck(true);

                std::string err = "Warning: Point " + evalPoint->display() + " has been checked and maybe updated because it was not eval ok.";
                NOMAD::OutputQueue::Add(err, NOMAD::OutputLevel::LEVEL_WARNING);

                // If eval status has been changed to evalOk in callback, check validity
                if(evalPoint->getEvalStatus(NOMAD::EvalType::BB)==NOMAD::EvalStatusType::EVAL_OK)
                {
                    NOMAD::OutputInfo* evalInfoPtr = &evalInfo;
                    evalOk[index] = checkIfEvalOk(evaluator, evalPoint, evalInfoPtr);

                }

            }
        }
        
        // User callback for global stop
        if (NOMAD::EvalType::BB == evalType)
        {
            bool globalStop=false;
            runEvalCallback<NOMAD::CallbackType::EVAL_STOP_CHECK>(evalQueuePoint, globalStop);
            
            if(globalStop)
            {
                NOMAD::AllStopReasons::set(NOMAD::EvalGlobalStopType::CUSTOM_GLOBAL_STOP);
                OUTPUT_DEBUG_START
                std::string s = "Custom global stop requested.";
                NOMAD::OutputQueue::Add(s, NOMAD::OutputLevel::LEVEL_DEBUG);
                OUTPUT_DEBUG_END
            }
            
        }

        // Update cache
        if (getUseCache(mainThreadNum))
        {
            if (!NOMAD::CacheBase::getInstance()->update(*evalPoint, evalType))
            {
                std::string updateFailed = "Warning: EvaluatorControl::evalBlockOfPoints: ";
                updateFailed += "Could not update to ";
                updateFailed += NOMAD::enumStr(evalPoint->getEvalStatus(evalType));
                updateFailed += " (" + NOMAD::evalTypeToString(evalType) + ")";
                OUTPUT_INFO_START
                evalInfo.addMsg(updateFailed);
                OUTPUT_INFO_END
                throw NOMAD::Exception(__FILE__,__LINE__,updateFailed);
            }
        }
    }

    if (evalTypeCounts(evalType))
    {
        // One more block evaluated (count only blocks of BB or SURROGATE evaluations).
        _blockEval++;
    }

    OUTPUT_INFO_START
    NOMAD::OutputQueue::Add(std::move(evalInfo));
    OUTPUT_INFO_END

    return evalOk;
}

bool NOMAD::EvaluatorControl::checkIfEvalOk(const NOMAD::Evaluator& evaluator, NOMAD::EvalPointPtr evalPoint,NOMAD::OutputInfo * evalInfo)
{
        bool isEvalOk = true;

        // Set EvalOk to false if f or h is not defined
        const int mainThreadNum = evalPoint->getThreadAlgo();
        NOMAD::ComputeType computeType = getComputeType(mainThreadNum);
        NOMAD::EvalType evalType = getCurrentEvalType(mainThreadNum);
        NOMAD::Eval * eval = evalPoint->getEval(evalType);
        auto bbOutputTypeList = evaluator.getBBOutputTypeList();

        if ((nullptr != eval)
            && (   !eval->getBBOutput().checkSizeMatch(bbOutputTypeList)
                || !evalPoint->getF(evalType, computeType).isDefined()
                || !evalPoint->getH(evalType, computeType).isDefined()))
        {
            std::string modifMsg = "Warning: EvaluatorControl: Point ";
            auto evalFormat = evaluator.getBBEvalFormat();
            modifMsg += evalPoint->display(evalFormat) + ": Eval ok but ";
            if (!eval->getBBOutput().checkSizeMatch(bbOutputTypeList))
            {
                modifMsg += "output \"" + eval->getBBO() + "\" does not match ";
                modifMsg += "parameter BB_OUTPUT_TYPE: \"";
                modifMsg += NOMAD::BBOutputTypeListToString(bbOutputTypeList) + "\"";
            }
            else
            {
                modifMsg += (!evalPoint->getF(evalType, computeType).isDefined()) ? "f not defined" : "h not defined";
            }
            modifMsg += ". Setting eval status to EVAL_FAILED.";
            OUTPUT_INFO_START
            evalInfo->addMsg(modifMsg);
            OUTPUT_INFO_END

            isEvalOk = false;
            evalPoint->setEvalStatus(NOMAD::EvalStatusType::EVAL_FAILED, evalType);
        }

        if (isEvalOk && nullptr == eval)
        {
            // Error: evalOk is true, but Eval is NULL.
            std::string err = "EvaluatorControl: Eval Single Point: no Eval on EvalPoint that was just evaluated. " + evalPoint->display();
            throw NOMAD::Exception(__FILE__, __LINE__, err);
        }

        return isEvalOk;

}

bool NOMAD::EvaluatorControl::updateEvalStatusBeforeEval(NOMAD::EvalPoint &evalPoint)
{
    bool goodForEval = true;
    std::string err;
    // Find the EvalPoint in the cache and set its eval status to IN_PROGRESS.
    NOMAD::EvalPoint foundEvalPoint;
    const int mainThreadNum = evalPoint.getThreadAlgo();
    if (getUseCache(mainThreadNum))
    {
        if (!NOMAD::CacheBase::getInstance()->find(evalPoint, foundEvalPoint))
        {
            err = "NOMAD::EvaluatorControl: updateEvalStatusBeforeEval: EvalPoint not found: ";
            err += evalPoint.display();
            throw NOMAD::Exception(__FILE__, __LINE__, err);
        }
    }
    else
    {
        foundEvalPoint = evalPoint;
    }

    NOMAD::EvalType evalType = getCurrentEvalType(mainThreadNum);
    NOMAD::EvalStatusType evalStatus = foundEvalPoint.getEvalStatus(evalType);
    if (evalStatus == NOMAD::EvalStatusType::EVAL_FAILED
        || evalStatus == NOMAD::EvalStatusType::EVAL_ERROR
        || evalStatus == NOMAD::EvalStatusType::EVAL_USER_REJECTED
        || evalStatus == NOMAD::EvalStatusType::EVAL_OK)
    {
        if (evalTypeAsBB(evalType, mainThreadNum))
        {
            err = "Warning: Point " + foundEvalPoint.display() + " will be re-evaluated.";
            NOMAD::OutputQueue::Add(err, NOMAD::OutputLevel::LEVEL_WARNING);
        }
    }
    else if (evalStatus == NOMAD::EvalStatusType::EVAL_IN_PROGRESS)
    {
        // May happen in context of multiple main threads.
        err = "Warning: NOMAD::EvaluatorControl: updateEvalStatusBeforeEval: ";
        err += "Evaluation of EvalPoint ";
        err += evalPoint.getX()->NOMAD::Point::display();
        err += " is already in progress";
        std::cout << err << std::endl;
        goodForEval = false;
    }
    else if (evalStatus == NOMAD::EvalStatusType::EVAL_WAIT)
    {
        // No point in cache should be in status EVAL_WAIT.
        // If a point is waiting, it means there is an identical point in cache with the
        // status IN_PROGRESS.
        err = "Error: ";
        err += "NOMAD::EvaluatorControl: updateEvalStatusBeforeEval: ";
        err += "Evaluation of EvalPoint ";
        err += evalPoint.getX()->NOMAD::Point::display();
        err += " has status " + NOMAD::enumStr(evalStatus);
        std::cout << err << std::endl;
        goodForEval = false;
    }
    else if (evalStatus == NOMAD::EvalStatusType::EVAL_NOT_STARTED
             || evalStatus == NOMAD::EvalStatusType::EVAL_STATUS_UNDEFINED)
    {
        // All good
    }
    else
    {
        // Failsafe
        err = "Unknown eval status: " + NOMAD::enumStr(evalStatus);
        throw NOMAD::Exception(__FILE__, __LINE__, err);
    }

    // IMPORTANT to set the eval status to EVAL_IN_PROGRESS.
    // When the simple setBBO(bbo) (see library mode examples) is called, the one Eval[evalType] which has NOMAD::EvalStatusType::EVAL_IN_PROGRESS is set.
    evalPoint.setEvalStatus(NOMAD::EvalStatusType::EVAL_IN_PROGRESS, evalType);
    if (getUseCache(mainThreadNum))
    {
        if (!NOMAD::CacheBase::getInstance()->update(evalPoint, evalType))
        {
            std::string updateFailed = "Warning: EvaluatorControl::updateEvalStatusBeforeEval: ";
            updateFailed += "Could not update to IN_PROGRESS";
            updateFailed += " (" + NOMAD::evalTypeToString(evalType) + ")";
            throw NOMAD::Exception(__FILE__,__LINE__,updateFailed);
        }
    }

    return goodForEval;
}


void NOMAD::EvaluatorControl::updateEvalStatusAfterEval(NOMAD::EvalPoint &evalPoint,
                                                        bool evalOk)
{
    const int mainThreadNum = evalPoint.getThreadAlgo();
    NOMAD::EvalType evalType = getCurrentEvalType(mainThreadNum);
    NOMAD::EvalStatusType evalStatus = evalPoint.getEvalStatus(evalType);
    if (evalStatus == NOMAD::EvalStatusType::EVAL_FAILED
        || evalStatus == NOMAD::EvalStatusType::EVAL_ERROR
        || evalStatus == NOMAD::EvalStatusType::EVAL_USER_REJECTED
        || evalStatus == NOMAD::EvalStatusType::EVAL_OK)
    {
        // Nothing to do
    }
    else if (evalStatus == NOMAD::EvalStatusType::EVAL_IN_PROGRESS)
    {
        // Evaluator did not update evalStatus. Lean on evalOk.
        evalPoint.setEvalStatus(evalOk ? NOMAD::EvalStatusType::EVAL_OK : NOMAD::EvalStatusType::EVAL_FAILED, evalType);
    }
    else if (evalStatus == NOMAD::EvalStatusType::EVAL_WAIT)
    {
        // Wait for evaluation to be done.
        // Note: if EVAL_USE_CACHE is false, we should not be waiting for evaluation.
        // Re-evaluation is permitted.
        NOMAD::EvalPoint foundEvalPoint;
        NOMAD::EvalStatusType foundEvalStatus = NOMAD::EvalStatusType::EVAL_NOT_STARTED;
        NOMAD::CacheBase::getInstance()->find(evalPoint, foundEvalPoint, evalType);

        // Eval status for waiting point is the same as for the point that was actually evaluated.
        evalPoint.setEvalStatus(foundEvalStatus, evalType);
    }
    else if (evalStatus == NOMAD::EvalStatusType::EVAL_NOT_STARTED
             || evalStatus == NOMAD::EvalStatusType::EVAL_STATUS_UNDEFINED)
    {
        std::string err = "Eval status after evaluation is: " + NOMAD::enumStr(evalStatus);
        err += ". Cannot be handled.";
        throw NOMAD::Exception(__FILE__, __LINE__, err);
    }
    else
    {
        std::string err = "Unknown eval status: " + NOMAD::enumStr(evalStatus);
        throw NOMAD::Exception(__FILE__, __LINE__, err);
    }
}
