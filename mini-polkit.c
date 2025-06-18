#define _GNU_SOURCE
#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <glib.h>
#include <glib/gmem.h>
#include <polkit/polkit.h>
#include <polkitagent/polkitagent.h>

static char *shell_escape(char *cmd);
static char *get_password(char *cmd);

typedef struct {
    PolkitAgentListener parent_instance;
    PolkitAgentSession *session;
    GTask *task;
    char *cmd;
    char *current_message;
} SimpleAgent;

typedef struct {
    PolkitAgentListenerClass parent_class;
} SimpleAgentClass;

static GType simple_agent_get_type(void) G_GNUC_CONST;

static GMainLoop *loop;

G_DEFINE_TYPE(SimpleAgent, simple_agent, POLKIT_AGENT_TYPE_LISTENER);

static void handle_signal(int sig) {
    (void)sig;
    g_main_loop_quit(loop);
}

static void
on_request(PolkitAgentSession *session,
           const gchar *request,
           gboolean echo_on,
           gpointer user_data)
{
    SimpleAgent *agent = (SimpleAgent *)user_data;
    gchar *password;

    (void)echo_on;

    if (strstr(request, "Password") || strstr(request, "password")) {
        password = get_password(g_strjoinv(shell_escape(agent->current_message), g_strsplit(agent->cmd, "{{MESSAGE}}", -1)));
        if (password) {
            polkit_agent_session_response(session, password);
            memset(password, 0, strlen(password));
            g_free(password);
        } else {
            polkit_agent_session_response(session, "");
        }
        free(password);
    }
}

static void
on_completed(PolkitAgentSession *session,
             gboolean gained_authorization,
             gpointer user_data)
{
    SimpleAgent *agent = (SimpleAgent *)user_data;

    (void)session;

    if (gained_authorization) {
        g_task_return_boolean(agent->task, TRUE);
    } else {
        g_task_return_new_error(agent->task, POLKIT_ERROR,
                               POLKIT_ERROR_NOT_AUTHORIZED,
                               "Authentication failed");
    }

    g_object_unref(agent->task);
    agent->task = NULL;
    g_object_unref(agent->session);
    agent->session = NULL;
}

static char
*get_cmd(int argc, char *argv[])
{
    if (argc < 2) return NULL;

    return g_strjoinv(" ", &argv[1]);
}

static char *
shell_escape(char *str) {
    GString *escaped = g_string_new("");

    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':  g_string_append(escaped, "ʺ"); break;  // ʺ
            case '\'': g_string_append(escaped, "ʹ"); break;  // ʹ
            case '`':  g_string_append(escaped, "ˋ"); break;  // ˋ
            default:   g_string_append_c(escaped, *p);   break;
        }
    }

    return g_string_free(escaped, FALSE);
}

static char
*get_password(char *cmd)
{
    if (!cmd) return NULL;

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    char *password = NULL;
    size_t len = 0;

    if (getline(&password, &len, fp) == -1 || !password || strlen(password) == 0) {
        free(password);
        password = NULL;
    } else {
        password[strcspn(password, "\n")] = '\0';
    }

    pclose(fp);
    return password;
}

static void
initiate_authentication(PolkitAgentListener *listener,
                       const gchar *action_id,
                       const gchar *message,
                       const gchar *icon_name,
                       PolkitDetails *details,
                       const gchar *cookie,
                       GList *identities,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer user_data)
{
    SimpleAgent *agent = (SimpleAgent *)listener;
    PolkitIdentity *identity;

    (void)action_id; (void)icon_name; (void)details;

    agent->task = g_task_new(listener, cancellable, callback, user_data);

    g_free(agent->current_message);
    agent->current_message = g_strdup(message ? message : "Authentication required");

    if (!identities) {
        g_task_return_new_error(agent->task, POLKIT_ERROR, POLKIT_ERROR_FAILED,
                               "No identities provided");
        g_object_unref(agent->task);
        agent->task = NULL;
        return;
    }

    identity = POLKIT_IDENTITY(identities->data);
    agent->session = polkit_agent_session_new(identity, cookie);

    g_signal_connect(agent->session, "request", G_CALLBACK(on_request), agent);
    g_signal_connect(agent->session, "completed", G_CALLBACK(on_completed), agent);

    polkit_agent_session_initiate(agent->session);
}

static gboolean
initiate_authentication_finish(PolkitAgentListener *listener,
                              GAsyncResult *res,
                              GError **error)
{
    (void)listener;
    return g_task_propagate_boolean(G_TASK(res), error);
}

static void
simple_agent_class_init(SimpleAgentClass *klass)
{
    PolkitAgentListenerClass *listener_class;

    listener_class = POLKIT_AGENT_LISTENER_CLASS(klass);
    listener_class->initiate_authentication = initiate_authentication;
    listener_class->initiate_authentication_finish = initiate_authentication_finish;
}

static void
simple_agent_init(SimpleAgent *agent)
{
    agent->session = NULL;
    agent->task = NULL;
    agent->cmd = NULL;
}

int main(int argc, char *argv[])
{
    SimpleAgent *agent;
    PolkitSubject *subject;
    GError *error = NULL;
    char *cmd;

    if (geteuid() == 0) {
        fprintf(stderr, "Don't run as root\n");
        return 1;
    }

    cmd = get_cmd(argc, argv);
    if (!cmd) {
        fprintf(stderr,
            "Usage: %s <command>\n"
            "  <command>: shell command to prompt user for password\n"
            "  Use {{MESSAGE}} to include the polkit prompt message\n\n"
            "Examples:\n"
            "  %s \"rofi -dmenu -password\"\n"
            "  %s \"zenity --password --title='{{MESSAGE}}'\"\n"
            "  %s \"echo '{{MESSAGE}}' | dmenu -p 'Password:'\"\n\n"
            "Note:\n"
            "  Should be run in an X session (e.g. from ~/.xinitrc)\n"
            "  Should also run as a background processs using &\n"
            "  Example:\n"
            "    %s \"rofi -dmenu -password -p '{{MESSAGE}}'\" &\n",
            argv[0], argv[0], argv[0], argv[0], argv[0]
        );
        return 1;
    }

    agent = g_object_new(simple_agent_get_type(), NULL);
    agent->cmd = cmd;
    subject = polkit_unix_session_new_for_process_sync(getpid(), NULL, &error);

    if (!subject) {
        fprintf(stderr, "Failed to get session: %s\n", error->message);
        g_error_free(error);
        g_free(cmd);
        g_object_unref(agent);
        return 1;
    }

    if (!polkit_agent_listener_register(POLKIT_AGENT_LISTENER(agent),
                                       POLKIT_AGENT_REGISTER_FLAGS_NONE,
                                       subject, NULL, NULL, &error)) {
        fprintf(stderr, "Failed to register agent: %s\n", error->message);
        g_error_free(error);
        g_free(cmd);
        g_object_unref(agent);
        g_object_unref(subject);
        return 1;
    }

    loop = g_main_loop_new(NULL, FALSE);
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    g_free(cmd);
    g_object_unref(agent);
    g_object_unref(subject);

    return 0;
}
