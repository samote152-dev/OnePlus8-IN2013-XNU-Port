#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../S6SY771/s6sy771_board_bridge.h"

struct mock { int started, stopped, irq, reads, reports; };
static int bus_start(void *p) { ((struct mock*)p)->started++; return 0; }
static void bus_stop(void *p) { ((struct mock*)p)->stopped++; }
static int irq_pending(void *p) { return ((struct mock*)p)->irq; }
static int read_command(void *p, uint8_t cmd, uint8_t *buf, size_t len) {
    struct mock *m=p; (void)cmd; m->reads++;
    memset(buf, 0, len);
    return 0;
}
static void report(void *p, unsigned slot, int down, unsigned x, unsigned y) {
    struct mock *m=p; (void)slot; (void)down; (void)x; (void)y; m->reports++;
}
int main(void) {
    struct mock m={0}; struct s6sy771_board_bridge b;
    const struct s6sy771_board_ops ops={bus_start,bus_stop,irq_pending,read_command,0,report};
    assert(s6sy771_board_bridge_start(&b,&ops,&m,&m)==0);
    assert(b.state==S6SY771_BRIDGE_READY && m.started==1);
    assert(s6sy771_board_bridge_handle_irq(&b)==0 && m.reads==0);
    m.irq=1;
    /* Empty event queue is a successful no-op; the transport reads the count command. */
    assert(s6sy771_board_bridge_handle_irq(&b)==0 && m.reads>=1);
    s6sy771_board_bridge_stop(&b);
    assert(b.state==S6SY771_BRIDGE_OFF && m.stopped==1);
    puts("S6SY771 board/HID bridge tests: 4/4 checks passed");
    return 0;
}
