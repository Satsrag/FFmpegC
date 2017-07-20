//
// Created by Saqrag Borgn on 18/07/2017.
//

#ifndef TRANSCODE_LOG_H
#define TRANSCODE_LOG_H
#define LOGD(format, ...) printf(format"\n",## __VA_ARGS__)
#define LOGE(format, ...) printf(format"\n",## __VA_ARGS__)
#endif //TRANSCODE_LOG_H
