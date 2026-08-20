# Car Lane Racer

A 2D lane-based car racing game built with **C++** and **Qt Widgets**. Choose your car, pick a difficulty, and dodge oncoming traffic, road logs, and crossing dogs to reach the finish line.

## Features
- **3 difficulty levels** — Easy, Moderate, and Hard, each with different obstacle patterns and spawn rates
- **Night Mode** on Hard difficulty — a dynamic headlight cone effect that limits visibility
- **Multiple obstacle types** — lane-changing cars, floating logs, and crossing dogs, each with their own movement logic
- **Car selection menu** — pick from 4 different car colors before starting
- **Sound effects** — engine loop, crash, and victory sounds via Qt Multimedia
- **Custom-drawn sprites** — all cars, logs, and dogs are hand-drawn with `QPainter`, no external image assets required

## Built with
- C++17
- Qt Widgets, Qt Multimedia
- Qt Graphics View Framework (`QGraphicsScene` / `QGraphicsView`)

## Building
Open `game.pro` in Qt Creator (Qt 6.x recommended, with the Multimedia module installed), select a kit, and build in Release mode. See the Releases page for a pre-built Windows executable.

## Controls
- **Left / Right arrow keys** or the on-screen buttons to switch lanes
- Avoid obstacles and collect enough score to trigger the finish line
