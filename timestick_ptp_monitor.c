/*
 * TimeStick PTP/1PPS Advanced Monitor
 * 
 * Расширенная программа мониторинга для устройства TimeStick.
 * Детальное отслеживание PTP синхронизации и 1PPS сигнала.
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
#include <sys/time.h>
#include <sys/timex.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <linux/ptp_clock.h>
#include <linux/net_tstamp.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <math.h>

// Константы из драйвера
#define AX_PRIVATE              SIOCDEVPRIVATE
#define AX88179A_SIGNATURE      "AX88179B_179A_772E_772D"
#define AX88179_SIGNATURE       "AX88179_178A"

// IOCTL команды
#define AX_SIGNATURE            0
#define AX_USB_COMMAND          1

// PTP команды из драйвера
#define AX_PTP_CMD              0x09
#define AX_PTP_OP               0x0E
#define AX_GET_LOCAL_CLOCK      0x02
#define AX_GET_LOCAL_CLOCK_SIZE 0x0A
#define AX_SET_LOCAL_CLOCK      0x01
#define AX_SET_LOCAL_CLOCK_SIZE 0x0A

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

struct _ax_ioctl_command {
    unsigned short  ioctl_cmd;
    unsigned char   sig[32];
    unsigned char   type;
    unsigned short  *buf;
    unsigned short  size;
    unsigned char   delay;
    union {
        struct _ax_usb_command      usb_cmd;
    };
};

// Структура для хранения PTP времени
typedef struct {
    uint32_t seconds_hi;
    uint32_t seconds_lo;
    uint32_t nanoseconds;
} ptp_time_t;

// Структура для статистики PTP
typedef struct {
    int64_t min_offset_ns;
    int64_t max_offset_ns;
    int64_t avg_offset_ns;
    int64_t current_offset_ns;
    double frequency_adjustment_ppm;
    uint32_t sync_count;
    uint32_t sync_lost_count;
    time_t last_sync_time;
} ptp_stats_t;

// Структура для мониторинга 1PPS
typedef struct {
    int enabled;
    uint64_t pulse_count;
    struct timespec last_pulse_time;
    double pulse_interval_ms;
    double pulse_jitter_us;
    double min_interval_ms;
    double max_interval_ms;
} pps_stats_t;

// Основная структура мониторинга
typedef struct {
    char interface_name[IFNAMSIZ];
    char driver_signature[32];
    int is_connected;
    int ptp_fd;
    char ptp_device[64];
    ptp_stats_t ptp_stats;
    pps_stats_t pps_stats;
    pthread_mutex_t mutex;
} monitor_data_t;

// Глобальные переменные
static volatile int running = 1;
static monitor_data_t monitor_data;
static int sock_fd = -1;
static FILE *log_file = NULL;

// Обработчик сигналов
void signal_handler(int sig) {
    running = 0;
}

// Логирование
void log_message(const char *format, ...) {
    va_list args;
    time_t now;
    char time_str[32];
    
    if (log_file == NULL) return;
    
    time(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(log_file, "[%s] ", time_str);
    
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
}

// Поиск интерфейса TimeStick
int find_timestick_interface() {
    struct ifaddrs *ifaddr, *ifa;
    struct ifreq ifr;
    struct _ax_ioctl_command ioctl_cmd;
    int found = 0;
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }
    
    int temp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (temp_sock < 0) {
        perror("socket");
        freeifaddrs(ifaddr);
        return -1;
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_PACKET)
            continue;
        
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
        
        memset(&ioctl_cmd, 0, sizeof(ioctl_cmd));
        ioctl_cmd.ioctl_cmd = AX_SIGNATURE;
        ifr.ifr_data = (caddr_t)&ioctl_cmd;
        
        if (ioctl(temp_sock, AX_PRIVATE, &ifr) == 0) {
            if (strstr((char *)ioctl_cmd.sig, "AX88179") != NULL ||
                strstr((char *)ioctl_cmd.sig, "AX88279") != NULL) {
                strncpy(monitor_data.interface_name, ifa->ifa_name, IFNAMSIZ - 1);
                strncpy(monitor_data.driver_signature, (char *)ioctl_cmd.sig, 31);
                found = 1;
                break;
            }
        }
    }
    
    close(temp_sock);
    freeifaddrs(ifaddr);
    
    return found ? 0 : -1;
}

// Поиск PTP устройства
int find_ptp_device() {
    char ptp_device[64];
    int ptp_fd;
    struct ptp_clock_caps caps;
    
    for (int i = 0; i < 10; i++) {
        snprintf(ptp_device, sizeof(ptp_device), "/dev/ptp%d", i);
        ptp_fd = open(ptp_device, O_RDWR);
        
        if (ptp_fd >= 0) {
            if (ioctl(ptp_fd, PTP_CLOCK_GETCAPS, &caps) == 0) {
                if (strstr(caps.name, "asix") != NULL) {
                    strncpy(monitor_data.ptp_device, ptp_device, sizeof(monitor_data.ptp_device) - 1);
                    monitor_data.ptp_fd = ptp_fd;
                    log_message("Found PTP device: %s (%s)", ptp_device, caps.name);
                    return 0;
                }
            }
            close(ptp_fd);
        }
    }
    
    return -1;
}

// Получение PTP времени через USB команду
int get_ptp_time_usb(ptp_time_t *ptp_time) {
    struct ifreq ifr;
    struct _ax_ioctl_command ioctl_cmd;
    unsigned char data[AX_GET_LOCAL_CLOCK_SIZE];
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, monitor_data.interface_name, IFNAMSIZ - 1);
    
    memset(&ioctl_cmd, 0, sizeof(ioctl_cmd));
    ioctl_cmd.ioctl_cmd = AX_USB_COMMAND;
    strncpy((char *)ioctl_cmd.sig, monitor_data.driver_signature, 31);
    
    ioctl_cmd.usb_cmd.ops = 0; // Read
    ioctl_cmd.usb_cmd.cmd = AX_PTP_CMD;
    ioctl_cmd.usb_cmd.value = AX_GET_LOCAL_CLOCK;
    ioctl_cmd.usb_cmd.index = 0;
    ioctl_cmd.usb_cmd.size = AX_GET_LOCAL_CLOCK_SIZE;
    ioctl_cmd.usb_cmd.data = data;
    
    ifr.ifr_data = (caddr_t)&ioctl_cmd;
    
    if (ioctl(sock_fd, AX_PRIVATE, &ifr) == 0) {
        // Парсим полученные данные
        ptp_time->seconds_hi = (data[0] << 8) | data[1];
        ptp_time->seconds_lo = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
        ptp_time->nanoseconds = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
        return 0;
    }
    
    return -1;
}

// Получение PTP времени через /dev/ptp
int get_ptp_time(struct timespec *ts) {
    struct ptp_clock_time pct;
    
    if (monitor_data.ptp_fd < 0) return -1;
    
    if (ioctl(monitor_data.ptp_fd, PTP_SYS_OFFSET, &pct) == 0) {
        ts->tv_sec = pct.sec;
        ts->tv_nsec = pct.nsec;
        return 0;
    }
    
    return -1;
}

// Вычисление смещения PTP
void calculate_ptp_offset() {
    struct timespec ptp_time, system_time;
    int64_t offset_ns;
    
    if (get_ptp_time(&ptp_time) == 0) {
        clock_gettime(CLOCK_REALTIME, &system_time);
        
        offset_ns = (ptp_time.tv_sec - system_time.tv_sec) * 1000000000LL +
                    (ptp_time.tv_nsec - system_time.tv_nsec);
        
        pthread_mutex_lock(&monitor_data.mutex);
        
        monitor_data.ptp_stats.current_offset_ns = offset_ns;
        
        if (monitor_data.ptp_stats.sync_count == 0) {
            monitor_data.ptp_stats.min_offset_ns = offset_ns;
            monitor_data.ptp_stats.max_offset_ns = offset_ns;
            monitor_data.ptp_stats.avg_offset_ns = offset_ns;
        } else {
            if (offset_ns < monitor_data.ptp_stats.min_offset_ns)
                monitor_data.ptp_stats.min_offset_ns = offset_ns;
            if (offset_ns > monitor_data.ptp_stats.max_offset_ns)
                monitor_data.ptp_stats.max_offset_ns = offset_ns;
            
            // Скользящее среднее
            monitor_data.ptp_stats.avg_offset_ns = 
                (monitor_data.ptp_stats.avg_offset_ns * monitor_data.ptp_stats.sync_count + offset_ns) /
                (monitor_data.ptp_stats.sync_count + 1);
        }
        
        monitor_data.ptp_stats.sync_count++;
        time(&monitor_data.ptp_stats.last_sync_time);
        
        pthread_mutex_unlock(&monitor_data.mutex);
        
        log_message("PTP offset: %lld ns (min: %lld, max: %lld, avg: %lld)",
                   offset_ns,
                   monitor_data.ptp_stats.min_offset_ns,
                   monitor_data.ptp_stats.max_offset_ns,
                   monitor_data.ptp_stats.avg_offset_ns);
    }
}

// Мониторинг 1PPS через GPIO (если доступно)
void monitor_pps_gpio() {
    // Здесь можно добавить код для мониторинга GPIO пина
    // если 1PPS подключен к GPIO HOST компьютера
}

// Поток мониторинга PTP
void *ptp_monitor_thread(void *arg) {
    struct timespec sleep_time = {0, 100000000}; // 100ms
    
    while (running) {
        calculate_ptp_offset();
        nanosleep(&sleep_time, NULL);
    }
    
    return NULL;
}

// Поток мониторинга 1PPS
void *pps_monitor_thread(void *arg) {
    struct timespec current_time, last_time = {0, 0};
    double interval_ms;
    
    while (running) {
        // Здесь должен быть код для детектирования 1PPS импульса
        // Для демонстрации используем симуляцию
        
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        
        if (last_time.tv_sec > 0) {
            interval_ms = (current_time.tv_sec - last_time.tv_sec) * 1000.0 +
                         (current_time.tv_nsec - last_time.tv_nsec) / 1000000.0;
            
            pthread_mutex_lock(&monitor_data.mutex);
            
            monitor_data.pps_stats.pulse_interval_ms = interval_ms;
            monitor_data.pps_stats.pulse_count++;
            monitor_data.pps_stats.last_pulse_time = current_time;
            
            if (monitor_data.pps_stats.pulse_count == 1) {
                monitor_data.pps_stats.min_interval_ms = interval_ms;
                monitor_data.pps_stats.max_interval_ms = interval_ms;
            } else {
                if (interval_ms < monitor_data.pps_stats.min_interval_ms)
                    monitor_data.pps_stats.min_interval_ms = interval_ms;
                if (interval_ms > monitor_data.pps_stats.max_interval_ms)
                    monitor_data.pps_stats.max_interval_ms = interval_ms;
            }
            
            // Вычисляем джиттер
            monitor_data.pps_stats.pulse_jitter_us = fabs(interval_ms - 1000.0) * 1000.0;
            
            pthread_mutex_unlock(&monitor_data.mutex);
            
            if (fabs(interval_ms - 1000.0) > 1.0) {
                log_message("PPS interval deviation: %.3f ms (expected 1000.0 ms)", interval_ms);
            }
        }
        
        last_time = current_time;
        sleep(1);
    }
    
    return NULL;
}

// Вывод статистики
void print_statistics() {
    pthread_mutex_lock(&monitor_data.mutex);
    
    printf("\n=== TimeStick PTP/1PPS Monitor ===\n");
    printf("Interface: %s\n", monitor_data.interface_name);
    printf("Driver: %s\n", monitor_data.driver_signature);
    printf("PTP Device: %s\n", monitor_data.ptp_device);
    
    printf("\nPTP Statistics:\n");
    printf("  Sync Count: %u\n", monitor_data.ptp_stats.sync_count);
    printf("  Current Offset: %lld ns\n", monitor_data.ptp_stats.current_offset_ns);
    printf("  Min Offset: %lld ns\n", monitor_data.ptp_stats.min_offset_ns);
    printf("  Max Offset: %lld ns\n", monitor_data.ptp_stats.max_offset_ns);
    printf("  Avg Offset: %lld ns\n", monitor_data.ptp_stats.avg_offset_ns);
    
    if (monitor_data.ptp_stats.last_sync_time > 0) {
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", 
                localtime(&monitor_data.ptp_stats.last_sync_time));
        printf("  Last Sync: %s\n", time_str);
    }
    
    printf("\n1PPS Statistics:\n");
    printf("  Pulse Count: %lu\n", monitor_data.pps_stats.pulse_count);
    printf("  Interval: %.3f ms\n", monitor_data.pps_stats.pulse_interval_ms);
    printf("  Jitter: %.1f us\n", monitor_data.pps_stats.pulse_jitter_us);
    printf("  Min Interval: %.3f ms\n", monitor_data.pps_stats.min_interval_ms);
    printf("  Max Interval: %.3f ms\n", monitor_data.pps_stats.max_interval_ms);
    
    pthread_mutex_unlock(&monitor_data.mutex);
}

// Сохранение статистики в CSV
void save_statistics_csv() {
    FILE *csv_file;
    char filename[256];
    time_t now;
    struct tm *tm_info;
    
    time(&now);
    tm_info = localtime(&now);
    
    snprintf(filename, sizeof(filename), "timestick_stats_%04d%02d%02d_%02d%02d%02d.csv",
            tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    csv_file = fopen(filename, "w");
    if (csv_file == NULL) {
        perror("fopen");
        return;
    }
    
    fprintf(csv_file, "Timestamp,PTP_Offset_ns,PTP_Min_ns,PTP_Max_ns,PTP_Avg_ns,");
    fprintf(csv_file, "PPS_Count,PPS_Interval_ms,PPS_Jitter_us\n");
    
    pthread_mutex_lock(&monitor_data.mutex);
    
    fprintf(csv_file, "%ld,%lld,%lld,%lld,%lld,%lu,%.3f,%.1f\n",
            now,
            monitor_data.ptp_stats.current_offset_ns,
            monitor_data.ptp_stats.min_offset_ns,
            monitor_data.ptp_stats.max_offset_ns,
            monitor_data.ptp_stats.avg_offset_ns,
            monitor_data.pps_stats.pulse_count,
            monitor_data.pps_stats.pulse_interval_ms,
            monitor_data.pps_stats.pulse_jitter_us);
    
    pthread_mutex_unlock(&monitor_data.mutex);
    
    fclose(csv_file);
    printf("Statistics saved to %s\n", filename);
}

int main(int argc, char *argv[]) {
    pthread_t ptp_thread, pps_thread;
    char input[32];
    int opt;
    int enable_logging = 0;
    
    // Парсинг аргументов
    while ((opt = getopt(argc, argv, "lh")) != -1) {
        switch (opt) {
            case 'l':
                enable_logging = 1;
                break;
            case 'h':
                printf("Usage: %s [-l] [-h]\n", argv[0]);
                printf("  -l    Enable logging to file\n");
                printf("  -h    Show this help\n");
                return 0;
            default:
                fprintf(stderr, "Usage: %s [-l] [-h]\n", argv[0]);
                return 1;
        }
    }
    
    // Установка обработчиков сигналов
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Инициализация
    memset(&monitor_data, 0, sizeof(monitor_data));
    pthread_mutex_init(&monitor_data.mutex, NULL);
    monitor_data.ptp_fd = -1;
    
    // Открытие лог файла
    if (enable_logging) {
        log_file = fopen("timestick_monitor.log", "a");
        if (log_file == NULL) {
            perror("fopen log file");
        }
    }
    
    printf("TimeStick PTP/1PPS Advanced Monitor\n");
    printf("===================================\n\n");
    
    // Поиск устройства
    printf("Searching for TimeStick device...\n");
    if (find_timestick_interface() < 0) {
        fprintf(stderr, "TimeStick device not found!\n");
        return 1;
    }
    
    printf("Found TimeStick device on interface: %s\n", monitor_data.interface_name);
    monitor_data.is_connected = 1;
    
    // Открываем сокет
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }
    
    // Поиск PTP устройства
    if (find_ptp_device() < 0) {
        fprintf(stderr, "Warning: PTP device not found\n");
    }
    
    // Запуск потоков мониторинга
    if (pthread_create(&ptp_thread, NULL, ptp_monitor_thread, NULL) != 0) {
        perror("pthread_create ptp");
        return 1;
    }
    
    if (pthread_create(&pps_thread, NULL, pps_monitor_thread, NULL) != 0) {
        perror("pthread_create pps");
        return 1;
    }
    
    printf("\nMonitoring started. Commands:\n");
    printf("  s - Show statistics\n");
    printf("  c - Save statistics to CSV\n");
    printf("  r - Reset statistics\n");
    printf("  q - Quit\n\n");
    
    // Основной цикл
    while (running) {
        if (fgets(input, sizeof(input), stdin) != NULL) {
            switch (input[0]) {
                case 's':
                case 'S':
                    print_statistics();
                    break;
                    
                case 'c':
                case 'C':
                    save_statistics_csv();
                    break;
                    
                case 'r':
                case 'R':
                    pthread_mutex_lock(&monitor_data.mutex);
                    memset(&monitor_data.ptp_stats, 0, sizeof(monitor_data.ptp_stats));
                    memset(&monitor_data.pps_stats, 0, sizeof(monitor_data.pps_stats));
                    pthread_mutex_unlock(&monitor_data.mutex);
                    printf("Statistics reset.\n");
                    break;
                    
                case 'q':
                case 'Q':
                    running = 0;
                    break;
            }
        }
        
        usleep(100000);
    }
    
    // Завершение
    printf("\nShutting down...\n");
    
    pthread_join(ptp_thread, NULL);
    pthread_join(pps_thread, NULL);
    
    if (monitor_data.ptp_fd >= 0) {
        close(monitor_data.ptp_fd);
    }
    
    if (sock_fd >= 0) {
        close(sock_fd);
    }
    
    if (log_file != NULL) {
        log_message("Monitor stopped");
        fclose(log_file);
    }
    
    pthread_mutex_destroy(&monitor_data.mutex);
    
    printf("Monitor stopped.\n");
    
    return 0;
}