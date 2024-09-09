#include "mcm_dp.h"

#define DEFAULT_FRAME_WIDTH 1920
#define DEFAULT_FRAME_HEIGHT 1080
#define DEFAULT_FPS 30.0
#define DEFAULT_RECV_IP "127.0.0.1"
#define DEFAULT_RECV_PORT "9001"
#define DEFAULT_SEND_IP "127.0.0.1"
#define DEFAULT_SEND_PORT "9001"
#define DEFAULT_PROTOCOL "auto"
#define DEFAULT_PAYLOAD_TYPE "st20"
#define DEFAULT_TOTAL_NUM 300 // sender only
#define DEFAULT_MEMIF_SOCKET_PATH "/run/mcm/mcm_rx_memif.sock"
#define DEFAULT_MEMIF_INTERFACE_ID 0
#define DEFAULT_INFINITY_LOOP 0 // sender only
#define DEFAULT_LOCAL_FILE "data-sdk.264" // recver only
#define DEFAULT_VIDEO_FMT "yuv422p10le"
#define DEFAULT_AUDIO_TYPE "frame"
#define DEFAULT_AUDIO_FORMAT "pcm16"
#define DEFAULT_AUDIO_SAMPLING "48k"
#define DEFAULT_AUDIO_PTIME "1ms"
#define DEFAULT_AUDIO_CHANNELS 1
#define DEFAULT_ANC_TYPE "frame"
#define DEFAULT_PAYLOAD_CODEC "jpegxs"

static int getAudioFrameSize(
    mcm_audio_format audio_fmt,
    mcm_audio_sampling sampling,
    mcm_audio_ptime ptime,
    uint32_t audio_channels)
{
    // AUDIO_PTIME_1_09MS, /**< packets time of 1.09ms, only for 44.1kHz sample */
    // AUDIO_PTIME_0_14MS, /**< packet time of 0.14ms, only for 44.1kHz sample */
    // AUDIO_PTIME_0_09MS, /**< packet time of 0.09ms, only for 44.1kHz sample */

    // Audio frame size
    int bits_per_sample;
    int samples_per_second;
    int slices_per_second;

    // mcm_audio_format
    if (audio_fmt == AUDIO_FMT_PCM8){
        bits_per_sample = 8;
    } else if (audio_fmt == AUDIO_FMT_PCM24){
        bits_per_sample = 24;
    } else if (audio_fmt == AUDIO_FMT_AM824){
        bits_per_sample = 32;
    } else { //if (audio_fmt == AUDIO_FMT_PCM16)
        bits_per_sample = 16;
    }

    // for 48k
    if (ptime == AUDIO_PTIME_125US){
        slices_per_second = 8000; // 6
    } else if (ptime == AUDIO_PTIME_250US){
        slices_per_second = 4000; // 12
    } else if (ptime == AUDIO_PTIME_4MS){
        slices_per_second = 250; // 192
    } else { // if (ptime == AUDIO_PTIME_1MS)
        slices_per_second = 1000; // 48 samples per slice
    }
    // } else if (ptime == AUDIO_PTIME_333US){
    //     slices_per_second = 3003; // TODO: not ideal
    // } else if (ptime == AUDIO_PTIME_80US){
    //     slices_per_second = 12500; // 0.384?
    // } else if (ptime == AUDIO_PTIME_1_09MS){
    //     slices_per_second = 917; // TODO: not ideal
    // } else if (ptime == AUDIO_PTIME_0_14MS){
    //     slices_per_second = 7142; // TODO: not ideal
    // } else if (ptime == AUDIO_PTIME_0_09MS){
    //     slices_per_second = 11111; // TODO: not ideal
    // }

    if (sampling == AUDIO_SAMPLING_96K){
        samples_per_second = 96000;
    } else if (sampling == AUDIO_SAMPLING_44K){
        samples_per_second = 44100;
    } else { // if (sampling == AUDIO_SAMPLING_48K)
        samples_per_second = 48000;
    }

    return audio_channels * bits_per_sample * samples_per_second * 1.0 / slices_per_second; // bits per slice
}

static int getFrameSize(video_pixel_format fmt, uint32_t width, uint32_t height, bool interlaced)
{
    size_t size = 0;
    size_t pixels = (size_t)(width*height);
    switch (fmt) {
        /* YUV 422 packed 8bit
        (aka ST20_FMT_YUV_422_8BIT,
        aka ST_FRAME_FMT_UYVY) */
        // TODO: Ensure it was supposed to be packed, not planar
        case PIX_FMT_YUV422P:
            size = pixels * 2;
            break;
        /* 8 bits RGB pixel in a 24 bits
        (aka ST_FRAME_FMT_RGB8) */
        case PIX_FMT_RGB8:
            size = pixels * 3;
            break;
/* Customized YUV 420 8bit, set transport format as ST20_FMT_YUV_420_8BIT. For direct transport of
none-RFC4175 formats like I420/NV12. When this input/output format is set, the frame is identical to
transport frame without conversion. The frame should not have lines padding) */
        /* PIX_FMT_NV12, YUV 420 planar 8bits
        (aka ST_FRAME_FMT_YUV420CUSTOM8,
        aka ST_FRAME_FMT_YUV420PLANAR8) */
        case PIX_FMT_NV12:
            size = pixels * 3 / 2;
            break;
        case PIX_FMT_YUV444P_10BIT_LE:
            size = pixels * 2 * 3;
            break;
        /* YUV 422 planar 10bits little indian, in two bytes
        (aka ST_FRAME_FMT_YUV422PLANAR10LE) */
        case PIX_FMT_YUV422P_10BIT_LE:
        default:
            size = pixels * 2 * 2;
    }
    if (interlaced) size /= 2; // TODO: Check if all formats support interlace
    return (int)size;
}

/* print a description of all supported options */
void usage(FILE* fp, const char* path, int is_sender)
{
    /* take only the last portion of the path */
    const char* basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    fprintf(fp, "Usage: %s [OPTION]\n", basename);
    fprintf(fp, "-H, --help\t\t\t\t"
                "Print this help and exit\n");
    fprintf(fp, "-r, --rcv_ip=<ip_address>\t\t"
                "Receiver's IP address (default: %s)\n",
        DEFAULT_RECV_IP);
    fprintf(fp, "-i, --rcv_port=<port_number>\t\t"
                "Receiver's port number (default: %s)\n",
        DEFAULT_RECV_PORT);
    fprintf(fp, "-s, --send_ip=<ip_address>\t\t"
                "Sender's IP address (default: %s)\n",
        DEFAULT_SEND_IP);
    fprintf(fp, "-p, --send_port=<port_number>\t\t"
                "Sender's port number (default: %s)\n",
        DEFAULT_SEND_PORT);
    fprintf(fp, "-o, --protocol=<protocol_type>\t\t"
                "Set protocol type (default: %s)\n",
        DEFAULT_PROTOCOL);
    fprintf(fp, "-t, --type=<payload_type>\t\t"
                "Payload type (default: %s)\n",
        DEFAULT_PAYLOAD_TYPE);
    fprintf(fp, "-k, --socketpath=<socket_path>\t\t"
                "Set memif socket path (default: %s)\n",
        DEFAULT_MEMIF_SOCKET_PATH);
    fprintf(fp, "-m, --master=<is_master>\t\t"
                "Set memif conn is master (default: 1 for sender, 0 for recver)\n");
    fprintf(fp, "-d, --interfaceid=<interface_id>\t"
                "Set memif conn interface id (default: %d)\n",
        DEFAULT_MEMIF_INTERFACE_ID);
    if (is_sender){
        fprintf(fp, "-b, --file=<input_file>\t\t\t"
                    "Input file name (optional)\n");
        fprintf(fp, "-l, --loop=<is_loop>\t\t\t"
                    "Set infinity loop sending (default: %d)\n",
            DEFAULT_INFINITY_LOOP);
    } else { // receiver
        fprintf(fp, "-b, --dumpfile=<file_name>\t\t"
                    "Save stream to local file (example: %s)\n",
            DEFAULT_LOCAL_FILE);
    }
    fprintf(fp, "--------------------------------------   VIDEO (ST2x)   --------------------------------------\n");
    fprintf(fp, "-w, --width=<frame_width>\t\t"
                "Width of test video frame (default: %d)\n",
        DEFAULT_FRAME_WIDTH);
    fprintf(fp, "-h, --height=<frame_height>\t\t"
                "Height of test video frame (default: %d)\n",
        DEFAULT_FRAME_HEIGHT);
    fprintf(fp, "-f, --fps=<video_fps>\t\t\t"
                "Test video FPS (frame per second) (default: %0.2f)\n",
        DEFAULT_FPS);
    fprintf(fp, "-x, --pix_fmt=<pixel_format>\t\t"
                "Pixel format (default: %s)\n",
        DEFAULT_VIDEO_FMT);
    if (is_sender){
        fprintf(fp, "-n, --number=<number_of_frames>\t\t"
                    "Total frame number to send (default: %d)\n",
            DEFAULT_TOTAL_NUM);
    }
    fprintf(fp, "--------------------------------------   AUDIO (ST3x)   --------------------------------------\n");
    fprintf(fp, "-a, --audio_type=<audio_type>\t\t"
                "Define audio type [frame|rtp] (default: %s)\n",
        DEFAULT_AUDIO_TYPE);
    fprintf(fp, "-j, --audio_format=<audio_format>\t"
                "Define audio format [pcm8|pcm16|pcm24|am824] (default: %s)\n",
        DEFAULT_AUDIO_FORMAT);
    fprintf(fp, "-g, --audio_sampling=<audio_sampling>\t"
                "Define audio sampling [48k|96k|44k] (default: %s)\n",
        DEFAULT_AUDIO_SAMPLING);
    fprintf(fp, "-e, --audio_ptime=<audio_ptime>\t\t"
                "Define audio ptime [1ms|125us|250us|333us|4ms|80us|1.09ms|0.14ms|0.09ms] (default: %s)\n",
        DEFAULT_AUDIO_PTIME);
    fprintf(fp, "-c, --audio_channels=<channels>\t\t"
                "Define number of audio channels [1|2] (default: %d)\n",
        DEFAULT_AUDIO_CHANNELS);
    fprintf(fp, "-------------------------------------- ANCILLARY (ST4x) --------------------------------------\n");
    fprintf(fp, "-q, --anc_type=<anc_type>\t\t"
                "Define anc type [frame|rtp] (default: %s)\n",
        DEFAULT_ANC_TYPE);
    fprintf(fp, "\n");
}