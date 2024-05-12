
#define RD_ONLY 0x01
#define WR_ONLY 0x10
#define RD_WR 0x11

struct platform_data
{
    int size;
    int perm;
    const char *serial_number;
};