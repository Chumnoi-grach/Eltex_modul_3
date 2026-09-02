#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 65536
#define MAX_FILENAME 256

typedef struct {
    struct timeval timestamp;
    unsigned char src_mac[6];
    unsigned char dest_mac[6];
    struct in_addr src_ip;
    struct in_addr dest_ip;
    unsigned short src_port;
    unsigned short dest_port;
    unsigned char *data;
    int data_len;
} packet_info_t;

int raw_sock;
struct timeval start_time;
FILE *output_file;
int capture_running = 1;
char filter_type[10];

void signal_handler(int sig) {
    capture_running = 0;
    printf("\n\nЗахват остановлен. Сохранение данных...\n");
}

long long get_time_diff(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) * 1000000 + (end->tv_usec - start->tv_usec);
}

void format_mac(unsigned char *mac, char *buffer) {
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int check_filter(packet_info_t *packet) {
    if (strcmp(filter_type, "chat") == 0) {
        return (packet->src_port == 8888 || packet->dest_port == 8888);
    }
    else if (strcmp(filter_type, "dns") == 0) {
        return (packet->src_port == 53 || packet->dest_port == 53);
    }
    else if (strcmp(filter_type, "http") == 0) {
        return (packet->src_port == 80 || packet->dest_port == 80);
    }
    else if (strcmp(filter_type, "all") == 0) {
        return 1;
    }
    return 0;
}

void print_packet_info(packet_info_t *packet) {
    char src_mac_str[18], dest_mac_str[18];
    char src_ip_str[INET_ADDRSTRLEN], dest_ip_str[INET_ADDRSTRLEN];
    
    format_mac(packet->src_mac, src_mac_str);
    format_mac(packet->dest_mac, dest_mac_str);
    
    inet_ntop(AF_INET, &packet->src_ip, src_ip_str, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &packet->dest_ip, dest_ip_str, INET_ADDRSTRLEN);
    
    long long diff = get_time_diff(&start_time, &packet->timestamp);
    
    printf("\n[%lld.%06lld ms] ", diff / 1000, diff % 1000);
    printf("MAC: %s -> %s\n", src_mac_str, dest_mac_str);
    printf("      IP: %s -> %s\n", src_ip_str, dest_ip_str);
    printf("      UDP: %d -> %d", packet->src_port, packet->dest_port);
    
    if (packet->data_len > 0) {
        printf("\n      Data: ");
        int show_len = packet->data_len < 100 ? packet->data_len : 100;
        for (int i = 0; i < show_len; i++) {
            if (packet->data[i] >= 32 && packet->data[i] <= 126) {
                printf("%c", packet->data[i]);
            } else {
                printf(".");
            }
        }
        if (packet->data_len > 100) {
            printf("... (ещё %d байт)", packet->data_len - 100);
        }
    }
    printf("\n");
    printf("      ---\n");
    
    if (output_file != NULL) {
        fprintf(output_file, "[%lld.%06lld ms] ", diff / 1000, diff % 1000);
        fprintf(output_file, "MAC: %s -> %s\n", src_mac_str, dest_mac_str);
        fprintf(output_file, "      IP: %s -> %s\n", src_ip_str, dest_ip_str);
        fprintf(output_file, "      UDP: %d -> %d\n", packet->src_port, packet->dest_port);
        if (packet->data_len > 0) {
            fprintf(output_file, "      Data: ");
            int show_len = packet->data_len < 100 ? packet->data_len : 100;
            for (int i = 0; i < show_len; i++) {
                if (packet->data[i] >= 32 && packet->data[i] <= 126) {
                    fprintf(output_file, "%c", packet->data[i]);
                } else {
                    fprintf(output_file, ".");
                }
            }
            if (packet->data_len > 100) {
                fprintf(output_file, "... (ещё %d байт)", packet->data_len - 100);
            }
        }
        fprintf(output_file, "\n      ---\n\n");
        fflush(output_file);
    }
}

int parse_packet(unsigned char *buffer, int data_size, packet_info_t *packet) {
    if (data_size < sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr)) {
        return -1;
    }
    
    struct ethhdr *eth = (struct ethhdr*)buffer;
    
    if (ntohs(eth->h_proto) != ETH_P_IP) {
        return -1;
    }
    
    memcpy(packet->src_mac, eth->h_source, 6);
    memcpy(packet->dest_mac, eth->h_dest, 6);
    
    struct iphdr *ip = (struct iphdr*)(buffer + sizeof(struct ethhdr));
    
    if (ip->protocol != IPPROTO_UDP) {
        return -1;
    }
    
    packet->src_ip.s_addr = ip->saddr;
    packet->dest_ip.s_addr = ip->daddr;
    
    int ip_header_len = ip->ihl * 4;
    
    struct udphdr *udp = (struct udphdr*)(buffer + sizeof(struct ethhdr) + ip_header_len);
    
    packet->src_port = ntohs(udp->source);
    packet->dest_port = ntohs(udp->dest);
    
    int udp_data_len = ntohs(udp->len) - sizeof(struct udphdr);
    
    if (udp_data_len > 0) {
        packet->data = (unsigned char*)malloc(udp_data_len + 1);
        if (packet->data != NULL) {
            memcpy(packet->data, buffer + sizeof(struct ethhdr) + ip_header_len + sizeof(struct udphdr), udp_data_len);
            packet->data[udp_data_len] = '\0';
            packet->data_len = udp_data_len;
        } else {
            packet->data_len = 0;
        }
    } else {
        packet->data = NULL;
        packet->data_len = 0;
    }
    
    return 0;
}

void capture_packets() {
    unsigned char buffer[BUFFER_SIZE];
    packet_info_t packet;
    int packet_count = 0;
    
    printf("\nНачинаю захват пакетов...\n");
    printf("Нажмите Ctrl+C для остановки\n");
    printf("====================================\n");
    
    gettimeofday(&start_time, NULL);
    
    while (capture_running) {
        struct sockaddr saddr;
        socklen_t saddr_len = sizeof(saddr);
        
        int data_size = recvfrom(raw_sock, buffer, BUFFER_SIZE, 0, &saddr, &saddr_len);
        
        if (data_size < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recvfrom");
            continue;
        }
        
        if (parse_packet(buffer, data_size, &packet) == 0) {
            if (check_filter(&packet)) {
                gettimeofday(&packet.timestamp, NULL);
                
                print_packet_info(&packet);
                packet_count++;
                
                if (packet.data != NULL) {
                    free(packet.data);
                }
            } else {
                if (packet.data != NULL) {
                    free(packet.data);
                }
            }
        }
    }
    
    printf("\n====================================\n");
    printf("Захват завершен. Всего пакетов: %d\n", packet_count);
}

void select_filter() {
    printf("Выберите фильтр для захвата:\n");
    printf("1. Чат (порт 8888) - обязательный фильтр\n");
    printf("2. DNS (порт 53)\n");
    printf("3. HTTP (порт 80)\n");
    printf("4. Все UDP пакеты\n");
    printf("Ваш выбор: ");
    
    int choice;
    scanf("%d", &choice);
    getchar();
    
    switch(choice) {
        case 1:
            strcpy(filter_type, "chat");
            printf("Выбран фильтр: ЧАТ (порт 8888)\n");
            break;
        case 2:
            strcpy(filter_type, "dns");
            printf("Выбран фильтр: DNS (порт 53)\n");
            break;
        case 3:
            strcpy(filter_type, "http");
            printf("Выбран фильтр: HTTP (порт 80)\n");
            break;
        case 4:
            strcpy(filter_type, "all");
            printf("Выбран фильтр: Все UDP пакеты\n");
            break;
        default:
            strcpy(filter_type, "all");
            printf("Неверный выбор. Использую фильтр: Все UDP пакеты\n");
    }
}

int create_raw_socket(const char *iface) {
    int sock;
    struct sockaddr_ll sll;
    struct ifreq ifr;
    
    sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    // Получаем индекс интерфейса
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl");
        close(sock);
        return -1;
    }
    
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);
    
    if (bind(sock, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }
    
    return sock;
}

void select_interface(char *iface) {
    printf("Введите имя сетевого интерфейса (например, eth0, wlan0, lo): ");
    fgets(iface, IFNAMSIZ, stdin);
    iface[strcspn(iface, "\n")] = '\0';
    
    if (strlen(iface) == 0) {
        strcpy(iface, "eth0");
        printf("Использую интерфейс по умолчанию: eth0\n");
    }
}

void setup_output_file() {
    char filename[MAX_FILENAME];
    printf("Сохранить результаты в файл? (y/n): ");
    char answer[10];
    fgets(answer, sizeof(answer), stdin);
    
    if (answer[0] == 'y' || answer[0] == 'Y') {
        printf("Введите имя файла (по умолчанию output.txt): ");
        fgets(filename, MAX_FILENAME, stdin);
        filename[strcspn(filename, "\n")] = '\0';
        
        if (strlen(filename) == 0) {
            strcpy(filename, "output.txt");
        }
        
        output_file = fopen(filename, "w");
        if (output_file == NULL) {
            perror("fopen");
            printf("Не удалось создать файл. Результаты будут только на экране.\n");
        } else {
            printf("Результаты сохраняются в файл: %s\n", filename);
        }
    }
}

int main(int argc, char *argv[]) {
    char iface[IFNAMSIZ];
    
    printf("====================================\n");
    printf("UDP Sniffer - Захват UDP пакетов\n");
    printf("====================================\n");
    printf("Программа использует RAW сокеты\n");
    printf("Требуются права root (запускайте с sudo)\n\n");
    
    if (geteuid() != 0) {
        printf("ВНИМАНИЕ: Программа должна быть запущена с правами root!\n");
        printf("Запустите: sudo %s\n", argv[0]);
        exit(1);
    }
    
    signal(SIGINT, signal_handler);
    
    select_interface(iface);
    
    raw_sock = create_raw_socket(iface);
    if (raw_sock < 0) {
        fprintf(stderr, "Не удалось создать RAW сокет\n");
        exit(1);
    }
    
    printf("Сокет успешно создан на интерфейсе %s\n", iface);
    
    select_filter();
    setup_output_file();
    capture_packets();
    
    if (output_file != NULL) {
        fclose(output_file);
        printf("Результаты сохранены в файл.\n");
    }
    
    close(raw_sock);
    printf("Программа завершена.\n");
    
    return 0;
}