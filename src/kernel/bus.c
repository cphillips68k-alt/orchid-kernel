#include "bus.h"
#include "sync.h"
#include "vmm.h"
#include <stddef.h>

#define MAX_SERVICES 32
#define NAME_MAX 32

typedef struct {
    char name[NAME_MAX];
    int port;
} service_t;

static service_t services[MAX_SERVICES];
static int next_port = 0;
static int num_services = 0;
static spinlock_t bus_lock = 0;

int bus_register(const char *name) {
    spin_lock(&bus_lock);
    if (num_services >= MAX_SERVICES) {
        spin_unlock(&bus_lock);
        return -1;
    }

    /* Copy name */
    for (int i = 0; i < NAME_MAX-1 && name[i]; i++)
        services[num_services].name[i] = name[i];
    services[num_services].name[NAME_MAX-1] = 0;
    services[num_services].port = next_port++;
    int port = services[num_services].port;
    num_services++;
    spin_unlock(&bus_lock);
    return port;
}

int bus_lookup(const char *name) {
    spin_lock(&bus_lock);
    for (int i = 0; i < num_services; i++) {
        int match = 1;
        for (int j = 0; name[j] || services[i].name[j]; j++) {
            if (name[j] != services[i].name[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            spin_unlock(&bus_lock);
            return services[i].port;
        }
    }
    spin_unlock(&bus_lock);
    return -1;
}