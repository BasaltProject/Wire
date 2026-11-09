#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/* Called before crypto context creation; installs Windows mutex callbacks once. */
void basalt_mbedtls_threading_init(void);
#ifdef __cplusplus
}
#endif
