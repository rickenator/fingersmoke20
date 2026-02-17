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

void Grid2D::set(int x, int y, float value) {
    if (x >= 0 && x < mWidth && y >= 0 && y < mHeight) {
        mData[x + y * mWidth] = value;
    }
}

float Grid2D::get(int x, int y) const {
    if (x >= 0 && x < mWidth && y >= 0 && y < mHeight) {
        return mData[x + y * mWidth];
    }
    return 0.0f;
}

} // namespace fluidsim
