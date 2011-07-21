/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "memcached.h"

/* zookeeper mgt */
static bool build_announcement_info(void);
static bool build_announcement_content(void);
static char *get_announce_ip(void);
static void node_completion(int, const struct String_vector *, const void *);
static void void_completion(int, const char *, const void *);
static void trigger_event(void);
static void zookeeper_event_handler(int, short, void *);
static const char* state2String(int);
static void process(zhandle_t *, int, int, const char *, void *);
static void close_zookeeper(void);

/**
 * Called from the main memcached code to initialize zookeeper code.
 */
bool mc_zookeeper_init() {
    //
    // Initialize Zookeeper stuff
    //
    switch (settings.verbose) {
    case 0:
        zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);
        break;
    case 1:
        zoo_set_debug_level(ZOO_LOG_LEVEL_INFO);
        break;
    default:
        zoo_set_debug_level(ZOO_LOG_LEVEL_DEBUG);
        break;
    }

    // enable deterministic order
    zoo_deterministic_conn_order(1); 

    // Build the name of the announcement name and the path on the zookeeper service

    bool result = true;

    result &= build_announcement_info();
    result &= build_announcement_content();

    LOG_DEBUG(("Init called,%s starting zookeeper!", result ? "" : "not "));

    return result;
}

/**
 * Called from the main memcached code on shutdown to clean up the zookeeper connection.
 */
void mc_zookeeper_shutdown() {
    close_zookeeper();
    LOG_DEBUG(("Zookeeper shutdown complete"));
}

char *announce_name = NULL;
char *announce_path = NULL;
char *announce_id = NULL;

/**
 * sets up the name, path and id for the service discovery announcement.
 */
static bool build_announcement_info() {
    char *safe_service_type = settings.zookeeper_service_type ? settings.zookeeper_service_type : "";

    uuid_t uuid;
    uuid_generate(uuid);

    // 36 char for UUID + \0
    announce_id = malloc(37);
    uuid_unparse(uuid, announce_id);
    announce_id[36] = '\0';
    
    // len of name + len of type if present + 36 for uuid + 2 for - + '\0'
    int announce_name_len = strlen(settings.zookeeper_service_name) + strlen(safe_service_type) + 39;
    announce_name = malloc(announce_name_len);
    snprintf(announce_name, announce_name_len, "%s-%s-%s", settings.zookeeper_service_name, safe_service_type, announce_id);
    
    // len of path + len of name + '/' + '\0'
    int announce_path_len = strlen(settings.zookeeper_path) + strlen(announce_name) + 2;
    announce_path = malloc(announce_path_len);
    snprintf(announce_path, announce_path_len, "%s/%s", settings.zookeeper_path, announce_name);
    LOG_DEBUG(("Announcement path is %s", announce_path));

    return true;
}

const char *announce_data = NULL;

/**
 * Creates service discovery announcement content.
 *
 * {
 *     "serviceType" : "user",
 *     "serviceName" : "memcached",
 *     "serviceId" : "eebf4159-c0d4-4d27-b002-43df95ef5824",
 *     "properties" : {
 *        "servicePort" : "21212",
 *        "serviceAddress" : "192.168.1.32",
 *        "serviceScheme" : "auto-negotiate"
 * }
 */
static bool build_announcement_content() {
    json_object * announce_object;
    json_object * properties_object;

    announce_object = json_object_new_object();
    properties_object = json_object_new_object();
    json_object_object_add(announce_object, SD_SERVICE_NAME, json_object_new_string(settings.zookeeper_service_name));
    json_object_object_add(announce_object, SD_SERVICE_ID,   json_object_new_string(announce_id));
    if (settings.zookeeper_service_type) {
        json_object_object_add(announce_object, SD_SERVICE_TYPE, json_object_new_string(settings.zookeeper_service_type));
    }

    json_object_object_add(properties_object, SD_PROP_SERVICE_SCHEME, json_object_new_string(prot_text(settings.binding_protocol)));
    json_object_object_add(properties_object, SD_PROP_SERVICE_PORT,   json_object_new_int(settings.port));
    json_object_object_add(announce_object,   SD_PROPERTIES,          properties_object);

    char *announce_ip = get_announce_ip();

    if (!announce_ip) {
        LOG_ERROR(("Could not determine announcement IP, vetoing announcement!"));
        return false;
    }

    json_object_object_add(properties_object, SD_PROP_SERVICE_ADDRESS, json_object_new_string(announce_ip));

    announce_data = json_object_get_string(announce_object);
    LOG_DEBUG(("Announcement data is %s", announce_data));

    return true;
}

/**
 * Get the IP address that should be announced. 
 */
static char *get_announce_ip()
{
    struct addrinfo *ai;
    struct addrinfo *next;

    struct addrinfo hints = { 
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    char * result = NULL;

    int error= getaddrinfo(settings.inter, NULL, &hints, &ai);
    if (error != 0) {
        if (error != EAI_SYSTEM) {
            LOG_ERROR(("getaddrinfo(): %s", gai_strerror(error)));
        }
        else {
          perror("getaddrinfo()");
        }
    }
    else {
        for (next= ai; next; next= next->ai_next) {
            if (next->ai_family == AF_INET) {
                struct sockaddr_in * foo = (struct sockaddr_in *) next->ai_addr;
                if (foo->sin_addr.s_addr == htonl(INADDR_ANY)) {
                    continue;
                }
                else if (foo->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
                    continue;
                }
                else {
                    // 4*3 + dots + \0 = 16
                    result = malloc(16);
                    getnameinfo(next->ai_addr, next->ai_addrlen, result, 16, NULL, 0, NI_NUMERICHOST);
                    break; // for
                }
            }
        }
        freeaddrinfo(ai);
    }
    return result;
}

bool connected = false;
unsigned long ticker = 0L;

// Current generation. Processing only happens if the generation
// changes (which implies a local state change). Start with 1 so that a
// full scan happens right after connection.
unsigned long generation = 1L;
unsigned long last_generation = 0;

zhandle_t *zh;
clientid_t myid;

/**
 * periodically called by the main memcache loop. This will connect zookeeper if
 * necessary. Also, if the generation has changed (because of a zookeeper even),
 * it will then trigger data processing.
 */
void mc_zookeeper_tick() {
    ticker++;

    if (!zh) { 
        LOG_DEBUG(("Connecting zookeeper..."));
        zh = zookeeper_init(settings.zookeeper_connect, process, 30000, &myid, 0, 0);
        if (!zh) {
            LOG_INFO(("Could not connect to zookeeper, error: %d", errno));
        }
        else {
            LOG_DEBUG(("Zookeeper connection ok, status is %d", zoo_state(zh)));
        }
        trigger_event();
    }

    if (connected) {
        long current_generation = generation;
        if (last_generation < current_generation) {
            LOG_DEBUG(("Tick (%d)", ticker));
            int rc = zoo_aget_children(zh, settings.zookeeper_path, 1, node_completion, &ticker);
            if (rc != 0) {
                LOG_WARN(("Error %s while retrieving children!", zerror(rc)));
            }
            else {
                last_generation = current_generation;
            }
        }
    }
}

/**
 * Callback with the list of children. If the local announcement is not present, create an ephemeral node on zk.
 */
static void node_completion(int rc, const struct String_vector *strings, const void *data) {
    if (rc) {
        LOG_WARN(("Error %s while retrieving children!", zerror(rc)));
        return;
    }

    unsigned long tick = *((unsigned long *)data);
    LOG_DEBUG(("Callback Tick (%d/%lx)", tick));

    if (!strings) {
        return;
    }

    for (int i = 0; i < strings->count; i++) {
        if (!strcmp(strings->data[i], announce_name)) {
            LOG_DEBUG(("Found local announcement %s", announce_name));
            return;
        }
    }
    
    // Need to add local announcement.
    LOG_DEBUG(("Local announcement not found, creating %s", announce_path));
    int rc2 = zoo_acreate(zh, announce_path, announce_data, strlen(announce_data), &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, void_completion, NULL);
    if (rc2) {
        LOG_WARN(("Error %s while creating announcement!", zerror(rc)));
    }
}

/**
 * Callback for the node creation.
 */
static void void_completion(int rc, const char *name, const void *data) {
    if (rc) {
        LOG_WARN(("Error %s while creating announcement!", zerror(rc)));
    }
    else {
        LOG_DEBUG(("Node creation complete, service now announced!"));
    }
}

// Zookeeper File descriptor
int fd;         

// libevent event;
struct event zk_event;

/**
 * register zk with libevent for callbacks. This gets driven through the
 * default timeout from zookeeper_interest (and subsequently whether there is
 * need to read/write data.
 */
static void trigger_event()
{
    int interest = 0;
    struct timeval tv;

    if (zh) {
        int res = zookeeper_interest(zh, &fd, &interest, &tv);
        if (res < 0) {
            LOG_WARN(("Error while checking zookeeper: %d", res));
        }
        else {
            // Always run with timeout.
            short event_flags = EV_TIMEOUT;

            if (fd != -1) {
                if (interest & ZOOKEEPER_READ) {
                    LOG_DEBUG(("READ ->"));
                    event_flags |= EV_READ;
                }

                if (interest & ZOOKEEPER_WRITE) {
                    LOG_DEBUG(("WRITE ->"));
                    event_flags |= EV_WRITE;
                }
            }
            else {
                LOG_DEBUG(("Not interested in anything right now..."));
            }

            LOG_DEBUG(("Triggering event (%d/%d)", tv.tv_sec, tv.tv_usec));
            event_set(&zk_event, fd, event_flags, zookeeper_event_handler, NULL);
            event_add(&zk_event, &tv);
        }
    }
    else {
        LOG_DEBUG(("No zookeeper handle!"));
    }
}

/**
 * libevent handler for zookeeper events on the fd. 
 */
static void zookeeper_event_handler(int fd, short event_type, void * arg)
{
    int event_flags = 0;

    if (event_type & EV_READ) {
        LOG_DEBUG((" -> READ"));
        event_flags |= ZOOKEEPER_READ;
    }
    if (event_type & EV_WRITE) {
        LOG_DEBUG((" -> WRITE"));
        event_flags |= ZOOKEEPER_WRITE;
    }

    if (event_flags) {
        if (zh) {
            zookeeper_process(zh, event_flags);
        }
        else {
            LOG_INFO(("Event handler called with zh == null!"));
        }
    }
    else {
        LOG_DEBUG(("Called from timeout!"));
    }
    
    trigger_event();
}


/**
 * Translate zookeeper status into string.
 */
static const char* state2String(int state){
  if (state == 0)
    return "CLOSED_STATE";
  if (state == ZOO_CONNECTING_STATE)
    return "CONNECTING_STATE";
  if (state == ZOO_ASSOCIATING_STATE)
    return "ASSOCIATING_STATE";
  if (state == ZOO_CONNECTED_STATE)
    return "CONNECTED_STATE";
  if (state == ZOO_EXPIRED_SESSION_STATE)
    return "EXPIRED_SESSION_STATE";
  if (state == ZOO_AUTH_FAILED_STATE)
    return "AUTH_FAILED_STATE";

  return "INVALID_STATE";
}

/**
 * Zookeeper callback when an event occurs.
 */
static void process(zhandle_t *zk_handle, int type, int state, const char *path, void *context)
{
    LOG_DEBUG(("Watcher %d state = %s", type, state2String(state)));

    if (type == ZOO_SESSION_EVENT) {
        if (state == ZOO_AUTH_FAILED_STATE) {
            // We don't do auth.
            LOG_WARN(("Got an auth request from zookeeper. Server config is not compatible to this client!"));
        }
        else if (state == ZOO_CONNECTED_STATE) {
            const clientid_t *id = zoo_client_id(zk_handle);
            if (myid.client_id == 0 || myid.client_id != id->client_id) {
                myid = *id;
                LOG_DEBUG(("Got a new session id: 0x%llx",(long long int) myid.client_id));
            }
            connected = true;
            generation++;
            LOG_DEBUG(("Client now connected!"));
        }
        else if (state == ZOO_EXPIRED_SESSION_STATE) {
            LOG_DEBUG(("Session expired, closing zookeeper"));
            connected = false;
            close_zookeeper();
        }
        else {
            LOG_DEBUG(("Disconnecting"));
            connected = false;
        }
    } 
    else {
        generation++;
        LOG_DEBUG(("Ignoring non-session event"));
    }
}

static void close_zookeeper() {
    if (zh) {
        zookeeper_close(zh);
        zh = NULL;
    }
}
