#pragma once

namespace geom {

struct Point2D {
    double x;
    double y;

    Point2D(double x_ = 0.0, double y_ = 0.0) : x(x_), y(y_) {}
};

inline bool operator==(const Point2D& lhs, const Point2D& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline bool operator!=(const Point2D& lhs, const Point2D& rhs) {
    return !(lhs == rhs);
}

}  // namespace geom