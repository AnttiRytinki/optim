#include "nomad_adapter.h"
#include "Nomad/nomad.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

using Clock = std::chrono::steady_clock;

struct NomadRun
{
    std::vector<NomadEvaluation> history;
    size_t reported = 0;
    double elapsedMs = 0.0;
};

namespace
{
double Milliseconds(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// Declared before MainStep so reset happens after all run objects are destroyed,
// including on an exception. Also resets before each new independent run.
struct ResetGuard
{
    ResetGuard()
    {
        NOMAD::MainStep::resetComponentsBetweenOptimization();
    }
    ~ResetGuard()
    {
        NOMAD::MainStep::resetComponentsBetweenOptimization();
    }
};

class Evaluator final : public NOMAD::Evaluator
{
public:
    Evaluator(const std::shared_ptr<NOMAD::EvalParameters> &params,
              const TestProblem &problem, NomadRun &run, Clock::time_point start)
        : NOMAD::Evaluator(params, NOMAD::EvalType::BB),
          problem_(problem), run_(run), start_(start)
    {
    }

    bool eval_x(NOMAD::EvalPoint &point, const NOMAD::Double &,
                bool &countEval) const override
    {
        countEval = false;
        NomadEvaluation entry{};
        for (int d = 0; d < problem_.dim; ++d)
        {
            entry.point[d] = point[d].todouble();
            if (!std::isfinite(entry.point[d]) ||
                entry.point[d] < problem_.lower || entry.point[d] > problem_.upper)
            {
                throw std::runtime_error("Out-of-bounds NOMAD evaluation");
            }
        }
        entry.value = problem_.function(entry.point, problem_.dim);
        if (!std::isfinite(entry.value))
        {
            throw std::runtime_error("Nonfinite objective value");
        }
        if (run_.history.empty() || entry.value < run_.history.back().bestValue)
        {
            entry.bestValue = entry.value;
            std::copy_n(entry.point, MAX_DIM, entry.bestPoint);
        }
        else
        {
            entry.bestValue = run_.history.back().bestValue;
            std::copy_n(run_.history.back().bestPoint, MAX_DIM, entry.bestPoint);
        }
        entry.elapsedMs = Milliseconds(start_);
        std::ostringstream output;
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << entry.value;
        point.setBBO(output.str());
        run_.history.push_back(entry);
        countEval = true;
        return true;
    }

private:
    const TestProblem &problem_;
    NomadRun &run_;
    Clock::time_point start_;
};
}

extern "C" NomadRun *NomadOptimize(const TestProblem *problem,
                                  const double *start, int seed, int budget,
                                  char *error, size_t errorSize)
{
    if (error && errorSize)
    {
        error[0] = '\0';
    }
    try
    {
        const auto begin = Clock::now();
        if (!problem || !start || !problem->function || problem->dim < 1 ||
            problem->dim > MAX_DIM || budget < 1 || seed < 0 ||
            !std::isfinite(problem->lower) || !std::isfinite(problem->upper) ||
            problem->lower >= problem->upper)
        {
            throw std::invalid_argument("Invalid NOMAD run configuration");
        }
        NOMAD::Point x0(static_cast<size_t>(problem->dim));
        for (int d = 0; d < problem->dim; ++d)
        {
            if (!std::isfinite(start[d]) || start[d] < problem->lower ||
                start[d] > problem->upper)
            {
                throw std::invalid_argument("Invalid NOMAD starting point");
            }
            x0[d] = start[d];
        }
        ResetGuard reset;
        auto result = std::make_unique<NomadRun>();
        auto params = std::make_shared<NOMAD::AllParameters>();
        const auto dim = static_cast<size_t>(problem->dim);
        params->setAttributeValue("DIMENSION", dim);
        params->setAttributeValue("X0", x0);
        params->setAttributeValue("LOWER_BOUND", NOMAD::ArrayOfDouble(dim, problem->lower));
        params->setAttributeValue("UPPER_BOUND", NOMAD::ArrayOfDouble(dim, problem->upper));
        params->setAttributeValue("BB_OUTPUT_TYPE",
                                  NOMAD::BBOutputTypeList{NOMAD::BBOutputType::OBJ});
        params->setAttributeValue("MAX_BB_EVAL", budget);
        params->setAttributeValue("SEED", seed);
        params->setAttributeValue("DISPLAY_DEGREE", 0);
        params->setAttributeValue("NB_THREADS_PARALLEL_EVAL", 1);
        params->checkAndComply();
        NOMAD::MainStep main;
        main.setAllParameters(params);
        main.addEvaluator(std::make_unique<Evaluator>(params->getEvalParams(),
                                                      *problem, *result, begin));
        main.start();
        main.run();
        main.end();
        result->reported = NOMAD::EvcInterface::getEvaluatorControl()->getBbEval();
        result->elapsedMs = Milliseconds(begin);
        if (result->history.empty() || result->reported != result->history.size() ||
            result->reported > static_cast<size_t>(budget))
        {
            throw std::runtime_error("NOMAD black-box evaluation count mismatch");
        }
        return result.release();
    }
    catch (const std::exception &exception)
    {
        if (error && errorSize)
        {
            std::snprintf(error, errorSize, "%s", exception.what());
        }
        return nullptr;
    }
}

extern "C" void NomadDestroy(NomadRun *run)
{
    delete run;
}
extern "C" size_t NomadEvaluationCount(const NomadRun *run)
{
    return run ? run->history.size() : 0;
}
extern "C" size_t NomadReportedCount(const NomadRun *run)
{
    return run ? run->reported : 0;
}
extern "C" double NomadElapsedMs(const NomadRun *run)
{
    return run ? run->elapsedMs : 0.0;
}
extern "C" const NomadEvaluation *NomadEvaluationAt(const NomadRun *run, size_t index)
{
    return run && index < run->history.size() ? &run->history[index] : nullptr;
}
extern "C" const NomadEvaluation *NomadAtBudget(const NomadRun *run, size_t budget)
{
    return run && budget && !run->history.empty()
        ? &run->history[std::min(budget, run->history.size()) - 1] : nullptr;
}
extern "C" const NomadEvaluation *NomadAtTime(const NomadRun *run, double elapsedMs)
{
    if (!run || !std::isfinite(elapsedMs))
    {
        return nullptr;
    }
    const auto it = std::upper_bound(run->history.begin(), run->history.end(), elapsedMs,
        [](double time, const NomadEvaluation &entry)
        {
            return time < entry.elapsedMs;
        });
    return it == run->history.begin() ? nullptr : &*(it - 1);
}
