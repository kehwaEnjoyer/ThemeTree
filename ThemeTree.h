#include <iostream>
#include <vector>
#include <string>
#include <Eigen/Dense>
#include <random>
#include <omp.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class themeTree {

public:

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

        Eigen::VectorXi labels(N);
        Eigen::MatrixXf distances(N, K);

        for (int iter = 0; iter < maxIters; ++iter) {
            // 1. Distance matrix computation across centroids (Parallel)
            #pragma omp parallel for schedule(static)
            for (int k = 0; k < K; ++k) {
                distances.col(k) = (labMatrix.rowwise() - centroids.row(k)).rowwise().squaredNorm();
            }

            // 2. Minimum distance assignment per pixel (Parallel)
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < N; ++i) {
                distances.row(i).minCoeff(&labels(i));
            }

            // 3. Thread-safe centroid update using local accumulator buffers to avoid data races
            Eigen::MatrixXf newCentroids = Eigen::MatrixXf::Zero(K, channels);
            Eigen::VectorXf counts = Eigen::VectorXf::Zero(K);

            #pragma omp parallel
            {
                Eigen::MatrixXf localCentroids = Eigen::MatrixXf::Zero(K, channels);
                Eigen::VectorXf localCounts = Eigen::VectorXf::Zero(K);

                #pragma omp for schedule(static)
                for (int i = 0; i < N; ++i) {
                    int cluster = labels(i);
                    localCentroids.row(cluster) += labMatrix.row(i);
                    localCounts(cluster) += 1.0f;
                }

                #pragma omp critical
                {
                    newCentroids += localCentroids;
                    counts += localCounts;
                }
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

            if (shift < tol) {
                std::cout << "K-Means converged at iteration " << iter + 1 << "\n";
                break;
            }
        }

        this->result.centroids = centroids;
        this->result.labels = labels;
        return true;
    }
};