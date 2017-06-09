#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

typedef struct {
	char mSignature[2]; // BM -> windows
	int mSize;
	char mReserved1[2]; // these depend on application
	char mReserved2[2];
	int mAddressOfPixelArray;
} __attribute__((packed)) bmpHeader;

typedef struct {
	int mHeaderSize; // must be 40!
	signed int mWidth;
	signed int mHeight;
	short mColorPlanes; // must be set to 1 !
	short mColorDepth;
	int mCompression;
	int mImageSize; // size of the PIXEL ARRAY NOT FILE
	int mPixelPerMeterHoriz;
	int mPixelPerMeterVert;
	int mNumberOfColorsInPalette;
	int mNumberOfImportantColors; // generally ignored
} __attribute__((packed)) bmpInfoHeader;

typedef struct {
	char B;
	char G;
	char R;
	char RES;
} __attribute__((packed)) bmpColor;


struct __fb_arg {
    uint32_t x;
    uint32_t y;
};

int
main(int argc, char *argv[]) 
{
    int fd, img, rc;
    struct stat st;

    fd = open("/dev/fb", O_RDWR, 0);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    img = open("/etc/kitten.bmp", O_RDONLY, 0);
    if (img < 0) {
        perror("kitten_open");
        return 1;
    }

    fstat(img, &st);

    void *addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, img, 0);

    struct __fb_arg arg = {
        .x = 1024,
        .y = 768,
    };


    bmpHeader* bmph = (bmpHeader*) addr;
	bmpInfoHeader* bmpinfo = (bmpInfoHeader*)(addr + sizeof(bmpHeader));

    if (bmph->mSignature[0] == 'B' && bmph->mSignature[1] == 'M') {
        printf("bitmap header is correct, header size: %d bytes\n", bmpinfo->mHeaderSize);
    } else {
        printf("bitmap header is not correct, but 0x%x 0x%x\n",
                bmph->mSignature[0], bmph->mSignature[1]);
        exit(1);
    }

	bmpColor* me = (bmpColor*)(addr + sizeof(bmpHeader) + bmpinfo->mHeaderSize);
	/*for(int i = 0; i < bmpinfo->mNumberOfColorsInPalette; i++)
	{
		VGA_map_color(i, me->R, me->G, me->B);
		me++;
	}*/

    uint32_t *img_buf = malloc(1024 * bmpinfo->mHeight * 4);
    if (!img_buf) {
        perror("allocation");
        return 1;
    }
    uint32_t *orig_img_buf = img_buf;
    memset(img_buf, 0, 1024 * bmpinfo->mHeight * 4);

    printf("displaying an image of size %dx%d %d bpp\n", bmpinfo->mWidth, bmpinfo->mHeight,
            bmpinfo->mColorDepth);

    printf("details: %d color planes, %d compression, offset of pixel array: 0x%x\n",
            bmpinfo->mColorPlanes, bmpinfo->mCompression, bmph->mAddressOfPixelArray);

    int x = 0, y = 0;

	for(int a = bmpinfo->mHeight; a > 0; a --)
	{
        x = 0;
		for(int b = bmpinfo->mWidth; b > 0; b --)
		{
            uint32_t *ptr = (uint32_t *)(addr + bmph->mAddressOfPixelArray + a * bmpinfo->mWidth + b);
            uint32_t color = *(uint32_t *)&me[*ptr];
            //printf("pixel (%d,%d) = 0x%x\n", a, b, color);
            //uint32_t color = *(uint32_t *)(addr + bmph->mAddressOfPixelArray + a * bmpinfo->mWidth - b);
            img_buf[x + y * 1024] = color;
            x ++;
			//mode.putpixel(x + bmpinfo->mWidth - b, y + bmpinfo->mHeight - a, *(char*)(buf + bmph->mAddressOfPixelArray + a * bmpinfo->mWidth - b));
		}
        y ++;
        //img_buf += 1024 * 4 - bmpinfo->mWidth * 4;
	}

    ioctl(fd, 0x1337, &arg);

    write(fd, orig_img_buf, bmpinfo->mHeight * 1024 * 4);

    return 0;
}
