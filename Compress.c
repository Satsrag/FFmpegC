//
// Created by Saqrag Borgn on 05/07/2017.
//
#include "Compress.h"
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include "libavformat/avformat.h"
#include "Log.h"

int ret = -1;
AVFormatContext *mInFormatContext, *mOutFormatContext;
AVCodecContext *mInVideoCodecContext, *mOutVideoCodecContext, *mInAudioCodecContext, *mOutAudioCodecContext;
AVPacket *mInPacket, *mOutPacket;
AVFrame *mInFrame, *mOutVideoFrame, *mOutAudioFrame;
int mInVideoIndex = -1, mInAudioIndex = -1, mOutVideoIndex = -1, mOutAudioIndex = -1;
struct SwsContext *mSwsContext;
struct SwrContext *mSwrContext;

static int initInput(const char *inFile) {

    //1. 获取AVFormatContext
    mInFormatContext = avformat_alloc_context();
    ret = avformat_open_input(&mInFormatContext, inFile, NULL, NULL);
    if (ret != 0) {
        LOGE("open input file: %s error!!", inFile);
        return -1;
    }

    //2. 查找AVStream信息，并填充到AVFormatContext
    ret = avformat_find_stream_info(mInFormatContext, NULL);
    if (ret < 0) {
        LOGE("find input AVStream error");
        return -2;
    }

    //3. 获取 StreamIndex
    for (int i = 0; i < mInFormatContext->nb_streams; ++i) {
        enum AVMediaType type = mInFormatContext->streams[i]->codecpar->codec_type;
        switch (type) {
            case AVMEDIA_TYPE_VIDEO:
                mInVideoIndex = i;
                mInVideoCodecContext = mInFormatContext->streams[mInVideoIndex]->codec;
                AVCodec *videoCodec = avcodec_find_decoder(
                        mInFormatContext->streams[i]->codecpar->codec_id);
                avcodec_open2(mInVideoCodecContext, videoCodec, NULL);
                break;
            case AVMEDIA_TYPE_AUDIO:
                mInAudioIndex = i;
                mInAudioCodecContext = mInFormatContext->streams[i]->codec;
                AVCodec *audioCodec = avcodec_find_decoder(
                        mInFormatContext->streams[i]->codecpar->codec_id);
                avcodec_open2(mInAudioCodecContext, audioCodec, NULL);
                break;
            default:
                break;
        }
    }

    if (mInVideoIndex == -1) {
        LOGD("not found video Stream");
        return -3;
    }
    if (mInAudioIndex == -1) {
        LOGD("not found audio Stream");
        return -4;
    }
    return 0;
}

static int initOutput(const char *outFile) {

    //1. 创建AVFormat
    ret = avformat_alloc_output_context2(&mOutFormatContext, NULL, NULL, outFile);
    if (ret < 0) {
        LOGE("alloc output format context error!");
        return -100;
    }

    //2. 创建AVIOContext
    ret = avio_open(&mOutFormatContext->pb, outFile, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("open output AVIOContext error");
        return -101;
    }

    AVStream *outStream;
    for (int i = 0; i < 2; ++i) {
        //3. 创建video AVStream
        outStream = avformat_new_stream(mOutFormatContext, NULL);
        outStream->id = mOutFormatContext->nb_streams - 1;

        if (i == 0) {
            mOutVideoIndex = outStream->id;

            //4. 配置AVCodecContext
            mOutVideoCodecContext = outStream->codec;
            mOutVideoCodecContext->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
            mOutVideoCodecContext->bit_rate = 1000000;
            mOutVideoCodecContext->gop_size = 250;
            mOutVideoCodecContext->thread_count = 16;
            mOutVideoCodecContext->time_base.num = mInVideoCodecContext->time_base.num;
            mOutVideoCodecContext->time_base.den = mInVideoCodecContext->time_base.den;
            mOutVideoCodecContext->max_b_frames = 3;
            mOutVideoCodecContext->codec_id = AV_CODEC_ID_H264;
            mOutVideoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
            mOutVideoCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
            mOutVideoCodecContext->width = 854;
            mOutVideoCodecContext->height = 480;

            //H264
            mOutVideoCodecContext->me_range = 16;
            mOutVideoCodecContext->max_qdiff = 4;
            mOutVideoCodecContext->qcompress = 0.6;
            mOutVideoCodecContext->qmin = 10;
            mOutVideoCodecContext->qmax = 51;

            // Set Option
            AVDictionary *param = 0;
            //H.264
            if (mOutVideoCodecContext->codec_id == AV_CODEC_ID_H264) {
                av_opt_set(mOutVideoCodecContext->priv_data, "preset", "ultrafast", 0);
                av_dict_set(&param, "profile", "baseline", 0);
            }

            //Show some Information
            av_dump_format(mOutFormatContext, 0, outFile, 1);

            //5. 配置AVCodec
            AVCodec *outCodec = NULL;
            outCodec = avcodec_find_encoder(mOutVideoCodecContext->codec_id);
            if (outCodec == NULL) {
                LOGE("find AVCodec error!");
                return -103;
            }
            ret = avcodec_open2(mOutVideoCodecContext, outCodec, &param);
            if (ret != 0) {
                LOGE("open codec error");
                return -104;
            }
        } else {
            mOutAudioIndex = outStream->id;

            //1. find encoder
            AVCodec *outCodec = NULL;
            outCodec = avcodec_find_encoder(mInAudioCodecContext->codec_id);
            if (outCodec == NULL) {
                LOGE("find audio AVCodec error!");
                return -105;
            }

            //2. setting encoder context
            mOutAudioCodecContext = outStream->codec;
            mOutAudioCodecContext->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
            mOutAudioCodecContext->codec_id = mInAudioCodecContext->codec_id;
            mOutAudioCodecContext->codec_type = mInAudioCodecContext->codec_type;
            mOutAudioCodecContext->frame_size = mInAudioCodecContext->frame_size;
            mOutAudioCodecContext->channel_layout = mInAudioCodecContext->channel_layout;
            if (outCodec->channel_layouts) {
                mOutAudioCodecContext->channel_layout = outCodec->channel_layouts[0];
                for (int a = 0; outCodec->channel_layouts[a]; a++) {
                    if (outCodec->channel_layouts[a] == mInAudioCodecContext->channel_layout) {
                        mOutAudioCodecContext->channel_layout = mInAudioCodecContext->channel_layout;
                    }
                }
            }
            mOutAudioCodecContext->channels = av_get_channel_layout_nb_channels(
                    mOutAudioCodecContext->channel_layout
            );
            mOutAudioCodecContext->sample_fmt = mInAudioCodecContext->sample_fmt;
            if (outCodec->sample_fmts) {
                mOutAudioCodecContext->sample_fmt = outCodec->sample_fmts[0];
                for (int b = 0; outCodec->sample_fmts[b]; b++) {
                    if (outCodec->sample_fmts[b] == mInAudioCodecContext->sample_fmt) {
                        mOutAudioCodecContext->sample_fmt = mInAudioCodecContext->sample_fmt;
                    }
                }
            }
            LOGD("channel layout: %lld", mOutAudioCodecContext->channel_layout);
            LOGD("channels :%d", mOutAudioCodecContext->channels);
            LOGD("sample_fmts: %d", mOutAudioCodecContext->sample_fmt);
            mOutAudioCodecContext->sample_rate = mInAudioCodecContext->sample_rate;
            mOutAudioCodecContext->bit_rate = 64000;

//          Show some Information
            av_dump_format(mOutFormatContext, 0, outFile, 1);

            ret = avcodec_open2(mOutAudioCodecContext, outCodec, NULL);
            if (ret != 0) {
                LOGE("open audio codec error, CODE: %d", ret);
                return -106;
            }
        }
    }
    return 0;
}

static int initAudioFrame() {

    mOutAudioFrame = av_frame_alloc();
    if (mOutAudioFrame == NULL) {
        LOGE("alloc out audio frame error");
        return -500;
    }

    if (ret < 0) {
        LOGE("alloc out audio frame sample error, CODE: %d", ret);
        return -501;
    }

    mSwrContext = swr_alloc_set_opts(
            NULL,
            mOutAudioCodecContext->channel_layout,
            mOutAudioCodecContext->sample_fmt,
            mOutAudioCodecContext->sample_rate,
            mInAudioCodecContext->channel_layout,
            mInAudioCodecContext->sample_fmt,
            mInAudioCodecContext->sample_rate,
            0,
            NULL
    );

    if (!mSwrContext) {
        LOGE("create mSwrContext error");
        return -502;
    }

    ret = swr_init(mSwrContext);
    if (ret < 0) {
        LOGE("init SwrContext error, CODE: %d", ret);
        return -503;
    }

    return 0;
}

static int transCodeVideo() {
    uint8_t *out_buffer = (unsigned char *) av_malloc((size_t) av_image_get_buffer_size(
            AV_PIX_FMT_YUV420P,
            mOutVideoCodecContext->width,
            mOutVideoCodecContext->height,
            1
    ));
    av_image_fill_arrays(
            mOutVideoFrame->data,
            mOutVideoFrame->linesize,
            out_buffer,
            AV_PIX_FMT_YUV420P,
            mOutVideoCodecContext->width,
            mOutVideoCodecContext->height,
            1
    );

    LOGD("------------------------------start video trans code-------------------------------------");

    LOGD(
            ">>>>in stream time base num: %d den: %d<<<<",
            mInFormatContext->streams[mInVideoIndex]->time_base.num,
            mInFormatContext->streams[mInVideoIndex]->time_base.den
    );

    LOGD(">>>>in packet pts: %lld dts: %lld<<<<", mInPacket->pts, mInPacket->dts);

    mInPacket->pts = av_rescale_q(
            mInPacket->pts,
            mInFormatContext->streams[mInVideoIndex]->time_base,
            mInVideoCodecContext->time_base
    );
    mInPacket->dts = mInPacket->pts;

    //1. 解码
    ret = avcodec_send_packet(mInVideoCodecContext, mInPacket);
    if (ret != 0) {
        LOGE("send decode packet error");
        return -200;
    }

    ret = avcodec_receive_frame(mInVideoCodecContext, mInFrame);
    if (ret != 0) {
        LOGE("receive decode frame error");
        return -201;
    }

    LOGD(
            ">>>>in codec context time base num: %d den: %d<<<<",
            mInVideoCodecContext->time_base.num,
            mInVideoCodecContext->time_base.den
    );

    LOGD(">>>>in frame pts: %lld<<<<", mInFrame->pts);

    sws_scale(
            mSwsContext,
            (const uint8_t *const *) mInFrame->data,
            mInFrame->linesize, 0,
            mInFrame->height,
            mOutVideoFrame->data,
            mOutVideoFrame->linesize
    );

//    mInFrame->pts = av_frame_get_best_effort_timestamp(mInFrame);

    mOutVideoFrame->format = mInFrame->format;
    mOutVideoFrame->width = mOutVideoCodecContext->width;
    mOutVideoFrame->height = mOutVideoCodecContext->height;
    mOutVideoFrame->pts = av_rescale_q(mInFrame->pts, mInVideoCodecContext->time_base,
                                       mOutVideoCodecContext->time_base);

    LOGD(
            "<<<<out codec context time base num: %d den: %d>>>>",
            mOutVideoCodecContext->time_base.num,
            mOutVideoCodecContext->time_base.den
    );

    LOGD("<<<<out packet pts: %lld>>>>", mOutVideoFrame->pts);


    ret = avcodec_send_frame(mOutVideoCodecContext, mOutVideoFrame);
    if (ret != 0) {
        LOGE("send encode frame error, CODE: %d", ret);
        return -203;
    }


    mOutPacket->data = NULL;
    mOutPacket->size = 0;

    ret = avcodec_receive_packet(mOutVideoCodecContext, mOutPacket);
    if (ret != 0) {
        LOGE("receive encode packet error, CODE: %d", ret);
        return -203;
    }

    mOutPacket->pts = av_rescale_q(
            mOutPacket->pts,
            mOutVideoCodecContext->time_base,
            mOutFormatContext->streams[mOutVideoIndex]->time_base
    );
    mOutPacket->dts = mOutPacket->pts;
    mOutPacket->stream_index = mOutVideoIndex;

    LOGD(
            "<<<<out stream time base num: %d den: %d>>>>",
            mOutFormatContext->streams[mOutVideoIndex]->time_base.num,
            mOutFormatContext->streams[mOutVideoIndex]->time_base.den
    );

    LOGD("<<<<out packet pts: %lld dts: %lld>>>>", mOutPacket->pts, mOutPacket->dts);

    //3. 写入 AVFormatContext
    ret = av_interleaved_write_frame(mOutFormatContext, mOutPacket);

    if (ret < 0) {
        LOGE("write frame error, CODE: %d", ret);
        return -204;
    }

    LOGD("------------------------------end video trans code-------------------------------------");
    av_free(out_buffer);
    return 0;
}

static int transCodeAudio() {

    LOGD("------------------------------start audio trans code-------------------------------------");

    LOGD(
            ">>>>in stream time base num: %d den: %d<<<<",
            mInFormatContext->streams[mInAudioIndex]->time_base.num,
            mInFormatContext->streams[mInAudioIndex]->time_base.den
    );

    LOGD(">>>>in packet pts: %lld dts: %lld<<<<", mInPacket->pts, mInPacket->dts);

    mInPacket->pts = av_rescale_q(
            mInPacket->pts,
            mInFormatContext->streams[mInAudioIndex]->time_base,
            mInAudioCodecContext->time_base
    );
    mInPacket->dts = mInPacket->pts;

    //1. 解码
    ret = avcodec_send_packet(mInAudioCodecContext, mInPacket);
    if (ret != 0) {
        LOGE("send decode packet error");
        return -200;
    }

    ret = avcodec_receive_frame(mInAudioCodecContext, mInFrame);
    if (ret != 0) {
        LOGE("receive decode frame error");
        return -201;
    }

    LOGD(
            ">>>>in codec context time base num: %d den: %d<<<<",
            mInAudioCodecContext->time_base.num,
            mInAudioCodecContext->time_base.den
    );

    LOGD(">>>>in frame pts: %lld<<<<", mInFrame->pts);

    int tempOutAudioSampleCount = (int) av_rescale_rnd(
            swr_get_delay(mSwrContext, mInAudioCodecContext->sample_rate) +
            mInFrame->nb_samples,
            mOutAudioCodecContext->sample_rate,
            mInAudioCodecContext->sample_rate,
            AV_ROUND_UP
    );

    int channelCount = av_get_channel_layout_nb_channels(mOutAudioCodecContext->channel_layout);
    ret = av_samples_alloc(
            mOutAudioFrame->data,
            mOutAudioFrame->linesize,
            channelCount,
            tempOutAudioSampleCount,
            mOutAudioCodecContext->sample_fmt,
            0
    );
    if (ret < 0) {
        LOGD("alloc out audio frame sample error, CODE: %d", ret);
        return -202;
    }

    ret = swr_convert(
            mSwrContext,
            mOutAudioFrame->data,
            mOutAudioFrame->nb_samples,
            (const uint8_t **) mInFrame->data,
            mInFrame->nb_samples
    );
    if (ret < 0) {
        LOGD("swr_convert error: %d", ret);
        return -203;
    }

    mOutAudioFrame->pts = av_rescale_q(
            mInFrame->pts,
            mInAudioCodecContext->time_base,
            mOutAudioCodecContext->time_base
    );
    mOutAudioFrame->nb_samples = tempOutAudioSampleCount;
    mOutAudioFrame->channel_layout = mOutAudioCodecContext->channel_layout;
    mOutAudioFrame->format = mOutAudioCodecContext->sample_fmt;
    mOutAudioFrame->sample_rate = mOutAudioCodecContext->sample_rate;

    ret = avcodec_send_frame(mOutAudioCodecContext, mOutAudioFrame);
    if (ret != 0) {
        LOGE("send encode frame error, CODE: %d", ret);
        return -203;
    }

    mOutPacket->data = NULL;
    mOutPacket->size = 0;

    ret = avcodec_receive_packet(mOutAudioCodecContext, mOutPacket);
    if (ret != 0) {
        LOGE("receive encode packet error, CODE: %d", ret);
        return -203;
    }

    av_freep(mOutAudioFrame->data);

    mOutPacket->pts = av_rescale_q(
            mOutPacket->pts,
            mOutAudioCodecContext->time_base,
            mOutFormatContext->streams[mOutAudioIndex]->time_base
    );
    mOutPacket->dts = mOutPacket->pts;
    mOutPacket->stream_index = mOutAudioIndex;

    LOGD(
            "<<<<out stream time base num: %d den: %d>>>>",
            mOutFormatContext->streams[mOutAudioIndex]->time_base.num,
            mOutFormatContext->streams[mOutAudioIndex]->time_base.den
    );

    LOGD("<<<<out packet pts: %lld dts: %lld>>>>", mOutPacket->pts, mOutPacket->dts);

//3. 写入 AVFormatContext
    ret = av_interleaved_write_frame(mOutFormatContext, mOutPacket);

    if (ret < 0) {
        LOGE("write frame error, CODE: %d", ret);
        return -204;
    }

    LOGD("------------------------------end audio trans code-------------------------------------");
    return 0;
}

static int flushEncoder(AVCodecContext *outCodecContext, unsigned int stream_index) {
    int ret;
    if (!(outCodecContext->codec->capabilities & CODEC_CAP_DELAY)) {
        return 0;
    }

    ret = avcodec_send_frame(outCodecContext, NULL);
    if (ret != 0) {
        LOGD("send frame error, CODE: %d", ret);
        return 0;
    }

    while (1) {
        mOutPacket->data = NULL;
        mOutPacket->size = 0;
        av_init_packet(mOutPacket);


        ret = avcodec_receive_packet(outCodecContext, mOutPacket);
        if (ret != 0) {
            LOGD("receive packet error, CODE: %d", ret);
            break;
        }

        LOGD("------------------------------start %d mux----------------------", stream_index);
        LOGD(
                "<<<<out stream time base num: %d den: %d>>>>",
                mOutFormatContext->streams[mOutAudioIndex]->time_base.num,
                mOutFormatContext->streams[mOutAudioIndex]->time_base.den
        );
        LOGD("<<<<out packet pts: %lld dts: %lld>>>>", mOutPacket->pts, mOutPacket->dts);

        mOutPacket->pts = av_rescale_q(
                mOutPacket->pts,
                outCodecContext->time_base,
                mOutFormatContext->streams[stream_index]->time_base
        );
        mOutPacket->dts = mOutPacket->pts;
        mOutPacket->stream_index = stream_index;


        LOGD(
                "<<<<out stream time base num: %d den: %d>>>>",
                mOutFormatContext->streams[mOutAudioIndex]->time_base.num,
                mOutFormatContext->streams[mOutAudioIndex]->time_base.den
        );

        LOGD("<<<<out packet pts: %lld dts: %lld>>>>", mOutPacket->pts, mOutPacket->dts);
        ret = av_interleaved_write_frame(mOutFormatContext, mOutPacket);
        LOGD("------------------------------end %d mux----------------------", stream_index);

        if (ret != 0) {
            LOGD("write frame error");
            break;
        }

        LOGD("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n", mOutPacket->size);
    }
    return 0;
}

int compress(const char *in_filename, const char *out_filename) {

    //1. 注册
    av_register_all();

    //2. 初始化input
    ret = initInput(in_filename);
    if (ret != 0) {
        LOGE("init input File error!!");
        goto end;
    }

    //3. 初始化output
    ret = initOutput(out_filename);
    if (ret != 0) {
        LOGE("init output file error");
        goto end;
    }

    //4. AVPacket 申请内存
    mInPacket = av_packet_alloc();
    mOutPacket = av_packet_alloc();


    //5. AVFrame 申请内存
    mInFrame = av_frame_alloc();
    mOutVideoFrame = av_frame_alloc();

    //6. 写头
    avformat_write_header(mOutFormatContext, NULL);

    mSwsContext = sws_getContext(
            mInVideoCodecContext->width,
            mInVideoCodecContext->height,
            AV_PIX_FMT_YUV420P,
            mOutVideoCodecContext->width,
            mOutVideoCodecContext->height,
            AV_PIX_FMT_YUV420P, SWS_BICUBIC,
            NULL, NULL, NULL
    );
    if (mSwsContext == NULL) {
        LOGE("getSwsContext error");
        goto end;
    }

    initAudioFrame();

    while (av_read_frame(mInFormatContext, mInPacket) == 0) {
        if (mInPacket->stream_index == mInVideoIndex) {
            ret = transCodeVideo();
        } else if (mInPacket->stream_index == mInAudioIndex) {
            ret = transCodeAudio();
        }
        av_packet_unref(mInPacket);
        av_packet_unref(mOutPacket);
    }

    flushEncoder(mOutVideoCodecContext, (unsigned int) mOutVideoIndex);
    flushEncoder(mOutAudioCodecContext, (unsigned int) mOutAudioIndex);

    ret = av_write_trailer(mOutFormatContext);
    if (ret != 0) {
        LOGE("write trailer error");
        goto end;
    }
    return 0;

    end:
    av_frame_free(&mInFrame);
    av_frame_free(&mOutVideoFrame);
    av_packet_free(&mInPacket);
    av_packet_free(&mOutPacket);
    sws_freeContext(mSwsContext);
    swr_free(&mSwrContext);
    avcodec_close(mInVideoCodecContext);
    avcodec_close(mOutVideoCodecContext);
    avformat_close_input(&mInFormatContext);
    avformat_free_context(mInFormatContext);
    avformat_close_input(&mOutFormatContext);
    avformat_free_context(mOutFormatContext);
    return -10;
}
