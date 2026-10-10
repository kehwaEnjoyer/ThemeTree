#include <iostream>
#include <string>
#include <algorithm>
#include "ThemeTree.h"

// Convert user string to enum mode
themeTree::SortMode parseSortMode(std::string arg) {
    std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);

    if (arg == "hybrid")    return themeTree::SortMode::HYBRID;
    if (arg == "dominance") return themeTree::SortMode::DOMINANCE;
    if (arg == "lightness") return themeTree::SortMode::LIGHTNESS;
    if (arg == "chroma")    return themeTree::SortMode::CHROMA;
    if (arg == "hue")       return themeTree::SortMode::HUE;

    std::cerr << "[!] Unknown sort mode '" << arg << "'. Defaulting to 'hybrid'.\n";
    return themeTree::SortMode::HYBRID;
}

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " <image_path> <css_out> <json_out> [options]\n\n"
              << "Options:\n"
              << "  --sort=<mode>    Set palette sorting mode (Default: hybrid)\n"
              << "                     hybrid    : Dark BG, bright text, spectrum accents\n"
              << "                     dominance : Highest pixel count -> lowest\n"
              << "                     lightness : Darkest (L* = 0) -> brightest (L* = 100)\n"
              << "                     chroma    : Most vibrant (Max C*) -> muted\n"
              << "                     hue       : Spectrum order (-180° to +180°)\n"
              << "  -h, --help       Show this usage message\n";
}

int main(int argc, char* argv[]) {
    // Check for help flag anywhere or minimum positional arguments
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string imagePath = argv[1];
    std::string cssPath   = argv[2];
    std::string jsonPath  = argv[3];

    themeTree::SortMode sortMode = themeTree::SortMode::HYBRID; // Default strategy

    // Parse optional flags
    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }

        // Handles --sort=lightness syntax
        if (arg.rfind("--sort=", 0) == 0) {
            std::string modeStr = arg.substr(7);
            sortMode = parseSortMode(modeStr);
        }
        // Handles --sort lightness syntax
        else if (arg == "--sort" && i + 1 < argc) {
            sortMode = parseSortMode(argv[++i]);
        }
    }

    // Pipeline Execution
    themeTree engine;

    if (!engine.loadImage(imagePath)) {
        std::cerr << "Failed to load image: " << imagePath << "\n";
        return 1;
    }

    engine.ConvertTmatrix();
    engine.rgbTcielab();

    // Run K-Means (K = 8 clusters)
    if (!engine.runKmeans(8, 100, 1e-4f)) {
        std::cerr << "K-Means execution failed.\n";
        return 1;
    }

    // Apply selected sorting strategy
    engine.sortResult(sortMode);

    // Export colors
    engine.exportPalette(engine.result.centroids,  cssPath, jsonPath);

    std::cout << "Successfully exported palette to " << cssPath << " and " << jsonPath << "\n";
    return 0;
}