#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#define WINDOW_WIDTH 1000.0
#define WINDOW_HEIGHT 600.0
#define GRID_SCALE 5.0

#define DENSITY 1000.0
#define S_SOLID 0.0
#define S_FLUID 1.0
#define N_ITERATIONS 100
#define OBS_RADIUS 10.0
#define OBS_INIT_X_OFFSET 0.2f

bool showSmoke = false;

typedef enum {
  OBSTACLE_TYPE_NONE,
  OBSTACLE_TYPE_RECTANGLE,
  OBSTACLE_TYPE_CIRCLE,
} ObstacleType;

ObstacleType obstacleType = OBSTACLE_TYPE_RECTANGLE;

typedef enum { U_FIELDTYPE, V_FIELDTYPE, S_FIELDTYPE } FieldType;

typedef struct {
  size_t rows;
  size_t cols;
  size_t dim;
  float scale;

  float *u;
  float *v;

  float *S;
  float *P;
  float *M;
} Grid;

Grid grid = {0};
Vector2 obstaclePos = {0};
float gravity = 0;
// float gravity = -9.81f;
float relaxation = 1.7f;

void fillArr(float *arr, float val, size_t len) {
  for (size_t i = 0; i < len; i++) {
    arr[i] = val;
  }
}

void initGrid(Grid *grid, float scale) {
  grid->scale = scale;
  grid->rows = (size_t)floorf(WINDOW_HEIGHT / scale);
  grid->cols = (size_t)floorf(WINDOW_WIDTH / scale);
  grid->dim = (size_t)floorf(grid->rows * grid->cols);
  printf("rows: %zu, cols: %zu, dim: %zu\n", grid->rows, grid->cols, grid->dim);

  grid->u = (float *)calloc(grid->dim, sizeof(float));
  grid->v = (float *)calloc(grid->dim, sizeof(float));

  grid->S = (float *)calloc(grid->dim, sizeof(float));
  fillArr(grid->S, S_FLUID, grid->dim);

  grid->P = (float *)calloc(grid->dim, sizeof(float));

  grid->M = (float *)malloc(grid->dim * sizeof(float));
  fillArr(grid->M, 1.0f, grid->dim);
}

void freeGrid(Grid *grid) {
  free(grid->u);
  free(grid->v);
  free(grid->S);
  free(grid->P);
  free(grid->M);
}

Vector2 screenToGrid(Grid g, float x, float y) {
  Vector2 v = {.x = floorf(x / g.scale), .y = floorf(y / g.scale)};
  return v;
}

Vector2 gridToScreen(Grid g, int x, int y) {
  Vector2 v = {.x = floorf(x * g.scale), .y = floorf(y * g.scale)};
  return v;
}

float avgU(Grid grid, int i, int j) {
  size_t id_ij = i * grid.rows + j;
  size_t id_ij1 = i * grid.rows + (j + 1);
  size_t id_in1j = (i - 1) * grid.rows + j;
  size_t id_in1j1 = (i - 1) * grid.rows + (j + 1);
  return (grid.u[id_ij1] + grid.u[id_ij] + grid.u[id_in1j1] + grid.u[id_in1j]) /
         4.f;
}

float avgV(Grid grid, int i, int j) {
  size_t id_ij = i * grid.rows + j;
  size_t id_ij1 = i * grid.rows + (j + 1);
  size_t id_in1j = (i - 1) * grid.rows + j;
  size_t id_in1j1 = (i - 1) * grid.rows + (j + 1);
  return (grid.v[id_ij1] + grid.v[id_ij] + grid.v[id_in1j1] + grid.v[id_in1j]) /
         4.f;
}

float sampleField(Grid grid, float x, float y, float h, FieldType field) {
  size_t n = grid.rows;
  float h1 = 1.f / h;
  float h2 = 0.5f * h;

  x = fmaxf(h, fmin(x, grid.cols * h));
  y = fmaxf(h, fmin(y, grid.rows * h));

  float dx, dy = 0.f;

  switch (field) {
  case U_FIELDTYPE:
    dy = h2;
    break;
  case V_FIELDTYPE:
    dx = h2;
    break;
  case S_FIELDTYPE:
    dx = h2;
    dy = h2;
    break;
  default:
    fprintf(stderr, "FIELD TYPE IS NOT SUPPORTED\n");
    return -1.f;
  }

  size_t x0 = (size_t)fminf(grid.cols - 1, floorf((x - dx) * h1));
  float tx = ((x - dx) - x0 * h) * h1;
  size_t x1 = (size_t)fminf(grid.cols - 1, x0 + 1);

  size_t y0 = (size_t)fminf(grid.rows - 1, floorf((y - dy) * h1));
  float ty = ((y - dy) - y0 * h) * h1;
  size_t y1 = (size_t)fminf(grid.rows - 1, y0 + 1);

  float sx = 1.0 - tx;
  float sy = 1.0 - ty;

  float val = 0.0f;

  switch (field) {
  case U_FIELDTYPE:
    val = sx * sy * grid.u[x0 * n + y0] + tx * sy * grid.u[x1 * n + y0] +
          tx * ty * grid.u[x1 * n + y1] + sx * ty * grid.u[x0 * n + y1];
    break;
  case V_FIELDTYPE:
    val = sx * sy * grid.v[x0 * n + y0] + tx * sy * grid.v[x1 * n + y0] +
          tx * ty * grid.v[x1 * n + y1] + sx * ty * grid.v[x0 * n + y1];
    break;
  case S_FIELDTYPE:
    val = sx * sy * grid.M[x0 * n + y0] + tx * sy * grid.M[x1 * n + y0] +
          tx * ty * grid.M[x1 * n + y1] + sx * ty * grid.M[x0 * n + y1];
    break;
  }

  return val;
}

Color getSciColor(float val, float minVal, float maxVal) {
  val = fminf(fmaxf(val, minVal), maxVal - 0.0001f);
  float d = maxVal - minVal;
  val = (d == 0.0f) ? 0.5f : (val - minVal) / d;

  // Simple 'hot' colormap: blue -> cyan -> green -> yellow -> red
  float r = 0.f, g = 0.f, b = 0.f;

  if (val <= 0.25f) {
    // Blue to Cyan
    float t = val / 0.25f;
    r = 0.f;
    g = t;
    b = 1.0f;
  } else if (val <= 0.5f) {
    // Cyan to Green
    float t = (val - 0.25f) / 0.25f;
    r = 0.f;
    g = 1.0f;
    b = 1.0f - t;
  } else if (val <= 0.75f) {
    // Green to Yellow
    float t = (val - 0.5f) / 0.25f;
    r = t;
    g = 1.0f;
    b = 0.f;
  } else {
    // Yellow to Red
    float t = (val - 0.75f) / 0.25f;
    r = 1.0f;
    g = 1.0f - t;
    b = 0.f;
  }

  Color color = {0};
  color.r = (int)(255.0 * r);
  color.g = (int)(255.0 * g);
  color.b = (int)(255.0 * b);
  color.a = 255;
  return color;
}

void integrate(Grid *grid, float dt, float gravity) {
  for (size_t i = 0; i < grid->cols; i++) {
    for (size_t j = 1; j < grid->rows - 1; j++) {
      size_t idx = i * grid->rows + j;
      if (grid->S[idx] == S_SOLID)
        continue;

      grid->v[idx] += (dt * gravity);
    }
  }
}

void solveIncompressibility(Grid *grid, float dt, float h, float relaxation) {
  fillArr(grid->P, 0.0f, grid->dim);
  float cp = (DENSITY * h / dt);
  for (int itr = 0; itr < N_ITERATIONS; itr++) {
    for (size_t i = 1; i < grid->cols - 1; i++) {
      for (size_t j = 1; j < grid->rows - 1; j++) {
        size_t idx = i * grid->rows + j;
        if (grid->S[idx] == S_SOLID) {
          continue;
        }

        float u_i1j = grid->u[(i + 1) * grid->rows + j];
        float u_ij = grid->u[i * grid->rows + j];
        float v_ij1 = grid->v[i * grid->rows + (j + 1)];
        float v_ij = grid->v[i * grid->rows + j];
        float div = u_i1j - u_ij + v_ij1 - v_ij;

        float s_in1j = grid->S[(i - 1) * grid->rows + j];
        float s_i1j = grid->S[(i + 1) * grid->rows + j];
        float s_ijn1 = grid->S[i * grid->rows + (j - 1)];
        float s_ij1 = grid->S[i * grid->rows + (j + 1)];
        float s = s_i1j + s_in1j + s_ij1 + s_ijn1;
        if (s <= 1e-3f)
          continue;

        float p = (-div / s) * relaxation;
        grid->P[i * grid->rows + j] += p * cp;

        grid->u[i * grid->rows + j] -= (p * s_in1j);
        grid->u[(i + 1) * grid->rows + j] += (p * s_i1j);
        grid->v[i * grid->rows + j] -= (p * s_ijn1);
        grid->v[i * grid->rows + (j + 1)] += (p * s_ij1);
      }
    }
  }
}

void extrapolate(Grid *grid) {
  for (size_t i = 0; i < grid->cols; i++) {
    grid->u[i * grid->rows + 0] = grid->u[i * grid->rows + 1]; // bottom edge
    grid->u[i * grid->rows + (grid->rows - 1)] =
        grid->u[i * grid->rows + (grid->rows - 2)]; // top edge
  }
  for (size_t j = 0; j < grid->rows; j++) {
    grid->v[0 * grid->rows + j] = grid->v[1 * grid->rows + j]; // left edge
    grid->v[(grid->cols - 1) * grid->rows + j] =
        grid->v[(grid->cols - 2) * grid->rows + j]; // right edge
  }
}

void advectionVelocity(Grid *grid, float dt, float h) {
  float *newTempV = (float *)calloc(grid->dim, sizeof(float));
  float *newTempU = (float *)calloc(grid->dim, sizeof(float));
  memcpy(newTempU, grid->u, grid->dim * sizeof(float));
  memcpy(newTempV, grid->v, grid->dim * sizeof(float));

  float h2 = 0.5f * h;

  for (size_t i = 1; i < grid->cols; i++) {
    for (size_t j = 1; j < grid->rows; j++) {

      // u component
      if (grid->S[i * grid->rows + j] != S_SOLID &&
          grid->S[(i - 1) * grid->rows + j] != S_SOLID && j < grid->rows - 1) {
        float x = i * h;
        float y = j * h + h2;
        float u = grid->u[i * grid->rows + j];
        float v = avgV(*grid, i, j);

        x = x - dt * u;
        y = y - dt * v;
        u = sampleField(*grid, x, y, h, U_FIELDTYPE);
        newTempU[i * grid->rows + j] = u;
      }

      // v component
      if (grid->S[i * grid->rows + j] != S_SOLID &&
          grid->S[i * grid->rows + j - 1] != S_SOLID && i < grid->cols - 1) {
        float x = i * h + h2;
        float y = j * h;
        float u = avgU(*grid, i, j);
        float v = grid->v[i * grid->rows + j];

        x = x - dt * u;
        y = y - dt * v;
        v = sampleField(*grid, x, y, h, V_FIELDTYPE);
        newTempV[i * grid->rows + j] = v;
      }
    }
  }

  free(grid->u);
  free(grid->v);
  grid->u = newTempU;
  grid->v = newTempV;
}

void advectionSmoke(Grid *grid, float dt, float h) {
  float h2 = 0.5f * h;
  float *newM = (float *)calloc(grid->dim, sizeof(float));
  memcpy(newM, grid->M, grid->dim * sizeof(float));

  for (size_t i = 1; i < grid->cols - 1; i++) {
    for (size_t j = 1; j < grid->rows - 1; j++) {
      size_t idx = i * grid->rows + j;
      if (grid->S[idx] == S_SOLID) {
        continue;
      }
      float u = (grid->u[idx] + grid->u[(i + 1) * grid->rows + j]) * 0.5f;
      float v = (grid->v[idx] + grid->v[i * grid->rows + (j + 1)]) * 0.5f;
      float x = i * h + h2 - dt * u;
      float y = j * h + h2 - dt * v;
      newM[idx] = sampleField(*grid, x, y, h, S_FIELDTYPE);
    }
  }

  free(grid->M);
  grid->M = newM;
}

void setObstacle(Grid *grid, Vector2 center, float radius, ObstacleType type) {
  float vx = 0.0f;
  float vy = 0.0f;

  if (obstacleType == OBSTACLE_TYPE_CIRCLE) {
    for (size_t i = 1; i < grid->cols - 2; i++) {
      for (size_t j = 1; j < grid->rows - 2; j++) {
        size_t idx = i * grid->rows + j;
        grid->S[idx] = S_FLUID;

        float dx = (i + 0.5f) - center.x;
        float dy = (j + 0.5f) - center.y;
        float distance = sqrtf(dx * dx + dy * dy);
        if (distance < radius) {
          grid->S[idx] = S_SOLID;

          grid->M[idx] = 1.0f;

          grid->u[idx] = vx;
          grid->u[(i + 1) * grid->rows + j] = vx;
          grid->v[idx] = vy;
          grid->v[i * grid->rows + (j + 1)] = vy;
        }
      }
    }
    // TODO: Fix rectangle obstacle
  } else if (obstacleType == OBSTACLE_TYPE_RECTANGLE) {
    for (size_t i = 1; i < grid->cols - 2; i++) {
      for (size_t j = 1; j < grid->rows - 2; j++) {
        size_t idx = i * grid->rows + j;
        grid->S[idx] = S_FLUID;

        float dx = (i + 0.5f) - center.x;
        float dy = (j + 0.5f) - center.y;

        if (dx > -radius && dx < radius && dy > -radius && dy < radius) {
          grid->S[idx] = S_SOLID;
          grid->M[idx] = 1.0f;

          if (fabsf(dx) == radius || fabsf(dy) == radius) {
            // TODO
          }

          grid->u[idx] = vx;
          grid->u[(i + 1) * grid->rows + j] = vx;
          grid->v[idx] = vy;
          grid->v[i * grid->rows + (j + 1)] = vy;
        }
      }
    }

  } else if (obstacleType == OBSTACLE_TYPE_NONE) {
    for (size_t i = 1; i < grid->cols - 2; i++) {
      for (size_t j = 1; j < grid->rows - 2; j++) {
        size_t idx = i * grid->rows + j;
        grid->S[idx] = S_FLUID;
      }
    }

  } else {
    fprintf(stderr, "OBSTACLE TYPE IS NOT SUPPORTED\n");
    return;
  }
}

static void UpdateDrawFrame(void); // Update and draw one frame

int main(int argc, char **argv) {
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Fluid Simulation");

  initGrid(&grid, GRID_SCALE);

  /*
  // we will set the borders to be solid and the rest to be fluid
  for (size_t i = 0; i < grid.cols; i++) {
    for (size_t j = 0; j < grid.rows; j++) {
      size_t idx = i * grid.rows + j;
      grid.S[idx] = S_FLUID; // fluid
      // borders are solid
      if (i == 0 || i == grid.cols - 1 || j == 0 || j == grid.rows - 1) {
        grid.S[idx] = S_SOLID; // solid
      }
    }
  }
  */

  for (size_t i = 0; i < grid.cols; i++) {
    for (size_t j = 0; j < grid.rows; j++) {
      size_t idx = i * grid.rows + j;
      grid.S[idx] = S_FLUID; // fluid
      if (i == 0 || j == 0 || j == grid.rows - 1) {
        grid.S[idx] = S_SOLID; // solid
      }

      if (i == 1) {
        grid.u[idx] = 250.0f;
      }
    }
  }

  // build 10 pipes each one is 0.02f * grid.rows tall and should be distributed
  // with reduced spacing by centering them in a smaller range
  const size_t numPipes = 5;
  float pipeH = 0.04f * grid.rows;
  float totalHeight = 0.5f * grid.rows; // Use 50% of the height for all pipes
  float offset = (grid.rows - totalHeight) / 2.0f;
  for (size_t i = 0; i < numPipes; i++) {
    size_t pipeCenter =
        (size_t)floorf(offset + (i + 0.5f) * totalHeight / numPipes);
    size_t pipeMinJ = (size_t)floorf(pipeCenter - 0.5f * pipeH);
    size_t pipeMaxJ = (size_t)floorf(pipeCenter + 0.5f * pipeH);
    for (size_t j = 0; j < grid.rows; j++) {
      if (j >= pipeMinJ && j < pipeMaxJ) {
        grid.M[j] = 0.0f;
      }
    }
  }

  obstaclePos =
      (Vector2){.x = grid.cols * OBS_INIT_X_OFFSET, .y = grid.rows * .5f};
  if (obstacleType != OBSTACLE_TYPE_NONE) {
    setObstacle(&grid, obstaclePos, OBS_RADIUS, obstacleType);
  }

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
  SetTargetFPS(120); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    UpdateDrawFrame();
  }
#endif

  freeGrid(&grid);
  CloseWindow();

  return 0;
}

static void UpdateDrawFrame(void) {
  float dt = GetFrameTime();

  if (IsKeyPressed(KEY_C)) {
    obstacleType = OBSTACLE_TYPE_CIRCLE;
    setObstacle(&grid, obstaclePos, OBS_RADIUS, obstacleType);
  } else if (IsKeyPressed(KEY_R)) {
    obstacleType = OBSTACLE_TYPE_RECTANGLE;
    setObstacle(&grid, obstaclePos, OBS_RADIUS, obstacleType);
  } else if (IsKeyPressed(KEY_SPACE)) {
    // we want to remove the obstacle
    obstacleType = OBSTACLE_TYPE_NONE;
    setObstacle(&grid, obstaclePos, 0.0f, obstacleType);
  }

  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
    Vector2 mousePos = GetMousePosition();
    obstaclePos = screenToGrid(grid, mousePos.x, mousePos.y);
    setObstacle(&grid, obstaclePos, OBS_RADIUS, obstacleType);
  }

  float h = grid.scale;

  // Integrate
  integrate(&grid, dt, gravity);
  // Solve Incompressibility (skip boundaries)
  solveIncompressibility(&grid, dt, h, relaxation);
  // Extrapolate
  extrapolate(&grid);
  /// Advection
  advectionVelocity(&grid, dt, h);

  /// ADVECTION OF SMOKE
  advectionSmoke(&grid, dt, h);

  BeginDrawing();
  ClearBackground(RAYWHITE);

  float minP = INFINITY;
  float maxP = -INFINITY;
  for (size_t i = 0; i < grid.dim; i++) {
    minP = fminf(minP, grid.P[i]);
    maxP = fmaxf(maxP, grid.P[i]);
  }

  for (size_t i = 0; i < grid.cols; i++) {
    for (size_t j = 0; j < grid.rows; j++) {
      size_t idx = i * grid.rows + j;
      // if (grid.S[idx] == S_SOLID) {
      //   continue;
      // }
      float p = grid.P[idx];
      float s = grid.M[idx];
      Color color = getSciColor(p, minP, maxP);

      color.r = fmaxf(0, color.r - 255 * s);
      color.g = fmaxf(0, color.g - 255 * s);
      color.b = fmaxf(0, color.b - 255 * s);
      color.a = 255;

      Vector2 pos = gridToScreen(grid, i, j);
      DrawRectangle(pos.x, pos.y, grid.scale, grid.scale, color);

      // draw the obstacle
    }
  }

  /*
  float minSpeed = INFINITY;
  float maxSpeed = -INFINITY;
  for (size_t i = 0; i < grid.dim; i++) {
    float u = grid.u[i];
    float v = grid.v[i];
    float speed = sqrtf(u * u + v * v);
    minSpeed = fminf(minSpeed, speed);
    maxSpeed = fmaxf(maxSpeed, speed);
  }

  for (size_t i = 0; i < grid.cols; i++) {
    for (size_t j = 0; j < grid.rows; j++) {
      size_t idx = i * grid.rows + j;
      float s = grid.M[idx];
      // get the velocity at the center of the cell
      float u = grid.u[idx];
      float v = grid.v[idx];
      float speed = sqrtf(u * u + v * v);
      Color c = getSciColor(speed, minSpeed, maxSpeed);
      c.a = 255 * (1.0f - s);
      // printf("%.2f (%d, %d, %d)\n", speed, c.r, c.g, c.b);

      // Color color = {
      //     .r = 255 * (1.0f - s),
      //     .g = 255 * (1.0f - s),
      //     .b = 255 * (1.0f - s),
      //     .a = 255,
      // };
      Vector2 pos = gridToScreen(grid, i, j);
      DrawRectangle(pos.x, pos.y, grid.scale, grid.scale, c);
    }
  }
  */

  Vector2 obstacleScrPos = gridToScreen(grid, obstaclePos.x, obstaclePos.y);

  switch (obstacleType) {
  case OBSTACLE_TYPE_CIRCLE: {
    float obsR = OBS_RADIUS + grid.scale / 4;
    float radius = obsR * grid.scale;
    float x = obstacleScrPos.x;
    float y = obstacleScrPos.y;
    DrawCircle(x, y, radius, MAGENTA);
    DrawCircleLines(x, y, radius, WHITE);
    break;
  }
  case OBSTACLE_TYPE_RECTANGLE: {
    float dim = OBS_RADIUS * grid.scale * 2;
    float x = obstacleScrPos.x - dim / 2;
    float y = obstacleScrPos.y - dim / 2;
    DrawRectangle(x, y, dim, dim, MAGENTA);
    DrawRectangleLines(x, y, dim, dim, WHITE);
    break;
  }
  default:
    break;
  }

  // get the fps and display it in the top left corner
  DrawRectangle(0, 0, WINDOW_WIDTH, 25, DARKGRAY);
  float fps = GetFPS();
  float frameTime = dt * 1000.0f;
  const char *texts[4] = {
      TextFormat("GRID: %zux%zu", grid.cols, grid.rows),
      TextFormat("Gravity: %.2f/Relax: %.2f", gravity, relaxation),
      TextFormat("FPS: %.0f (%.2fms)", fps, frameTime),
  };
  const float textSize = 20;
  float currentOffset = 5;
  for (size_t i = 0; i < 4; i++) {
    const char *text = texts[i];
    float mTextWidth = MeasureText(text, textSize);
    // side by side in the same line, (margin of Xpx between them)
    DrawText(text, currentOffset, 5, textSize, WHITE);
    currentOffset += mTextWidth + 20;
  }

  EndDrawing();
}