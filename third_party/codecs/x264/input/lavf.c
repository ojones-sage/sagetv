/*****************************************************************************
 * lavf.c: x264 libavformat input module
 *****************************************************************************
 * Copyright (C) 2009 x264 project
 *
 * Authors: Mike Gurlitz <mike.gurlitz@gmail.com>
 *          Steven Walters <kemuri9@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *****************************************************************************/

#include "input.h"
#define FAIL_IF_ERROR( cond, ... ) FAIL_IF_ERR( cond, "lavf", __VA_ARGS__ )
#undef DECLARE_ALIGNED
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>

typedef struct
{
    AVFormatContext *lavf;
    int stream_id;
    int next_frame;
    int vfr_input;
    cli_pic_t *first_pic;
} lavf_hnd_t;

static int read_frame_internal( cli_pic_t *p_pic, lavf_hnd_t *h, int i_frame, video_info_t *info )
{
    if( h->first_pic && !info )
    {
        /* see if the frame we are requesting is the frame we have already read and stored.
         * if so, retrieve the pts and image data before freeing it. */
        if( !i_frame )
        {
            XCHG( cli_image_t, p_pic->img, h->first_pic->img );
            p_pic->pts = h->first_pic->pts;
            XCHG( void*, p_pic->opaque, h->first_pic->opaque );
        }
        lavf_input.release_frame( h->first_pic, NULL );
        lavf_input.picture_clean( h->first_pic );
        free( h->first_pic );
        h->first_pic = NULL;
        if( !i_frame )
            return 0;
    }

    AVCodecContext *c = h->lavf->streams[h->stream_id]->codec;
    AVPacket *pkt = p_pic->opaque;
    AVFrame frame;
    avcodec_get_frame_defaults( &frame );

    while( i_frame >= h->next_frame )
    {
        int finished = 0;
        while( !finished && av_read_frame( h->lavf, pkt ) >= 0 )
            if( pkt->stream_index == h->stream_id )
            {
                c->reordered_opaque = pkt->pts;
                if( avcodec_decode_video2( c, &frame, &finished, pkt ) < 0 )
                    x264_cli_log( "lavf", X264_LOG_WARNING, "video decoding failed on frame %d\n", h->next_frame );
            }
        if( !finished )
        {
            if( avcodec_decode_video2( c, &frame, &finished, pkt ) < 0 )
                x264_cli_log( "lavf", X264_LOG_WARNING, "video decoding failed on frame %d\n", h->next_frame );
            if( !finished )
                return -1;
        }
        h->next_frame++;
    }

    memcpy( p_pic->img.stride, frame.linesize, sizeof(p_pic->img.stride) );
    memcpy( p_pic->img.plane, frame.data, sizeof(p_pic->img.plane) );
    p_pic->img.height  = c->height;
    p_pic->img.csp     = c->pix_fmt | X264_CSP_OTHER;
    p_pic->img.width   = c->width;

    if( info )
    {
        info->interlaced = frame.interlaced_frame;
        info->tff = frame.top_field_first;
    }

    if( h->vfr_input )
    {
        p_pic->pts = p_pic->duration = 0;
        if( frame.reordered_opaque != AV_NOPTS_VALUE )
            p_pic->pts = frame.reordered_opaque;
        else if( pkt->dts != AV_NOPTS_VALUE )
            p_pic->pts = pkt->dts; // for AVI files
        else if( info )
        {
            h->vfr_input = info->vfr = 0;
            return 0;
        }
    }

    return 0;
}

static int open_file( char *psz_filename, hnd_t *p_handle, video_info_t *info, cli_input_opt_t *opt )
{
    lavf_hnd_t *h = calloc( 1, sizeof(lavf_hnd_t) );
    if( !h )
        return -1;
    av_register_all();
    if( !strcmp( psz_filename, "-" ) )
        psz_filename = "pipe:";

    /* if resolution was passed in, parse it and colorspace into parameters. this allows raw video support */
    AVFormatParameters *param = NULL;
    if( opt->resolution )
    {
        param = calloc( 1, sizeof(AVFormatParameters) );
        if( !param )
            return -1;
        sscanf( opt->resolution, "%dx%d", &param->width, &param->height );
        param->pix_fmt = opt->colorspace ? av_get_pix_fmt( opt->colorspace ) : PIX_FMT_YUV420P;
    }

    FAIL_IF_ERROR( av_open_input_file( &h->lavf, psz_filename, NULL, 0, param ), "could not open input file\n" )
    if( param )
        free( param );
    FAIL_IF_ERROR( av_find_stream_info( h->lavf ) < 0, "could not find input stream info\n" )

    int i = 0;
    while( i < h->lavf->nb_streams && h->lavf->streams[i]->codec->codec_type != CODEC_TYPE_VIDEO )
        i++;
    FAIL_IF_ERROR( i == h->lavf->nb_streams, "could not find video stream\n" )
    h->stream_id       = i;
    h->next_frame      = 0;
    AVCodecContext *c  = h->lavf->streams[i]->codec;
    info->fps_num      = h->lavf->streams[i]->r_frame_rate.num;
    info->fps_den      = h->lavf->streams[i]->r_frame_rate.den;
    info->timebase_num = h->lavf->streams[i]->time_base.num;
    info->timebase_den = h->lavf->streams[i]->time_base.den;
    /* lavf is thread unsafe as calling av_read_frame invalidates previously read AVPackets */
    info->thread_safe  = 0;
    h->vfr_input       = info->vfr;
    FAIL_IF_ERROR( avcodec_open( c, avcodec_find_decoder( c->codec_id ) ),
                   "could not find decoder for video stream\n" )

    /* prefetch the first frame and set/confirm flags */
    h->first_pic = malloc( sizeof(cli_pic_t) );
    FAIL_IF_ERROR( !h->first_pic || lavf_input.picture_alloc( h->first_pic, X264_CSP_OTHER, info->width, info->height ),
                   "malloc failed\n" )
    else if( read_frame_internal( h->first_pic, h, 0, info ) )
        return -1;

    info->width      = c->width;
    info->height     = c->height;
    info->csp        = h->first_pic->img.csp;
    info->num_frames = 0; /* FIXME */
    info->sar_height = c->sample_aspect_ratio.den;
    info->sar_width  = c->sample_aspect_ratio.num;

    /* avisynth stores rgb data vertically flipped. */
    if( !strcasecmp( get_filename_extension( psz_filename ), "avs" ) &&
        (c->pix_fmt == PIX_FMT_BGRA || c->pix_fmt == PIX_FMT_BGR24) )
        info->csp |= X264_CSP_VFLIP;

    *p_handle = h;

    return 0;
}

static int picture_alloc( cli_pic_t *pic, int csp, int width, int height )
{
    if( x264_cli_pic_alloc( pic, csp, width, height ) )
        return -1;
    pic->img.planes = 4;
    pic->opaque = malloc( sizeof(AVPacket) );
    if( !pic->opaque )
        return -1;
    av_init_packet( pic->opaque );
    return 0;
}

static int read_frame( cli_pic_t *pic, hnd_t handle, int i_frame )
{
    return read_frame_internal( pic, handle, i_frame, NULL );
}

static int release_frame( cli_pic_t *pic, hnd_t handle )
{
    av_free_packet( pic->opaque );
    av_init_packet( pic->opaque );
    return 0;
}

static void picture_clean( cli_pic_t *pic )
{
    free( pic->opaque );
    memset( pic, 0, sizeof(cli_pic_t) );
}

static int close_file( hnd_t handle )
{
    lavf_hnd_t *h = handle;
    avcodec_close( h->lavf->streams[h->stream_id]->codec );
    av_close_input_file( h->lavf );
    free( h );
    return 0;
}

const cli_input_t lavf_input = { open_file, picture_alloc, read_frame, release_frame, picture_clean, close_file };
