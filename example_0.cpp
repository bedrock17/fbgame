#include <iostream>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    // 프레임버퍼 장치 열기
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        std::cerr << "Error: cannot open framebuffer device." << std::endl;
        return 1;
    }

    // 가변 화면 정보 가져오기
    fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
        std::cerr << "Error reading variable information." << std::endl;
        close(fb_fd);
        return 1;
    }

    // 고정 화면 정보 가져오기
    fb_fix_screeninfo finfo;
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
        std::cerr << "Error reading fixed information." << std::endl;
        close(fb_fd);
        return 1;
    }

    // 화면 크기 계산
    long screensize = vinfo.yres_virtual * finfo.line_length;

    // 메모리 매핑
    uint8_t* fb_ptr = (uint8_t*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((intptr_t)fb_ptr == -1) {
        std::cerr << "Error: failed to map framebuffer device to memory." << std::endl;
        close(fb_fd);
        return 1;
    }

    // 픽셀 그리기 (예: 빨간색 사각형 그리기)
    int x, y;
    for (y = 100; y < 200; ++y) {
        for (x = 100; x < 200; ++x) {
            long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                            (y + vinfo.yoffset) * finfo.line_length;
            if (vinfo.bits_per_pixel == 32) {
                *(fb_ptr + location) = 255;        // Blue
                *(fb_ptr + location + 1) = 0;      // Green
                *(fb_ptr + location + 2) = 0;      // Red
                *(fb_ptr + location + 3) = 0;      // Transparency
            } else { // Assuming 16bpp
                int b = 10;
                int g = (x - 100) / 6;     // A little green
                int r = 31 - (y - 100) / 16;    // A lot of red
                unsigned short int t = r << 11 | g << 5 | b;
                *((unsigned short int*)(fb_ptr + location)) = t;
            }
        }
    }

    // 메모리 매핑 해제 및 파일 닫기
    munmap(fb_ptr, screensize);
    close(fb_fd);

    return 0;
}
