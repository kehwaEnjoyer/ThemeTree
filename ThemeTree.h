#include <iostream>
#include <vector>
#include <string>
#include <Eigen/Dense>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class themeTree{

    public:

    struct image{
        int width =0 ;
        int height=0;
        int channels =0;
        std::vector<uint8_t> pixels;

        uint8_t& at(int x, int y, int c){
            return pixels[(y*width+x) * channels +c];
        };

        const uint8_t& at(int x, int y, int c) const{
            return pixels[(y*width+x) * channels +c];
        };
    };

    image wallpaper;
    Eigen::MatrixXf imgMatrix;
    Eigen::MatrixXf labMatrix;
    themeTree(){
    }

    bool loadImage(const std::string filePath){
        int w,h,oriChannels;

        unsigned char* data = stbi_load(filePath.c_str(), &w, &h, &oriChannels, 3);
        
        if(!data){
            std::cerr<<"image error loading image at" << filePath<<"\n";
            return false;
        }

        wallpaper.width = w;
        wallpaper.height = h;
        wallpaper.channels=3;

        wallpaper.pixels.assign(data, data+(w*h*3));

        stbi_image_free(data);
        std::cout<<"image loaded succesfully. \n";
        return true;
    }

    bool ConvertTmatrix(){
        const int N = wallpaper.width * wallpaper.height;
        imgMatrix.resize(N, 3);
        for(int i=0; i<N; ++i){
            imgMatrix(i,0) = wallpaper.pixels[i*3+0]/255.0f;
            imgMatrix(i,1) = wallpaper.pixels[i*3+1]/255.0f;
            imgMatrix(i,2) = wallpaper.pixels[i*3+2]/255.0f;
        }

        return true;
    }

    bool rgbTcielab(){
        const int N = imgMatrix.rows();
        labMatrix.resize(N, 3);

        auto inverseGamma=[](float c) -> float {
            return (c<=0.04045f) ? (c/12.92f) : std::pow((c+0.055f)/1.055f, 2.4f);
        };

        Eigen::Matrix3f rgbTxyzMat;
        rgbTxyzMat << 0.4124564f, 0.3575761f, 0.1804375f,
                      0.2126729f, 0.7151522f, 0.0721750f,
                      0.0193339f, 0.1191920f, 0.9503041f;

        const float Xn = 0.95047f;
        const float Yn = 1.00000f;
        const float Zn = 1.08883f;

        const float delta = 6.0f / 29.0f;
        const float delta_cubed = delta * delta * delta; // (6/29)^3
        const float three_delta_sq = 3.0f * delta * delta;

        auto f_lab = [&](float t) -> float {
        return (t > delta_cubed) ? std::cbrt(t) : (t / three_delta_sq + 4.0f / 29.0f);
        };

        for (int i = 0; i < N; ++i) {
        // Step A: Linearize RGB channels
        Eigen::Vector3f linear_rgb(
            inverseGamma(imgMatrix(i, 0)),
            inverseGamma(imgMatrix(i, 1)),
            inverseGamma(imgMatrix(i, 2))
        );

        // Step B: Linear RGB to XYZ
        Eigen::Vector3f xyz = rgbTxyzMat * linear_rgb;

        // Step C: XYZ to CIELAB
        float fx = f_lab(xyz(0) / Xn);
        float fy = f_lab(xyz(1) / Yn);
        float fz = f_lab(xyz(2) / Zn);

        labMatrix(i, 0) = 116.0f * fy - 16.0f; // L*
        labMatrix(i, 1) = 500.0f * (fx - fy);   // a*
        labMatrix(i, 2) = 200.0f * (fy - fz);   // b*

    }
    return true;
    }
};

