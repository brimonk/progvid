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

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define VIDEO_RESOLUTION_W     640
#define VIDEO_RESOLUTION_H     480
#define CHARACTER_W              9
#define CHARACTER_H             16
#define TICKS_PER_CHARACTER      2
#define TAB_WIDTH                4
#define DEFAULT_FONT           "./fonts/TX-02-JMJPQ031/BerkeleyMono-Regular.ttf"
#define FALLBACK_FONT          "/usr/share/fonts/truetype/freefont/FreeMono.ttf"

#define FONT_ATLAS_PIXELS_W   2048
#define FONT_ATLAS_PIXELS_H   2048

#define TEXT_COLS (VIDEO_RESOLUTION_W / CHARACTER_W)
#define TEXT_ROWS (VIDEO_RESOLUTION_H / CHARACTER_H)

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

typedef struct Pixel {
    u8 r;
    u8 g;
    u8 b;
} Pixel;

typedef struct Image {
    u32 width;
    u32 height;
    u32 pitch;
    Pixel *pixels;
} Image;

typedef struct FilePlayback {
    FILE *fp;
    size_t file_size;
    u32 row;
    u32 col;
    u64 tick;
} FilePlayback;

typedef struct Font {
    u32 width;
    u32 height;
    u32 pitch;
    u8 *pixels;
    void *chars;
    f32 pixel_height;
    i32 atlas_w;
    i32 atlas_h;
    i32 first_char;
    i32 num_chars;
    f32 ascent;
    f32 descent;
    f32 line_gap;
} Font;

typedef struct Rect {
    i32 x;
    i32 y;
    i32 w;
    i32 h;
} Rect;

// ReadFile: just reads a file using stdlib
char *ReadFile(char *path);

// CreateImage: creates an image
Image *CreateImage(u32 width, u32 height);

// DestroyImage: destroys an image
void DestroyImage(Image *image);

// CreateFont: loads a font, packs it into our atlas, and prepares for rendering to video
Font *CreateFont();

// DestroyFont: destroys the font object
void DestroyFont();

// WriteImageToSTDOUT: writes the image as-is to STDOUT
void WriteImageToSTDOUT(Image *image);

// ClearImage: clears the image
void ClearImage(Image *img);

// IsFilePlaybackDone: returns true if the file playback is done
int IsFilePlaybackDone(FilePlayback *pb);

// ProcessPlayback: runs a single 'tick' of playback (loosely assumed output is 60fps)
void ProcessPlayback(FilePlayback *pb, Image *img, Font *font);

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, USAGE, argv[0]);
        exit(1);
    }

    Font *font = CreateFont();
    Image *img = CreateImage(VIDEO_RESOLUTION_W, VIDEO_RESOLUTION_H);

    for (i32 i = 1; i < argc; i++) {
        // TODO We should check if the files exist before we even get here.
        FILE *fp = fopen(argv[i], "rb");
        if (fp == NULL) {
            fprintf(stderr, "SKIPPING %s - doesn't exist?\n", argv[i]);
            continue;
        }

        fseek(fp, 0, SEEK_END);
        size_t file_size = ftell(fp);
        rewind(fp);

        FilePlayback pb = {
            .fp = fp,
            .file_size = file_size,
            .row  = 0,
            .col  = 0,
            .tick = 0
        };

        ClearImage(img);

        while (!IsFilePlaybackDone(&pb)) {
            ProcessPlayback(&pb, img, font);
            WriteImageToSTDOUT(img);
        }

        fclose(fp);
    }

    DestroyImage(img);
    DestroyFont(font);
}

int IsFilePlaybackDone(FilePlayback *pb)
{
    return pb->tick >= pb->file_size * TICKS_PER_CHARACTER;
}

void BlitBlendMod(Image *img, Font *font, Rect dst, Rect src, Pixel colormod)
{
    i32 w = src.w;
    i32 h = src.h;

    // Clip against the left/top of 'dst', shifting 'src' to match.
    if (dst.x < 0) { src.x -= dst.x; w += dst.x; dst.x = 0; }
    if (dst.y < 0) { src.y -= dst.y; h += dst.y; dst.y = 0; }

    // Clip against the left/top of 'src'.
    if (src.x < 0) { dst.x -= src.x; w += src.x; src.x = 0; }
    if (src.y < 0) { dst.y -= src.y; h += src.y; src.y = 0; }

    // Clip against the right/bottom of both.
    if (dst.x + w > (i32)img->width)  w = (i32)img->width  - dst.x;
    if (dst.y + h > (i32)img->height) h = (i32)img->height - dst.y;

    if (src.x + w > font->atlas_w) w = font->atlas_w - src.x;
    if (src.y + h > font->atlas_h) h = font->atlas_h - src.y;

    if (w <= 0 || h <= 0) {
        return;
    }

    for (i32 row = 0; row < h; row++) {
        Pixel *dp = img->pixels  + (dst.y + row) * img->pitch  + dst.x;
        u8 *sp    = font->pixels + (src.y + row) * font->pitch + src.x;

        for (i32 i = 0; i < w; i++) {
            u32 a = sp[i];      // glyph coverage

            if (a == 0) {
                continue;
            }

            if (a == 255) {
                dp[i].r = colormod.r;
                dp[i].g = colormod.g;
                dp[i].b = colormod.b;
                continue;
            }

            u32 ia = 255 - a;

            // Rounded "over" blend: (src * a + dst * (255 - a)) / 255
            dp[i].r = (u8)((colormod.r * a + dp[i].r * ia + 127) / 255);
            dp[i].g = (u8)((colormod.g * a + dp[i].g * ia + 127) / 255);
            dp[i].b = (u8)((colormod.b * a + dp[i].b * ia + 127) / 255);
        }
    }
}

void ScrollImageUp(Image *img, u32 pixels)
{
    if (pixels >= img->height) {
        ClearImage(img);
        return;
    }

    size_t keep = (size_t)(img->height - pixels) * img->pitch;

    memmove(img->pixels, img->pixels + (size_t)pixels * img->pitch, keep * sizeof(Pixel));
    memset(img->pixels + keep, 0, (size_t)pixels * img->pitch * sizeof(Pixel));
}

void WriteCHARToImage(Image *img, Font *font, u32 codepoint, u32 row, u32 col, u32 color)
{
    Pixel colormod = {
        .r = (color & 0xff),
        .g = ((color >>  8) & 0xff),
        .b = ((color >> 16) & 0xff),
    };

    if ((i32)codepoint < font->first_char || (i32)codepoint >= font->first_char + font->num_chars) {
        codepoint = '?';
    }

    stbtt_packedchar *pc = &((stbtt_packedchar *)font->chars)[codepoint - font->first_char];

    Rect src = { pc->x0, pc->y0, pc->x1 - pc->x0, pc->y1 - pc->y0 };

    f32 pen_x = (f32)(col * CHARACTER_W);
    f32 pen_y = (f32)(row * CHARACTER_H) + roundf(font->ascent);   // baseline

    Rect dst = {
        (i32)floorf(pen_x + pc->xoff + 0.5f),
        (i32)floorf(pen_y + pc->yoff + 0.5f),
        src.w,
        src.h,
    };

    BlitBlendMod(img, font, dst, src, colormod);
}

void ProcessPlayback(FilePlayback *pb, Image *img, Font *font)
{
    assert(pb);
    assert(img);
    assert(font);

    if (pb->tick == 0 || pb->tick % TICKS_PER_CHARACTER != 0) {
        pb->tick++;
        return;
    }

SPEED_THROUGH_SPACES:
    // We're just going to write one character at a time to the output.
    u32 codepoint = fgetc(pb->fp);

    // TODO HANDLE ALL CONTROL CHARACTERS THAT MIGHT BE IN THE FILE

    switch (codepoint) {
        case '\n': {
            pb->col = 0;
            pb->row++;
            break;
        }

        case ' ': {
            pb->col++;
            pb->tick++;
            goto SPEED_THROUGH_SPACES;
            break;
        }

        case '\r': { // UNIX HANDLING
            pb->col = 0;
            break;
        }

        case '\t': {
            pb->col += TAB_WIDTH - (pb->col % TAB_WIDTH);
            break;
        }

        default: {
            if (pb->col >= TEXT_COLS) { // wrap
                pb->col = 0;
                pb->row++;
            }

            if (pb->row >= TEXT_ROWS) {
                ScrollImageUp(img, CHARACTER_H);
                pb->row = TEXT_ROWS - 1;
            }

            WriteCHARToImage(img, font, codepoint, pb->row, pb->col, 0xffffff);
            pb->col++;

            break;
        }
    }

    if (pb->row >= TEXT_ROWS) {
        ScrollImageUp(img, CHARACTER_H * (pb->row - (TEXT_ROWS - 1)));
        pb->row = TEXT_ROWS - 1;
    }

    pb->tick++;
}

void WriteImageToSTDOUT(Image *image)
{
    printf("P6\n%d %d\n255\n", image->width, image->height);

    size_t written = fwrite(
        image->pixels,
        sizeof(Pixel),
        (image->width * image->height),
        stdout
    );

    assert(written == (image->width * image->height));
}

void ClearImage(Image *img)
{
    memset(img->pixels, 0, img->width * img->height * sizeof(*img->pixels));
}

Image *CreateImage(u32 width, u32 height)
{
    size_t size = sizeof(Image) + (sizeof(Pixel) * width * height);
    Image *img = calloc(1, size);
    assert(img != NULL);

    img->width  = width;
    img->height = height;
    img->pitch  = width;
    img->pixels = (Pixel *)(((u8 *)img) + sizeof(*img));

    return img;
}

void DestroyImage(Image *image)
{
    free(image);
}

Font *CreateFont()
{
    char *font_path = NULL;

    autofree u8 *ttf = (u8 *)ReadFile(DEFAULT_FONT);
    if (ttf == NULL) {
        ttf = (u8 *)ReadFile(FALLBACK_FONT);
        if (ttf == NULL) {
            fprintf(stderr, "No fonts available (default and fallback) - BAILING\n");
            exit(1);
        } else {
            font_path = FALLBACK_FONT;
        }
    } else {
        font_path = DEFAULT_FONT;
    }

    Font *font = calloc(1, sizeof(*font));
    assert(font);

    font->atlas_w = FONT_ATLAS_PIXELS_W;
    font->atlas_h = FONT_ATLAS_PIXELS_H;
    font->width   = font->atlas_w;
    font->height  = font->atlas_h;
    font->pitch   = font->atlas_w;

    // NOTE ASCII is from 32 .. 126. If we ever want UTF-8 characters, we'll need to do something
    // interesting, where we have dynamically rendering font atlas stuff??
    // 
    // Dunno.
    //
    // TBD.

    font->pixel_height = CHARACTER_H;
    font->first_char = ' ';
    font->num_chars = '~' - ' ' + 1;

    font->chars = calloc(font->num_chars, sizeof(stbtt_packedchar));
    assert(font->chars);

    font->pixels = calloc(font->atlas_w * font->atlas_h, sizeof(*font->pixels));
    assert(font->pixels);

    stbtt_pack_context pc;

    stbtt_PackBegin(&pc, font->pixels, font->atlas_w, font->atlas_h, 0, 1, NULL);
    // stbtt_PackSetOversampling(&pc, 2, 2);
    i32 ok = stbtt_PackFontRange(&pc, ttf, 0, font->pixel_height, font->first_char, font->num_chars, font->chars);
    stbtt_PackEnd(&pc);

    if (!ok) {
        fprintf(stderr, "Failed to load our font atlas at current sizes for '%s'. Increase limits!\n", font_path);
        exit(1);
    }

    stbtt_fontinfo info;
    stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0));
    f32 scale = stbtt_ScaleForPixelHeight(&info, font->pixel_height);

    i32 a, d, g;

    stbtt_GetFontVMetrics(&info, &a, &d, &g);

    font->ascent = a * scale;
    font->descent = d * scale;
    font->line_gap = g * scale;

    return font;
}

void DestroyFont(Font *font)
{
    assert(font);
    assert(font->pixels);
    assert(font->chars);

    free(font->pixels);
    free(font->chars);
    free(font);
}

char *ReadFile(char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    i64 size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *bytes = calloc(1, size + 1);

    fread(bytes, 1, size, fp);

    fclose(fp);

    return bytes;
}
