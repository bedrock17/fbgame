#include <iostream>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <poll.h>

#if !defined(uint8_t)
#define uint8_t unsigned char
#endif
#if !defined(uint16_t)
#define uint16_t unsigned short
#endif
#if !defined(uint32_t)
#define uint32_t unsigned int
#endif

// 화면 크기
const int WIDTH = 1280;
const int HEIGHT = 720;

const int GROUND_LEVEL = HEIGHT - 60;
const int BOUND_GRAVITY = -10;

// 색상 정의
struct Color {
    uint8_t r, g, b, a;
};

// 색상 상수
const Color SKY_BLUE = {135, 206, 235, 0};
const Color BROWN = {139, 69, 19, 0};
const Color RED = {255, 0, 0, 0};
const Color DARK_GREEN = {0, 100, 0, 0};
const Color DARK_GRAY = {169, 169, 169, 0};

const Color PLAYER_COLOR = RED;
const Color BLOCK_COLOR = DARK_GRAY;

// #define USE_FIXEL_FORMAT_32

#if defined(USE_FIXEL_FORMAT_32)
#define FIXEL_FORMAT uint32_t
    #define ARGB8888
    // #define RGBA8888
#else
#define FIXEL_FORMAT uint16_t
#endif

// 32비트 색상을 16비트 RGB565로 변환
FIXEL_FORMAT convertTo(Color color) {
    #if defined(USE_FIXEL_FORMAT_32)
    #if defined(RGBA8888)
    return (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
    #elif defined(ARGB8888)
    return ((255 - color.a) << 24) | (color.r << 16) | (color.g << 8) | color.b;
    #else
        #err
    #endif
    #else
    return ((color.r & 0xF8) << 8) | ((color.g & 0xFC) << 3) | (color.b >> 3);
    #endif
}

void fillRect(uint8_t* fb_ptr, fb_var_screeninfo vinfo, fb_fix_screeninfo finfo, int x, int y, int w, int h, FIXEL_FORMAT color) {
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            int px = x + i;
            int py = y + j;
            long location = (px + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                            (py + vinfo.yoffset) * finfo.line_length;
            *((FIXEL_FORMAT*)(fb_ptr + location)) = color;
        }
    }
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

    virtual void moveTo(int targetX) {
        x = targetX;
    }

    virtual void setY(int targetY) {
        y = targetY;
    }

    int getX() const { return x; }
    int getY() const { return y; }
};

// 플레이어 클래스
class Player : public Unit {
private:
    // y중력 가속도
    int gravity = 1;

public:
    const int width = 10;
    const int height = 10;

    Player(int startX, int startY) : Unit(startX, startY) {}

    void draw(uint8_t* fb_ptr, fb_var_screeninfo vinfo, fb_fix_screeninfo finfo) override {
        // 간단한 사각형으로 플레이어 그리기
        FIXEL_FORMAT playerColor = convertTo(PLAYER_COLOR);
        fillRect(fb_ptr, vinfo, finfo, x, y, width, height, playerColor);
    }

    int getGravity() {
        return this->gravity;
    }

    void setGravity(int g) {
        this->gravity = g;
    }
};

enum CrashCode {
    NONE = 0,
    TOP = 1,
    BOTTOM = 2,
    LEFT = 3,
    RIGHT = 4,
};

class Block: public Unit {
private:

public:
    int width, height;
    Block(int startX, int startY, int w, int h) : Unit(startX, startY), width(w), height(h) {}

    void draw(uint8_t* fb_ptr, fb_var_screeninfo vinfo, fb_fix_screeninfo finfo) override {
        FIXEL_FORMAT blockColor = convertTo(BLOCK_COLOR);
        fillRect(fb_ptr, vinfo, finfo, x, y, width, height, blockColor);
    }

    CrashCode checkCrash(Player player) {
        if (player.getX() + player.width >= x && player.getX() <= x + width) {
            if (player.getY() + player.height >= y && player.getY() <= y + height) {
                if (player.getY() + player.height >= y && player.getY() + player.height <= y + height) {
                    return TOP;
                }
                if (player.getY() >= y && player.getY() <= y + height) {
                    return BOTTOM;
                }
                if (player.getX() + player.width >= x && player.getX() + player.width <= x + width) {
                    return LEFT;
                }
                if (player.getX() >= x && player.getX() <= x + width) {
                    return RIGHT;
                }
            }
        }
        return NONE;
    }
};

// 배경 색상 채우기 함수
void fillBackground(uint8_t* fb_ptr, fb_var_screeninfo vinfo, fb_fix_screeninfo finfo, Color color) {
    FIXEL_FORMAT colorData = convertTo(color);
    fillRect(fb_ptr, vinfo, finfo, 0, 0, WIDTH, HEIGHT, colorData);
}

// 땅 색상 채우기 함수
void fillGround(uint8_t* fb_ptr, fb_var_screeninfo vinfo, fb_fix_screeninfo finfo, Color color) {
    FIXEL_FORMAT colorData = convertTo(color);
    fillRect(fb_ptr, vinfo, finfo, 0, HEIGHT - 50, WIDTH, 50, colorData);
}

// 입력 장치 열기
int openInputDevice(const std::string& device) {
    printf("openInputDevice: %s\n", device.c_str());
    int fd = open(device.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Error: cannot open input device " << device << "." << std::endl;
        return -1;
    }
    return fd;
}

// 화면 업데이트 함수
void updateScreen(uint8_t* fb_ptr, uint8_t* buffer_ptr, fb_var_screeninfo vinfo, fb_fix_screeninfo finfo) {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                            (y + vinfo.yoffset) * finfo.line_length;
            *((FIXEL_FORMAT*)(fb_ptr + location)) = *((FIXEL_FORMAT*)(buffer_ptr + location));
        }
    }
}

#include <termios.h>
void disableInputEcho() {
    printf("disableInputEcho\n");
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty); // 현재 터미널 속성 가져오기
    tty.c_lflag &= ~ECHO; // ECHO 플래그를 끄기
    tcsetattr(STDIN_FILENO, TCSANOW, &tty); // 변경된 속성 설정
}

void enableInputEcho() {
    printf("enableInputEcho\n");
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty); // 현재 터미널 속성 가져오기
    tty.c_lflag |= ECHO; // ECHO 플래그를 켜기
    tcsetattr(STDIN_FILENO, TCSANOW, &tty); // 변경된 속성 설정
}

int main() {
    atexit(enableInputEcho);
    disableInputEcho();
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
    long screensize = vinfo.yres_virtual * finfo.line_length * 2;
    printf("width = %d, height = %d, xres_virtual = %d, yres_virtual = %d\n",
           vinfo.xres, vinfo.yres, vinfo.xres_virtual, vinfo.yres_virtual);
    printf("screensize = %ld\n", screensize);
    printf("bits_per_pixel = %d\n", vinfo.bits_per_pixel);

    std::vector<Block> blocks;
    for (int i = 0; i < 10; i++) {
        int x = 120 + i * 100;
        int y = (HEIGHT - 80) - 20 * i;
        blocks.push_back(Block(x, y, 50, 10));
    }

    // 메모리 매핑
    uint8_t* fb_ptr = (uint8_t*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((intptr_t)fb_ptr == -1) {
        std::cerr << "Error: failed to map framebuffer device to memory." << std::endl;
        close(fb_fd);
        return 1;
    }
    uint8_t* buffer_ptr = (uint8_t*)malloc(screensize);

    // 입력 장치 파일 열기
    int keyboard_fd = -1;

    // 키보드 파일 찾기
    for (int eventid = 0; eventid < 32; ++eventid) {
        std::string device = "/dev/input/event" + std::to_string(eventid);
        int fd = openInputDevice(device);
        if (fd != -1) {
            keyboard_fd = fd;
            int flags = fcntl(keyboard_fd, F_GETFL, 0);
            fcntl(keyboard_fd, F_SETFL, flags | O_NONBLOCK);
            break;
        }
    }

    if (
        keyboard_fd == -1
    ) {
        munmap(fb_ptr, screensize);
        close(fb_fd);
        return 1;
    }

    // 플레이어 초기화
    Player player(100, GROUND_LEVEL);

    // 키 상태를 저장할 플래그
    bool key_left_pressed = false;
    bool key_right_pressed = false;

    // pollfd 구조체 설정
    struct pollfd fds;
    fds.fd = keyboard_fd;
    fds.events = POLLIN;

    // 이벤트 루프
    bool running = true;
    while (running) {
        struct input_event ev;

        // poll 함수를 사용하여 키보드 이벤트 폴링
        int ret = poll(&fds, 1, 16); // 16ms 타임아웃 (약 60 FPS)
        if (ret > 0) {
            if (fds.revents & POLLIN) {
                if (read(keyboard_fd, &ev, sizeof(ev)) > 0) {
                    if (ev.type == EV_KEY) {
                        if (ev.value == 1) { // 키가 눌림
                            switch (ev.code) {
                                case KEY_LEFT:
                                    key_left_pressed = true;
                                    break;
                                case KEY_RIGHT:
                                    key_right_pressed = true;
                                    break;
                                case KEY_ESC:
                                    running = false;
                                    break;
                            }
                        } else if (ev.value == 0) { // 키가 떼어짐
                            switch (ev.code) {
                                case KEY_LEFT:
                                    key_left_pressed = false;
                                    break;
                                case KEY_RIGHT:
                                    key_right_pressed = false;
                                    break;
                            }
                        }
                    }
                }
            }
        }

        // 키 상태에 따라 플레이어 이동
        if (key_left_pressed) {
            player.move(-5);
        }
        if (key_right_pressed) {
            player.move(5);
        }

        player.setGravity(player.getGravity() + 1);
        player.setY(player.getY() + player.getGravity());

        if (player.getY() >= GROUND_LEVEL) {
            player.setY(GROUND_LEVEL);
            player.setGravity(BOUND_GRAVITY);
        }

        // 배경 및 땅 다시 그리기
        fillBackground(buffer_ptr, vinfo, finfo, SKY_BLUE);
        fillGround(buffer_ptr, vinfo, finfo, BROWN);

        for (Block block : blocks) {
            block.draw(buffer_ptr, vinfo, finfo);
            CrashCode code = block.checkCrash(player);
            switch (code) {
                case TOP:
                    player.setGravity(0);
                    player.setY(block.getY() - player.height);
                    break;
                case BOTTOM:
                    player.setGravity(BOUND_GRAVITY);
                    player.setY(block.getY() + block.height);
                    break;
                case LEFT:
                    player.move(block.getX() - player.width);
                    break;
                case RIGHT:
                    player.move(block.getX() + block.width);
                    break;
            }
        }

        // 플레이어 그리기
        player.draw(buffer_ptr, vinfo, finfo);
        updateScreen(fb_ptr, buffer_ptr, vinfo, finfo);

        // 간단한 지연
        // usleep(16000); // 약 60 FPS
    }

    // 메모리 매핑 해제 및 파일 닫기
    munmap(fb_ptr, screensize);
    free(buffer_ptr);
    close(fb_fd);
    close(keyboard_fd);

    return 0;
}
