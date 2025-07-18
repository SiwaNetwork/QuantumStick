/*
 * TimeStick Control Utility
 * 
 * Утилита для управления PTP и 1PPS функциями устройства TimeStick.
 * Позволяет включать/выключать функции, настраивать параметры.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <ifaddrs.h>
#include <getopt.h>

// Константы из драйвера
#define AX_PRIVATE              SIOCDEVPRIVATE
#define AX88179A_SIGNATURE      "AX88179B_179A_772E_772D"
#define AX88179_SIGNATURE       "AX88179_178A"

// IOCTL команды
#define AX_SIGNATURE            0
#define AX_USB_COMMAND          1

// PTP команды
#define AX_PTP_CMD              0x09
#define AX_PTP_OP               0x0E
#define AX_SET_LOCAL_CLOCK      0x01
#define AX_SET_LOCAL_CLOCK_SIZE 0x0A
#define AX_GET_LOCAL_CLOCK      0x02
#define AX_GET_LOCAL_CLOCK_SIZE 0x0A
#define AX_SET_ADDEND           0x03
#define AX_SET_ADDEND_SIZE      0x04

// Регистры для управления 1PPS (AX88179A)
#define AX88179A_PPS_CTRL_REG   0x1894
#define AX88179A_PPS_ENABLE     0x01

// Регистры для управления 1PPS (AX88279)
#define AX88279_PPS_CTRL_REG    0xF8C8
#define AX88279_PPS_ENABLE      0x10

// USB операции
#define USB_READ_OPS            0
#define USB_WRITE_OPS           1

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

// Глобальные переменные
static char interface_name[IFNAMSIZ];
static char driver_signature[32];
static int sock_fd = -1;

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
                strncpy(interface_name, ifa->ifa_name, IFNAMSIZ - 1);
                strncpy(driver_signature, (char *)ioctl_cmd.sig, 31);
                found = 1;
                break;
            }
        }
    }
    
    close(temp_sock);
    freeifaddrs(ifaddr);
    
    return found ? 0 : -1;
}

// Чтение регистра через USB
int read_register(unsigned short reg, unsigned short *value) {
    struct ifreq ifr;
    struct _ax_ioctl_command ioctl_cmd;
    unsigned char data[2];
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    
    memset(&ioctl_cmd, 0, sizeof(ioctl_cmd));
    ioctl_cmd.ioctl_cmd = AX_USB_COMMAND;
    strncpy((char *)ioctl_cmd.sig, driver_signature, 31);
    
    ioctl_cmd.usb_cmd.ops = USB_READ_OPS;
    ioctl_cmd.usb_cmd.cmd = 0x11; // Read register command
    ioctl_cmd.usb_cmd.value = reg;
    ioctl_cmd.usb_cmd.index = 0;
    ioctl_cmd.usb_cmd.size = 2;
    ioctl_cmd.usb_cmd.data = data;
    
    ifr.ifr_data = (caddr_t)&ioctl_cmd;
    
    if (ioctl(sock_fd, AX_PRIVATE, &ifr) == 0) {
        *value = (data[0] << 8) | data[1];
        return 0;
    }
    
    return -1;
}

// Запись регистра через USB
int write_register(unsigned short reg, unsigned short value) {
    struct ifreq ifr;
    struct _ax_ioctl_command ioctl_cmd;
    unsigned char data[2];
    
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    
    memset(&ioctl_cmd, 0, sizeof(ioctl_cmd));
    ioctl_cmd.ioctl_cmd = AX_USB_COMMAND;
    strncpy((char *)ioctl_cmd.sig, driver_signature, 31);
    
    ioctl_cmd.usb_cmd.ops = USB_WRITE_OPS;
    ioctl_cmd.usb_cmd.cmd = 0x10; // Write register command
    ioctl_cmd.usb_cmd.value = reg;
    ioctl_cmd.usb_cmd.index = 0;
    ioctl_cmd.usb_cmd.size = 2;
    ioctl_cmd.usb_cmd.data = data;
    
    ifr.ifr_data = (caddr_t)&ioctl_cmd;
    
    return ioctl(sock_fd, AX_PRIVATE, &ifr);
}

// Управление 1PPS
int control_pps(int enable) {
    unsigned short reg;
    unsigned short value;
    unsigned short enable_bit;
    
    // Определяем регистр и бит в зависимости от чипа
    if (strstr(driver_signature, "AX88279") != NULL) {
        reg = AX88279_PPS_CTRL_REG;
        enable_bit = AX88279_PPS_ENABLE;
    } else {
        reg = AX88179A_PPS_CTRL_REG;
        enable_bit = AX88179A_PPS_ENABLE;
    }
    
    // Читаем текущее значение регистра
    if (read_register(reg, &value) < 0) {
        fprintf(stderr, "Failed to read PPS control register\n");
        return -1;
    }
    
    // Модифицируем значение
    if (enable) {
        value |= enable_bit;
    } else {
        value &= ~enable_bit;
    }
    
    // Записываем обратно
    if (write_register(reg, value) < 0) {
        fprintf(stderr, "Failed to write PPS control register\n");
        return -1;
    }
    
    printf("1PPS %s successfully\n", enable ? "enabled" : "disabled");
    return 0;
}

// Получение PTP времени
int get_ptp_time() {
    struct ifreq ifr;
    struct _ax_ioctl_command ioctl_cmd;
    unsigned char data[AX_GET_LOCAL_CLOCK_SIZE];
    uint32_t seconds_hi, seconds_lo, nanoseconds;
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    
    memset(&ioctl_cmd, 0, sizeof(ioctl_cmd));
    ioctl_cmd.ioctl_cmd = AX_USB_COMMAND;
    strncpy((char *)ioctl_cmd.sig, driver_signature, 31);
    
    ioctl_cmd.usb_cmd.ops = USB_READ_OPS;
    ioctl_cmd.usb_cmd.cmd = AX_PTP_CMD;
    ioctl_cmd.usb_cmd.value = AX_GET_LOCAL_CLOCK;
    ioctl_cmd.usb_cmd.index = 0;
    ioctl_cmd.usb_cmd.size = AX_GET_LOCAL_CLOCK_SIZE;
    ioctl_cmd.usb_cmd.data = data;
    
    ifr.ifr_data = (caddr_t)&ioctl_cmd;
    
    if (ioctl(sock_fd, AX_PRIVATE, &ifr) == 0) {
        seconds_hi = (data[0] << 8) | data[1];
        seconds_lo = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
        nanoseconds = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
        
        uint64_t total_seconds = ((uint64_t)seconds_hi << 32) | seconds_lo;
        time_t time_sec = (time_t)total_seconds;
        
        struct tm *tm_info = gmtime(&time_sec);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        printf("PTP Time: %s.%09u UTC\n", time_str, nanoseconds);
        printf("Raw: 0x%04X%08X.%09u\n", seconds_hi, seconds_lo, nanoseconds);
        
        return 0;
    }
    
    fprintf(stderr, "Failed to get PTP time\n");
    return -1;
}

// Синхронизация PTP времени с системным
int sync_ptp_time() {
    struct ifreq ifr;
    struct _ax_ioctl_command ioctl_cmd;
    unsigned char data[AX_SET_LOCAL_CLOCK_SIZE];
    struct timespec ts;
    uint64_t seconds;
    uint32_t nanoseconds;
    
    // Получаем текущее системное время
    if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
        perror("clock_gettime");
        return -1;
    }
    
    seconds = (uint64_t)ts.tv_sec;
    nanoseconds = (uint32_t)ts.tv_nsec;
    
    // Подготавливаем данные для отправки
    data[0] = (seconds >> 40) & 0xFF;
    data[1] = (seconds >> 32) & 0xFF;
    data[2] = (seconds >> 24) & 0xFF;
    data[3] = (seconds >> 16) & 0xFF;
    data[4] = (seconds >> 8) & 0xFF;
    data[5] = seconds & 0xFF;
    data[6] = (nanoseconds >> 24) & 0xFF;
    data[7] = (nanoseconds >> 16) & 0xFF;
    data[8] = (nanoseconds >> 8) & 0xFF;
    data[9] = nanoseconds & 0xFF;
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    
    memset(&ioctl_cmd, 0, sizeof(ioctl_cmd));
    ioctl_cmd.ioctl_cmd = AX_USB_COMMAND;
    strncpy((char *)ioctl_cmd.sig, driver_signature, 31);
    
    ioctl_cmd.usb_cmd.ops = USB_WRITE_OPS;
    ioctl_cmd.usb_cmd.cmd = AX_PTP_CMD;
    ioctl_cmd.usb_cmd.value = AX_SET_LOCAL_CLOCK;
    ioctl_cmd.usb_cmd.index = 0;
    ioctl_cmd.usb_cmd.size = AX_SET_LOCAL_CLOCK_SIZE;
    ioctl_cmd.usb_cmd.data = data;
    
    ifr.ifr_data = (caddr_t)&ioctl_cmd;
    
    if (ioctl(sock_fd, AX_PRIVATE, &ifr) == 0) {
        printf("PTP time synchronized with system time\n");
        return 0;
    }
    
    fprintf(stderr, "Failed to sync PTP time\n");
    return -1;
}

// Установка PTP частотной коррекции (addend)
int set_ptp_addend(uint32_t addend) {
    struct ifreq ifr;
    struct _ax_ioctl_command ioctl_cmd;
    unsigned char data[AX_SET_ADDEND_SIZE];
    
    data[0] = (addend >> 24) & 0xFF;
    data[1] = (addend >> 16) & 0xFF;
    data[2] = (addend >> 8) & 0xFF;
    data[3] = addend & 0xFF;
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    
    memset(&ioctl_cmd, 0, sizeof(ioctl_cmd));
    ioctl_cmd.ioctl_cmd = AX_USB_COMMAND;
    strncpy((char *)ioctl_cmd.sig, driver_signature, 31);
    
    ioctl_cmd.usb_cmd.ops = USB_WRITE_OPS;
    ioctl_cmd.usb_cmd.cmd = AX_PTP_CMD;
    ioctl_cmd.usb_cmd.value = AX_SET_ADDEND;
    ioctl_cmd.usb_cmd.index = 0;
    ioctl_cmd.usb_cmd.size = AX_SET_ADDEND_SIZE;
    ioctl_cmd.usb_cmd.data = data;
    
    ifr.ifr_data = (caddr_t)&ioctl_cmd;
    
    if (ioctl(sock_fd, AX_PRIVATE, &ifr) == 0) {
        printf("PTP addend set to 0x%08X\n", addend);
        return 0;
    }
    
    fprintf(stderr, "Failed to set PTP addend\n");
    return -1;
}

// Показать помощь
void show_help(const char *prog_name) {
    printf("TimeStick Control Utility\n");
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  -h, --help              Show this help\n");
    printf("  -i, --info              Show device information\n");
    printf("  -p, --pps <on|off>      Enable/disable 1PPS output\n");
    printf("  -t, --time              Get current PTP time\n");
    printf("  -s, --sync              Sync PTP time with system time\n");
    printf("  -a, --addend <value>    Set PTP frequency adjustment (hex)\n");
    printf("  -m, --monitor           Monitor PTP time (continuous)\n");
    printf("\nExamples:\n");
    printf("  %s -i                   # Show device info\n", prog_name);
    printf("  %s -p on                # Enable 1PPS output\n", prog_name);
    printf("  %s -t                   # Get PTP time\n", prog_name);
    printf("  %s -s                   # Sync PTP with system\n", prog_name);
    printf("  %s -a 0xCCCCCCCC        # Set frequency adjustment\n", prog_name);
}

// Показать информацию об устройстве
void show_device_info() {
    printf("TimeStick Device Information\n");
    printf("===========================\n");
    printf("Interface: %s\n", interface_name);
    printf("Driver: %s\n", driver_signature);
    
    // Проверяем статус 1PPS
    unsigned short reg, value;
    if (strstr(driver_signature, "AX88279") != NULL) {
        reg = AX88279_PPS_CTRL_REG;
    } else {
        reg = AX88179A_PPS_CTRL_REG;
    }
    
    if (read_register(reg, &value) == 0) {
        printf("1PPS Status: %s\n", (value & 0x01) ? "Enabled" : "Disabled");
    }
    
    // Показываем текущее PTP время
    printf("\n");
    get_ptp_time();
}

// Мониторинг PTP времени
void monitor_ptp_time() {
    printf("Monitoring PTP time (press Ctrl+C to stop)...\n\n");
    
    while (1) {
        printf("\r");
        get_ptp_time();
        printf("\033[1A"); // Переместить курсор на строку вверх
        fflush(stdout);
        sleep(1);
    }
}

int main(int argc, char *argv[]) {
    int opt;
    int action = 0;
    char *pps_state = NULL;
    uint32_t addend = 0;
    int has_addend = 0;
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"info", no_argument, 0, 'i'},
        {"pps", required_argument, 0, 'p'},
        {"time", no_argument, 0, 't'},
        {"sync", no_argument, 0, 's'},
        {"addend", required_argument, 0, 'a'},
        {"monitor", no_argument, 0, 'm'},
        {0, 0, 0, 0}
    };
    
    if (argc == 1) {
        show_help(argv[0]);
        return 0;
    }
    
    while ((opt = getopt_long(argc, argv, "hip:tsa:m", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                show_help(argv[0]);
                return 0;
            case 'i':
                action = 'i';
                break;
            case 'p':
                action = 'p';
                pps_state = optarg;
                break;
            case 't':
                action = 't';
                break;
            case 's':
                action = 's';
                break;
            case 'a':
                action = 'a';
                addend = strtoul(optarg, NULL, 0);
                has_addend = 1;
                break;
            case 'm':
                action = 'm';
                break;
            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
        }
    }
    
    // Поиск устройства
    if (find_timestick_interface() < 0) {
        fprintf(stderr, "TimeStick device not found!\n");
        return 1;
    }
    
    // Открываем сокет
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }
    
    // Выполняем действие
    int ret = 0;
    switch (action) {
        case 'i':
            show_device_info();
            break;
        case 'p':
            if (strcmp(pps_state, "on") == 0) {
                ret = control_pps(1);
            } else if (strcmp(pps_state, "off") == 0) {
                ret = control_pps(0);
            } else {
                fprintf(stderr, "Invalid PPS state: %s (use 'on' or 'off')\n", pps_state);
                ret = 1;
            }
            break;
        case 't':
            ret = get_ptp_time();
            break;
        case 's':
            ret = sync_ptp_time();
            break;
        case 'a':
            if (has_addend) {
                ret = set_ptp_addend(addend);
            }
            break;
        case 'm':
            monitor_ptp_time();
            break;
        default:
            show_help(argv[0]);
            break;
    }
    
    close(sock_fd);
    return ret;
}