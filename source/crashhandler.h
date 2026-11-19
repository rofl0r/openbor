/*
 * OpenBOR native crash reporting layer.
 *
 * The handler is intentionally small. It is intended to leave a useful crash
 * report on desktop Unix builds without adding runtime overhead to the game.
 */
#ifndef OPENBOR_CRASHHANDLER_H
#define OPENBOR_CRASHHANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

void bor_install_crash_handler(void);
void bor_crash_set_context(const char *context);

#ifdef __cplusplus
}
#endif

#endif
