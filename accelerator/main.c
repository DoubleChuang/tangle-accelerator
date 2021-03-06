#include <errno.h>

#include "common/logger.h"
#include "common/ta_errors.h"
#include "connectivity/http/http.h"
#include "utils/handles/signal.h"

#define MAIN_LOGGER "main"

static ta_core_t ta_core;
static ta_http_t ta_http;
static logger_id_t logger_id;

static void ta_stop(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    ta_http_stop(&ta_http);
  }
}

int main(int argc, char* argv[]) {
  if (signal_handle_register(SIGINT, ta_stop) == SIG_ERR || signal_handle_register(SIGTERM, ta_stop) == SIG_ERR) {
    return EXIT_FAILURE;
  }

  // Initialize logger
  if (ta_logger_init() != SC_OK) {
    return EXIT_FAILURE;
  }

  logger_id = logger_helper_enable(MAIN_LOGGER, LOGGER_DEBUG, true);

  // Initialize configurations with default value
  if (ta_core_default_init(&ta_core) != SC_OK) {
    return EXIT_FAILURE;
  }

  // Initialize configurations with file value
  if (ta_core_file_init(&ta_core, argc, argv) != SC_OK) {
    return EXIT_FAILURE;
  }

  // Initialize configurations with CLI value
  if (ta_core_cli_init(&ta_core, argc, argv) != SC_OK) {
    return EXIT_FAILURE;
  }

  if (ta_core_set(&ta_core) != SC_OK) {
    ta_log_error("Configure failed %s.\n", MAIN_LOGGER);
    return EXIT_FAILURE;
  }

  // Initialize apis cJSON lock
  if (apis_lock_init() != SC_OK) {
    ta_log_error("Lock initialization failed %s.\n", MAIN_LOGGER);
    return EXIT_FAILURE;
  }

  if (ta_http_init(&ta_http, &ta_core) != SC_OK) {
    ta_log_error("HTTP initialization failed %s.\n", MAIN_LOGGER);
    return EXIT_FAILURE;
  }

  if (ta_http_start(&ta_http) != SC_OK) {
    ta_log_error("Starting TA failed %s.\n", MAIN_LOGGER);
    goto cleanup;
  }

  log_info(logger_id, "Tangle-accelerator starts running\n");

  // Disable loggers when quiet mode is on
  if (quiet_mode) {
    // Destroy logger when quiet mode is on
    logger_helper_release(logger_id);
    logger_helper_destroy();
  } else {
    http_logger_init();
    apis_logger_init();
    cc_logger_init();
    pow_logger_init();
    timer_logger_init();
  }

  /* pause() cause TA to sleep until it catch a signal,
   * also the return value and errno should be -1 and EINTR on success.
   */
  int sig_ret = pause();
  if (sig_ret == -1 && errno != EINTR) {
    ta_log_error("Signal caught failed %s.\n", MAIN_LOGGER);
    return EXIT_FAILURE;
  }

cleanup:
  log_info(logger_id, "Destroying API lock\n");
  if (apis_lock_destroy() != SC_OK) {
    ta_log_error("Destroying api lock failed %s.\n", MAIN_LOGGER);
    return EXIT_FAILURE;
  }
  log_info(logger_id, "Destroying TA configurations\n");
  ta_core_destroy(&ta_core);

  if (quiet_mode == false) {
    http_logger_release();
    apis_logger_release();
    cc_logger_release();
    serializer_logger_release();
    pow_logger_release();
    timer_logger_release();
    logger_helper_release(logger_id);
    if (logger_helper_destroy() != RC_OK) {
      ta_log_error("Destroying logger failed %s.\n", MAIN_LOGGER);
      return EXIT_FAILURE;
    }
  }
  return 0;
}
