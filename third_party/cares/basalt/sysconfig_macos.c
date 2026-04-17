/* SPDX-License-Identifier: GPL-3.0-only */
#include "ares_private.h"

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

/* Only public default DNS configuration. Split/VPN resolver selection needs a separate contract. */
static ares_status_t append_strings(ares_buf_t* buffer, CFDictionaryRef config, CFStringRef key, const char* prefix,
                                    ares_bool_t combine) {
    CFTypeRef value = CFDictionaryGetValue(config, key);
    CFIndex i;
    CFIndex count;
    ares_status_t status;
    if (value == NULL)
        return ARES_SUCCESS;
    if (CFGetTypeID(value) != CFArrayGetTypeID())
        return ARES_EFORMERR;
    count = CFArrayGetCount((CFArrayRef)value);
    if (count > 64)
        return ARES_EFORMERR;
    for (i = 0; i < count; ++i) {
        CFTypeRef entry = CFArrayGetValueAtIndex((CFArrayRef)value, i);
        char text[1024];
        const unsigned char* cursor;
        if (CFGetTypeID(entry) != CFStringGetTypeID() ||
            !CFStringGetCString((CFStringRef)entry, text, sizeof(text), kCFStringEncodingUTF8))
            return ARES_EFORMERR;
        for (cursor = (const unsigned char*)text; *cursor; ++cursor) {
            if (*cursor <= 32 || *cursor == 127)
                return ARES_EFORMERR;
        }
        status = ares_buf_append_str(buffer, combine && i != 0 ? " " : prefix);
        if (status != ARES_SUCCESS)
            return status;
        status = ares_buf_append_str(buffer, text);
        if (status != ARES_SUCCESS)
            return status;
        if (!combine || i == count - 1) {
            status = ares_buf_append_str(buffer, "\n");
            if (status != ARES_SUCCESS)
                return status;
        }
    }
    return ARES_SUCCESS;
}

ares_status_t ares_init_sysconfig_macos(const ares_channel_t* channel, ares_sysconfig_t* sysconfig) {
    SCDynamicStoreRef store = NULL;
    CFStringRef key = NULL;
    CFPropertyListRef value = NULL;
    ares_buf_t* buffer = NULL;
    ares_status_t status;

    store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("BasaltDNS"), NULL, NULL);
    if (store != NULL)
        key = SCDynamicStoreKeyCreateNetworkGlobalEntity(kCFAllocatorDefault, kSCDynamicStoreDomainState, kSCEntNetDNS);
    if (key != NULL)
        value = SCDynamicStoreCopyValue(store, key);
    if (value == NULL) {
        status = ares_init_sysconfig_files(channel, sysconfig, ARES_TRUE);
        goto done;
    }
    if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
        status = ARES_EFORMERR;
        goto done;
    }
    status = ares_init_sysconfig_files(channel, sysconfig, ARES_FALSE);
    if (status != ARES_SUCCESS)
        goto done;
    buffer = ares_buf_create();
    if (buffer == NULL) {
        status = ARES_ENOMEM;
        goto done;
    }
    status = append_strings(buffer, (CFDictionaryRef)value, kSCPropNetDNSServerAddresses, "nameserver ", ARES_FALSE);
    if (status != ARES_SUCCESS)
        goto done;
    status = append_strings(buffer, (CFDictionaryRef)value, kSCPropNetDNSSearchDomains, "search ", ARES_TRUE);
    if (status == ARES_SUCCESS)
        status = ares_sysconfig_process_buf(channel, sysconfig, buffer, ares_sysconfig_parse_resolv_line);
done:
    ares_buf_destroy(buffer);
    if (value != NULL)
        CFRelease(value);
    if (key != NULL)
        CFRelease(key);
    if (store != NULL)
        CFRelease(store);
    return status;
}
