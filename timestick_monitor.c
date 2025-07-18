/*
 * TimeStick Device Monitor
 * 
 * Программа для мониторинга устройства TimeStick на HOST компьютере.
 * Отслеживает состояние устройства, PTP синхронизацию и сигнал 1PPS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <linux/ptp_clock.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <ncurses.h>

// Константы из драйвера
#define AX_PRIVATE              SIOCDEVPRIVATE
#define AX88179A_SIGNATURE      "AX88179B_179A_772E_772D"
#define AX88179_SIGNATURE       "AX88179_178A"

// IOCTL команды
#define AX_SIGNATURE            0
#define AX_USB_COMMAND          1
#define AX88179A_READ_VERSION   2
#define AX88179A_IEEE_TEST      10

// Структуры из драйвера
struct _ax_usb_command {
    unsigned char   ops;
    unsigned char   cmd;
    unsigned short  value;
    unsigned short  index;
    unsigned short  size;
    unsigned char   *data;
    unsigned long   cmd_data;
};

struct _ax88179a_version {
    unsigned char version[16];
};

struct _ax_ioctl_command {
    unsigned short  ioctl_cmd;
    unsigned char   sig[32];
    unsigned char   type;
    unsigned short  *buf;
    unsigned short  size;
    unsigned char   delay;
    union {
        struct _ax88179a_version    version;
        struct _ax_usb_command      usb_cmd;
    };
};

// Структура для хранения информации о устройстве
typedef struct {
    char interface_name[IFNAMSIZ];
    char driver_signature[32];
    char version[16];
    int is_connected;
    int ptp_enabled;
    int pps_enabled;
    struct timespec last_pps_time;
    long long ptp_offset_ns;
    double link_speed_mbps;
    unsigned long rx_packets;
    unsigned long tx_packets;
    unsigned long rx_bytes;
    unsigned long tx_bytes;
    unsigned long rx_errors;
    unsigned long tx_errors;
} timestick_device_t;

// Глобальные переменные
static volatile int running = 1;
static timestick_device_t device_info;
static pthread_mutex_t device_mutex = PTHREAD_MUTEX_INITIALIZER;
static int sock_fd = -1;

// Обработчик сигналов
void signal_handler(int sig) {
    running = 0;
}

// Поиск интерфейса TimeStick
int find_timestick_interface(char *interface_name) {
    struct ifaddrs *ifaddr, *ifa;
    struct ifreq ifr;
    struct _ax_ioctl_command ioctl_cmd;
    int found = 0;
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }
    
    // Создаем сокет для IOCTL
    int temp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (temp_sock < 0) {
        perror("socket");
        freeifaddrs(ifaddr);
        return -1;
    }
    
    // Проверяем каждый интерфейс
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        
        if (ifa->ifa_addr->sa_family != AF_PACKET)
            continue;
        
        // Подготавливаем IOCTL запрос
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
        
        memset(&ioctl_cmd, 0, sizeof(ioctl_cmd));
        ioctl_cmd.ioctl_cmd = AX_SIGNATURE;
        ifr.ifr_data = (caddr_t)&ioctl_cmd;
        
        // Отправляем IOCTL для получения сигнатуры драйвера
        if (ioctl(temp_sock, AX_PRIVATE, &ifr) == 0) {
            // Проверяем сигнатуру
            if (strstr((char *)ioctl_cmd.sig, "AX88179") != NULL ||
                strstr((char *)ioctl_cmd.sig, "AX88279") != NULL) {
                strncpy(interface_name, ifa->ifa_name, IFNAMSIZ - 1);
                strncpy(device_info.driver_signature, (char *)ioctl_cmd.sig, 31);
                found = 1;
                break;
            }
        }
    }
    
    close(temp_sock);
    freeifaddrs(ifaddr);
    
    return found ? 0 : -1;
}

// Получение версии устройства
int get_device_version() {
    struct ifreq ifr;
    struct _ax_ioctl_command ioctl_cmd;
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, device_info.interface_name, IFNAMSIZ - 1);
    
    memset(&ioctl_cmd, 0, sizeof(ioctl_cmd));
    ioctl_cmd.ioctl_cmd = AX88179A_READ_VERSION;
    strncpy((char *)ioctl_cmd.sig, device_info.driver_signature, 31);
    
    ifr.ifr_data = (caddr_t)&ioctl_cmd;
    
    if (ioctl(sock_fd, AX_PRIVATE, &ifr) == 0) {
        strncpy(device_info.version, (char *)ioctl_cmd.version.version, 15);
        return 0;
    }
    
    return -1;
}

// Получение статистики сетевого интерфейса
int get_network_stats() {
    FILE *fp;
    char line[512];
    char iface[32];
    int found = 0;
    
    fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        return -1;
    }
    
    // Пропускаем заголовки
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%31[^:]:%lu %lu %*u %*u %*u %*u %*u %*u %lu %lu %*u %*u %*u %*u %*u %*u",
               iface,
               &device_info.rx_bytes,
               &device_info.rx_packets,
               &device_info.tx_bytes,
               &device_info.tx_packets);
        
        if (strcmp(iface, device_info.interface_name) == 0) {
            found = 1;
            break;
        }
    }
    
    fclose(fp);
    return found ? 0 : -1;
}

// Получение скорости соединения
int get_link_speed() {
    struct ifreq ifr;
    struct ethtool_cmd ecmd;
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, device_info.interface_name, IFNAMSIZ - 1);
    
    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (caddr_t)&ecmd;
    
    if (ioctl(sock_fd, SIOCETHTOOL, &ifr) == 0) {
        device_info.link_speed_mbps = ethtool_cmd_speed(&ecmd);
        return 0;
    }
    
    return -1;
}

// Проверка PTP статуса
int check_ptp_status() {
    char ptp_device[64];
    int ptp_fd;
    struct ptp_clock_caps caps;
    
    // Ищем PTP устройство для нашего интерфейса
    for (int i = 0; i < 10; i++) {
        snprintf(ptp_device, sizeof(ptp_device), "/dev/ptp%d", i);
        ptp_fd = open(ptp_device, O_RDONLY);
        
        if (ptp_fd >= 0) {
            if (ioctl(ptp_fd, PTP_CLOCK_GETCAPS, &caps) == 0) {
                // Проверяем, что это наше устройство
                if (strstr(caps.name, "asix") != NULL) {
                    device_info.ptp_enabled = 1;
                    close(ptp_fd);
                    return 0;
                }
            }
            close(ptp_fd);
        }
    }
    
    device_info.ptp_enabled = 0;
    return -1;
}

// Поток мониторинга устройства
void *monitor_thread(void *arg) {
    while (running) {
        pthread_mutex_lock(&device_mutex);
        
        // Обновляем статистику
        get_network_stats();
        get_link_speed();
        check_ptp_status();
        
        pthread_mutex_unlock(&device_mutex);
        
        sleep(1);
    }
    
    return NULL;
}

// Инициализация ncurses интерфейса
void init_ui() {
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);
    
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);
        init_pair(3, COLOR_RED, COLOR_BLACK);
        init_pair(4, COLOR_CYAN, COLOR_BLACK);
    }
}

// Отображение интерфейса
void display_ui() {
    clear();
    
    // Заголовок
    attron(A_BOLD | COLOR_PAIR(4));
    mvprintw(0, 0, "TimeStick Device Monitor v1.0");
    attroff(A_BOLD | COLOR_PAIR(4));
    
    mvprintw(2, 0, "═══════════════════════════════════════════════════════════════════");
    
    pthread_mutex_lock(&device_mutex);
    
    // Информация об устройстве
    mvprintw(4, 0, "Device Information:");
    mvprintw(5, 2, "Interface: %s", device_info.interface_name);
    mvprintw(6, 2, "Driver: %s", device_info.driver_signature);
    mvprintw(7, 2, "Version: %s", device_info.version);
    
    // Статус соединения
    mvprintw(9, 0, "Connection Status:");
    if (device_info.is_connected) {
        attron(COLOR_PAIR(1));
        mvprintw(10, 2, "Status: Connected");
        attroff(COLOR_PAIR(1));
        mvprintw(11, 2, "Speed: %.0f Mbps", device_info.link_speed_mbps);
    } else {
        attron(COLOR_PAIR(3));
        mvprintw(10, 2, "Status: Disconnected");
        attroff(COLOR_PAIR(3));
    }
    
    // PTP статус
    mvprintw(13, 0, "PTP Status:");
    if (device_info.ptp_enabled) {
        attron(COLOR_PAIR(1));
        mvprintw(14, 2, "PTP: Enabled");
        attroff(COLOR_PAIR(1));
        mvprintw(15, 2, "Offset: %lld ns", device_info.ptp_offset_ns);
    } else {
        attron(COLOR_PAIR(2));
        mvprintw(14, 2, "PTP: Disabled");
        attroff(COLOR_PAIR(2));
    }
    
    // 1PPS статус
    mvprintw(17, 0, "1PPS Status:");
    if (device_info.pps_enabled) {
        attron(COLOR_PAIR(1));
        mvprintw(18, 2, "1PPS: Active");
        attroff(COLOR_PAIR(1));
        if (device_info.last_pps_time.tv_sec > 0) {
            char time_str[64];
            struct tm *tm_info = localtime(&device_info.last_pps_time.tv_sec);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
            mvprintw(19, 2, "Last Pulse: %s.%09ld", time_str, device_info.last_pps_time.tv_nsec);
        }
    } else {
        attron(COLOR_PAIR(2));
        mvprintw(18, 2, "1PPS: Inactive");
        attroff(COLOR_PAIR(2));
    }
    
    // Статистика
    mvprintw(21, 0, "Network Statistics:");
    mvprintw(22, 2, "RX: %lu packets (%lu bytes)", device_info.rx_packets, device_info.rx_bytes);
    mvprintw(23, 2, "TX: %lu packets (%lu bytes)", device_info.tx_packets, device_info.tx_bytes);
    mvprintw(24, 2, "Errors: RX=%lu TX=%lu", device_info.rx_errors, device_info.tx_errors);
    
    pthread_mutex_unlock(&device_mutex);
    
    mvprintw(26, 0, "═══════════════════════════════════════════════════════════════════");
    mvprintw(27, 0, "Press 'q' to quit, 'r' to refresh");
    
    refresh();
}

int main(int argc, char *argv[]) {
    pthread_t monitor_tid;
    int ch;
    
    // Установка обработчиков сигналов
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("TimeStick Device Monitor starting...\n");
    
    // Инициализация структуры устройства
    memset(&device_info, 0, sizeof(device_info));
    
    // Поиск устройства TimeStick
    printf("Searching for TimeStick device...\n");
    if (find_timestick_interface(device_info.interface_name) < 0) {
        fprintf(stderr, "TimeStick device not found!\n");
        fprintf(stderr, "Please make sure the device is connected and driver is loaded.\n");
        return 1;
    }
    
    printf("Found TimeStick device on interface: %s\n", device_info.interface_name);
    device_info.is_connected = 1;
    
    // Открываем сокет для IOCTL
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }
    
    // Получаем версию устройства
    get_device_version();
    
    // Запускаем поток мониторинга
    if (pthread_create(&monitor_tid, NULL, monitor_thread, NULL) != 0) {
        perror("pthread_create");
        close(sock_fd);
        return 1;
    }
    
    // Инициализация UI
    init_ui();
    
    // Основной цикл
    while (running) {
        display_ui();
        
        ch = getch();
        if (ch == 'q' || ch == 'Q') {
            running = 0;
        } else if (ch == 'r' || ch == 'R') {
            // Принудительное обновление
            pthread_mutex_lock(&device_mutex);
            get_network_stats();
            get_link_speed();
            check_ptp_status();
            pthread_mutex_unlock(&device_mutex);
        }
        
        usleep(100000); // 100ms
    }
    
    // Завершение
    endwin();
    
    printf("\nShutting down...\n");
    
    pthread_join(monitor_tid, NULL);
    
    if (sock_fd >= 0) {
        close(sock_fd);
    }
    
    printf("Monitor stopped.\n");
    
    return 0;
}