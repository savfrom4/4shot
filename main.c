#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <png.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* You can configure keys/mouse buttons here (see X11/keysymdef.h) */
#define KEY_UP XK_Up
#define KEY_DOWN XK_Down
#define KEY_LEFT XK_Left
#define KEY_RIGHT XK_Right
#define KEY_SAVE XK_Return
#define KEY_CANCEL XK_Escape
#define MOUSE_SELECT 1
#define MOUSE_SAVE 3

#define OUTLINE_PIXEL ((uint64_t)255 << 16)
#define DARKNESS 1.6

#define VERSION "1.02"

typedef struct
{
  char* buffer;
  size_t size;
} png_encoded_t;

enum
{
    OPTION_NONE,
    OPTION_FULLSCREEN = 1 << 1,
    OPTION_RECTANGLE = 1 << 2,
    OPTION_FILENAME = 1 << 3,
    OPTION_STDOUT = 1 << 4
} options = OPTION_NONE;

FILE* output_file = NULL;
png_encoded_t encoded_image;

bool parse_arguments(int argc, char** argv);
bool take_screnshoot(Display* display, XImage** output);
XImage* darken_screenshot(XImage* image);
bool save_screenshot(XImage* image, uint32_t xx, uint32_t yy, uint32_t width, uint32_t height);

void show_help(const char* program_name)
{
    printf(
        "4shot " VERSION " (https://github.com/savfrom4/4shot)\n\n"

        "Usage:\n"
        "%s [mode...] [output...]\n\n"

        "Mode:\n"
        "--full: Take a screenshot of fullscreen. (default)\n"
        "--rect: Select an area and take a screenshot.\n\n"

        "Output:\n"
        "--stdout: Output PNG to stdout. (default)\n"
        "--file [filename]: Output PNG to a file.\n\n",

        program_name
    );
}

int main(int argc, char** argv)
{
    if(!parse_arguments(argc, argv))
        return 1;

    Display* display = XOpenDisplay(NULL);
    if(!display)
    {
        puts("Failed to open X display.");
        return 1;
    }

    XImage* scr_image, *darkend_image;
    if(!take_screnshoot(display, &scr_image))
    {
        puts("Failed to take a screenshot");
        return 1;
    }

    if(!(darkend_image = darken_screenshot(scr_image)))
    {
        puts("Failed to create a screenshot clone");
        return 1;
    }

    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display,
        RootWindow(display, screen),
        0,
        0,
        scr_image->width,
        scr_image->height,
        0,
        0,
        0
    );
    GC gc = XCreateGC(display, window, 0, NULL);

    // setup backbuffer
    Pixmap back_buffer = XCreatePixmap(display, window, darkend_image->width, darkend_image->height, 24);
    XPutImage(display, back_buffer, gc, darkend_image, 0, 0, 0, 0, darkend_image->width, darkend_image->height);
    XSetWindowBackgroundPixmap(display, window, back_buffer);

    Atom atoms[2] = { XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False), None };
    XChangeProperty(display, window, XInternAtom(display, "_NET_WM_STATE", False), 4, 32, PropModeReplace, (unsigned char*)atoms, 1);
    XSelectInput(display, window,
        ExposureMask | KeyPressMask | ButtonPress | ButtonReleaseMask |
        PointerMotionMask | Button1MotionMask | PropertyChangeMask
    );

    // in fullscreen mode we dont need to show the window
    if(options & OPTION_FULLSCREEN)
        return !save_screenshot(scr_image, 0, 0, scr_image->width, scr_image->height);

    XMapWindow(display, window);

    uint32_t start_x = 0, start_y = 0;
    uint32_t end_x = 0, end_y = 0;
    bool pressed = false, repaint = false;

    while(1)
    {
        XEvent ev;
        while(XPending(display))
        {
            XNextEvent(display, &ev);
            switch(ev.type)
            {
                case MotionNotify:
                {
                    if(!pressed)
                        break;

                    end_x = ev.xmotion.x;
                    end_y = ev.xmotion.y;
                    repaint = true;
                    break;
                }

                case ButtonPress:
                {
                    if(ev.xbutton.button != MOUSE_SELECT)
                        break;

                    pressed = true;
                    start_x = end_x = ev.xbutton.x;
                    start_y = end_y = ev.xbutton.y;
                    repaint = true;
                    break;
                }

                case ButtonRelease:
                {
                    if(ev.xbutton.button == MOUSE_SAVE)
                    {
                        if(!save_screenshot(scr_image, start_x, start_y, end_x-start_x, end_y-start_y))
                        {
                            puts("Failed to save output image");
                            return 1;
                        }
                        goto quit;
                    }

                    pressed = false;
                    break;
                }

                case KeyPress:
                {
                    switch(XLookupKeysym(&ev.xkey, 0))
                    {
                        case KEY_LEFT:
                            start_x--;
                            repaint = true;
                            break;

                        case KEY_RIGHT:
                            end_x++;
                            repaint = true;
                            break;

                        case KEY_UP:
                            start_y--;
                            repaint = true;
                            break;

                        case KEY_DOWN:
                            end_y++;
                            repaint = true;
                            break;

                        case KEY_CANCEL:
                            goto quit;

                        case KEY_SAVE:
                        {
                            if(!save_screenshot(scr_image, start_x, start_y, end_x-start_x, end_y-start_y))
                            {
                                puts("Failed to save output image");
                                return 1;
                            }
                            goto quit;
                        }
                    }
                    break;
                }
            }

            if(start_x >= end_x)
                end_x = start_x + 1;

            if(start_y >= end_y)
                end_y = start_y + 1;
        }

        if(repaint)
        {
            XPutImage(display, back_buffer, gc, darkend_image, 0, 0, 0, 0, darkend_image->width, darkend_image->height);
            XPutImage(display, back_buffer, gc, scr_image, start_x, start_y, start_x, start_y, end_x - start_x, end_y - start_y);
            XSetForeground(display, gc, OUTLINE_PIXEL);
            XDrawRectangle(display, back_buffer, gc, start_x, start_y, end_x - start_x, end_y - start_y);

            char dim[32];
            snprintf(dim, 32, "(%d, %d) (%d, %d)", start_x, start_y, end_x - start_x, end_y - start_y);

            // Draw dimensions & Shadow
            XSetForeground(display, gc, BlackPixel(display, screen));
            XDrawString(display, back_buffer, gc, start_x+1, start_y - 10+1, dim, strlen(dim));
            XSetForeground(display, gc, WhitePixel(display, screen));
            XDrawString(display, back_buffer, gc, start_x, start_y - 10, dim, strlen(dim));

            XCopyArea(display,
                back_buffer,
                window,
                gc,
                0,
                0,
                darkend_image->width,
                darkend_image->height,
                0,
                0
            );

            repaint = false;
        }

        usleep(1000);
    }

quit:
    XUnmapWindow(display, window);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}

bool parse_arguments(int argc, char** argv)
{
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--rect") == 0)
            options |= OPTION_RECTANGLE;
        else if(strcmp(argv[i], "--full") == 0)
            options |= OPTION_FULLSCREEN;
        else if(strcmp(argv[i], "--stdout") == 0)
            options |= OPTION_STDOUT;
        else if(strcmp(argv[i], "--file") == 0)
        {
            options |= OPTION_FILENAME;
            if(++i >= argc)
            {
                show_help(argv[0]);
                return false;
            }

            output_file = fopen(argv[i], "wb");
            if(!output_file)
            {
                perror("Failed to open the file");
                return false;
            }
        }
        else
        {
            show_help(argv[0]);
            return false;
        }
    }

    // defaults

    if(!(options & OPTION_STDOUT) && !(options & OPTION_FILENAME))
        options |= OPTION_STDOUT;

    if(!(options & OPTION_FULLSCREEN) && !(options & OPTION_RECTANGLE))
        options |= OPTION_FULLSCREEN;

    return true;
}

bool take_screnshoot(Display* display, XImage** output)
{
    Window root = DefaultRootWindow(display);
    XWindowAttributes attr;

    XGetWindowAttributes(display, root, &attr);
    (*output) = XGetImage(display, root, 0, 0, attr.width, attr.height, AllPlanes, ZPixmap);
    return (*output);
}


XImage* darken_screenshot(XImage* image)
{
    XImage* clone = XSubImage(image, 0, 0, image->width, image->height);

    for(int i = 0; i < image->width; i++)
    {
        for(int j = 0; j < image->height; j++)
        {
            unsigned long pixel = XGetPixel(clone, i, j);
            uint8_t a = (pixel >> 16);
            uint8_t b = (pixel >> 8);
            uint8_t c = (pixel);

            a /= DARKNESS;
            b /= DARKNESS;
            c /= DARKNESS;

            XPutPixel(clone, i, j, c | (b << 8) | (a << 16));
        }
    }

    return clone;
}

void image_png_write(png_structp png_ptr, png_bytep data, png_size_t length)
{
    png_encoded_t* file = (png_encoded_t*)png_get_io_ptr(png_ptr);
    size_t nsize = file->size + length;

    if(file->buffer)
        file->buffer = realloc(file->buffer, nsize);
    else
        file->buffer = malloc(nsize);

    if(!file->buffer)
    {
        png_error(png_ptr, "Write Error");
        return;
    }

    memcpy(file->buffer + file->size, data, length);
    file->size += length;
}

bool save_screenshot(XImage* image, uint32_t xx, uint32_t yy, uint32_t width, uint32_t height)
{
    encoded_image.buffer = NULL;
    encoded_image.size = 0;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        return false;

    png_infop info = png_create_info_struct(png);
    if (!info)
        return false;

    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_write_struct(&png, &info);
        free(encoded_image.buffer);
        return false;
    }

    png_set_write_fn(png, &encoded_image, image_png_write, NULL);
    png_set_IHDR(png,
        info,
        width,
        height,
        8,
        PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    uint8_t** rows = (uint8_t**)malloc(height * sizeof(uint8_t*));
    for(uint32_t y = 0; y < height; y++)
    {
        uint8_t* row = malloc(width * 3 * sizeof(uint8_t));
        for(uint32_t x = 0; x < width * 3; x += 3)
        {
            unsigned long pixel = XGetPixel(image, xx + x/3, yy + y);
            row[x] = (uint8_t)(pixel >> 16);
            row[x + 1] = (uint8_t)(pixel >> 8);
            row[x + 2] = (uint8_t)(pixel);
        }

        rows[y] = row;
    }

    png_write_image(png, rows);
    png_write_end(png, NULL);

    for(uint32_t i = 0; i < height; i++)
        free(rows[i]);

    free(rows);
    png_destroy_write_struct(&png, &info);

    if(options & OPTION_FILENAME)
    {
        fwrite(encoded_image.buffer, encoded_image.size, 1, output_file);
        fclose(output_file);
    }

    if(options & OPTION_STDOUT)
        fwrite(encoded_image.buffer, encoded_image.size, 1, stdout);

    free(encoded_image.buffer);
    return true;
}
