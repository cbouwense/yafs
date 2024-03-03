#include <stdlib.h>
#include <stdio.h>
#include <expat.h>

#define GUPPY_VERBOSE
#include "guppy.h"

#define BUFSIZE 4096

typedef struct Rectangle {
    float x;
    float y;
    float width;
    float height;
} Rectangle;

void start(void *data, const char *el, const char **attr) {
    printf("<%s>\n", el);
    Rectangle *rect = (Rectangle *)data;
    
    // TODO: this overwrites the one rectangle in main, would be
    // nice if it could append to a list of rectangles or something.
    if (strcmp(el, "object") == 0) {
        attr += 2;
        printf("%s: %s\n", attr[0], attr[1]);
        rect->x = (float)atof(attr[1]);
        attr += 2;
        rect->y = (float)atof(attr[1]);
        attr += 2;
        rect->width = (float)atof(attr[1]);
        attr += 2;
        rect->height = (float)atof(attr[1]);
    }
}

void end(void *data, const char *el) {
    printf("</%s>\n", el);
}

int main(void) {
    Rectangle rect = {0};
    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetElementHandler(parser, start, end);
    XML_SetUserData(parser, &rect);

    char *xml = gup_file_read_as_cstr("../resources/tilesets/map2.tmx");

    int done = 1;
    if (XML_Parse(parser, xml, strlen(xml), done) == XML_STATUS_ERROR) {
        printf("Error: %s\n", XML_ErrorString(XML_GetErrorCode(parser)));
    }

    printf("rect: {.x = %f, .y = %f, .width = %f, .height = %f }\n", rect.x, rect.y, rect.width, rect.height);

    XML_ParserFree(parser);
    free(xml);
    return 0;
}
