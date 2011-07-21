#ifndef ZOOKEEPER_DEFS_H
#define ZOOKEEPER_DEFS_H 1

#ifdef ENABLE_ZOOKEEPER

#define _SKIP_ZOOKEEPER_HTONLL
#include <zookeeper.h>
#include <zookeeper_log.h>
#include <uuid/uuid.h>
#include <json/json.h>

bool mc_zookeeper_init(void);
void mc_zookeeper_shutdown(void);
void mc_zookeeper_tick(void);

/*
 * field definitions for service announcement. Must match the java client.
 */
#define SD_SERVICE_NAME "serviceName"
#define SD_SERVICE_TYPE "serviceType"
#define SD_SERVICE_ID "serviceId"
#define SD_PROPERTIES "properties"

#define SD_PROP_SERVICE_SCHEME "serviceScheme"
#define SD_PROP_SERVICE_ADDRESS "serviceAddress"
#define SD_PROP_SERVICE_PORT "servicePort"

#endif

#endif /* ZOOKEEPER_DEFS_H */
