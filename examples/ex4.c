#!/usr/local/bin/swirl -run -L/usr/X11R6/lib -lX11
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>

int main(int argc, char **argv)
{
    Display *display;
    Screen *screen;

    display = XOpenDisplay("");
    if (!display) {
        fprintf(stderr, "could not open X11 display\n");
        exit(1);
    }
    printf("X11 display opened\n");
    screen = XScreenOfDisplay(display, 0);
    printf("width = %d\nheight = %d\ndepth = %d\n",
           screen->width,
           screen->height,
           screen->root_depth);
    XCloseDisplay(display);
    return 0;
}
