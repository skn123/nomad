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
/*--------------------------------------------------------------------------*/
/*  Example of a program that uses a custom comparison (2 elements) for     */
/*  ordering trial points before evaluation.                                */
/*  The rank of points is established in the completeTrialPointsInformation */
/*  and comp functions.                                                     */
/*--------------------------------------------------------------------------*/
#include "Nomad/nomad.hpp"
#include "Algos/EvcInterface.hpp"
#include "Algos/Mads/MadsMegaIteration.hpp"
#include "Cache/CacheBase.hpp"

const size_t n = 5;

/*----------------------------------------*/
/*               The problem              */
/*----------------------------------------*/
class My_Evaluator : public NOMAD::Evaluator
{
private:

public:
    explicit My_Evaluator(const std::shared_ptr<NOMAD::EvalParameters>& evalParams)
    : NOMAD::Evaluator(evalParams, NOMAD::EvalType::BB)
    {}

    ~My_Evaluator() override = default;

    bool eval_x(NOMAD::EvalPoint &x, const NOMAD::Double &hMax, bool &countEval) const override;
};


/*----------------------------------------*/
/*           user-defined eval_x          */
/*----------------------------------------*/
bool My_Evaluator::eval_x(NOMAD::EvalPoint &x,
                          const NOMAD::Double &hMax,
                          bool &countEval) const
{
    NOMAD::Double c1 = 0.0 , c2 = 0.0;
    for ( int i = 0 ; i < n ; i++ )
    {

        c1 += (x[i]-1).pow2();
        c2 += (x[i]+1).pow2();
    }
    NOMAD::Double constr1 = c1-25;
    NOMAD::Double constr2 = 25-c2;
    std::string bbo = x[4].tostring();
    bbo += " " + constr1.tostring();
    bbo += " " + constr2.tostring();
    x.setBBO(bbo);

    countEval = true; // count a black-box evaluation

    return true;       // the evaluation succeeded
}

void initAllParams(const std::shared_ptr<NOMAD::AllParameters>& allParams)
{
    // Parameters creation
    allParams->setAttributeValue("DIMENSION", n);

    // Starting point
    allParams->setAttributeValue("X0", NOMAD::Point(n, 0.0) );

    // Bounds
    allParams->setAttributeValue("LOWER_BOUND", NOMAD::ArrayOfDouble(n, -6.0 )); // all var. >= -6
    NOMAD::ArrayOfDouble ub(n);
    ub[0] = 5.0;    // x_1 <= 5
    ub[1] = 6.0;    // x_2 <= 6
    ub[2] = 7.0;    // x_3 <= 7
    allParams->setAttributeValue("UPPER_BOUND", ub);

    // Constraints and objective
    NOMAD::BBOutputTypeList bbOutputTypes;
    bbOutputTypes.emplace_back(NOMAD::BBOutputType::Type::OBJ);
    bbOutputTypes.emplace_back(NOMAD::BBOutputType::Type::PB);
    bbOutputTypes.emplace_back(NOMAD::BBOutputType::Type::PB);
    allParams->setAttributeValue("BB_OUTPUT_TYPE", bbOutputTypes );

    allParams->setAttributeValue("MAX_BB_EVAL", 200);

    allParams->setAttributeValue("QUAD_MODEL_SEARCH", false);

    allParams->setAttributeValue("DISPLAY_DEGREE", 2);
    allParams->setAttributeValue("DISPLAY_STATS", NOMAD::ArrayOfString("bbe ( sol ) obj"));
    allParams->setAttributeValue("DISPLAY_ALL_EVAL", true);

    // Parameters validation requested to have access to their value.
    allParams->checkAndComply();

}


// Fantasy comparison
// The closer to 0, the lower the priority.
class CustomOrder : public NOMAD::ComparePriorityMethod
{
    // The evaluation point tags are unique. Let's use
    // them as map key. The value for ordering is the
    // second element of the map, the distance to P0.
    std::map<size_t,NOMAD::Double> _valByTags;

public:
    bool comp(NOMAD::EvalQueuePointPtr& p1, NOMAD::EvalQueuePointPtr& p2) const override
    {
        bool lowerPriority = false;

        auto itP1 = _valByTags.find(p1->getTag());
        auto itP2 = _valByTags.find(p2->getTag());
        if (itP1 != _valByTags.end() && itP2 != _valByTags.end())
        {
            lowerPriority = (itP1->second < itP2->second);
        }

        return lowerPriority;

    }


    void completeTrialPointsInformation(const NOMAD::Step *step, NOMAD::EvalPointSet & trialPoints) override
    {
        NOMAD::Point P0(n, 0);

        _valByTags.clear();

        // We could std::copy_if but it is not worth it. Clarity is better.
        for ( const auto & evalQueuePoint : trialPoints)
        {
            // Let use a reference point to compute a distance.
            // The distance is used in comp function to order the points.
            auto tag = evalQueuePoint.getTag();
            NOMAD::Double d1 = NOMAD::Point::dist(P0, evalQueuePoint);
            _valByTags.insert(std::pair<size_t,NOMAD::Double>(tag,d1));
        }
    }
};


/*------------------------------------------*/
/*            NOMAD main function           */
/*------------------------------------------*/
int main()
{
    NOMAD::MainStep TheMainStep;

    // Set parameters
    auto params = std::make_shared<NOMAD::AllParameters>();
    initAllParams(params);
    TheMainStep.setAllParameters(params);

    // Custom Evaluator creation
    auto ev = std::make_unique<My_Evaluator>(params->getEvalParams());
    TheMainStep.addEvaluator(std::move(ev));

    // The run
    TheMainStep.start();

    // Define new sort function and sort according to that function
    // Must be done after main step start.
    auto customOrder = std::make_shared<CustomOrder>();
    NOMAD::EvcInterface::getEvaluatorControl()->setUserCompMethod(customOrder);

    TheMainStep.run();
    TheMainStep.end();

    return 0;
}
