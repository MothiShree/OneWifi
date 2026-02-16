/************************************************************************************
  If not stated otherwise in this file or this component's LICENSE file the
  following copyright and licenses apply:

  Copyright 2018 RDK Management

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 **************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "wifi_hal.h"
#include "wifi_services_mgr.h"
#include "wifi_ctrl.h"

extern wifi_service_descriptor_t public_service_desc;
extern wifi_service_descriptor_t private_service_desc;
extern wifi_service_descriptor_t mesh_service_desc;

char *get_service_name_from_type(wifi_service_type_t type, wifi_service_name_t name)
{
    char *tmp = NULL;

    switch (type) {
        case wifi_service_type_private:
            tmp = name;
            strncpy(name, "Private", sizeof(wifi_service_name_t));
            break;

        case wifi_service_type_public:
            tmp = name;
            strncpy(name, "Public", sizeof(wifi_service_name_t));
            break;

        case wifi_service_type_mesh:
            tmp = name;
            strncpy(name, "Mesh", sizeof(wifi_service_name_t));
            break;

        case wifi_service_type_managed:
            tmp = name;
            strncpy(name, "Managed", sizeof(wifi_service_name_t));
            break;

        case wifi_service_type_dynamic_ps:
            tmp = name;
            strncpy(name, "Configurable Persistent", sizeof(wifi_service_name_t));
            break;

        case wifi_service_type_dynamic_tr:
            tmp = name;
            strncpy(name, "Configurable Transient", sizeof(wifi_service_name_t));
            break;

        default:
            break;
    }

    return tmp;
}

wifi_service_t *get_service_by_type(wifi_services_mgr_t *mgr, wifi_service_type_t type)
{
    wifi_service_t *svc = NULL;
    bool found = false;

    svc = hash_map_get_first(mgr->svcs_map);
    while (svc != NULL) {
        if (svc->desc.type == type) {
            found = true;
            break;
        }
        svc = hash_map_get_next(mgr->svcs_map, svc);
    }

    return (found == true) ? svc:NULL;
}

void services_mgr_delete_service(wifi_services_mgr_t *mgr, wifi_service_name_t name)
{
    wifi_service_t *svc;

    svc = hash_map_remove(mgr->svcs_map, name);
    if (svc == NULL) {
        return;
    }
    svc->desc.delete_nodes_fn(svc);
    free(svc);
}

void services_mgr_delete_service_by_type(wifi_services_mgr_t *mgr, wifi_service_type_t type)
{
    wifi_service_name_t name;

    if (get_service_name_from_type(type, name) == NULL) {
        return;
    }

    services_mgr_delete_service(mgr, name);
}

wifi_service_t *create_service(wifi_services_mgr_t *mgr,
                               rdk_wifi_radio_t *radio_config,
                               wifi_hal_capability_t *hal_cap,
                               const service_t *service)
{
    wifi_service_t *svc = NULL;
    struct timeval func_start, func_end;
    struct timeval block_start, block_end;
    long diff_ms = 0;

    gettimeofday(&func_start, NULL);

    wifi_util_dbg_print(WIFI_SERVICES,
        "%s:%d: MJ Enter create_service for %s\n",
        __func__, __LINE__, service->name);

    if (strncmp(service->name, "Public", 6) == 0) {

        svc = malloc(sizeof(wifi_service_t));
        if (!svc) {
            wifi_util_error_print(WIFI_SERVICES,
                "%s:%d: MJ malloc failed for Public\n",
                __func__, __LINE__);
            return NULL;
        }

        memset(svc, 0, sizeof(wifi_service_t));
        memcpy(&svc->desc, &public_service_desc,
               sizeof(wifi_service_descriptor_t));

        svc->nodes = hash_map_create();

        gettimeofday(&block_start, NULL);
        svc->desc.create_nodes_fn(svc, radio_config, hal_cap, service);
        gettimeofday(&block_end, NULL);

    } else if (strncmp(service->name, "Private", 7) == 0) {

        svc = malloc(sizeof(wifi_service_t));
        if (!svc) {
            wifi_util_error_print(WIFI_SERVICES,
                "%s:%d: MJ malloc failed for Private\n",
                __func__, __LINE__);
            return NULL;
        }

        memset(svc, 0, sizeof(wifi_service_t));
        memcpy(&svc->desc, &private_service_desc,
               sizeof(wifi_service_descriptor_t));

        svc->nodes = hash_map_create();

        gettimeofday(&block_start, NULL);
        svc->desc.create_nodes_fn(svc, radio_config, hal_cap, service);
        gettimeofday(&block_end, NULL);

    } else if (strncmp(service->name, "Mesh", 4) == 0) {

        wifi_util_dbg_print(WIFI_SERVICES,
            "%s:%d: MJ >>> Creating Mesh service <<<\n",
            __func__, __LINE__);

        svc = malloc(sizeof(wifi_service_t));
        if (!svc) {
            wifi_util_error_print(WIFI_SERVICES,
                "%s:%d: MJ malloc failed for Mesh\n",
                __func__, __LINE__);
            return NULL;
        }

        memset(svc, 0, sizeof(wifi_service_t));
        memcpy(&svc->desc, &mesh_service_desc,
               sizeof(wifi_service_descriptor_t));

        svc->nodes = hash_map_create();

        gettimeofday(&block_start, NULL);
        svc->desc.create_nodes_fn(svc, radio_config, hal_cap, service);
        gettimeofday(&block_end, NULL);

        diff_ms = (block_end.tv_sec - block_start.tv_sec) * 1000 +
                  (block_end.tv_usec - block_start.tv_usec) / 1000;

        if (diff_ms > 50) {   // log only if delay noticeable
            wifi_util_error_print(WIFI_SERVICES,
                "%s:%d: MJ Mesh create_nodes_fn took %ld ms\n",
                __func__, __LINE__, diff_ms);
        }
    }
    else {
        wifi_util_error_print(WIFI_SERVICES,
            "%s:%d: MJ Service descriptor: %s not found\n",
            __func__, __LINE__, service->name);
        return NULL;
    }

    svc->mgr = mgr;
    svc->ctrl = mgr->ctrl;
    svc->cap = hal_cap;

    gettimeofday(&func_end, NULL);

    diff_ms = (func_end.tv_sec - func_start.tv_sec) * 1000 +
              (func_end.tv_usec - func_start.tv_usec) / 1000;

    if (diff_ms > 100) {
        wifi_util_dbg_print(WIFI_SERVICES,
            "%s:%d: MJ create_service(%s) took %ld ms\n",
            __func__, __LINE__, service->name, diff_ms);
    }

    return svc;
}

int services_mgr_start_service(wifi_services_mgr_t *mgr, wifi_service_name_t name)
{
    wifi_service_t *svc;

    svc = hash_map_get(mgr->svcs_map, name);
    if (svc == NULL) {
        return -1;
    }

    return svc->desc.start_fn(svc);
}

int services_mgr_start_service_by_type(wifi_services_mgr_t *mgr, wifi_service_type_t type)
{
    wifi_service_name_t name;

    if (get_service_name_from_type(type, name) == NULL) {
        return -1;
    }

    return services_mgr_start_service(mgr, name);
}

int services_mgr_stop_service(wifi_services_mgr_t *mgr, wifi_service_name_t name)
{
    wifi_service_t *svc;

    svc = hash_map_get(mgr->svcs_map, name);
    if (svc == NULL) {
        return -1;
    }

    return svc->desc.stop_fn(svc);
}

int services_mgr_stop_service_by_type(wifi_services_mgr_t *mgr, wifi_service_type_t type)
{
    wifi_service_name_t name;

    if (get_service_name_from_type(type, name) == NULL) {
        return -1;
    }

    return services_mgr_stop_service(mgr, name);
}

int services_mgr_event(wifi_services_mgr_t *mgr, wifi_event_t *event)
{
    wifi_service_t *svc = NULL;

    svc = hash_map_get_first(mgr->svcs_map);
    while (svc != NULL) {
        svc->desc.event_fn(svc, event);
        svc = hash_map_get_next(mgr->svcs_map, svc);
    }

    return 0;
}

void services_mgr_deinit(wifi_services_mgr_t *mgr)
{

}

int services_mgr_init(wifi_ctrl_t *ctrl, rdk_wifi_radio_t *radio_config, wifi_hal_capability_t *hal_cap, const service_t *service, unsigned int num_svcs)
{
    unsigned int i;
    wifi_service_t *svc;
    wifi_services_mgr_t *svcs_mgr = &ctrl->svcs_mgr;

    svcs_mgr->ctrl = ctrl;
    svcs_mgr->cap = hal_cap;
    svcs_mgr->svcs_map = hash_map_create();

    for (i = 0; i < num_svcs; i++) {
        if ((svc = create_service(svcs_mgr, radio_config, hal_cap, service)) != NULL) {
            hash_map_put(svcs_mgr->svcs_map, strdup(service->name), svc);
        }
        service++;
    }

    return 0;
}

