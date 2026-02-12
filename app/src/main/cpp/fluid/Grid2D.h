#ifndef GRID2D_H
#define GRID2D_H

#include <vector>

namespace fluidsim {

class Grid2D {
public:
    Grid2D(int width, int height);
    ~Grid2D();

    int getWidth() const { return mWidth; }
    int getHeight() const { return mHeight; }
    size_t getSize() const { return mWidth * mHeight; }

    float& at(int x, int y) { return mData[x + y * mWidth]; }
    const float& at(int x, int y) const { return mData[x + y * mWidth]; }

    void clear();
    void copyFrom(const float* data);
    float* getData() { return mData.data(); }
    const float* getData() const { return mData.data(); }

private:
    int mWidth, mHeight;
    std::vector<float> mData;
};

} // namespace fluidsim

#endif // GRID2D_H
