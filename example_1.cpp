#include <iostream>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

// 화면 크기
const int WIDTH = 720;
const int HEIGHT = 480;

// 색상 정의
struct Color {
    uint8_t r, g, b, a;
};

// 색상 상수
const Color SKY_BLUE = {135, 206, 235, 0};
const Color BROWN = {139, 69, 19, 0};
const Color RED = {255, 0, 0, 0};
const Color PLAYER_COLOR = RED;

// 32비트 색상을 16비트 RGB565로 변환
uint16_t convertTo16Bit(Color color) {
    return ((color.r & 0xF8) << 8) | ((color.g & 0xFC) << 3) | (color.b >> 3);
}

// 유닛 클래스
class Unit {
protected:
    int x, y;

public:
    Unit(int startX, int startY) : x(startX), y(startY) {}

    virtual void draw(uint8_t* fb_ptr, fb_var_screeninfo vinfo, fb_fix_screeninfo finfo) = 0;

    virtual void move(int dx) {
        x += dx;
    }

    int getX() const { return x; }
    int getY() const { return y; }
};

// 플레이어 클래스
class Player : public Unit {
public:
    Player(int startX, int startY) : Unit(startX, startY) {}

    void draw(uint8_t* fb_ptr, fb_var_screeninfo vinfo, fb_fix_screeninfo finfo) override {
        // 간단한 사각형으로 플레이어 그리기
        uint16_t playerColor = convertTo16Bit(PLAYER_COLOR);
        for (int j = 0; j < 10; ++j) {
            for (int i = 0; i < 10; ++i) {
                int px = x + i;
                int py = y + j;
                long location = (px + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                                (py + vinfo.yoffset) * finfo.line_length;
                *((uint16_t*)(fb_ptr + location)) = playerColor;
            }
        }
    }
};

// 배경 색상 채우기 함수
void fillBackground(uint8_t* fb_ptr, fb_var_screeninfo vinfo, fb_fix_screeninfo finfo, Color color) {
    uint16_t color16 = convertTo16Bit(color);
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                            (y + vinfo.yoffset) * finfo.line_length;
            *((uint16_t*)(fb_ptr + location)) = color16;
        }
    }
}

// 땅 색상 채우기 함수
void fillGround(uint8_t* fb_ptr, fb_var_screeninfo vinfo, fb_fix_screeninfo finfo, Color color) {
    uint16_t color16 = convertTo16Bit(color);
    for (int y = HEIGHT - 50; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                            (y + vinfo.yoffset) * finfo.line_length;
            *((uint16_t*)(fb_ptr + location)) = color16;
        }
    }
}

void updateScreen(uint8_t* fb_ptr, uint8_t* buffer_ptr, fb_var_screeninfo vinfo, fb_fix_screeninfo finfo) {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                            (y + vinfo.yoffset) * finfo.line_length;
            *((uint16_t*)(fb_ptr + location)) = *((uint16_t*)(buffer_ptr + location));
        }
    }
}

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
    uint8_t * buffer_ptr = (uint8_t*)malloc(screensize);

    // 배경 색상 채우기
    fillBackground(buffer_ptr, vinfo, finfo, SKY_BLUE);

    // 땅 색상 채우기
    fillGround(buffer_ptr, vinfo, finfo, BROWN);
    
    updateScreen(fb_ptr, buffer_ptr, vinfo, finfo);

    // 플레이어 초기화
    Player player(100, HEIGHT - 60);

    // 간단한 이동 예제
    bool movingRight = true;
    for (int i = 0; i < 200; ++i) {
        // 배경 다시 그리기
        fillBackground(buffer_ptr, vinfo, finfo, SKY_BLUE);
        fillGround(buffer_ptr, vinfo, finfo, BROWN);
        updateScreen(fb_ptr, buffer_ptr, vinfo, finfo);

        // 플레이어 이동
        if (movingRight) {
            player.move(2);
            if (player.getX() > WIDTH - 10) movingRight = false;
        } else {
            player.move(-2);
            if (player.getX() < 0) movingRight = true;
        }

        // 플레이어 그리기
        player.draw(fb_ptr, vinfo, finfo);

        // 간단한 지연
        usleep(5000);
    }
    movingRight = false;
    for (int i = 0; i < 200; ++i) {
        // 배경 다시 그리기
        fillBackground(buffer_ptr, vinfo, finfo, SKY_BLUE);
        fillGround(buffer_ptr, vinfo, finfo, BROWN);
        updateScreen(fb_ptr, buffer_ptr, vinfo, finfo);

        // 플레이어 이동
        if (movingRight) {
            player.move(2);
            if (player.getX() > WIDTH - 10) movingRight = false;
        } else {
            player.move(-2);
            if (player.getX() < 0) movingRight = true;
        }

        // 플레이어 그리기
        player.draw(fb_ptr, vinfo, finfo);

        // 간단한 지연
        usleep(5000);
    }

    // 메모리 매핑 해제 및 파일 닫기
    munmap(fb_ptr, screensize);
    close(fb_fd);

    return 0;
}
