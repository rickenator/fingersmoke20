#include "Grid2D.h"
#include <cstring>

namespace fluidsim {

Grid2D::Grid2D(int width, int height)
    : mWidth(width), mHeight(height) {
    mData.resize(width * height, 0.0f);
}

Grid2D::~Grid2D() {
}

void Grid2D::clear() {
    std::fill(mData.begin(), mData.end(), 0.0f);
}

void Grid2D::copyFrom(const float* data) {
    std::memcpy(mData.data(), data, mWidth * mHeight * sizeof(float));
}

} // namespace fluidsim
