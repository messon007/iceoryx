// Copyright (c) 2020, 2021 by Robert Bosch GmbH, Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "iceoryx_binding_c/publisher.h"
#include "iceoryx_binding_c/runtime.h"
#include "sleep_for.h"
#include "topic_data.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool killswitch = false;

static void sigHandler(int signalValue)
{
    (void)signalValue;
    // caught SIGINT, now exit gracefully
    killswitch = true;
}

void sending()
{
    iox_runtime_init("iox-c-publisher");

    const uint64_t historyRequest = 10U;
    struct cpp2c_PublisherOptions options;
    strncpy(options.nodeName, "iox-c-publisher-node", MAX_NODE_NAME_LENGTH_ON_C);
    iox_pub_storage_t publisherStorage;
    iox_pub_t publisher = iox_pub_init(&publisherStorage, "Radar", "FrontLeft", "Object", historyRequest, options);

    iox_pub_offer(publisher);

    double ct = 0.0;

    while (!killswitch)
    {
        void* chunk = NULL;
        if (AllocationResult_SUCCESS == iox_pub_allocate_chunk(publisher, &chunk, sizeof(struct RadarObject)))
        {
            struct RadarObject* sample = (struct RadarObject*)chunk;

            sample->x = ct;
            sample->y = ct;
            sample->z = ct;

            printf("Sent value: %.0f\n", ct);

            iox_pub_send_chunk(publisher, chunk);

            ++ct;

            sleep_for(400);
        }
        else
        {
            printf("Failed to allocate chunk!");
        }
    }

    iox_pub_stop_offer(publisher);
    iox_pub_deinit(publisher);
}

int main()
{
    signal(SIGINT, sigHandler);

    sending();

    return 0;
}
