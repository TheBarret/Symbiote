#include <stdint.h>
#include <stddef.h>
#include <core/boot.h>

/* Limine & Kernel
 * The requests must not be optimised away: "used" + section placement does that,
 * and the linker script keeps the section.
 * The official limine.h provides the correct magic numbers, so we never hand-type them. */

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

bool boot_protocol_ok(void) {
    return LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision);
}

struct limine_framebuffer *boot_framebuffer(void) {
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1)
        return NULL;
    return framebuffer_request.response->framebuffers[0];
}
