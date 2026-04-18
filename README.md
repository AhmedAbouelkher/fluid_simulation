# Fluid Simulation

A passion project implementing an Eulerian fluid solver in C, inspired by my studies in physics and computer science. This is an in-progress simulator that captures fluid motion, incompressibility, and smoke advection.

## Overview

This project implements a grid-based (Eulerian) fluid simulation. It solves the Euler equations for incompressible flow using a finite difference approach. The simulation includes:

- **Incompressibility**: Solved using the Gauss-Seidel method to ensure constant density.
- **Advection**: Semi-Lagrangian advection for both velocity and "smoke" (dye) fields.
- **Interactivity**: Real-time interaction with obstacles (circles and rectangles) and flow parameters.
- **Visualization**: Pressure field mapping with scientific colormaps and smoke density rendering.

## Current Features

- [x] Eulerian fluid solver (Grid-based).
- [x] Pressure-based incompressibility solver.
- [x] Velocity and smoke advection.
- [x] Interactive obstacles (Circle, Rectangle).
- [x] Real-time visualization using [Raylib](https://www.raylib.com/).
- [x] Adjustable simulation parameters (Relaxation, Gravity).

## Getting Started

### Prerequisites

- C Compiler (GCC/Clang)
- [Raylib](https://www.raylib.com/) library installed.

### Controls

- **Mouse Left Click**: Move/Place obstacle.
- **C Key**: Switch to Circle obstacle.
- **R Key**: Switch to Rectangle obstacle.
- **Space Key**: Remove obstacle.

## Resources & Inspiration

This project was built using insights from these excellent resources:

- [Coding Adventure: Simulating Smoke](https://www.youtube.com/watch?v=Q78wvrQ9xsU) by Sebastian Lague.
- [Ten-minute physics: Fluid Simulation](https://www.youtube.com/watch?v=iKAVRgIrUOU) by Matthias Müller.
- [Euler-Fluid-Simulation](https://github.com/xzips/Euler-Fluid-Simulation) by xzips.
- [Fluid Simulation for Dummies](https://mikeash.com/pyblog/fluid-simulation-for-dummies.html) by Mike Ash.

## Status

This is an **in-progress** passion project. Future plans include performance optimizations, more complex boundary conditions, and enhanced visualization modes (vorticity, streamlines).
