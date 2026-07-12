#pragma once
#include <cmath>
#include <random>

struct Point {
    double x, y;

    Point(double x = 0, double y = 0) : x(x), y(y) {}

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

struct Road {
    double x0, y0, x1, y1;

    Road(double x0, double y0, double x1, double y1)
        : x0(x0), y0(y0), x1(x1), y1(y1) {
    }

    double length() const {
        return std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    }

    Point random_point(std::mt19937& rng) const {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double t = dist(rng);
        return Point(
            x0 + (x1 - x0) * t,
            y0 + (y1 - y0) * t
        );
    }
};