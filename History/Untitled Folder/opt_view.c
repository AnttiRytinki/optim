#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "optimizers.h"

#define WIDTH 900
#define HEIGHT 900

#define SEEKER_EXPLORER 0
#define SEEKER_EXPLOITER 1
#define SEEKER_REFINER 2

void ExploreExploitInteractiveInit(const TestProblem *problem);
void ExploreExploitInteractiveStep(void);
int ExploreExploitInteractiveGetPointCount(void);
void ExploreExploitInteractiveGetPoint(int index, double *x, double *y, int *type);
void ExploreExploitInteractiveGetBest(double *x, double *y, double *value, int *evaluations);

static double Rastrigin(const double *x, int dim)
{
    double sum = 10.0 * dim;

    for (int i = 0; i < dim; i++)
        sum += x[i] * x[i] - 10.0 * cos(2.0 * M_PI * x[i]);

    return sum;
}

static int ToScreenX(double x, const TestProblem *problem)
{
    return (int)((x - problem->lower) / (problem->upper - problem->lower) * WIDTH);
}

static int ToScreenY(double y, const TestProblem *problem)
{
    return HEIGHT - (int)((y - problem->lower) / (problem->upper - problem->lower) * HEIGHT);
}

static void DrawCircle(SDL_Renderer *renderer, int cx, int cy, int radius)
{
    for (int y = -radius; y <= radius; y++)
    {
        for (int x = -radius; x <= radius; x++)
        {
            if (x * x + y * y <= radius * radius)
                SDL_RenderDrawPoint(renderer, cx + x, cy + y);
        }
    }
}

static void DrawBackground(SDL_Renderer *renderer, const TestProblem *problem)
{
    double point[MAX_DIM];

    for (int py = 0; py < HEIGHT; py += 4)
    {
        for (int px = 0; px < WIDTH; px += 4)
        {
            point[0] = problem->lower + (double)px / WIDTH * (problem->upper - problem->lower);
            point[1] = problem->lower + (double)(HEIGHT - py) / HEIGHT * (problem->upper - problem->lower);

            double value = problem->function(point, problem->dim);

            int shade = (int)(value * 4.0);

            if (shade < 0)
                shade = 0;

            if (shade > 255)
                shade = 255;

            SDL_SetRenderDrawColor(renderer, shade, shade, shade, 255);

            SDL_Rect rect = { px, py, 4, 4 };
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}

static void DrawSeekers(SDL_Renderer *renderer, const TestProblem *problem)
{
    int count = ExploreExploitInteractiveGetPointCount();

    for (int i = 0; i < count; i++)
    {
        double x;
        double y;
        int type;

        ExploreExploitInteractiveGetPoint(i, &x, &y, &type);

        if (type == SEEKER_EXPLORER)
            SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
        else if (type == SEEKER_EXPLOITER)
            SDL_SetRenderDrawColor(renderer, 80, 160, 255, 255);
        else
            SDL_SetRenderDrawColor(renderer, 255, 255, 80, 255);

        DrawCircle(renderer, ToScreenX(x, problem), ToScreenY(y, problem), 4);
    }
}

static void DrawBest(SDL_Renderer *renderer, const TestProblem *problem)
{
    double x;
    double y;
    double value;
    int evaluations;

    ExploreExploitInteractiveGetBest(&x, &y, &value, &evaluations);

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    DrawCircle(renderer, ToScreenX(x, problem), ToScreenY(y, problem), 8);
}

int main(void)
{
    srand((unsigned int)time(NULL));

    TestProblem problem;

    problem.name = "Rastrigin";
    problem.dim = 2;
    problem.lower = -5.12;
    problem.upper = 5.12;
    problem.optimum[0] = 0.0;
    problem.optimum[1] = 0.0;
    problem.optimumValue = 0.0;
    problem.function = Rastrigin;

    ExploreExploitInteractiveInit(&problem);

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Explore/Exploit Optimizer Viewer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH,
        HEIGHT,
        0);

    if (window == NULL)
    {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (renderer == NULL)
    {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int running = 1;

    while (running)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                running = 0;

            if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = 0;

                if (event.key.keysym.sym == SDLK_SPACE)
                    ExploreExploitInteractiveStep();

                if (event.key.keysym.sym == SDLK_RETURN)
                {
                    for (int i = 0; i < 100; i++)
                        ExploreExploitInteractiveStep();
                }

                if (event.key.keysym.sym == SDLK_r)
                    ExploreExploitInteractiveInit(&problem);
            }
        }

        DrawBackground(renderer, &problem);
        DrawSeekers(renderer, &problem);
        DrawBest(renderer, &problem);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
