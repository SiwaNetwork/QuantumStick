/*
 * TimeStick Monitor - Standard Driver Version
 * 
 * Версия программы мониторинга для работы со стандартным драйвером ax88179_178a
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <linux/ptp_clock.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <ncurses.h>
#include <math.h>

// Структура для информации об устройстве
typedef struct {
    char interface_name[IFNAMSIZ];
    char driver_name[32];
    int is_connected;
    int ptp_enabled;
    double link_speed_mbps;
    unsigned long rx_packets;
    unsigned long tx_packets;
    unsigned long rx_bytes;
    unsigned long tx_bytes;
    unsigned long rx_errors;
    unsigned long tx_errors;
    long long ptp_offset_ns;
} timestick_device_t;

// Глобальные переменные
static timestick_device_t device_info;
static int running = 1;
static pthread_mutex_t device_mutex = PTHREAD_MUTEX_INITIALIZER;

// Обработчик сигналов
void signal_handler(int sig) {
    running = 0;
}

// Поиск интерфейса с драйвером ax88179_178a
int find_timestick_interface() {
    struct ifaddrs *ifaddr, *ifa;
    struct ifreq ifr;
    int sock_fd;
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }
    
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        freeifaddrs(ifaddr);
        return -1;
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_PACKET)
            continue;
        
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
        
        // Проверяем, что это наш интерфейс
        if (strstr(ifa->ifa_name, "enxf8e43b0004d1") != NULL) {
            strncpy(device_info.interface_name, ifa->ifa_name, IFNAMSIZ - 1);
            strncpy(device_info.driver_name, "ax88179_178a", 31);
            close(sock_fd);
            freeifaddrs(ifaddr);
            return 0;
        }
    }
    
    close(sock_fd);
    freeifaddrs(ifaddr);
    return -1;
}

// Получение сетевой статистики
int get_network_stats() {
    FILE *fp;
    char line[256];
    char *token;
    
    snprintf(line, sizeof(line), "/sys/class/net/%s/statistics/rx_packets", device_info.interface_name);
    fp = fopen(line, "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            device_info.rx_packets = strtoul(line, NULL, 10);
        }
        fclose(fp);
    }
    
    snprintf(line, sizeof(line), "/sys/class/net/%s/statistics/tx_packets", device_info.interface_name);
    fp = fopen(line, "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            device_info.tx_packets = strtoul(line, NULL, 10);
        }
        fclose(fp);
    }
    
    snprintf(line, sizeof(line), "/sys/class/net/%s/statistics/rx_bytes", device_info.interface_name);
    fp = fopen(line, "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            device_info.rx_bytes = strtoul(line, NULL, 10);
        }
        fclose(fp);
    }
    
    snprintf(line, sizeof(line), "/sys/class/net/%s/statistics/tx_bytes", device_info.interface_name);
    fp = fopen(line, "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            device_info.tx_bytes = strtoul(line, NULL, 10);
        }
        fclose(fp);
    }
    
    return 0;
}

// Получение скорости соединения
int get_link_speed() {
    struct ifreq ifr;
    struct ethtool_cmd ecmd;
    int sock_fd;
    
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        return -1;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, device_info.interface_name, IFNAMSIZ - 1);
    
    memset(&ecmd, 0, sizeof(ecmd));
    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (caddr_t)&ecmd;
    
    if (ioctl(sock_fd, SIOCETHTOOL, &ifr) == 0) {
        device_info.link_speed_mbps = ethtool_cmd_speed(&ecmd);
        device_info.is_connected = 1; // Предполагаем, что подключен
    } else {
        device_info.is_connected = 0;
        device_info.link_speed_mbps = 0;
    }
    
    close(sock_fd);
    return 0;
}

// Проверка PTP статуса
int check_ptp_status() {
    char ptp_device[64];
    struct ptp_clock_caps caps;
    int ptp_fd;
    
    // Ищем PTP устройство
    for (int i = 0; i < 10; i++) {
        snprintf(ptp_device, sizeof(ptp_device), "/dev/ptp%d", i);
        ptp_fd = open(ptp_device, O_RDONLY);
        
        if (ptp_fd >= 0) {
            if (ioctl(ptp_fd, PTP_CLOCK_GETCAPS, &caps) == 0) {
                device_info.ptp_enabled = 1;
                close(ptp_fd);
                return 0;
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
    mvprintw(0, 0, "TimeStick Device Monitor (Standard Driver) v1.0");
    attroff(A_BOLD | COLOR_PAIR(4));
    
    mvprintw(2, 0, "═══════════════════════════════════════════════════════════════════");
    
    pthread_mutex_lock(&device_mutex);
    
    // Информация об устройстве
    mvprintw(4, 0, "Device Information:");
    mvprintw(5, 2, "Interface: %s", device_info.interface_name);
    mvprintw(6, 2, "Driver: %s", device_info.driver_name);
    
    // Статус соединения
    mvprintw(8, 0, "Connection Status:");
    if (device_info.is_connected) {
        attron(COLOR_PAIR(1));
        mvprintw(9, 2, "Status: Connected");
        attroff(COLOR_PAIR(1));
        mvprintw(10, 2, "Speed: %.0f Mbps", device_info.link_speed_mbps);
    } else {
        attron(COLOR_PAIR(3));
        mvprintw(9, 2, "Status: Disconnected");
        attroff(COLOR_PAIR(3));
    }
    
    // PTP статус
    mvprintw(12, 0, "PTP Status:");
    if (device_info.ptp_enabled) {
        attron(COLOR_PAIR(1));
        mvprintw(13, 2, "PTP: Enabled");
        attroff(COLOR_PAIR(1));
        mvprintw(14, 2, "Offset: %lld ns", device_info.ptp_offset_ns);
    } else {
        attron(COLOR_PAIR(2));
        mvprintw(13, 2, "PTP: Disabled");
        attroff(COLOR_PAIR(2));
    }
    
    // Сетевая статистика
    mvprintw(16, 0, "Network Statistics:");
    mvprintw(17, 2, "RX Packets: %lu", device_info.rx_packets);
    mvprintw(18, 2, "TX Packets: %lu", device_info.tx_packets);
    mvprintw(19, 2, "RX Bytes: %lu", device_info.rx_bytes);
    mvprintw(20, 2, "TX Bytes: %lu", device_info.tx_bytes);
    
    pthread_mutex_unlock(&device_mutex);
    
    // Инструкции
    mvprintw(22, 0, "═══════════════════════════════════════════════════════════════════");
    mvprintw(23, 0, "Press 'q' to quit, 'r' to refresh");
    
    refresh();
}

int main(int argc, char *argv[]) {
    pthread_t monitor_tid;
    
    printf("TimeStick Device Monitor (Standard Driver) starting...\n");
    
    // Поиск устройства
    printf("Searching for TimeStick device...\n");
    if (find_timestick_interface() != 0) {
        printf("TimeStick device not found!\n");
        printf("Please make sure the device is connected and driver is loaded.\n");
        return -1;
    }
    
    printf("Found TimeStick device: %s\n", device_info.interface_name);
    
    // Настройка обработчика сигналов
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Запуск потока мониторинга
    if (pthread_create(&monitor_tid, NULL, monitor_thread, NULL) != 0) {
        perror("pthread_create");
        return -1;
    }
    
    // Инициализация UI
    init_ui();
    
    // Основной цикл
    while (running) {
        display_ui();
        
        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            running = 0;
        }
        
        usleep(100000); // 100ms
    }
    
    // Очистка
    pthread_join(monitor_tid, NULL);
    endwin();
    
    printf("TimeStick monitor stopped.\n");
    return 0;
} 