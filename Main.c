//
// Created by Saqrag Borgn on 18/07/2017.
//
#include "Compress.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    const char *inFile = "/Users/saqrag/Downloads/258.mp4";
    time_t t;
    time(&t);
    char *outFile;
    outFile = malloc(500);
    sprintf(outFile, "/Users/saqrag/Downloads/%ld.mp4", t);
    compress(inFile, outFile, 800000, 64000, 854, 480, 30);
    time_t d;
    time(&d);
    printf("cost time: %ld", d - t);
}

