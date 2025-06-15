#define _GNU_SOURCE
#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <glib.h>
#include <polkit/polkit.h>
#include <polkitagent/polkitagent.h>

static char *read_password(const char *prompt);

typedef struct {
    PolkitAgentListener parent_instance;
    PolkitAgentSession *session;
    GTask *task;
} SimpleAgent;

typedef struct {
    PolkitAgentListenerClass parent_class;
} SimpleAgentClass;

static GType simple_agent_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(SimpleAgent, simple_agent, POLKIT_AGENT_TYPE_LISTENER);

static void
on_request(PolkitAgentSession *session,
           const gchar *request,
           gboolean echo_on,
           gpointer user_data)
{
    gchar *password;
    
    (void)echo_on; (void)user_data;
    
    if (strstr(request, "Password")) {
        password = read_password(request);
        polkit_agent_session_response(session, password);
        memset(password, 0, strlen(password));
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

static char *
read_password(const char *prompt)
{
    struct termios old, new;
    char *password;
    int c, i = 0;
    
    password = malloc(256);
    printf("%s", prompt);
    fflush(stdout);
    
    tcgetattr(STDIN_FILENO, &old);
    new = old;
    new.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new);
    
    while ((c = getchar()) != '\n' && c != EOF && i < 255) {
        password[i++] = c;
    }
    password[i] = '\0';
    
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    printf("\n");
    
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

    (void)action_id; (void)message; (void)icon_name; (void)details;

    agent->task = g_task_new(listener, cancellable, callback, user_data);

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
}

int main(int argc, char *argv[])
{
    GMainLoop *loop;
    SimpleAgent *agent;
    PolkitSubject *subject;
    GError *error = NULL;

    (void)argc; (void)argv;

    if (geteuid() == 0) {
        fprintf(stderr, "Don't run as root\n");
        return 1;
    }

    agent = g_object_new(simple_agent_get_type(), NULL);
    subject = polkit_unix_session_new_for_process_sync(getpid(), NULL, &error);
    
    if (!subject) {
        fprintf(stderr, "Failed to get session: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    if (!polkit_agent_listener_register(POLKIT_AGENT_LISTENER(agent),
                                       POLKIT_AGENT_REGISTER_FLAGS_NONE,
                                       subject, NULL, NULL, &error)) {
        fprintf(stderr, "Failed to register agent: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    printf("Polkit agent registered\n");

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    g_object_unref(agent);
    g_object_unref(subject);

    return 0;
}
