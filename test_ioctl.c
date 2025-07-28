#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/sockios.h>

#define AX_PRIVATE SIOCDEVPRIVATE
#define AX_SIGNATURE 0

struct _ax_ioctl_command {
    unsigned short  ioctl_cmd;
    unsigned char   sig[32];
    unsigned char   type;
    unsigned short  *buf;
    unsigned short  size;
    unsigned char   delay;
};

int main() {
    struct ifreq ifr;
    struct _ax_ioctl_command ioctl_cmd;
    int sock_fd;
    
    printf("Testing IOCTL commands for ax88179_178a driver\n");
    
    // Создаем сокет
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }
    
    // Тестируем интерфейс enxf8e43b0004d1
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "enxf8e43b0004d1", IFNAMSIZ - 1);
    
    memset(&ioctl_cmd, 0, sizeof(ioctl_cmd));
    ioctl_cmd.ioctl_cmd = AX_SIGNATURE;
    ifr.ifr_data = (caddr_t)&ioctl_cmd;
    
    printf("Testing AX_SIGNATURE command...\n");
    if (ioctl(sock_fd, AX_PRIVATE, &ifr) == 0) {
        printf("✓ AX_SIGNATURE command successful\n");
        printf("Driver signature: %s\n", ioctl_cmd.sig);
    } else {
        printf("✗ AX_SIGNATURE command failed: %s\n", strerror(errno));
    }
    
    close(sock_fd);
    return 0;
} 