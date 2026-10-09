#include <iostream>
#include <vector>
#include <string>
#include <Eigen/Dense>
#include <random>
#include <omp.h>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class themeTree {

public:
    
    enum class SortMode {
        HYBRID,    // Recommended: Dark BG + Bright Text + Spectrum Accents
        DOMINANCE, // Highest pixel count -> Lowest pixel count
        LIGHTNESS, // Darkest (L* = 0) -> Lightest (L* = 100)
        CHROMA,    // Most vibrant (Max C*) -> Most muted (Min C*)
        HUE        // Color wheel order (-180° to +180°)
    };

    struct image {
        int width = 0;
        int height = 0;
        int channels = 0;
        std::vector<uint8_t> pixels;

        uint8_t& at(int x, int y, int c) {
            return pixels[(y * width + x) * channels + c];
        }

        const uint8_t& at(int x, int y, int c) const {
            return pixels[(y * width + x) * channels + c];
        }
    };

    struct KMeansResult {
        Eigen::MatrixXf centroids; // (K x 3) Final mean CIELAB colors
        Eigen::VectorXi labels;    // (N x 1) Cluster assignment ID [0, K-1] per pixel
        Eigen::VectorXf counts;
    };

    struct ColorRGB {
    int r, g, b;
    std::string hex;
    };

    image wallpaper;
    KMeansResult result;
    Eigen::MatrixXf imgMatrix;
    Eigen::MatrixXf labMatrix;

    themeTree() {}

    bool loadImage(const std::string filePath) {
        int w, h, oriChannels;

        unsigned char* data = stbi_load(filePath.c_str(), &w, &h, &oriChannels, 3);
        
        if (!data) {
            std::cerr << "Image error loading image at: " << filePath << "\n";
            return false;
        }

        wallpaper.width = w;
        wallpaper.height = h;
        wallpaper.channels = 3;

        wallpaper.pixels.assign(data, data + (w * h * 3));

        stbi_image_free(data);
        std::cout << "Image loaded successfully.\n";
        return true;
    }

    bool ConvertTmatrix() {
        const int N = wallpaper.width * wallpaper.height;
        imgMatrix.resize(N, 3);

        // Parallelize RGB normalization
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < N; ++i) {
            imgMatrix(i, 0) = wallpaper.pixels[i * 3 + 0] / 255.0f;
            imgMatrix(i, 1) = wallpaper.pixels[i * 3 + 1] / 255.0f;
            imgMatrix(i, 2) = wallpaper.pixels[i * 3 + 2] / 255.0f;
        }

        return true;
    }

    bool rgbTcielab() {
        const int N = imgMatrix.rows();
        labMatrix.resize(N, 3);

        float gammaLut[256];
        for (int i = 0; i < 256; ++i) {
            float c = i / 255.0f;
            gammaLut[i] = (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }

        Eigen::Matrix3f rgbTxyzMat;
        rgbTxyzMat << 0.4124564f, 0.3575761f, 0.1804375f,
                      0.2126729f, 0.7151522f, 0.0721750f,
                      0.0193339f, 0.1191920f, 0.9503041f;

        const float Xn = 0.95047f;
        const float Yn = 1.00000f;
        const float Zn = 1.08883f;

        const float delta = 6.0f / 29.0f;
        const float delta_cubed = delta * delta * delta;
        const float three_delta_sq = 3.0f * delta * delta;

        auto f_lab = [&](float t) -> float {
            return (t > delta_cubed) ? std::cbrt(t) : (t / three_delta_sq + 4.0f / 29.0f);
        };

        // Parallelize CIELAB conversion across all pixels
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < N; ++i) {
            uint8_t r_byte = wallpaper.pixels[i * 3 + 0];
            uint8_t g_byte = wallpaper.pixels[i * 3 + 1];
            uint8_t b_byte = wallpaper.pixels[i * 3 + 2];

            Eigen::Vector3f linear_rgb(
                gammaLut[r_byte],
                gammaLut[g_byte],
                gammaLut[b_byte]
            );

            Eigen::Vector3f xyz = rgbTxyzMat * linear_rgb;

            float fx = f_lab(xyz(0) / Xn);
            float fy = f_lab(xyz(1) / Yn);
            float fz = f_lab(xyz(2) / Zn);

            labMatrix(i, 0) = 116.0f * fy - 16.0f; // L*
            labMatrix(i, 1) = 500.0f * (fx - fy);   // a*
            labMatrix(i, 2) = 200.0f * (fy - fz);   // b*
        }

        return true;
    }

    bool runKmeans(int K, int maxIters = 100, float tol = 1e-4f) {
        const int N = labMatrix.rows();
        const int channels = labMatrix.cols();

        Eigen::MatrixXf centroids(K, channels);
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(0, N - 1);

        for (int k = 0; k < K; ++k) {
            centroids.row(k) = labMatrix.row(dist(rng));
        }

        this->result.labels.resize(N);
        this->result.counts.resize(K);
        Eigen::VectorXf finalCounts = Eigen::VectorXf::Zero(K);

        // 1. PRE-ALLOCATE THREAD-LOCAL BUFFERS OUTSIDE THE ITERATION LOOP
        int max_threads = omp_get_max_threads();
        std::vector<Eigen::MatrixXf> thread_centroids(max_threads, Eigen::MatrixXf::Zero(K, channels));
        std::vector<Eigen::VectorXf> thread_counts(max_threads, Eigen::VectorXf::Zero(K));

        for (int iter = 0; iter < maxIters; ++iter) {
            // Reset thread-local accumulators (no heap allocations!)
            for (int t = 0; t < max_threads; ++t) {
                thread_centroids[t].setZero();
                thread_counts[t].setZero();
            }

            // 2. FUSED PARALLEL PASS OVER ALL N PIXELS
            #pragma omp parallel
            {
                int tid = omp_get_thread_num();

                #pragma omp for schedule(static)
                for (int i = 0; i < N; ++i) {
                    float L = labMatrix(i, 0);
                    float a = labMatrix(i, 1);
                    float b = labMatrix(i, 2);

                    float min_dist = std::numeric_limits<float>::max();
                    int best_k = 0;

                    // Fast scalar distance check in CPU registers (Auto-vectorized by -O3)
                    for (int k = 0; k < K; ++k) {
                        float dL = L - centroids(k, 0);
                        float da = a - centroids(k, 1);
                        float db = b - centroids(k, 2);
                        float dist = dL * dL + da * da + db * db;

                        if (dist < min_dist) {
                            min_dist = dist;
                            best_k = k;
                        }
                    }

                    // Assign label & accumulate directly into thread-local buffer
                    this->result.labels(i) = best_k;
                    thread_centroids[tid].row(best_k) += labMatrix.row(i);
                    thread_counts[tid](best_k) += 1.0f;
                }
            }

            // 3. COMBINE THREAD RESULTS
            Eigen::MatrixXf newCentroids = Eigen::MatrixXf::Zero(K, channels);
            Eigen::VectorXf counts = Eigen::VectorXf::Zero(K);

            for (int t = 0; t < max_threads; ++t) {
                newCentroids += thread_centroids[t];
                counts += thread_counts[t];
            }

            for (int k = 0; k < K; ++k) {
                if (counts(k) > 0.0f) {
                    newCentroids.row(k) /= counts(k);
                } else {
                    newCentroids.row(k) = labMatrix.row(dist(rng));
                }
            }

            float shift = (newCentroids - centroids).squaredNorm();
            centroids = newCentroids;
            finalCounts= counts;

            if (shift < tol) {
                std::cout << "K-Means converged at iteration " << iter + 1 << "\n";
                break;
            }
        }

        this->result.centroids = centroids;
        this->result.counts= finalCounts;
        return true;
    }

    void sortResult(SortMode mode) {
        int K = this->result.centroids.rows();
        if (K == 0 || this->result.counts.size() != K) return;

        // Use the custom UI pipeline if selected
        if (mode == SortMode::HYBRID) {
            this->applyHybridThemePipeline();
            return;
        }

        // Create index array [0, 1, ..., K-1] to sort indirectly
        std::vector<int> indices(K);
        std::iota(indices.begin(), indices.end(), 0);

        switch (mode) {
            case SortMode::DOMINANCE: {
                // Sort by pixel count descending
                std::sort(indices.begin(), indices.end(), [this](int a, int b) {
                    return this->result.counts(a) > this->result.counts(b);
                });
                break;
            }
            case SortMode::LIGHTNESS: {
                // Sort by L* channel ascending (Darkest -> Brightest)
                std::sort(indices.begin(), indices.end(), [this](int a, int b) {
                    return this->result.centroids(a, 0) < this->result.centroids(b, 0);
                });
                break;
            }
            case SortMode::CHROMA: {
                // Sort by Chroma C* = sqrt(a*^2 + b*^2) descending (Vibrant -> Muted)
                std::sort(indices.begin(), indices.end(), [this](int a, int b) {
                    float chromaA = std::hypot(this->result.centroids(a, 1), this->result.centroids(a, 2));
                    float chromaB = std::hypot(this->result.centroids(b, 1), this->result.centroids(b, 2));
                    return chromaA > chromaB;
                });
                break;
            }
            case SortMode::HUE: {
                // Sort by Hue angle hab = atan2(b*, a*) ascending (Spectrum order)
                std::sort(indices.begin(), indices.end(), [this](int a, int b) {
                    float hueA = std::atan2(this->result.centroids(a, 2), this->result.centroids(a, 1));
                    float hueB = std::atan2(this->result.centroids(b, 2), this->result.centroids(b, 1));
                    return hueA < hueB;
                });
                break;
            }
            default:
                break;
        }

        // Reorder centroids and counts matrices using sorted indices
        Eigen::MatrixXf sortedCentroids(K, this->result.centroids.cols());
        Eigen::VectorXf sortedCounts(K);

        for (int i = 0; i < K; ++i) {
            sortedCentroids.row(i) = this->result.centroids.row(indices[i]);
            sortedCounts(i)        = this->result.counts(indices[i]);
        }

        this->result.centroids = sortedCentroids;
        this->result.counts    = sortedCounts;
    }

    void applyHybridThemePipeline() {
        int K = result.centroids.rows();
        if (K < 8) return; // Works best with K >= 8

        struct ColorInfo {
            Eigen::Vector3f lab;
            float count;
            float L;       // Lightness
            float chroma;  // Vibrancy sqrt(a*^2 + b*^2)
            float hue;     // Hue angle in radians
        };

        std::vector<ColorInfo> candidates(K);
        for (int i = 0; i < K; ++i) {
            Eigen::Vector3f lab = result.centroids.row(i);
            float L = lab(0);
            float a = lab(1);
            float b = lab(2);
            float chroma = std::hypot(a, b);
            float hue = std::atan2(b, a);

            candidates[i] = { lab, result.counts(i), L, chroma, hue };
        }

        std::vector<ColorInfo> palette(K);

        // 1. Pick Background (color0): Highest pixel count with L* < 40
        auto bgIt = std::max_element(candidates.begin(), candidates.end(), [](const ColorInfo& x, const ColorInfo& y) {
            bool x_dark = x.L < 40.0f;
            bool y_dark = y.L < 40.0f;
            if (x_dark != y_dark) return !x_dark; // Prefer dark colors
            return x.count < y.count;             // Tie-breaker: higher pixel count
        });
        palette[0] = *bgIt;
        candidates.erase(bgIt);

        // 2. Pick Foreground (color7): Highest L* for readable text
        auto fgIt = std::max_element(candidates.begin(), candidates.end(), [](const ColorInfo& x, const ColorInfo& y) {
            return x.L < y.L;
        });
        palette[K - 1] = *fgIt; // Last slot (e.g. color7)
        candidates.erase(fgIt);

        // 3. Sort remaining accents (color1 through color6) by Hue Angle
        std::sort(candidates.begin(), candidates.end(), [](const ColorInfo& x, const ColorInfo& y) {
            return x.hue < y.hue;
        });

        for (size_t i = 0; i < candidates.size(); ++i) {
            palette[i + 1] = candidates[i];
        }

        // 4. Write back sorted colors into result matrix
        for (int i = 0; i < K; ++i) {
            result.centroids.row(i) = palette[i].lab;
            result.counts(i)        = palette[i].count;
        }
    }

    ColorRGB labToRgb(float L, float a, float b) {
        // 1. Lab -> XYZ (D65 Illuminant reference)
        float fy = (L + 16.0f) / 116.0f;
        float fx = fy + (a / 500.0f);
        float fz = fy - (b / 200.0f);

        auto f_inv = [](float t) {
            return (t > 0.2068966f) ? (t * t * t) : ((t - 16.0f / 116.0f) / 7.787f);
        };

        float X = 0.95047f * f_inv(fx);
        float Y = 1.00000f * f_inv(fy);
        float Z = 1.08883f * f_inv(fz);

        // 2. XYZ -> Linear RGB
        float r_lin =  3.2406f * X - 1.5372f * Y - 0.4986f * Z;
        float g_lin = -0.9689f * X + 1.8758f * Y + 0.0415f * Z;
        float b_lin =  0.0557f * X - 0.2040f * Y + 1.0570f * Z;

        // 3. Linear RGB -> sRGB (Gamma companding)
        auto gamma = [](float c) {
            c = std::clamp(c, 0.0f, 1.0f);
            return (c > 0.0031308f) ? (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f) : (12.92f * c);
        };

        int r = static_cast<int>(std::round(gamma(r_lin) * 255.0f));
        int g = static_cast<int>(std::round(gamma(g_lin) * 255.0f));
        int b_val = static_cast<int>(std::round(gamma(b_lin) * 255.0f));

        std::stringstream ss;
        ss << "#" << std::setfill('0') << std::setw(2) << std::hex << r
        << std::setfill('0') << std::setw(2) << std::hex << g
        << std::setfill('0') << std::setw(2) << std::hex << b_val;

        return {r, g, b_val, ss.str()};
    }

    void exportPalette(const Eigen::MatrixXf& centroids, 
                   const std::string& cssPath, 
                   const std::string& jsonPath) 
        {
        std::vector<ColorRGB> colors;
        for (int k = 0; k < centroids.rows(); ++k) {
            colors.push_back(labToRgb(centroids(k, 0), centroids(k, 1), centroids(k, 2)));
        }

        std::ofstream cssFile(cssPath);
        cssFile << "/* Generated by K-Means Palette Extractor */\n";
        cssFile << ":root {\n";
        for (size_t i = 0; i < colors.size(); ++i) {
            cssFile << "  --color" << i << ": " << colors[i].hex << "; /* rgb(" 
                    << colors[i].r << ", " << colors[i].g << ", " << colors[i].b << ") */\n";
        }
        cssFile << "}\n";

        std::ofstream jsonFile(jsonPath);
        jsonFile << "{\n";
        jsonFile << "  \"colors\": {\n";
        for (size_t i = 0; i < colors.size(); ++i) {
            jsonFile << "    \"color" << i << "\": \"" << colors[i].hex << "\""
                    << (i + 1 < colors.size() ? "," : "") << "\n";
        }
        jsonFile << "  }\n";
        jsonFile << "}\n";
        }
};