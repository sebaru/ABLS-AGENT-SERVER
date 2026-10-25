#include <abls-agent-libs/abls-agent-libs.h>
#include <pwd.h>
#include <systemd/sd-login.h>

static GPtrArray *commands;
static guint fail_command;
static guint detached_commands;
static guint status_depth;
static guint unrefs;
static gint session_mode;
static gboolean package_installed;

static gint Test_run_shell ( const gchar *format, ... )
 { va_list args;
   va_start(args, format);
   g_ptr_array_add(commands, g_strdup_vprintf(format, args));
   va_end(args);
   return(commands->len == fail_command ? 1 : 0);
 }

static void Test_run_shell_detached ( const gchar *command, ... )
 { detached_commands++;
   g_ptr_array_add(commands, g_strdup(command));
 }

static void Test_info ( const gchar *function, const gchar *facility, const gchar *prefix,
                        guint priority, const gchar *format, ... )
 { }

static gchar *Test_get_tech_id ( struct ABLS_AGENT *agent ) { return("SERVER"); }
static gchar *Test_get_classe ( struct ABLS_AGENT *agent ) { return("server"); }
static gboolean Test_is_apt ( struct ABLS_AGENT *agent ) { return(TRUE); }

static gpointer Test_status_push ( struct ABLS_AGENT *agent, gchar *format, ... )
 { status_depth++;
   return(g_malloc(1));
 }

static void Test_status_pop ( struct ABLS_AGENT *agent, gpointer handle )
 { g_assert_cmpuint(status_depth, >, 0);
   status_depth--;
   g_free(handle);
 }

static gint Test_seat_get_active ( const gchar *seat, gchar **session, uid_t *uid )
 { if (session_mode != 1) return(-1);
   *session = g_strdup("test-session");
   *uid = 1000;
   return(0);
 }

static struct passwd *Test_getpwuid ( uid_t uid )
 { static struct passwd user = { .pw_name = "desktop", .pw_uid = 1000 };
   return(&user);
 }

static struct passwd *Test_getpwnam ( const gchar *name )
 { static struct passwd user = { .pw_name = "abls-agent-audio", .pw_uid = 1001 };
   g_assert_cmpstr(name, ==, "abls-agent-audio");
   return(session_mode == 0 ? &user : NULL);
 }

static gchar *Test_find_program ( const gchar *name )
 { return(package_installed ? g_strdup("/test/agent") : NULL);
 }

static void Test_json_unref ( JsonNode *node )
 { unrefs++;
   json_node_unref(node);
 }

#define main Agent_server_main
#define Run_shell Test_run_shell
#define Run_shell_detached Test_run_shell_detached
#define Info Test_info
#define Agent_get_tech_id Test_get_tech_id
#define Agent_get_classe Test_get_classe
#define Agent_is_apt Test_is_apt
#define Agent_status_push Test_status_push
#define Agent_status_pop Test_status_pop
#define sd_seat_get_active Test_seat_get_active
#define getpwuid Test_getpwuid
#define getpwnam Test_getpwnam
#define g_find_program_in_path Test_find_program
#define Json_unref Test_json_unref
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "../src/server.c"
#pragma GCC diagnostic pop
#undef main

static void Test_reset ( void )
 { g_assert_cmpuint(status_depth, ==, 0);
   g_ptr_array_set_size(commands, 0);
   fail_command = 0;
   detached_commands = 0;
   unrefs = 0;
   session_mode = 1;
   package_installed = TRUE;
 }

static void Test_sequence ( const gchar *classe, const gchar *unit )
 { const gchar *actions[] = { "disable", "stop", "enable", "start" };
   Test_reset();
   g_assert_true(Agent_restart_one((gchar *)classe, "TEST", "Test agent"));
   g_assert_cmpuint(commands->len, ==, G_N_ELEMENTS(actions));
   for (guint index = 0; index < G_N_ELEMENTS(actions); index++)
    { gchar *expected = g_strdup_printf("sudo -n systemctl %s%s%s", actions[index],
                                        index == 1 && g_strcmp0(classe, "DLS") != 0 ? "    " : " ", unit);
      g_assert_cmpstr(g_ptr_array_index(commands, index), ==, expected);
      g_free(expected);
    }
   g_assert_cmpuint(detached_commands, ==, 0);
   g_assert_cmpuint(status_depth, ==, 0);
 }

static void Test_generic ( void ) { Test_sequence("modbus", "abls-agent-modbus@TEST"); }
static void Test_dls ( void ) { Test_sequence("DLS", "abls-agent-dls"); }

static void Test_failures ( void )
 { for (guint index = 1; index <= 4; index++)
    { Test_reset();
      fail_command = index;
      g_assert_false(Agent_restart_one("modbus", "TEST", NULL));
      g_assert_cmpuint(commands->len, ==, index);
      g_assert_cmpuint(status_depth, ==, 0);
    }
   Test_reset();
   package_installed = FALSE;
   fail_command = 3;
   g_assert_false(Agent_restart_one("modbus", "TEST", NULL));
   g_assert_cmpuint(commands->len, ==, 3);
   g_assert_cmpstr(g_ptr_array_index(commands, 2), ==, "sudo -n apt install -y abls-agent-modbus");
   g_assert_cmpuint(status_depth, ==, 0);
 }

static void Test_audio ( void )
 { const gchar *actions[] = { "disable", "stop", "enable", "start" };
   for (gint mode = -1; mode <= 1; mode++)
    { Test_reset();
      session_mode = mode;
      g_assert_cmpint(Agent_restart_one("audio", "TEST", NULL), ==, mode >= 0);
      g_assert_cmpuint(commands->len, ==, mode < 0 ? 0 : 4);
      for (guint index = 0; index < commands->len; index++)
       { guint uid = mode == 1 ? 1000 : 1001;
         gchar *expected = g_strdup_printf("env XDG_RUNTIME_DIR=/run/user/%u DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/%u/bus "
                                           "sudo -n -E -u %s systemctl --user %s abls-agent-audio@TEST",
                                           uid, uid, mode == 1 ? "desktop" : "abls-agent-audio", actions[index]);
         g_assert_cmpstr(g_ptr_array_index(commands, index), ==, expected);
         g_free(expected);
       }
      g_assert_cmpuint(status_depth, ==, 0);
    }
   Test_reset();
   session_mode = -1;
   g_assert_false(Agent_start_one("audio", "TEST", NULL));
   g_assert_cmpuint(status_depth, ==, 0);
 }

static void Test_server ( void )
 { Test_reset();
   g_assert_true(Agent_restart_one("server", "SERVER", NULL));
   g_assert_cmpuint(commands->len, ==, 2);
   g_assert_cmpstr(g_ptr_array_index(commands, 0), ==, "sudo -n systemctl enable abls-agent-server");
   g_assert_cmpstr(g_ptr_array_index(commands, 1), ==, "sudo -n systemctl restart abls-agent-server");
   g_assert_cmpuint(detached_commands, ==, 1);
   g_assert_cmpuint(status_depth, ==, 0);
   Test_reset();
   fail_command = 1;
   g_assert_false(Agent_restart_one("server", "SERVER", NULL));
   g_assert_cmpuint(detached_commands, ==, 0);
   g_assert_cmpuint(status_depth, ==, 0);
 }

static void Test_wrappers ( void )
 { GThreadFunc wrappers[] = { Agent_stop_thread, Agent_restart_thread, Agent_upgrade_thread };
   guint expected_counts[] = { 2, 4, 6 };
   for (guint index = 0; index < G_N_ELEMENTS(wrappers); index++)
    { Test_reset();
      JsonNode *message = json_from_string("{\"agent_classe\":\"modbus\",\"mqtt_topic_lvl4\":\"TEST\","
                                           "\"mqtt_topic\":\"domain/AGENT/SERVER/RESTART/TEST\",\"description\":\"Test\"}", NULL);
      wrappers[index](message);
      g_assert_cmpuint(unrefs, ==, 1);
      g_assert_cmpuint(commands->len, ==, expected_counts[index]);
      g_assert_cmpuint(status_depth, ==, 0);
      Test_reset();
      wrappers[index](json_from_string("{\"agent_classe\":null,\"mqtt_topic_lvl4\":null,\"description\":null}", NULL));
      g_assert_cmpuint(unrefs, ==, 1);
      g_assert_cmpuint(commands->len, ==, 0);
      g_assert_cmpuint(status_depth, ==, 0);
    }
 }

gint main ( gint argc, gchar **argv )
 { g_test_init(&argc, &argv, NULL);
   commands = g_ptr_array_new_with_free_func(g_free);
   g_test_add_func("/restart/generic", Test_generic);
   g_test_add_func("/restart/dls", Test_dls);
   g_test_add_func("/restart/failures", Test_failures);
   g_test_add_func("/restart/audio", Test_audio);
   g_test_add_func("/restart/server", Test_server);
   g_test_add_func("/restart/wrappers", Test_wrappers);
   gint result = g_test_run();
   g_ptr_array_unref(commands);
   return(result);
 }