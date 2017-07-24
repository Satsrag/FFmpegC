//
// Created by Saqrag Borgn on 18/07/2017.
//
#include "Compress.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    const char *inFile = "/Users/saqrag/Downloads/test.mp4";
    time_t t;
    time(&t);
    char *outFile;
    outFile = malloc(500);
    sprintf(outFile, "/Users/saqrag/Downloads/%ld.mp4", t);
    compress(inFile, outFile);
    time_t d;
    time(&d);
    printf("cost time: %ld", d - t);
}

