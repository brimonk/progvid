// Brian Chrzanowski
//
// My small program to help me create tiny programming videos.
// This program dumps out PPM images to stdout. Ideally, the caller will use `ffmpeg` to stitch
// these images together. Something like this probably does it:
//
//   ./progvid ./src/main.c ./src/*.c | ffmpeg -i pipe: output.mp4
//
// To update the resolution of the images produced, update the two macros:
//
//   #define VIDEO_RESOLUTION_W
//   #define VIDEO_RESOLUTION_H
//
// To change the size of the characters in the output video, change these macros:
//
//   #define CHARACTER_W
//   #define CHARACTER_H
//
// To fit an 80 x 24 terminal

#define COMMON_IMPLEMENTATION
#include "common.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define VIDEO_RESOLUTION_W     640
#define VIDEO_RESOLUTION_H     480
#define CHARACTER_W              9
#define CHARACTER_H             16

#define USAGE "%s <source files>\n"

char *KEYWORDS_GRN[] = {
     "i8",  "u8",
    "i16", "u16",
    "i32", "u32",
    "i64", "u64",

    "struct",
    "union",

    "unsigned",
    "char",
    "short",
    "int",
    "long",
    "float",
    "double"
};

char *KEYWORDS_RED[] = {
    "stdout",
    "stdin",
    "stderr",
};

char *KEYWORDS_ORG[] = {
    "if", "for", "do", "while", "return", "switch", "case", "goto"
};

typedef struct FontTexture {
} FontTexture;

typedef struct PPMPixel {
    u8 r;
    u8 g;
    u8 b;
} PPMPixel;

typedef struct Image {
    u32 width;
    u32 height;
    PPMPixel *pixels;
} Image;

// CreateImage: creates an image
Image *CreateImage(u32 width, u32 height);

// DestroyImage: destroys an image
void DestroyImage(Image *image);

// WriteImageToSTDOUT: writes the image as-is to STDOUT
void WriteImageToSTDOUT(Image *image);

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, USAGE, argv[0]);
        exit(1);
    }

    Image *img = CreateImage(VIDEO_RESOLUTION_W, VIDEO_RESOLUTION_H);
    for (i32 i = 0; i < 6000; i++) {
        WriteImageToSTDOUT(img);
    }
    DestroyImage(img);
}

void WriteImageToSTDOUT(Image *image)
{
    printf("P6\n%d %d\n255\n", image->width, image->height);
    size_t written = fwrite(
        image->pixels,
        sizeof(PPMPixel),
        (image->width * image->height),
        stdout
    );

    assert(written == (image->width * image->height));
}

Image *CreateImage(u32 width, u32 height)
{
    size_t size = sizeof(Image) + (sizeof(PPMPixel) * width * height);
    Image *img = calloc(1, size);
    assert(img != NULL);

    img->width  = width;
    img->height = height;
    img->pixels = (PPMPixel *)(((u8 *)img) + sizeof(*img));

    return img;
}

void DestroyImage(Image *image)
{
    free(image);
}
