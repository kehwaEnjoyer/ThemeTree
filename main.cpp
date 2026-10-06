// main.cpp
#include "ThemeTree.h"
#include <iostream>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <input_image> <output_css> <output_json> [K_clusters]\n";
        std::cout << "Example: " << argv[0] << " wallpaper.png palette.css palette.json 8\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::string cssPath   = argv[2];
    std::string jsonPath  = argv[3];
    int K = (argc >= 5) ? std::stoi(argv[4]) : 8;

    themeTree engine;

    auto t_start = std::chrono::high_resolution_clock::now();

    // 1. Load image from disk
    std::cout << "[1/5] Loading image: " << inputPath << "...\n";
    if (!engine.loadImage(inputPath)) {
        return 1;
    }

    // 2. Convert raw uint8_t buffer to normalized RGB float matrix
    std::cout << "[2/5] Building RGB matrix...\n";
    engine.ConvertTmatrix();

    // 3. Convert RGB matrix to CIELAB space
    std::cout << "[3/5] Converting RGB to CIELAB color space...\n";
    engine.rgbTcielab();

    // 4. Run multi-threaded K-Means clustering
    std::cout << "[4/5] Running K-Means clustering (K = " << K << ")...\n";
    if (!engine.runKmeans(K)) {
        std::cerr << "Error running K-Means.\n";
        return 1;
    }

    // 5. Convert centroids back to sRGB and write palette files
    std::cout << "[5/5] Exporting palette files...\n";
    engine.exportPalette(engine.result.centroids, cssPath, jsonPath);

    auto t_end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    std::cout << "Done! Palette exported successfully in " << total_ms << " ms.\n";

    return 0;
}