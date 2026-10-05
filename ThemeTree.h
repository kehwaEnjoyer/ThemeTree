#include <iostream>
#include <vector>
#include <string>
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

};

