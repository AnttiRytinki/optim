#include "Nomad/nomad.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
constexpr size_t dimension = 2;
constexpr int maxEvaluations = 1000;
constexpr double bound = 5.12;
constexpr double target = 1e-12;

struct Evaluation {
  double x, y, value, best;
  bool operator==(const Evaluation &other) const {
    return x == other.x && y == other.y && value == other.value &&
           best == other.best;
  }
};

class SphereEvaluator final : public NOMAD::Evaluator {
public:
  SphereEvaluator(const std::shared_ptr<NOMAD::EvalParameters> &params,
                  std::vector<Evaluation> &history, int run)
      : NOMAD::Evaluator(params, NOMAD::EvalType::BB), history_(history),
        run_(run) {}

  bool eval_x(NOMAD::EvalPoint &point, const NOMAD::Double &,
              bool &countEval) const override {
    countEval = false;
    const double x = point[0].todouble();
    const double y = point[1].todouble();
    if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > bound ||
        std::abs(y) > bound)
      throw std::runtime_error("NOMAD evaluated a point outside the bounds");

    // Only this callback computes the black-box objective. Cache hits and
    // internal model evaluations therefore do not add entries to the history.
    const double value = x * x + y * y;
    const double best = history_.empty() ? value
                                        : std::min(history_.back().best, value);
    std::ostringstream output;
    output << std::setprecision(std::numeric_limits<double>::max_digits10)
           << value;
    point.setBBO(output.str());
    countEval = true;
    history_.push_back({x, y, value, best});
    std::cout << "Run " << run_ << " | Evaluation " << history_.size()
              << " | x " << x << " | y " << y << " | Objective " << value
              << " | Best " << best << '\n';
    return true;
  }

private:
  std::vector<Evaluation> &history_;
  int run_;
};

std::vector<Evaluation> optimize(int run, int seed, double x, double y) {
  // Call only after the preceding MainStep and its evaluator are destroyed.
  NOMAD::MainStep::resetComponentsBetweenOptimization();
  std::vector<Evaluation> history;
  auto params = std::make_shared<NOMAD::AllParameters>();
  params->setAttributeValue("DIMENSION", dimension);
  params->setAttributeValue("MAX_BB_EVAL", maxEvaluations);
  NOMAD::Point start(dimension);
  start[0] = x;
  start[1] = y;
  params->setAttributeValue("X0", start);
  params->setAttributeValue("LOWER_BOUND", NOMAD::ArrayOfDouble(dimension, -bound));
  params->setAttributeValue("UPPER_BOUND", NOMAD::ArrayOfDouble(dimension, bound));
  params->setAttributeValue(
      "BB_OUTPUT_TYPE", NOMAD::BBOutputTypeList{NOMAD::BBOutputType::OBJ});
  params->setAttributeValue("SEED", seed);
  params->setAttributeValue("DISPLAY_DEGREE", 0);
  // Serial evaluations make history order unambiguous and reproducible.
  params->setAttributeValue("NB_THREADS_PARALLEL_EVAL", 1);
  params->checkAndComply();

  NOMAD::MainStep main;
  main.setAllParameters(params);
  main.addEvaluator(std::make_unique<SphereEvaluator>(params->getEvalParams(),
                                                    history, run));
  main.start();
  main.run();
  main.end();

  const auto counted = NOMAD::EvcInterface::getEvaluatorControl()->getBbEval();
  if (history.empty() || counted != history.size() ||
      counted > static_cast<size_t>(maxEvaluations))
    throw std::runtime_error("Actual evaluation history and NOMAD count disagree");
  if (history.front().x != x || history.front().y != y)
    throw std::runtime_error("Starting point was not independently evaluated");
  if (history.back().best > target)
    throw std::runtime_error("Sphere did not converge to the required objective");

  std::cout << "Final run " << run << ": actual evaluations = " << history.size()
            << ", NOMAD BB evaluations = " << counted
            << ", best = " << history.back().best << '\n';
  return history;
}
} // namespace

int main() {
  try {
    std::cout << std::scientific
              << std::setprecision(std::numeric_limits<double>::max_digits10);
    const auto first = optimize(1, 42, 4.0, -3.0);
    const auto second = optimize(2, 7, -4.5, 4.25);
    const auto third = optimize(3, 123, 5.12, -5.12);
    const auto repeat = optimize(4, 42, 4.0, -3.0);
    if (first != repeat)
      throw std::runtime_error("Repeated run changed: optimization state leaked");
    NOMAD::MainStep::resetComponentsBetweenOptimization();
    std::cout << "PASS: all runs converged, evaluation counts agree, and the "
                 "repeated run has an identical evaluation history.\n";
  } catch (const std::exception &e) {
    std::cerr << "NOMAD Sphere test failed: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
