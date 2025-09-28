#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <signal.h>
#include <sys/select.h>
#include <time.h>
#include <getopt.h>

// Number of buffers to request from the driver
#define NB_BUFFERS 8
#define MAX_BUFFERS 16

struct buffer {
    void   *start;
    size_t length;
};

static struct buffer buffers[MAX_BUFFERS];
static int fd = 0;
static int nb_buffers = 0;

//handle ctrl+c
void handle_sigint(int sig) { exit(EXIT_SUCCESS); }

//handle verbosity
static int verbose = 0;
static int opt;
static struct option long_options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {0, 0, 0, 0}
};

static void HandleOptions(int argc, char *argv[])
{
    while ((opt = getopt_long(argc, argv, "v", long_options, NULL)) != -1) {
        switch (opt) {
            case 'v':
                verbose = 1;
                break;
            case '?':
            default:
                // Unknown option
                fprintf(stderr, "Usage: %s [-v|--verbose]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
}


static int SetupFormat(struct v4l2_format *fmt, __u32 width, __u32 height)
{
    if (!fmt || ! width || !height)
    {
        perror("SetupFormat invalid parameters");
        return -1;
    }
    fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt->fmt.pix.width = width;
    fmt->fmt.pix.height = height;
    fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt->fmt.pix.field = V4L2_FIELD_NONE;

    return 0;
}

static int SetupFrameRate(int fd, __u32 numerator, __u32 denominator)
{
    struct v4l2_streamparm parm = {0};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = numerator;      // e.g. 1/30 for 30 fps
    parm.parm.capture.timeperframe.denominator = denominator;
    if (ioctl(fd, VIDIOC_S_PARM, &parm) < 0) {
        perror("VIDIOC_S_PARM");
        return -1;
    }
    return 0;
}

static void PrintSetup(int fd_vid)
{

    // --- Fetch format ---
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
        perror("VIDIOC_G_FMT");
    } else {
        printf("Format:\n");
        printf("  Width: %u\n", fmt.fmt.pix.width);
        printf("  Height: %u\n", fmt.fmt.pix.height);
        printf("  Pixel format: %c%c%c%c\n",
               fmt.fmt.pix.pixelformat & 0xFF,
               (fmt.fmt.pix.pixelformat >> 8) & 0xFF,
               (fmt.fmt.pix.pixelformat >> 16) & 0xFF,
               (fmt.fmt.pix.pixelformat >> 24) & 0xFF);
        printf("  Field: %u\n", fmt.fmt.pix.field);
    }

    // --- Get streaming parameters ---
    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_G_PARM, &parm) < 0) {
        perror("VIDIOC_G_PARM");
    } else {
        printf("FPS: %u/%u (%.2f fps)\n",
               parm.parm.capture.timeperframe.numerator,
               parm.parm.capture.timeperframe.denominator,
               (float)parm.parm.capture.timeperframe.denominator /
               parm.parm.capture.timeperframe.numerator);
    }
}

static int SetupBufferRequest(struct v4l2_requestbuffers* req, __u32 nbBuffersRequested)
{
    if (!req || !nbBuffersRequested)
    {
        perror("SetupBuffer : invalid parameters");
        return -1;
    }

    req->count = nbBuffersRequested;
    req->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req->memory = V4L2_MEMORY_MMAP;
    return 0;
}

static int MapBuffers(struct buffer *pBufferArray, const size_t bufferSize, int fd_vid, const struct v4l2_requestbuffers* req)
{
    // validate params 
    if(!pBufferArray || !req)
    {
        perror("MapBuffers : invalid parameters");
        return -1;
    }
    //validate buffer size :
    if (bufferSize < req->count)
    {
        perror("MapBuffers : buffersize is smaller than request count.");
        return -1;
    } 

    for (int i = 0; i < req->count; i++) {
        //initialize buffer struct for query
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        //query for bufer i
        if (ioctl(fd_vid, VIDIOC_QUERYBUF, &buf) < 0) { perror("VIDIOC_QUERYBUF"); return -1; }
        //assign info from query into local buffer data & map into userspace
        pBufferArray[i].length = buf.length;
        pBufferArray[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_vid, buf.m.offset);
        if (pBufferArray[i].start == MAP_FAILED) { perror("mmap"); return -1; }
        //Tell the driver that buffer is ready to be filled
        if (ioctl(fd_vid, VIDIOC_QBUF, &buf) < 0) { perror("VIDIOC_QBUF"); return -1; }
    }

    //keep count for cleanup
    nb_buffers = req->count;
    return 0;
}

static int Capture(int fd_vid, int nbFrames, struct buffer *pBufferArray)
{
    fd_set fds;
    struct timeval tv;
    int r;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < nbFrames; i++) {
        FD_ZERO(&fds);
        FD_SET(fd_vid, &fds);

        // Wait max 1 second for a frame
        tv.tv_sec = 0;
        tv.tv_usec = 66000; // fully missed frames at 15fps, 33ms at 30fps, 66ms at 15fps

        r = select(fd_vid + 1, &fds, NULL, NULL, &tv);
        if (r == -1) { perror("select"); return -1; }
        if (r == 0) {
            if (verbose)
                printf("Timeout waiting for frame %d\n", i);
            continue;
        }

        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd_vid, VIDIOC_DQBUF, &buf) < 0) { perror("VIDIOC_DQBUF"); return -1; }

        // Here, consume the frame (stdout), verbosity option to print info instead.
        if (!verbose)
            fwrite(pBufferArray[buf.index].start, buf.bytesused, 1, stdout);

        if (ioctl(fd_vid, VIDIOC_QBUF, &buf) < 0) { perror("VIDIOC_QBUF"); return -1; }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
    if (verbose)
        printf("Captured %d frames in %.3f seconds (%.2f fps)\n", nbFrames, elapsed, nbFrames / elapsed);

    return 0;
}



static void _atexit_(void)
{
    // Unmap all buffers
    for (int i = 0; i < nb_buffers; ++i) {
        if (buffers[i].start && buffers[i].length > 0) {
            munmap(buffers[i].start, buffers[i].length);
            buffers[i].start = NULL;
            buffers[i].length = 0;
        }
    }

    if (fd)
    {
        close(fd);
    }
}



int main(int argc, char *argv[]) {
    //setup ctrl+c handler
    signal(SIGINT, handle_sigint);
    //handle cleanup at exit
    atexit(_atexit_);

    // Handle command line options
    HandleOptions(argc, argv);

    // OPEN video camera, return if it fails
    fd = open("/dev/video0", O_RDWR);
    if (fd < 0) 
        { 
            perror("open"); return 1; 
        }

    // Set format ; Declare, Set format, Apply format to io
    struct v4l2_format fmt = {0};
    if (SetupFormat(&fmt, 640, 480) < 0) 
        { perror("SetupFormat Failed"); return 1; }

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) 
        { perror("VIDIOC_S_FMT"); return 1; }
     
    // Set frame rate
    if (SetupFrameRate(fd, 1, 30) < 0)
        { perror("SetupFrameRate Failed"); return 1; }

    // Print setup to check framerate and format
    if(verbose) { PrintSetup(fd); }

    // Set Request buffers
    struct v4l2_requestbuffers req = {0};
    if (SetupBufferRequest(&req, NB_BUFFERS) < 0) 
        { perror("SetupBufferRequest Failed"); return 1;}

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) 
        { perror("VIDIOC_REQBUFS"); return 1; }


    // Map buffers
    if (MapBuffers(buffers, NB_BUFFERS, fd, &req) < 0) 
        { perror("Map Buffer Failed"); return 1; }


    // Start streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) { perror("VIDIOC_STREAMON"); return 1; }


    // Capture loop (grab 100 frames)   
    if (Capture(fd, 100, buffers) < 0) 
        {perror("Capture failed"); return 1;}


    // Stop streaming
    ioctl(fd, VIDIOC_STREAMOFF, &type);
    close(fd);
    return 0;
}