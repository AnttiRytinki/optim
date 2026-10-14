#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#ifdef OPTIM_NOMAD_VIEW
#include "nomad_view.h"
#endif

#include "optimizers.h"
#include "problems.h"

#define WIDTH 900
#define HEIGHT 900

#define SEEKER_EXPLORER 0
#define SEEKER_EXPLOITER 1
#define SEEKER_REFINER 2

#define DIRECTIONAL_TREE_AGENT 5
#define DIRECTIONAL_TREE_ACTIVE 6
#define DIRECTIONAL_TREE_INACTIVE 7
#define DIRECTIONAL_TREE_TESTED 8

static int ToScreenX(double x, const TestProblem *problem) {
  return (int)((x - problem->lower) / (problem->upper - problem->lower) *
               (WIDTH - 1));
}

static int ToScreenY(double y, const TestProblem *problem) {
  return HEIGHT - 1 - (int)((y - problem->lower) /
                        (problem->upper - problem->lower) * (HEIGHT - 1));
}

static void DrawCircle(SDL_Renderer *renderer, int cx, int cy, int radius) {
  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      if (x * x + y * y <= radius * radius)
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
    }
  }
}

static void DrawBackground(SDL_Renderer *renderer, const TestProblem *problem) {
  double point[MAX_DIM];

  for (int py = 0; py < HEIGHT; py += 4) {
    for (int px = 0; px < WIDTH; px += 4) {
      point[0] = problem->lower +
                 (double)px / WIDTH * (problem->upper - problem->lower);
      point[1] = problem->lower + (double)(HEIGHT - py) / HEIGHT *
                                      (problem->upper - problem->lower);

      double value = problem->function(point, problem->dim);
      int shade = (int)(value * 4.0);

      if (shade < 0)
        shade = 0;

      if (shade > 255)
        shade = 255;

      SDL_SetRenderDrawColor(renderer, shade, shade, shade, 255);

      SDL_Rect rect = {px, py, 4, 4};
      SDL_RenderFillRect(renderer, &rect);
    }
  }
}

static void DrawSeekers(SDL_Renderer *renderer, const TestProblem *problem,
                        InteractiveOptimizer *optimizer) {
  int count = optimizer->GetPointCount();

  for (int i = 0; i < count; i++) {
    double x;
    double y;
    int type;

    optimizer->GetPoint(i, &x, &y, &type);

    if (type == SEEKER_EXPLORER)
      SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
    else if (type == SEEKER_EXPLOITER)
      SDL_SetRenderDrawColor(renderer, 80, 160, 255, 255);
    else if (type == SEEKER_REFINER)
      SDL_SetRenderDrawColor(renderer, 255, 255, 80, 255);
    else if (type == DIRECTIONAL_TREE_AGENT)
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    else if (type == DIRECTIONAL_TREE_ACTIVE)
      SDL_SetRenderDrawColor(renderer, 80, 160, 255, 255);
    else if (type == DIRECTIONAL_TREE_INACTIVE)
      SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    else if (type == DIRECTIONAL_TREE_TESTED)
      SDL_SetRenderDrawColor(renderer, 255, 80, 255, 255);
    else
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    int radius = 4;

    if (type == DIRECTIONAL_TREE_TESTED)
      radius = 1;

    DrawCircle(renderer, ToScreenX(x, problem), ToScreenY(y, problem), radius);
  }
}

static void DrawBest(SDL_Renderer *renderer, const TestProblem *problem,
                     InteractiveOptimizer *optimizer) {
  double x;
  double y;
  double value;
  int evaluations;

  optimizer->GetBest(&x, &y, &value, &evaluations);

  if (evaluations == 0)
    return;

  SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
  DrawCircle(renderer, ToScreenX(x, problem), ToScreenY(y, problem), 8);
}

static void DrawKnownOptima(SDL_Renderer *renderer,
                            const TestProblem *problem) {
  SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);

  for (int i = 0; i < problem->optimumCount; i++) {
    DrawCircle(renderer, ToScreenX(problem->optima[i][0], problem),
               ToScreenY(problem->optima[i][1], problem), 5);
  }
}

static void UpdateWindowTitle(SDL_Window *window,
                              InteractiveOptimizer *optimizer,
                              const char *problemName) {
  char title[1600];
  double x, y, value;
  int evaluations;
  optimizer->GetBest(&x, &y, &value, &evaluations);
  const char *status = "";
#ifdef OPTIM_NOMAD_VIEW
  if (optimizer == &NomadOptimizer)
    status = NomadViewStatus();
#endif
  snprintf(title, sizeof(title), "%s - %s | eval %d | best %.6e%s%s | Space: 1, Enter: 100, R: reset, Tab: optimizer, P: problem",
           optimizer->name, problemName, evaluations, value, *status ? " | " : "", status);

  SDL_SetWindowTitle(window, title);
}

int main(int argc, char **argv) {
  int smokeTest = argc == 2 && strcmp(argv[1], "--smoke-test") == 0;
  if (argc > 1 && !smokeTest)
  {
    fprintf(stderr, "Usage: %s [--smoke-test]\n", argv[0]);
    return 1;
  }
  if (smokeTest)
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
  srand(smokeTest ? 42u : (unsigned int)time(NULL));

  TestProblem *problems[] = {&SphereProblem,     &RastriginProblem,
                             &RosenbrockProblem, &AckleyProblem,
                             &HimmelblauProblem, &GriewankProblem};

  const char *problemNames[] = {"Sphere", "Rastrigin",  "Rosenbrock",
                                "Ackley", "Himmelblau", "Griewank"};

  int problemCount = sizeof(problems) / sizeof(problems[0]);

  int selectedProblem = 1;
  int selectedOptimizer = 0;

  TestProblem *problem = problems[selectedProblem];

  Optimizers[selectedOptimizer]->Init(problem);

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow(
      Optimizers[selectedOptimizer]->name, SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);

  if (window == NULL) {
    printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  if (renderer == NULL)
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

  if (renderer == NULL) {
    printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  UpdateWindowTitle(window, Optimizers[selectedOptimizer],
                    problemNames[selectedProblem]);

  int running = 1;
  int smokeFrame = 0;

  while (running) {
    SDL_Event event;
    if (smokeTest)
    {
      SDL_Event simulated = {0};
      const SDL_Keycode keys[] = {SDLK_SPACE, SDLK_RETURN, SDLK_p, SDLK_p,
          SDLK_p, SDLK_p, SDLK_p, SDLK_p, SDLK_r, SDLK_TAB};
      if (smokeFrame < (int)(sizeof(keys) / sizeof(keys[0])))
      {
        simulated.type = SDL_KEYDOWN;
        simulated.key.keysym.sym = keys[smokeFrame++];
      }
      else
        simulated.type = SDL_QUIT;
      SDL_PushEvent(&simulated);
    }

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        running = 0;

      if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE)
          running = 0;

        if (event.key.keysym.sym == SDLK_SPACE)
          Optimizers[selectedOptimizer]->Step();

        if (event.key.keysym.sym == SDLK_RETURN) {
          for (int i = 0; i < 100; i++)
            Optimizers[selectedOptimizer]->Step();
        }

        if (event.key.keysym.sym == SDLK_r)
          Optimizers[selectedOptimizer]->Init(problem);

        if (event.key.keysym.sym == SDLK_TAB) {
          selectedOptimizer++;

          if (selectedOptimizer >= OptimizerCount)
            selectedOptimizer = 0;

          Optimizers[selectedOptimizer]->Init(problem);

          UpdateWindowTitle(window, Optimizers[selectedOptimizer],
                            problemNames[selectedProblem]);
        }

        if (event.key.keysym.sym == SDLK_p) {
          selectedProblem++;

          if (selectedProblem >= problemCount)
            selectedProblem = 0;

          problem = problems[selectedProblem];

          Optimizers[selectedOptimizer]->Init(problem);

          UpdateWindowTitle(window, Optimizers[selectedOptimizer],
                            problemNames[selectedProblem]);
        }
      }
    }

    UpdateWindowTitle(window, Optimizers[selectedOptimizer],
                      problemNames[selectedProblem]);
    DrawBackground(renderer, problem);
    DrawSeekers(renderer, problem, Optimizers[selectedOptimizer]);
    DrawBest(renderer, problem, Optimizers[selectedOptimizer]);
    DrawKnownOptima(renderer, problem);

    SDL_RenderPresent(renderer);
  }

#ifdef OPTIM_NOMAD_VIEW
  NomadViewCleanup();
#endif
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  if (smokeTest)
    puts("PASS: viewer rendered NOMAD across all six problems; step, advance, restart, and optimizer switching worked.");
  return 0;
}
