#include <iostream>
#include "ThemeTree.h"

int main() {
    themeTree img;
    img.loadImage("test.png");

    if (img.wallpaper.pixels.empty()) {
        return 1;
    }

    std::cout << "Image Loaded Successfully!\n";
    std::cout << "Width:    " << img.wallpaper.width << " px\n";
    std::cout << "Height:   " << img.wallpaper.height << " px\n";
    std::cout << "Total Pixels (N): " << img.wallpaper.width * img.wallpaper.height << "\n";
    std::cout << "Total Bytes:      " << img.wallpaper.pixels.size() << " bytes\n";

    // Inspect the top-left pixel (0, 0)
    int r = img.wallpaper.at(0, 0, 0);
    int g = img.wallpaper.at(0, 0, 1);
    int b = img.wallpaper.at(0, 0, 2);

    std::cout << "Top-Left Pixel RGB: (" << r << ", " << g << ", " << b << ")\n";

    img.ConvertTmatrix();
    std::cout << "Eigen Matrix Shape: " << img.imgMatrix.rows() << " x " << img.imgMatrix.cols() << "\n";
    std::cout << "First Pixel Normalized RGB: " << img.imgMatrix.row(0) << "\n";

    img.rgbTcielab();

    std::cout << "\nCIELAB Matrix Shape: " << img.labMatrix.rows() << " x " << img.labMatrix.cols() << "\n";
    std::cout << "First Pixel CIELAB (L*, a*, b*): " << img.labMatrix.row(0) << "\n";
    return 0;
}